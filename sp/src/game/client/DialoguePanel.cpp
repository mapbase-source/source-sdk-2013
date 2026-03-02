#include "cbase.h"
#include "IDialoguePanel.h"
#include <vgui/IVGui.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/RichText.h>
#include <vgui_controls/Label.h>
#include "KeyValues.h"
#include "filesystem.h"
#include "c_baseplayer.h"
#include "cliententitylist.h"
#include "hud_macros.h"
#include "engine/IEngineSound.h"

#define TYPEWRITER_BASE_SPEED 2.0f // Base characters per tick at speed multiplier 1.0
#define DIALOGUE_DEFAULT_FOV 75    // Default player FOV
#define DIALOGUE_ZOOM_RATE 0.3f    // How fast to zoom in/out (seconds)

using namespace vgui;

// Message types matching the server
#define DIALOGUE_MSG_START    0
#define DIALOGUE_MSG_STOP     1
#define DIALOGUE_MSG_NODE     2
#define DIALOGUE_MSG_SETTINGS 3

void __MsgFunc_DialogueMsg(bf_read &msg)
{
	if (!g_pDialoguePanel)
		return;

	int type = msg.ReadByte();

	switch (type)
	{
	case DIALOGUE_MSG_START:
		{
			char str1[256], str2[64];
			msg.ReadString(str1, sizeof(str1));
			msg.ReadString(str2, sizeof(str2));
			g_pDialoguePanel->LoadFile(str1);
			if (str2[0])
				g_pDialoguePanel->ShowNode(str2);
			g_pDialoguePanel->Show();
		}
		break;

	case DIALOGUE_MSG_STOP:
		g_pDialoguePanel->Hide();
		break;

	case DIALOGUE_MSG_SETTINGS:
		{
			bool bTypewriter = msg.ReadByte() != 0;
			float flSpeed = msg.ReadFloat();
			char szTypewriterSound[256], szOpenSound[256], szCloseSound[256];
			msg.ReadString(szTypewriterSound, sizeof(szTypewriterSound));
			msg.ReadString(szOpenSound, sizeof(szOpenSound));
			msg.ReadString(szCloseSound, sizeof(szCloseSound));
			g_pDialoguePanel->ApplySettings(bTypewriter, flSpeed, szTypewriterSound, szOpenSound, szCloseSound);
		}
		break;
	}
}

//-----------------------------------------------------------------------------
// Tag parsing helper
// Tries to parse a tag at position p in the format <name=value>
// Returns the number of characters consumed (0 if no tag matched)
//-----------------------------------------------------------------------------
static int ParseTag(const char* p, const char* tagName, char* outValue, int outValueSize)
{
	// Build expected prefix: "<tagName="
	char prefix[64];
	Q_snprintf(prefix, sizeof(prefix), "<%s=", tagName);
	int prefixLen = Q_strlen(prefix);

	if (Q_strnicmp(p, prefix, prefixLen) != 0)
		return 0;

	const char* valStart = p + prefixLen;
	const char* valEnd = Q_strstr(valStart, ">");
	if (!valEnd)
		return 0;

	int valLen = valEnd - valStart;
	if (valLen <= 0 || valLen >= outValueSize)
		return 0;

	Q_strncpy(outValue, valStart, valLen + 1);
	return (int)(valEnd - p) + 1; // total consumed including '>'
}

//-----------------------------------------------------------------------------
// Parses a <color=rr.gg.bb.aaa> tag into a Color
// Returns true if parsed successfully
//-----------------------------------------------------------------------------
static bool ParseColorValue(const char* value, Color &outColor)
{
	int r, g, b, a;
	if (sscanf(value, "%d.%d.%d.%d", &r, &g, &b, &a) == 4)
	{
		outColor = Color(r, g, b, a);
		return true;
	}
	// Allow without alpha: rr.gg.bb
	if (sscanf(value, "%d.%d.%d", &r, &g, &b) == 3)
	{
		outColor = Color(r, g, b, 255);
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Plays a 2D sound on a specific channel from the local player.
// Different channels allow sounds to play simultaneously without cutting each other.
//-----------------------------------------------------------------------------
static void PlayDialogueSound2D(const char* soundName, int channel)
{
	if (!soundName || !soundName[0])
		return;

	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	CLocalPlayerFilter filter;
	enginesound->EmitSound(filter, pPlayer->entindex(), channel, soundName,
		1.0f, SNDLVL_NONE, 0, PITCH_NORM, 0, NULL);
}

//-----------------------------------------------------------------------------
// Calculates a zoom FOV based on distance to the target entity.
// Close targets get a tighter zoom, far targets get a wider one.
// Returns a value clamped between 30 and default FOV.
//-----------------------------------------------------------------------------
static int CalcDialogueZoomFOV(float flDistance)
{
	// At ~64 units (face-to-face) -> FOV 30
	// At ~256 units (across a room) -> FOV 55
	// At ~512+ units (far away) -> FOV ~65 (barely zoomed)
	float flFOV = RemapValClamped(flDistance, 64.0f, 512.0f, 30.0f, 65.0f);
	return (int)clamp(flFOV, 30.0f, (float)DIALOGUE_DEFAULT_FOV);
}

class CDialoguePanel : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE(CDialoguePanel, vgui::Frame);

	CDialoguePanel(vgui::VPANEL parent);
	~CDialoguePanel();
	virtual void LoadFile(const char* filePath);
	virtual void ShowNode(const char* nodeName);
	void ShowPanel(void);
	void HidePanel(void);
	void ApplyDialogueSettings(bool bTypewriter, float flSpeed, const char* szTypewriterSound, const char* szOpenSound, const char* szCloseSound);

	protected:
	virtual void OnTick();
	virtual void OnCommand(const char* pcCommand);
	virtual void OnMousePressed(vgui::MouseCode code);
	virtual void PerformLayout();

	private:
		void LookAtTarget(const char* targetName);
		void PlayNPCAnimation(const char* actName);
		void ExecuteCommand(const char* cmdText);
		void PlayNPCSound(const char* soundName);
		void PlayGameSound(const char* soundName);
		void SkipTypewriter(void);
		void StartTypewriterSound(void);
		void StopTypewriterSound(void);

	KeyValues* m_pDialogueKV;
	RichText* m_pDialogueText;
	Button* m_pOptions[5];
	Label* m_pCharacterName;

	char m_szTypewriterBuffer[2048];
	int m_iTypewriterPos;
	bool m_bTypewriterActive;
	float m_flTypewriterSpeed;
	float m_flTypewriterAccum;

	// Focus tracking (NPC or info_target)
	EHANDLE m_hFocusEntity;        // Handle to the entity we're focusing on
	bool m_bShouldTrackTarget;     // Whether to keep tracking the target each tick
	bool m_bZoomActive;            // Whether we've applied a zoom

	bool m_bIsDialogueActive;      // Whether the dialogue panel is currently shown

	// Settings from logic_dialogue (defaults, overrideable by node/inline tags)
	bool  m_bDefaultTypewriter;    // Default typewriter mode from Hammer
	float m_flDefaultSpeed;        // Default typewriter speed from Hammer
	char  m_szTypewriterSound[256];// Typewriter tick sound (looping while printing)
	char  m_szOpenSound[256];      // Sound when panel opens
	char  m_szCloseSound[256];     // Sound when panel closes
};

CDialoguePanel::CDialoguePanel(vgui::VPANEL parent)
	: BaseClass(NULL, "DialoguePanel")
{
	SetParent(parent);

	m_pDialogueKV = NULL;
	m_bIsDialogueActive = false;

	// Initialize typewriter state
	m_szTypewriterBuffer[0] = '\0';
	m_iTypewriterPos = 0;
	m_bTypewriterActive = false;
	m_flTypewriterSpeed = 1.0f;
	m_flTypewriterAccum = 0.0f;

	// Initialize NPC focus
	m_hFocusEntity = NULL;
	m_bShouldTrackTarget = false;
	m_bZoomActive = false;

	// Initialize settings defaults
	m_bDefaultTypewriter = true;
	m_flDefaultSpeed = 1.0f;
	m_szTypewriterSound[0] = '\0';
	m_szOpenSound[0] = '\0';
	m_szCloseSound[0] = '\0';

	SetKeyBoardInputEnabled(true);
	SetMouseInputEnabled(true);

	SetProportional(false);
	SetTitleBarVisible(false);
	SetMinimizeButtonVisible(false);
	SetMaximizeButtonVisible(false);
	SetCloseButtonVisible(true);
	SetSizeable(false);
	SetMoveable(true);
	SetVisible(false);
	SetAlpha(127); // 50% transparent
	SetRoundedCorners(15);

	//SetScheme(vgui::scheme()->LoadSchemeFromFile("resource/SourceScheme.res", "SourceScheme"));

	// Character name label
	m_pCharacterName = new Label(this, "DiagCharName", "DiagCharName");
	m_pCharacterName->SetContentAlignment(Label::a_west);
	m_pCharacterName->SetVisible(true);

	// Dialogue rich text
	m_pDialogueText = new RichText(this, "DiagText");
	m_pDialogueText->SetText("");
	m_pDialogueText->SetMaximumCharCount(4096);
	m_pDialogueText->SetVerticalScrollbar(false);
	m_pDialogueText->SetVisible(true);
	m_pDialogueText->SetRoundedCorners(15);

	// Dialogue option buttons
	const char* optionNames[] = { "DiagOption1", "DiagOption2", "DiagOption3", "DiagOption4", "DiagOption5"};
	for (int i = 0; i < 5; i++)
	{
		m_pOptions[i] = new Button(this, optionNames[i], optionNames[i], this, "");
		m_pOptions[i]->SetContentAlignment(Label::a_west);
		m_pOptions[i]->SetTextInset(6, 0);
		m_pOptions[i]->SetVisible(true);
		m_pOptions[i]->SetEnabled(true);
		m_pOptions[i]->SetArmedSound("ui/buttonrollover.wav");
		m_pOptions[i]->SetReleasedSound("common/bugreporter_succeeded.wav");
		m_pOptions[i]->SetButtonActivationType(Button::ACTIVATE_ONPRESSEDANDRELEASED);
	}

	// Hook the server dialogue message
	HOOK_MESSAGE(DialogueMsg);
}

CDialoguePanel::~CDialoguePanel()
{
	if(m_pDialogueKV)
	{
		m_pDialogueKV->deleteThis();
		m_pDialogueKV = NULL;
	}
}

void CDialoguePanel::ShowPanel(void)
{
	m_bIsDialogueActive = true;
	SetVisible(true);
	SetKeyBoardInputEnabled(true);
	SetMouseInputEnabled(true);
	MoveToFront();

	// Play open sound on CHAN_ITEM so it doesn't conflict with NPC voice or typewriter
	if (m_szOpenSound[0])
		PlayDialogueSound2D(m_szOpenSound, CHAN_ITEM);

	// Hide the HUD during dialogue
	engine->ClientCmd_Unrestricted("sv_dialogue_hud 0");

	// Start receiving ticks only when dialogue is active
	vgui::ivgui()->AddTickSignal(GetVPanel(), 100);
}

void CDialoguePanel::HidePanel(void)
{
	m_bIsDialogueActive = false;
	m_bShouldTrackTarget = false;
	m_hFocusEntity = NULL;

	// Stop typewriter if still running
	StopTypewriterSound();
	m_bTypewriterActive = false;
	m_szTypewriterBuffer[0] = '\0';
	m_iTypewriterPos = 0;

	// Play close sound on CHAN_ITEM so it doesn't conflict with NPC voice or typewriter
	if (m_szCloseSound[0])
		PlayDialogueSound2D(m_szCloseSound, CHAN_ITEM);

	// Restore default FOV if we zoomed in
	if (m_bZoomActive)
	{
		engine->ClientCmd_Unrestricted(VarArgs("sv_dialogue_zoom 0 %.1f", DIALOGUE_ZOOM_RATE));
		m_bZoomActive = false;
	}

	// Restore the HUD
	engine->ClientCmd_Unrestricted("sv_dialogue_hud 1");

	SetVisible(false);
	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);

	// Stop receiving ticks while dialogue is inactive
	vgui::ivgui()->RemoveTickSignal(GetVPanel());
}

void CDialoguePanel::SkipTypewriter(void)
{
	if (!m_bTypewriterActive)
		return;

	// Flush remaining buffer: process all tags and insert all text instantly
	while (m_szTypewriterBuffer[m_iTypewriterPos] != '\0')
	{
		if (m_szTypewriterBuffer[m_iTypewriterPos] == '<')
		{
			char tagValue[256];
			int consumed = 0;

			consumed = ParseTag(&m_szTypewriterBuffer[m_iTypewriterPos], "speed", tagValue, sizeof(tagValue));
			if (consumed > 0) { m_iTypewriterPos += consumed; continue; }

			consumed = ParseTag(&m_szTypewriterBuffer[m_iTypewriterPos], "color", tagValue, sizeof(tagValue));
			if (consumed > 0)
			{
				Color clr;
				if (ParseColorValue(tagValue, clr))
					m_pDialogueText->InsertColorChange(clr);
				m_iTypewriterPos += consumed;
				continue;
			}

			consumed = ParseTag(&m_szTypewriterBuffer[m_iTypewriterPos], "focus", tagValue, sizeof(tagValue));
			if (consumed > 0) { LookAtTarget(tagValue); m_iTypewriterPos += consumed; continue; }

			consumed = ParseTag(&m_szTypewriterBuffer[m_iTypewriterPos], "anim", tagValue, sizeof(tagValue));
			if (consumed > 0) { PlayNPCAnimation(tagValue); m_iTypewriterPos += consumed; continue; }

			consumed = ParseTag(&m_szTypewriterBuffer[m_iTypewriterPos], "command", tagValue, sizeof(tagValue));
			if (consumed > 0) { ExecuteCommand(tagValue); m_iTypewriterPos += consumed; continue; }

			consumed = ParseTag(&m_szTypewriterBuffer[m_iTypewriterPos], "sound_npc", tagValue, sizeof(tagValue));
			if (consumed > 0) { PlayNPCSound(tagValue); m_iTypewriterPos += consumed; continue; }

			consumed = ParseTag(&m_szTypewriterBuffer[m_iTypewriterPos], "sound_world", tagValue, sizeof(tagValue));
			if (consumed > 0) { PlayGameSound(tagValue); m_iTypewriterPos += consumed; continue; }

			consumed = ParseTag(&m_szTypewriterBuffer[m_iTypewriterPos], "sound_typewriter", tagValue, sizeof(tagValue));
			if (consumed > 0) { m_iTypewriterPos += consumed; continue; }
		}

		// Collect a run of plain text until the next '<' or end
		const char* start = &m_szTypewriterBuffer[m_iTypewriterPos];
		const char* end = start + 1;
		while (*end && *end != '<')
			end++;

		int len = end - start;
		char buf[512];
		if (len >= (int)sizeof(buf))
			len = sizeof(buf) - 1;
		Q_strncpy(buf, start, len + 1);
		m_pDialogueText->InsertString(buf);
		m_iTypewriterPos += len;
	}

	m_bTypewriterActive = false;
	StopTypewriterSound();
}

void CDialoguePanel::OnMousePressed(vgui::MouseCode code)
{
	if (m_bTypewriterActive && code == MOUSE_LEFT)
	{
		SkipTypewriter();
		return;
	}

	BaseClass::OnMousePressed(code);
}

void CDialoguePanel::ApplyDialogueSettings(bool bTypewriter, float flSpeed, const char* szTypewriterSound, const char* szOpenSound, const char* szCloseSound)
{
	m_bDefaultTypewriter = bTypewriter;
	m_flDefaultSpeed = (flSpeed > 0.0f) ? flSpeed : 1.0f;
	Q_strncpy(m_szTypewriterSound, szTypewriterSound ? szTypewriterSound : "", sizeof(m_szTypewriterSound));
	Q_strncpy(m_szOpenSound, szOpenSound ? szOpenSound : "", sizeof(m_szOpenSound));
	Q_strncpy(m_szCloseSound, szCloseSound ? szCloseSound : "", sizeof(m_szCloseSound));
}

void CDialoguePanel::StartTypewriterSound(void)
{
	// Sound is now played per-tick in OnTick, this is kept for potential future use
}

void CDialoguePanel::StopTypewriterSound(void)
{
	// One-shot sounds stop naturally when we stop calling PlaySound each tick
}

class CDialoguePanelInterface : public IDialoguePanel
{
	private:
	CDialoguePanel* m_pPanel;
	public:
	CDialoguePanelInterface()
	{
		m_pPanel = NULL;
	}
	void Create(vgui::VPANEL parent)
	{
		m_pPanel = new CDialoguePanel(parent);
	}
	void Destroy()
	{
		if (m_pPanel)
		{
			m_pPanel->SetParent((vgui::Panel*)NULL);
			delete m_pPanel;
		}
	}
	void Activate(void)
	{
		if (m_pPanel)
		{
			m_pPanel->Activate();
		}
	}
	void Show(void)
	{
		if (m_pPanel)
		{
			m_pPanel->ShowPanel();
		}
	}
	void Hide(void)
	{
		if (m_pPanel)
		{
			m_pPanel->HidePanel();
		}
	}
	void LoadFile(const char* filePath)
	{
		if (m_pPanel)
		{
			m_pPanel->LoadFile(filePath);
		}
	}
	void ShowNode(const char* nodeName)
	{
		if (m_pPanel)
		{
			m_pPanel->ShowNode(nodeName);
		}
	}
	void ApplySettings(bool bTypewriter, float flSpeed, const char* szTypewriterSound, const char* szOpenSound, const char* szCloseSound)
	{
		if (m_pPanel)
		{
			m_pPanel->ApplyDialogueSettings(bTypewriter, flSpeed, szTypewriterSound, szOpenSound, szCloseSound);
		}
	}
};
static CDialoguePanelInterface g_DialoguePanel;
IDialoguePanel* g_pDialoguePanel = (IDialoguePanel*)&g_DialoguePanel;

//-----------------------------------------------------------------------------
// Helper: get the head/center position of an entity.
// For NPCs: tries "ValveBiped.Bip01_Head1" bone, falls back to bounding-box center.
// For non-animated entities (info_target, etc.): returns GetAbsOrigin().
//-----------------------------------------------------------------------------
static Vector GetEntityFocusPosition(C_BaseEntity* pEnt)
{
	C_BaseAnimating* pAnimating = pEnt->GetBaseAnimating();
	if (pAnimating)
	{
		int iBone = pAnimating->LookupBone("ValveBiped.Bip01_Head1");
		if (iBone >= 0)
		{
			Vector vecPos;
			QAngle angDummy;
			pAnimating->GetBonePosition(iBone, vecPos, angDummy);
			return vecPos;
		}
	}
	// info_target and other point entities have no model — use origin
	return pEnt->GetAbsOrigin();
}

void CDialoguePanel::LookAtTarget(const char* targetName)
{
	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	// Find entity by targetname (works for NPCs, info_target, etc.)
	C_BaseEntity* ent = NULL;
	C_BaseEntity* pFoundEntity = NULL;
	while ((ent = ClientEntityList().NextBaseEntity(ent)) != NULL)
	{
		const char* entName = ent->GetEntityName();
		if (entName && entName[0] && !Q_stricmp(entName, targetName))
		{
			pFoundEntity = ent;
			break;
		}
	}

	if (!pFoundEntity)
	{
		Warning("CDialoguePanel::LookAtTarget: Entity '%s' not found!\n", targetName);
		m_hFocusEntity = NULL;
		m_bShouldTrackTarget = false;
		return;
	}

	m_hFocusEntity = pFoundEntity;
	m_bShouldTrackTarget = true;

	Vector vecTarget = GetEntityFocusPosition(pFoundEntity);
	Vector vecPlayerEye = pPlayer->EyePosition();
	Vector vecDir = vecTarget - vecPlayerEye;
	float flDistance = vecDir.Length();
	VectorNormalize(vecDir);

	QAngle angLookAt;
	VectorAngles(vecDir, angLookAt);
	engine->SetViewAngles(angLookAt);

	// Apply distance-based zoom
	int iFOV = CalcDialogueZoomFOV(flDistance);
	engine->ClientCmd_Unrestricted(VarArgs("sv_dialogue_zoom %d %.1f", iFOV, DIALOGUE_ZOOM_RATE));
	m_bZoomActive = true;

	// Ask server to make the NPC face and look at the player (only affects NPCs, ignored for info_target)
	char szCmd[256];
	Q_snprintf(szCmd, sizeof(szCmd), "sv_dialogue_lookatplayer %s", targetName);
	engine->ClientCmd_Unrestricted(szCmd);
}

void CDialoguePanel::PlayNPCAnimation(const char* actName)
{
	// Send animation request to the server for the currently focused NPC
	C_BaseEntity* pNPC = m_hFocusEntity.Get();
	if (!pNPC)
	{
		Warning("CDialoguePanel::PlayNPCAnimation: No focused NPC to animate!\n");
		return;
	}

	const char* entName = pNPC->GetEntityName();
	if (!entName || !entName[0])
	{
		Warning("CDialoguePanel::PlayNPCAnimation: Focused NPC has no entity name!\n");
		return;
	}

	char szCmd[256];
	Q_snprintf(szCmd, sizeof(szCmd), "sv_dialogue_animate %s %s", entName, actName);
	engine->ClientCmd_Unrestricted(szCmd);
	Msg("CDialoguePanel::PlayNPCAnimation: Requesting '%s' on '%s'\n", actName, entName);
}

void CDialoguePanel::ExecuteCommand(const char* cmdText)
{
	if (!cmdText || !cmdText[0])
		return;

	Msg("CDialoguePanel::ExecuteCommand: '%s'\n", cmdText);
	engine->ClientCmd_Unrestricted(cmdText);
}

void CDialoguePanel::PlayNPCSound(const char* soundName)
{
	if (!soundName || !soundName[0])
		return;

	C_BaseEntity* pNPC = m_hFocusEntity.Get();
	if (!pNPC)
	{
		Warning("CDialoguePanel::PlayNPCSound: No focused NPC to play sound '%s'!\n", soundName);
		return;
	}

	// Use enginesound directly so raw .wav paths work (EmitSound wrapper expects soundscript names).
	// Emit from the NPC's position at talking volume so the sound is spatialized.
	CLocalPlayerFilter filter;
	Vector vecOrigin = pNPC->GetAbsOrigin();
	enginesound->EmitSound(filter, pNPC->entindex(), CHAN_VOICE, soundName,
		1.0f, SNDLVL_TALKING, 0, PITCH_NORM, 0,
		&vecOrigin);
	Msg("CDialoguePanel::PlayNPCSound: '%s' on entity %d\n", soundName, pNPC->entindex());
}

void CDialoguePanel::PlayGameSound(const char* soundName)
{
	if (!soundName || !soundName[0])
		return;

	enginesound->EmitAmbientSound(soundName, 1.0f);
	Msg("CDialoguePanel::PlayGameSound: '%s'\n", soundName);
}

void CDialoguePanel::PerformLayout()
{
	BaseClass::PerformLayout();

	// Get screen size
	int screenW, screenH;
	vgui::surface()->GetScreenSize(screenW, screenH);

	// Panel size: proportional to screen
	int panelW = (int)(screenW * 0.42f);
	int panelH = (int)(screenH * 0.29f);

	// Panel position: centered horizontally, near the bottom
	int panelX = (screenW - panelW) / 2;
	int panelY = screenH - panelH - (int)(screenH * 0.04f);

	// Only update if changed — prevents infinite invalidation loop
	int oldW, oldH, oldX, oldY;
	GetSize(oldW, oldH);
	GetPos(oldX, oldY);
	if (oldW != panelW || oldH != panelH)
		SetSize(panelW, panelH);
	if (oldX != panelX || oldY != panelY)
		SetPos(panelX, panelY);

	// Margins and spacing relative to panel size
	int margin = (int)(panelW * 0.03f);
	int topPad = (int)(panelH * 0.02f);
	int labelH = (int)(panelH * 0.11f);
	int textTop = (int)(panelH * 0.15f);
	int btnW = (int)(panelW * 0.214f);
	int btnH = labelH;
	int btnX = panelW - margin - btnW;
	int textW = btnX - margin * 2;

	// Character name label
	m_pCharacterName->SetPos(margin, topPad);
	m_pCharacterName->SetSize(textW, labelH);

	// Dialogue rich text
	int textH = panelH - textTop - margin;
	m_pDialogueText->SetPos(margin, textTop);
	m_pDialogueText->SetSize(textW, textH);

	// Option buttons: evenly spaced in the button column
	int btnSpacing = btnH + (int)(panelH * 0.01f);
	for (int i = 0; i < 4; i++)
	{
		m_pOptions[i]->SetPos(btnX, textTop + i * btnSpacing);
		m_pOptions[i]->SetSize(btnW, btnH);
	}

	// Exit button: at the bottom of the button column
	m_pOptions[4]->SetPos(btnX, textTop + 4 * btnSpacing);
	m_pOptions[4]->SetSize(btnW, btnH);
}

void CDialoguePanel::OnTick()
{
	BaseClass::OnTick();

	if (!m_bIsDialogueActive)
		return;

	// Track target: keep camera focused on the entity while dialogue is open
	if (m_bShouldTrackTarget)
	{
		C_BaseEntity* pTarget = m_hFocusEntity.Get();
		C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
		if (pTarget && pPlayer)
		{
			Vector vecTarget = GetEntityFocusPosition(pTarget);

			Vector vecPlayerEye = pPlayer->EyePosition();
			Vector vecDir = vecTarget - vecPlayerEye;
			VectorNormalize(vecDir);

			QAngle angLookAt;
			VectorAngles(vecDir, angLookAt);
			engine->SetViewAngles(angLookAt);
		}
		else
		{
			m_bShouldTrackTarget = false;
			m_hFocusEntity = NULL;
		}
	}

	// Typewriter effect: reveal characters gradually
	if (m_bTypewriterActive && m_szTypewriterBuffer[m_iTypewriterPos] != '\0')
	{
		m_flTypewriterAccum += m_flTypewriterSpeed * TYPEWRITER_BASE_SPEED;

		// Replay typewriter sound each tick while printing (CHAN_BODY won't conflict with voice or UI sounds)
		if (m_szTypewriterSound[0] && m_flTypewriterAccum >= 1.0f)
			PlayDialogueSound2D(m_szTypewriterSound, CHAN_BODY);

		while (m_flTypewriterAccum >= 1.0f)
		{
			if (m_szTypewriterBuffer[m_iTypewriterPos] == '\0')
			{
				m_bTypewriterActive = false;
				StopTypewriterSound();
				break;
			}

			// Check for tags starting with '<'
			if (m_szTypewriterBuffer[m_iTypewriterPos] == '<')
			{
				char tagValue[256];
				int consumed = 0;

				// <speed=1.0>
				consumed = ParseTag(&m_szTypewriterBuffer[m_iTypewriterPos], "speed", tagValue, sizeof(tagValue));
				if (consumed > 0)
				{
					float speed = (float)atof(tagValue);
					if (speed > 0.0f)
						m_flTypewriterSpeed = speed;
					m_iTypewriterPos += consumed;
					continue;
				}

				// <color=rr.gg.bb.aaa>
				consumed = ParseTag(&m_szTypewriterBuffer[m_iTypewriterPos], "color", tagValue, sizeof(tagValue));
				if (consumed > 0)
				{
					Color clr;
					if (ParseColorValue(tagValue, clr))
						m_pDialogueText->InsertColorChange(clr);
					m_iTypewriterPos += consumed;
					continue;
				}

				// <focus=targetname>
				consumed = ParseTag(&m_szTypewriterBuffer[m_iTypewriterPos], "focus", tagValue, sizeof(tagValue));
				if (consumed > 0)
				{
					LookAtTarget(tagValue);
					m_iTypewriterPos += consumed;
					continue;
				}

				// <anim=ACT_TEMPLATE>
				consumed = ParseTag(&m_szTypewriterBuffer[m_iTypewriterPos], "anim", tagValue, sizeof(tagValue));
				if (consumed > 0)
				{
					PlayNPCAnimation(tagValue);
					m_iTypewriterPos += consumed;
					continue;
				}

				// <command=console command here>
				consumed = ParseTag(&m_szTypewriterBuffer[m_iTypewriterPos], "command", tagValue, sizeof(tagValue));
				if (consumed > 0)
				{
					ExecuteCommand(tagValue);
					m_iTypewriterPos += consumed;
					continue;
				}

				// <sound_npc=sound/path.wav>
				consumed = ParseTag(&m_szTypewriterBuffer[m_iTypewriterPos], "sound_npc", tagValue, sizeof(tagValue));
				if (consumed > 0)
				{
					PlayNPCSound(tagValue);
					m_iTypewriterPos += consumed;
					continue;
				}

				// <sound_world=sound/path.wav>
				consumed = ParseTag(&m_szTypewriterBuffer[m_iTypewriterPos], "sound_world", tagValue, sizeof(tagValue));
				if (consumed > 0)
				{
					PlayGameSound(tagValue);
					m_iTypewriterPos += consumed;
					continue;
				}

				// <sound_typewriter=sound/path.wav> — change typewriter tick sound mid-text
				consumed = ParseTag(&m_szTypewriterBuffer[m_iTypewriterPos], "sound_typewriter", tagValue, sizeof(tagValue));
				if (consumed > 0)
				{
					Q_strncpy(m_szTypewriterSound, tagValue, sizeof(m_szTypewriterSound));
					m_iTypewriterPos += consumed;
					continue;
				}
			}

			// Figure out how many bytes this UTF-8 character occupies
			unsigned char ch = (unsigned char)m_szTypewriterBuffer[m_iTypewriterPos];
			int charBytes = 1;
			if (ch >= 0xF0)      charBytes = 4;
			else if (ch >= 0xE0) charBytes = 3;
			else if (ch >= 0xC0) charBytes = 2;

			// Extract this single character as a null-terminated string
			char oneChar[8];
			int j;
			for (j = 0; j < charBytes && m_szTypewriterBuffer[m_iTypewriterPos + j] != '\0'; j++)
			{
				oneChar[j] = m_szTypewriterBuffer[m_iTypewriterPos + j];
			}
			oneChar[j] = '\0';

			m_pDialogueText->InsertString(oneChar);
			m_iTypewriterPos += j;

			m_flTypewriterAccum -= 1.0f;
		}
	}
	else if (m_bTypewriterActive)
	{
		m_bTypewriterActive = false;
		StopTypewriterSound();
	}
}

void CDialoguePanel::OnCommand(const char* pcCommand)
{
	if (!Q_stricmp(pcCommand, "Close"))
	{
		HidePanel();
		return;
	}

	BaseClass::OnCommand(pcCommand);

	if (!Q_stricmp(pcCommand, "turnoff"))
	{
		HidePanel();
	}
	else if(!Q_strnicmp(pcCommand, "gotonode ", 9))
	{
		const char* nodeName = pcCommand + 9;
		ShowNode(nodeName);
	}
	else if (!Q_strnicmp(pcCommand, "startdiag ", 10))
	{
		const char* filePath = pcCommand + 10;
		LoadFile(filePath);
	}
	else if (!Q_strnicmp(pcCommand, "cmd ", 4))
	{
		const char* cmdText = pcCommand + 4;
		engine->ClientCmd_Unrestricted(cmdText);
	}
}

void CDialoguePanel::LoadFile(const char* pathFile)
{
	// Clean up previous data before loading new file
	if(m_pDialogueKV)
	{
		m_pDialogueKV->deleteThis();
		m_pDialogueKV = NULL;
	}

	m_pDialogueKV = new KeyValues("DialogueFile");

	if (!m_pDialogueKV->LoadFromFile(filesystem, pathFile, "MOD"))
	{
		Warning("CDialoguePanel: Failed to load %s!\n", pathFile);
		m_pDialogueKV->deleteThis();
		m_pDialogueKV = NULL;
		return;
	}
	Msg("CDialoguePanel: Successfully loaded dialogue file %s!\n", pathFile);
}

void CDialoguePanel::ShowNode(const char* nodeName)
{
	if (!m_pDialogueKV)
	{
		Warning("CDialoguePanel: Dialogue file not loaded, cannot show node %s!\n", nodeName);
		return;
	}

	// Find the node in the KeyValues
	KeyValues* pNode = m_pDialogueKV->FindKey(nodeName);

	// Check if the node exists
	if (!pNode)
	{
		Warning("CDialoguePanel: Node %s not found in dialogue file!\n", nodeName);
		return;
	}

	// =========================================================
	// Reset all UI elements to default state before applying new node data.
	// This prevents state from a previous node leaking into the current one.
	// =========================================================

	m_pCharacterName->SetText("...");
	m_pDialogueText->SetText("");
	m_bTypewriterActive = false;
	m_szTypewriterBuffer[0] = '\0';
	m_iTypewriterPos = 0;
	m_flTypewriterSpeed = 1.0f;
	m_flTypewriterAccum = 0.0f;

	for (int i = 0; i < 5; i++)
	{
		m_pOptions[i]->SetVisible(false);
		m_pOptions[i]->SetEnabled(true);
		m_pOptions[i]->SetText("");
		m_pOptions[i]->SetCommand("");
		m_pOptions[i]->SetAsDefaultButton(false);
		m_pOptions[i]->SetArmedSound("ui/buttonrollover.wav");
		m_pOptions[i]->SetReleasedSound("common/bugreporter_succeeded.wav");
	}

	SetCloseButtonVisible(false);

	// =========================================================
	// Apply new node data
	// =========================================================

	// --- Character name ---
	const char* speaker = pNode->GetString("speaker", NULL);
	if (speaker)
		m_pCharacterName->SetText(speaker);

	// --- Focus camera on entity (NPC or info_target, before other actions so m_hFocusEntity is set) ---
	const char* focus = pNode->GetString("focus", "");
	if (focus && focus[0] != '\0')
		LookAtTarget(focus);

	// --- Node-level actions (executed once, before typewriter starts) ---
	const char* nodeAnim = pNode->GetString("anim", "");
	if (nodeAnim && nodeAnim[0] != '\0')
		PlayNPCAnimation(nodeAnim);

	const char* nodeSndNpc = pNode->GetString("sound_npc", "");
	if (nodeSndNpc && nodeSndNpc[0] != '\0')
		PlayNPCSound(nodeSndNpc);

	const char* nodeSndGame = pNode->GetString("sound_world", "");
	if (nodeSndGame && nodeSndGame[0] != '\0')
		PlayGameSound(nodeSndGame);

	const char* nodeCmd = pNode->GetString("command", "");
	if (nodeCmd && nodeCmd[0] != '\0')
		ExecuteCommand(nodeCmd);

	// --- Node-level default color (can be overridden by inline <color=...>) ---
	const char* nodeColor = pNode->GetString("color", "");
	if (nodeColor && nodeColor[0] != '\0')
	{
		Color clr;
		if (ParseColorValue(nodeColor, clr))
			m_pDialogueText->InsertColorChange(clr);
	}

	// --- Node-level default speed (can be overridden by inline <speed=...>) ---
	// Use entity default speed, then let node override if specified
	float nodeSpeed = pNode->GetFloat("speed", m_flDefaultSpeed);
	m_flTypewriterSpeed = nodeSpeed;

	// --- Node-level typewriter sound (can be overridden by inline <sound_typewriter=...>) ---
	const char* nodeTwSound = pNode->GetString("sound_typewriter", "");
	if (nodeTwSound && nodeTwSound[0] != '\0')
		Q_strncpy(m_szTypewriterSound, nodeTwSound, sizeof(m_szTypewriterSound));

	// --- Typewriter colored text output ---
	const char* text = pNode->GetString("text", NULL);
	if (text)
	{
		// Node "typewriter" key overrides entity default; if not set, use entity default
		bool bTypewriter = pNode->GetBool("typewriter", m_bDefaultTypewriter);

		if (bTypewriter)
		{
			Q_strncpy(m_szTypewriterBuffer, text, sizeof(m_szTypewriterBuffer));
			m_iTypewriterPos = 0;
			m_flTypewriterAccum = 0.0f;
			m_bTypewriterActive = true;
		}
		else
		{
			// Instant mode: parse tags and insert text immediately
			const char* p = text;
			while (*p)
			{
				if (*p == '<')
				{
					char tagValue[256];
					int consumed = 0;

					// <speed=...> — skip in instant mode (no effect)
					consumed = ParseTag(p, "speed", tagValue, sizeof(tagValue));
					if (consumed > 0)
					{
						p += consumed;
						continue;
					}

					// <color=rr.gg.bb.aaa>
					consumed = ParseTag(p, "color", tagValue, sizeof(tagValue));
					if (consumed > 0)
					{
						Color clr;
						if (ParseColorValue(tagValue, clr))
							m_pDialogueText->InsertColorChange(clr);
						p += consumed;
						continue;
					}

					// <focus=targetname>
					consumed = ParseTag(p, "focus", tagValue, sizeof(tagValue));
					if (consumed > 0)
					{
						LookAtTarget(tagValue);
						p += consumed;
						continue;
					}

					// <anim=ACT_TEMPLATE>
					consumed = ParseTag(p, "anim", tagValue, sizeof(tagValue));
					if (consumed > 0)
					{
						PlayNPCAnimation(tagValue);
						p += consumed;
						continue;
					}

					// <command=console command> — execute immediately in instant mode
					consumed = ParseTag(p, "command", tagValue, sizeof(tagValue));
					if (consumed > 0)
					{
						ExecuteCommand(tagValue);
						p += consumed;
						continue;
					}

					// <sound_npc=sound/path.wav>
					consumed = ParseTag(p, "sound_npc", tagValue, sizeof(tagValue));
					if (consumed > 0)
					{
						PlayNPCSound(tagValue);
						p += consumed;
						continue;
					}

					// <sound_world=sound/path.wav>
					consumed = ParseTag(p, "sound_world", tagValue, sizeof(tagValue));
					if (consumed > 0)
					{
						PlayGameSound(tagValue);
						p += consumed;
						continue;
					}

					// <sound_typewriter=...> — skip in instant mode (no typewriter playing)
					consumed = ParseTag(p, "sound_typewriter", tagValue, sizeof(tagValue));
					if (consumed > 0)
					{
						p += consumed;
						continue;
					}
				}

				// Collect plain text until the next '<' or end of string
				const char* start = p;
				p++;
				while (*p && *p != '<')
					p++;

				if (p > start)
				{
					int len = p - start;
					char buf[512];
					if (len >= (int)sizeof(buf))
						len = sizeof(buf) - 1;
					Q_strncpy(buf, start, len + 1);
					m_pDialogueText->InsertString(buf);
				}
			}
		}
	}
	else
	{
		m_pDialogueText->InsertString("...");
	}

	// --- Dialogue options ---
	for (int i = 0; i < 5; i++)
	{
		KeyValues* pNodeOption = pNode->FindKey(VarArgs("choice%d", i + 1), false);
		if (!pNodeOption)
			continue;

		m_pOptions[i]->SetVisible(true);
		m_pOptions[i]->SetEnabled(pNodeOption->GetBool("enabled", true));
		m_pOptions[i]->SetArmedSound(pNodeOption->GetString("sound_hover", "ui/buttonrollover.wav"));
		m_pOptions[i]->SetReleasedSound(pNodeOption->GetString("sound_press", "common/bugreporter_succeeded.wav"));

		KeyValues* pExitOption = pNodeOption->FindKey("exit", false);
		if (pExitOption)
		{
			m_pOptions[i]->SetAsDefaultButton(true);
			m_pOptions[i]->SetText(pNodeOption->GetString("text", "..."));
			m_pOptions[i]->SetCommand(pNodeOption->GetString("command", "turnoff"));
		}
		else
		{
			m_pOptions[i]->SetText(pNodeOption->GetString("text", "..."));
			m_pOptions[i]->SetCommand(pNodeOption->GetString("command", ""));
		}
	}

	// --- Closable (close X button) ---
	if (pNode->FindKey("closable", false))
		SetCloseButtonVisible(true);
}
