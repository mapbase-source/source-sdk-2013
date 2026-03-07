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
#include "utlstring.h"
#include "utlvector.h"
#include "GameEventListener.h"

#define DIALOGUE_DEFAULT_FOV 75    // Default player FOV
#define DIALOGUE_ZOOM_RATE 0.3f    // How fast to zoom in/out (seconds)
#define DIALOGUE_HIDE_DELAY 0.15f  // Delay before hiding panel (lets button sounds play)
#define DIALOGUE_ANIM_DURATION 0.35f // Slide-in animation duration (seconds)
#define DIALOGUE_ANIM_TICK_MS 10   // Tick interval during animations (ms)
#define DIALOGUE_BTN_FADE_DURATION 0.25f // Button fade-in duration (seconds)
#define DIALOGUE_CAMERA_SMOOTH_SPEED 4.0f // Camera smoothing speed (higher = faster tracking)

// Typewriter timing: characters are printed based on elapsed realtime.
// The interval between characters = BASE_INTERVAL / speed (in milliseconds).
// At speed 1.0 -> 50ms per char (20 chars/sec, natural reading pace)
// At speed 0.25 -> 200ms per char (slow, dramatic)
// At speed 5.0 -> 10ms per char (very fast)
#define TYPEWRITER_BASE_INTERVAL_MS 50

using namespace vgui;

// Message types matching the server
#define DIALOGUE_MSG_START    0
#define DIALOGUE_MSG_STOP     1
#define DIALOGUE_MSG_NODE     2
#define DIALOGUE_MSG_SETTINGS 3
#define DIALOGUE_MSG_FOCUS    4
#define DIALOGUE_MSG_UNLOCK   5
#define DIALOGUE_MSG_LOCK     6

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

	case DIALOGUE_MSG_FOCUS:
		{
			float x = msg.ReadFloat();
			float y = msg.ReadFloat();
			float z = msg.ReadFloat();
			g_pDialoguePanel->ApplyFocusPosition(x, y, z);
		}
		break;

	case DIALOGUE_MSG_UNLOCK:
		{
			char szCondName[64];
			msg.ReadString(szCondName, sizeof(szCondName));
			g_pDialoguePanel->UnlockCondition(szCondName);
		}
		break;

	case DIALOGUE_MSG_LOCK:
		{
			char szCondName[64];
			msg.ReadString(szCondName, sizeof(szCondName));
			g_pDialoguePanel->LockCondition(szCondName);
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
	float flFOV = RemapValClamped(flDistance, 64.0f, 512.0f, 30.0f, 65.0f);
	return (int)clamp(flFOV, 30.0f, (float)DIALOGUE_DEFAULT_FOV);
}

//-----------------------------------------------------------------------------
// Client-side helper: find an entity by targetname by iterating client entities.
// Returns NULL if entity has no client-side representation (e.g. info_target).
//-----------------------------------------------------------------------------
static C_BaseEntity* FindClientEntityByName(const char* targetName)
{
	if (!targetName || !targetName[0])
		return NULL;

	C_BaseEntity* ent = NULL;
	while ((ent = ClientEntityList().NextBaseEntity(ent)) != NULL)
	{
		const char* entName = ent->GetEntityName();
		if (entName && entName[0] && !Q_stricmp(entName, targetName))
			return ent;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Client-side helper: get the head/focus position of an entity.
// Tries the head bone first, then falls back to WorldSpaceCenter.
//-----------------------------------------------------------------------------
static Vector GetClientEntityFocusPosition(C_BaseEntity* pEnt)
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
	return pEnt->WorldSpaceCenter();
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
	void HidePanelImmediate(void);
	void ApplyDialogueSettings(bool bTypewriter, float flSpeed, const char* szTypewriterSound, const char* szOpenSound, const char* szCloseSound);
	void ApplyFocusPosition(float x, float y, float z);
	void UnlockCondition(const char* condName);
	void LockCondition(const char* condName);

	protected:
	virtual void OnTick();
	virtual void OnCommand(const char* pcCommand);
	virtual void OnMousePressed(vgui::MouseCode code);
	virtual void PerformLayout();
	virtual void PaintBackground();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

	private:
		void LookAtTarget(const char* targetName);
		void PlayNPCAnimation(const char* actName);
		void ExecuteCommand(const char* cmdText);
		void PlayNPCSound(const char* soundName);
		void PlayGameSound(const char* soundName);
		void SkipTypewriter(void);
		void UpdateTickInterval(void);
		void BeginButtonsFade(void);
		bool IsConditionUnlocked(const char* condName);
		void RefreshButtonStates(void);

	KeyValues* m_pDialogueKV;
	RichText* m_pDialogueText;
	Button* m_pOptions[5];
	Label* m_pCharacterName;
	Panel* m_pSeparator;

	// Cached layout positions for PaintBackground
	int m_iLayoutMargin;
	int m_iLayoutSepY;
	int m_iLayoutTextTop;
	int m_iLayoutTextH;
	int m_iLayoutContentW;
	int m_iLayoutDialogH;  // Height of the dialog box (excluding gap + buttons)

	char m_szTypewriterBuffer[2048];
	int m_iTypewriterPos;
	bool m_bTypewriterActive;
	float m_flTypewriterSpeed;
	float m_flTypewriterLastCharTime; // Realtime when last character was printed
	int m_iCurrentTickInterval;    // Current tick interval in ms (adjusted by speed)

	// Focus tracking
	char m_szFocusTargetName[128];
	Vector m_vecServerFocusPos;
	bool m_bHasServerFocusPos;
	bool m_bShouldTrackTarget;
	bool m_bZoomActive;

	// Smooth camera tracking
	float m_flCameraLastTime;        // Last realtime when camera was updated (for delta time)

	bool m_bIsDialogueActive;

	// Deferred hide: lets button sounds finish playing before hiding
	bool  m_bHidePending;
	float m_flHideTime;

	// Animation phases:
	//   ANIM_NONE         — idle, no animation
	//   ANIM_SLIDE_IN     — panel slides up from off-screen to final position, fading in
	//   ANIM_BUTTONS_FADE — buttons gradually become visible after typewriter finishes
	//   ANIM_SLIDE_OUT    — panel slides down off-screen, fading out (on close)
	enum AnimPhase { ANIM_NONE = 0, ANIM_SLIDE_IN, ANIM_BUTTONS_FADE, ANIM_SLIDE_OUT };
	AnimPhase m_eAnimPhase;
	float m_flAnimStartTime;
	int   m_iAnimStartY;    // Y position at start of slide-in (off-screen)
	int   m_iFinalY;        // Final resting Y position

	// Settings from logic_dialogue (defaults, overrideable by node/inline tags)
	bool  m_bDefaultTypewriter;
	float m_flDefaultSpeed;
	char  m_szTypewriterSound[256];
	char  m_szOpenSound[256];
	char  m_szCloseSound[256];

	// Condition-based button enable/disable
	CUtlVector<CUtlString> m_UnlockedConditions;
	char m_szOptionCondition[5][64];  // Per-button condition string (empty = always enabled)
};

CDialoguePanel::CDialoguePanel(vgui::VPANEL parent)
	: BaseClass(NULL, "DialoguePanel")
{
	SetParent(parent);

	m_pDialogueKV = NULL;
	m_bIsDialogueActive = false;

	// Initialize cached layout
	m_iLayoutMargin = 0;
	m_iLayoutSepY = 0;
	m_iLayoutTextTop = 0;
	m_iLayoutTextH = 0;
	m_iLayoutContentW = 0;
	m_iLayoutDialogH = 0;

	// Initialize typewriter state
	m_szTypewriterBuffer[0] = '\0';
	m_iTypewriterPos = 0;
	m_bTypewriterActive = false;
	m_flTypewriterSpeed = 1.0f;
	m_flTypewriterLastCharTime = 0.0f;
	m_iCurrentTickInterval = TYPEWRITER_BASE_INTERVAL_MS;

	// Initialize focus tracking
	m_szFocusTargetName[0] = '\0';
	m_vecServerFocusPos = vec3_origin;
	m_bHasServerFocusPos = false;
	m_bShouldTrackTarget = false;
	m_bZoomActive = false;

	// Initialize camera tracking
	m_flCameraLastTime = 0.0f;

	// Initialize deferred hide
	m_bHidePending = false;
	m_flHideTime = 0.0f;

	// Initialize animation state
	m_eAnimPhase = ANIM_NONE;
	m_flAnimStartTime = 0.0f;
	m_iAnimStartY = 0;
	m_iFinalY = 0;

	// Initialize settings defaults
	m_bDefaultTypewriter = true;
	m_flDefaultSpeed = 1.0f;
	m_szTypewriterSound[0] = '\0';
	m_szOpenSound[0] = '\0';
	m_szCloseSound[0] = '\0';

	// Initialize condition tracking
	for (int i = 0; i < 5; i++)
		m_szOptionCondition[i][0] = '\0';

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
	SetRoundedCorners(15);
	
	// Black panel background
	SetBgColor(Color(20, 20, 20, 230));

	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile(
		"resource/DialogueScheme.res", "DialogueScheme");
	SetScheme(scheme);
	vgui::HFont hFont = vgui::scheme()->GetIScheme(scheme)->GetFont("DialogueFont");

	// Character name label — white text
	m_pCharacterName = new Label(this, "DiagCharName", "DiagCharName");
	m_pCharacterName->SetContentAlignment(Label::a_west);
	m_pCharacterName->SetVisible(true);
	m_pCharacterName->SetFgColor(Color(255, 255, 255, 255));
	m_pCharacterName->SetBgColor(Color(0, 0, 0, 0));
	m_pCharacterName->SetPaintBackgroundEnabled(false);
	m_pCharacterName->SetMouseInputEnabled(false);
	m_pCharacterName->SetFont(hFont);

	// Separator line — drawn manually in PaintBackground for reliability
	m_pSeparator = new Panel(this, "DiagSeparator");
	m_pSeparator->SetVisible(false);

	// Dialogue rich text — background drawn manually in PaintBackground
	m_pDialogueText = new RichText(this, "DiagText");
	m_pDialogueText->SetText("");
	m_pDialogueText->SetMaximumCharCount(4096);
	m_pDialogueText->SetVerticalScrollbar(false);
	m_pDialogueText->SetVisible(true);
	m_pDialogueText->SetMouseInputEnabled(false);
	m_pDialogueText->SetPaintBackgroundEnabled(false);
	m_pDialogueText->SetFgColor(Color(255, 255, 255, 255));
	m_pDialogueText->SetFont(hFont);

	// Dialogue option buttons
	const char* optionNames[] = { "DiagOption1", "DiagOption2", "DiagOption3", "DiagOption4", "DiagOption5"};
	for (int i = 0; i < 5; i++)
	{
		m_pOptions[i] = new Button(this, optionNames[i], optionNames[i], this, "");
		m_pOptions[i]->SetContentAlignment(Label::a_center);
		m_pOptions[i]->SetTextInset(4, 0);
		m_pOptions[i]->SetVisible(true);
		m_pOptions[i]->SetEnabled(true);
		m_pOptions[i]->SetArmedSound("ui/buttonrollover.wav");
		m_pOptions[i]->SetReleasedSound("common/bugreporter_succeeded.wav");
		m_pOptions[i]->SetButtonActivationType(Button::ACTIVATE_ONPRESSEDANDRELEASED);
		m_pOptions[i]->SetFont(hFont);
		m_pOptions[i]->SetAlpha(0);
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

void CDialoguePanel::UpdateTickInterval(void)
{
	// Always use fast tick rate for smooth camera tracking.
	// Typewriter speed is controlled by time-based accumulation, not tick interval.
	if (m_iCurrentTickInterval != DIALOGUE_ANIM_TICK_MS)
	{
		m_iCurrentTickInterval = DIALOGUE_ANIM_TICK_MS;
		vgui::ivgui()->RemoveTickSignal(GetVPanel());
		vgui::ivgui()->AddTickSignal(GetVPanel(), m_iCurrentTickInterval);
	}
}

void CDialoguePanel::BeginButtonsFade(void)
{
	// Start fading buttons in after typewriter finished
	m_eAnimPhase = ANIM_BUTTONS_FADE;
	m_flAnimStartTime = gpGlobals->realtime;

	// Switch to fast tick rate for smooth fade
	m_iCurrentTickInterval = DIALOGUE_ANIM_TICK_MS;
	vgui::ivgui()->RemoveTickSignal(GetVPanel());
	vgui::ivgui()->AddTickSignal(GetVPanel(), m_iCurrentTickInterval);
}

void CDialoguePanel::ShowPanel(void)
{
	m_bIsDialogueActive = true;
	m_bHidePending = false;
	SetVisible(true);
	SetKeyBoardInputEnabled(true);
	SetMouseInputEnabled(true);
	MoveToFront();

	// Play open sound on CHAN_ITEM so it doesn't conflict with NPC voice or typewriter
	if (m_szOpenSound[0])
		PlayDialogueSound2D(m_szOpenSound, CHAN_ITEM);

	// Hide the HUD during dialogue
	engine->ClientCmd_Unrestricted("internal_dialogue_hud 0");

	// Hide buttons — they fade in after typewriter finishes
	for (int i = 0; i < 5; i++)
		m_pOptions[i]->SetAlpha(0);

	// Pause typewriter during slide-in — it will resume when slide-in finishes.
	// ShowNode may have already started it before ShowPanel was called.
	m_bTypewriterActive = false;

	// Start ANIM_SLIDE_IN: panel slides from off-screen to final position
	int screenW, screenH;
	vgui::surface()->GetScreenSize(screenW, screenH);
	m_iAnimStartY = screenH;
	m_eAnimPhase = ANIM_SLIDE_IN;
	m_flAnimStartTime = gpGlobals->realtime;
	SetAlpha(0);

	// Start receiving ticks at a fast rate for smooth animation
	m_iCurrentTickInterval = DIALOGUE_ANIM_TICK_MS;
	vgui::ivgui()->RemoveTickSignal(GetVPanel());
	vgui::ivgui()->AddTickSignal(GetVPanel(), m_iCurrentTickInterval);
}

void CDialoguePanel::HidePanel(void)
{
	// If already sliding out, ignore duplicate calls
	if (m_eAnimPhase == ANIM_SLIDE_OUT)
		return;

	// Stop typewriter if still running
	m_bTypewriterActive = false;
	m_szTypewriterBuffer[0] = '\0';
	m_iTypewriterPos = 0;

	// Cancel any pending deferred hide — we're handling it now
	m_bHidePending = false;

	// Play close sound on CHAN_ITEM so it doesn't conflict with NPC voice or typewriter
	if (m_szCloseSound[0])
		PlayDialogueSound2D(m_szCloseSound, CHAN_ITEM);

	// Start ANIM_SLIDE_OUT: panel slides down off-screen while fading out
	int curX, curY;
	GetPos(curX, curY);
	m_iAnimStartY = curY;
	m_eAnimPhase = ANIM_SLIDE_OUT;
	m_flAnimStartTime = gpGlobals->realtime;

	// Disable input immediately so player can't click during slide-out
	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);

	// Switch to fast tick rate for smooth animation
	m_iCurrentTickInterval = DIALOGUE_ANIM_TICK_MS;
	vgui::ivgui()->RemoveTickSignal(GetVPanel());
	vgui::ivgui()->AddTickSignal(GetVPanel(), m_iCurrentTickInterval);
}

void CDialoguePanel::HidePanelImmediate(void)
{
	if (!m_bIsDialogueActive)
		return;

	m_bIsDialogueActive = false;
	m_bShouldTrackTarget = false;
	m_szFocusTargetName[0] = '\0';
	m_bHasServerFocusPos = false;

	// Stop typewriter if still running
	m_bTypewriterActive = false;
	m_szTypewriterBuffer[0] = '\0';
	m_iTypewriterPos = 0;

	// Cancel any pending deferred hide
	m_bHidePending = false;

	// Stop animation
	m_eAnimPhase = ANIM_NONE;
	SetAlpha(255);
	for (int i = 0; i < 5; i++)
		m_pOptions[i]->SetAlpha(255);

	// Restore default FOV if we zoomed in
	if (m_bZoomActive)
	{
		engine->ClientCmd_Unrestricted(VarArgs("internal_dialogue_zoom 0 %.1f", DIALOGUE_ZOOM_RATE));
		m_bZoomActive = false;
	}

	// Restore the HUD
	engine->ClientCmd_Unrestricted("internal_dialogue_hud 1");

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

	// Text finished — fade buttons in
	BeginButtonsFade();
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

bool CDialoguePanel::IsConditionUnlocked(const char* condName)
{
	if (!condName || !condName[0])
		return true;

	for (int i = 0; i < m_UnlockedConditions.Count(); i++)
	{
		if (!Q_stricmp(m_UnlockedConditions[i].Get(), condName))
			return true;
	}
	return false;
}

void CDialoguePanel::RefreshButtonStates(void)
{
	for (int i = 0; i < 5; i++)
	{
		if (!m_pOptions[i]->IsVisible())
			continue;

		if (m_szOptionCondition[i][0])
			m_pOptions[i]->SetEnabled(IsConditionUnlocked(m_szOptionCondition[i]));
	}
}

void CDialoguePanel::UnlockCondition(const char* condName)
{
	if (!condName || !condName[0])
		return;

	if (!IsConditionUnlocked(condName))
		m_UnlockedConditions.AddToTail(CUtlString(condName));

	if (m_bIsDialogueActive)
		RefreshButtonStates();
}

void CDialoguePanel::LockCondition(const char* condName)
{
	if (!condName || !condName[0])
		return;

	for (int i = 0; i < m_UnlockedConditions.Count(); i++)
	{
		if (!Q_stricmp(m_UnlockedConditions[i].Get(), condName))
		{
			m_UnlockedConditions.Remove(i);
			break;
		}
	}

	if (m_bIsDialogueActive)
		RefreshButtonStates();
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
	void HideImmediate(void)
	{
		if (m_pPanel)
		{
			m_pPanel->HidePanelImmediate();
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
	void ApplyFocusPosition(float x, float y, float z)
	{
		if (m_pPanel)
		{
			m_pPanel->ApplyFocusPosition(x, y, z);
		}
	}
	void UnlockCondition(const char* condName)
	{
		if (m_pPanel)
		{
			m_pPanel->UnlockCondition(condName);
		}
	}
	void LockCondition(const char* condName)
	{
		if (m_pPanel)
		{
			m_pPanel->LockCondition(condName);
		}
	}
};
static CDialoguePanelInterface g_DialoguePanel;
IDialoguePanel* g_pDialoguePanel = (IDialoguePanel*)&g_DialoguePanel;

//-----------------------------------------------------------------------------
// Game event listener: closes dialogue immediately on player death,
// map change, or save load (game_newmap fires for all three).
//-----------------------------------------------------------------------------

class CDialogueEventListener : public CGameEventListener
{
public:
	CDialogueEventListener()
	{
		ListenForGameEvent("player_death");
		ListenForGameEvent("game_newmap");
	}

	virtual void FireGameEvent(IGameEvent *event)
	{
		if (g_pDialoguePanel)
			g_pDialoguePanel->HideImmediate();
	}
};
static CDialogueEventListener s_DialogueEventListener;

void CDialoguePanel::LookAtTarget(const char* targetName)
{
	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	// Store the target name for client-side tracking and server commands (animate, sound_npc)
	Q_strncpy(m_szFocusTargetName, targetName, sizeof(m_szFocusTargetName));
	m_bShouldTrackTarget = true;
	m_bHasServerFocusPos = false;

	// Reset camera tracking timer so first tick computes a proper delta
	m_flCameraLastTime = gpGlobals->realtime;

	// Try to find the entity on the client right now for zoom calculation
	C_BaseEntity* pEnt = FindClientEntityByName(targetName);
	if (pEnt)
	{
		Vector vecTarget = GetClientEntityFocusPosition(pEnt);
		Vector vecPlayerEye = pPlayer->EyePosition();
		Vector vecDir = vecTarget - vecPlayerEye;
		float flDistance = vecDir.Length();

		// Apply distance-based zoom
		if (!m_bZoomActive)
		{
			int iFOV = CalcDialogueZoomFOV(flDistance);
			engine->ClientCmd_Unrestricted(VarArgs("internal_dialogue_zoom %d %.1f", iFOV, DIALOGUE_ZOOM_RATE));
			m_bZoomActive = true;
		}
	}

	// Always ask server — handles NPC look-at-player, and provides position for
	// server-only entities (info_target, etc.) that don't exist on the client.
	char szCmd[256];
	Q_snprintf(szCmd, sizeof(szCmd), "internal_dialogue_focus %s", targetName);
	engine->ClientCmd_Unrestricted(szCmd);
}

void CDialoguePanel::ApplyFocusPosition(float x, float y, float z)
{
	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	m_vecServerFocusPos = Vector(x, y, z);
	m_bHasServerFocusPos = true;

	// If the entity doesn't exist on the client (info_target, etc.), use this
	// server position for zoom calculation.
	C_BaseEntity* pEnt = FindClientEntityByName(m_szFocusTargetName);
	if (!pEnt)
	{
		Vector vecPlayerEye = pPlayer->EyePosition();
		float flDistance = (m_vecServerFocusPos - vecPlayerEye).Length();

		if (!m_bZoomActive)
		{
			int iFOV = CalcDialogueZoomFOV(flDistance);
			engine->ClientCmd_Unrestricted(VarArgs("internal_dialogue_zoom %d %.1f", iFOV, DIALOGUE_ZOOM_RATE));
			m_bZoomActive = true;
		}
	}
}

void CDialoguePanel::PlayNPCAnimation(const char* actName)
{
	if (!m_szFocusTargetName[0])
	{
		Warning("CDialoguePanel::PlayNPCAnimation: No focused entity to animate!\n");
		return;
	}

	char szCmd[256];
	Q_snprintf(szCmd, sizeof(szCmd), "internal_dialogue_animate %s %s", m_szFocusTargetName, actName);
	engine->ClientCmd_Unrestricted(szCmd);
}

void CDialoguePanel::ExecuteCommand(const char* cmdText)
{
	if (!cmdText || !cmdText[0])
		return;

	engine->ClientCmd_Unrestricted(cmdText);
}

void CDialoguePanel::PlayNPCSound(const char* soundName)
{
	if (!soundName || !soundName[0])
		return;

	if (!m_szFocusTargetName[0])
	{
		Warning("CDialoguePanel::PlayNPCSound: No focused entity to play sound '%s'!\n", soundName);
		return;
	}

	char szCmd[256];
	Q_snprintf(szCmd, sizeof(szCmd), "internal_dialogue_sound %s %s", m_szFocusTargetName, soundName);
	engine->ClientCmd_Unrestricted(szCmd);
}

void CDialoguePanel::PlayGameSound(const char* soundName)
{
	if (!soundName || !soundName[0])
		return;

	enginesound->EmitAmbientSound(soundName, 1.0f);
}

void CDialoguePanel::PerformLayout()
{
	BaseClass::PerformLayout();

	// Get screen size
	int screenW, screenH;
	vgui::surface()->GetScreenSize(screenW, screenH);

	// Layout: dialog box on top, gap, then buttons below — all inside one VGUI panel.
	// PaintBackground only draws the black bg for the dialog portion.
	int margin = (int)(screenW * 0.0125f);
	int contentW = (int)(screenW * 0.50f) - margin * 2;
	int panelW = contentW + margin * 2;

	// Dialog box content heights
	int topPad = 4;
	int labelH = 20;
	int sepY = topPad + labelH + 2;
	int textTop = sepY + 6;
	int textH = (int)(screenH * 0.15f);
	int dialogH = textTop + textH + margin;

	// Button row below dialog with a gap
	int gap = 6;
	int btnH = 22;
	int btnSpacing = (int)(panelW * 0.008f);

	// Total panel height includes dialog + gap + buttons
	int panelH = dialogH + gap + btnH;

	// Panel position: centered horizontally, dialog area near the bottom
	int panelX = (screenW - panelW) / 2;
	int panelY = screenH - panelH - (int)(screenH * 0.04f);

	// Cache final position for animations
	m_iFinalY = panelY;

	// During slide-in animation, override Y position
	int actualY = panelY;
	if (m_eAnimPhase == ANIM_SLIDE_IN)
	{
		float flElapsed = gpGlobals->realtime - m_flAnimStartTime;
		float flFraction = clamp(flElapsed / DIALOGUE_ANIM_DURATION, 0.0f, 1.0f);
		// Ease-out: starts fast (off-screen), decelerates into final position
		float flSmooth = 1.0f - (1.0f - flFraction) * (1.0f - flFraction);
		actualY = m_iAnimStartY + (int)((float)(panelY - m_iAnimStartY) * flSmooth);
	}
	else if (m_eAnimPhase == ANIM_SLIDE_OUT)
	{
		float flElapsed = gpGlobals->realtime - m_flAnimStartTime;
		float flFraction = clamp(flElapsed / DIALOGUE_ANIM_DURATION, 0.0f, 1.0f);
		// Ease-in: starts slow, accelerates off-screen
		float flSmooth = flFraction * flFraction;
		actualY = m_iAnimStartY + (int)((float)(screenH - m_iAnimStartY) * flSmooth);
	}

	// Only update if changed — prevents infinite invalidation loop
	int oldW, oldH, oldX, oldY;
	GetSize(oldW, oldH);
	GetPos(oldX, oldY);
	if (oldW != panelW || oldH != panelH)
		SetSize(panelW, panelH);
	if (oldX != panelX || oldY != panelY)
		SetPos(panelX, actualY);

	// Cache for PaintBackground
	m_iLayoutMargin = margin;
	m_iLayoutSepY = sepY;
	m_iLayoutTextTop = textTop;
	m_iLayoutTextH = textH;
	m_iLayoutContentW = contentW;
	m_iLayoutDialogH = dialogH;

	// Character name label — full width
	m_pCharacterName->SetPos(margin, topPad);
	m_pCharacterName->SetSize(contentW, labelH);

	// Dialogue rich text — full width
	m_pDialogueText->SetPos(margin, textTop);
	m_pDialogueText->SetSize(contentW, textH);

	// Option buttons: horizontal row below dialog box
	int btnAreaY = dialogH + gap;

	int visibleCount = 0;
	for (int i = 0; i < 5; i++)
	{
		if (m_pOptions[i]->IsVisible())
			visibleCount++;
	}
	if (visibleCount == 0)
		visibleCount = 5;

	int totalSpacing = btnSpacing * (visibleCount - 1);
	int btnW = (contentW - totalSpacing) / visibleCount;
	int btnX = margin;

	for (int i = 0; i < 5; i++)
	{
		m_pOptions[i]->SetPos(btnX, btnAreaY);
		m_pOptions[i]->SetSize(btnW, btnH);
		// Re-apply colors every layout since ApplySchemeSettings overrides them
		m_pOptions[i]->SetDefaultColor(Color(255, 255, 255, 255), Color(20, 20, 20, 230));
		m_pOptions[i]->SetArmedColor(Color(255, 255, 255, 255), Color(40, 40, 40, 240));
		m_pOptions[i]->SetDepressedColor(Color(200, 200, 200, 255), Color(10, 10, 10, 240));
		m_pOptions[i]->SetDisabledFgColor1(Color(100, 100, 100, 255));
		m_pOptions[i]->SetDisabledFgColor2(Color(0, 0, 0, 0));
		if (m_pOptions[i]->IsVisible())
			btnX += btnW + btnSpacing;
	}
}

void CDialoguePanel::PaintBackground()
{
	int w, h;
	GetSize(w, h);

	// Black background — only for the dialog box area (not the buttons below)
	vgui::surface()->DrawSetColor(20, 20, 20, 230);
	vgui::surface()->DrawFilledRect(0, 0, w, m_iLayoutDialogH);

	// Grey RichText background
	vgui::surface()->DrawSetColor(40, 40, 40, 220);
	vgui::surface()->DrawFilledRect(m_iLayoutMargin, m_iLayoutTextTop,
		m_iLayoutMargin + m_iLayoutContentW, m_iLayoutTextTop + m_iLayoutTextH);

	// White separator line between label and rich text
	vgui::surface()->DrawSetColor(180, 180, 180, 200);
	vgui::surface()->DrawFilledRect(m_iLayoutMargin, m_iLayoutSepY,
		m_iLayoutMargin + m_iLayoutContentW, m_iLayoutSepY + 2);
}

void CDialoguePanel::OnTick()
{
	BaseClass::OnTick();

	if (!m_bIsDialogueActive)
		return;

	// =====================================================
	// ANIM_SLIDE_IN: panel slides from off-screen to final position, fading in
	// =====================================================
	if (m_eAnimPhase == ANIM_SLIDE_IN)
	{
		float flElapsed = gpGlobals->realtime - m_flAnimStartTime;
		float flFraction = clamp(flElapsed / DIALOGUE_ANIM_DURATION, 0.0f, 1.0f);
		// Ease-out: starts fast (off-screen), decelerates into final position
		float flSmooth = 1.0f - (1.0f - flFraction) * (1.0f - flFraction);

		// Interpolate alpha: 0 -> 255
		int iAlpha = (int)(255.0f * flSmooth);
		SetAlpha(iAlpha);

		// Interpolate Y position
		int currentY = m_iAnimStartY + (int)((float)(m_iFinalY - m_iAnimStartY) * flSmooth);
		int curX, curY;
		GetPos(curX, curY);
		if (curY != currentY)
			SetPos(curX, currentY);

		if (flFraction >= 1.0f)
		{
			// Slide-in finished — snap to final position
			m_eAnimPhase = ANIM_NONE;
			SetAlpha(255);
			SetPos(curX, m_iFinalY);

			// Start typewriter if text was buffered
			if (m_szTypewriterBuffer[0] != '\0')
			{
				m_bTypewriterActive = true;
				m_flTypewriterLastCharTime = gpGlobals->realtime;
				UpdateTickInterval();
			}
			else
			{
				// No typewriter text — fade buttons in immediately
				BeginButtonsFade();
			}
		}

		return;
	}

	// =====================================================
	// ANIM_BUTTONS_FADE: buttons gradually become visible
	// =====================================================
	if (m_eAnimPhase == ANIM_BUTTONS_FADE)
	{
		float flElapsed = gpGlobals->realtime - m_flAnimStartTime;
		float flFraction = clamp(flElapsed / DIALOGUE_BTN_FADE_DURATION, 0.0f, 1.0f);
		float flSmooth = flFraction * flFraction;

		int btnAlpha = (int)(255.0f * flSmooth);
		for (int i = 0; i < 5; i++)
			m_pOptions[i]->SetAlpha(btnAlpha);

		if (flFraction >= 1.0f)
		{
			m_eAnimPhase = ANIM_NONE;
			for (int i = 0; i < 5; i++)
				m_pOptions[i]->SetAlpha(255);
		}

		// Don't return — fall through so camera tracking continues during button fade
	}

	// =====================================================
	// ANIM_SLIDE_OUT: panel slides down off-screen, fading out
	// =====================================================
	if (m_eAnimPhase == ANIM_SLIDE_OUT)
	{
		float flElapsed = gpGlobals->realtime - m_flAnimStartTime;
		float flFraction = clamp(flElapsed / DIALOGUE_ANIM_DURATION, 0.0f, 1.0f);
		// Ease-in curve (accelerating) — reverse of slide-in's ease-out
		float flSmooth = flFraction * flFraction;

		// Interpolate alpha: 255 -> 0
		int iAlpha = (int)(255.0f * (1.0f - flSmooth));
		SetAlpha(iAlpha);

		// Interpolate Y position: current -> off-screen bottom
		int screenW, screenH;
		vgui::surface()->GetScreenSize(screenW, screenH);
		int targetY = screenH;
		int currentY = m_iAnimStartY + (int)((float)(targetY - m_iAnimStartY) * flSmooth);
		int curX, curY;
		GetPos(curX, curY);
		if (curY != currentY)
			SetPos(curX, currentY);

		if (flFraction >= 1.0f)
		{
			HidePanelImmediate();
		}

		return;
	}

	// Deferred hide: wait for button sounds to finish, then actually close
	if (m_bHidePending)
	{
		if (gpGlobals->curtime >= m_flHideTime)
		{
			m_bHidePending = false;
			HidePanel();
		}
		return;
	}

	// =====================================================
	// Smooth camera tracking: always use realtime-based delta
	// for frame-independent exponential smoothing
	// =====================================================
	if (m_bShouldTrackTarget && m_szFocusTargetName[0])
	{
		C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
		if (pPlayer)
		{
			Vector vecTarget;
			bool bHasTarget = false;

			// Try client-side entity first (NPCs, props, etc.)
			C_BaseEntity* pEnt = FindClientEntityByName(m_szFocusTargetName);
			if (pEnt)
			{
				vecTarget = GetClientEntityFocusPosition(pEnt);
				bHasTarget = true;
			}
			else if (m_bHasServerFocusPos)
			{
				// Entity not on client (info_target, etc.) — request updated position from server
				vecTarget = m_vecServerFocusPos;
				bHasTarget = true;

				char szCmd[256];
				Q_snprintf(szCmd, sizeof(szCmd), "internal_dialogue_focus_update %s", m_szFocusTargetName);
				engine->ClientCmd_Unrestricted(szCmd);
			}

			if (bHasTarget)
			{
				Vector vecPlayerEye = pPlayer->EyePosition();
				Vector vecDir = vecTarget - vecPlayerEye;
				VectorNormalize(vecDir);

				QAngle angGoal;
				VectorAngles(vecDir, angGoal);

				// Compute real delta time since last camera update
				float flNow = gpGlobals->realtime;
				float flDt = flNow - m_flCameraLastTime;
				m_flCameraLastTime = flNow;
				flDt = clamp(flDt, 0.001f, 0.1f);

				// Exponential smoothing: fraction = 1 - e^(-speed * dt)
				// This gives consistent, frame-rate independent smoothing.
				float flFrac = 1.0f - expf(-DIALOGUE_CAMERA_SMOOTH_SPEED * flDt);

				QAngle angCurrent;
				engine->GetViewAngles(angCurrent);

				QAngle angSmooth;
				InterpolateAngles(angCurrent, angGoal, angSmooth, flFrac);
				engine->SetViewAngles(angSmooth);
			}
		}
	}

	// Typewriter effect: time-based character printing.
	// Tags are processed immediately; only visible characters are time-gated.
	if (m_bTypewriterActive && m_szTypewriterBuffer[m_iTypewriterPos] != '\0')
	{
		// Always process tags immediately, regardless of timing
		while (m_szTypewriterBuffer[m_iTypewriterPos] == '<')
		{
			char tagValue[256];
			int consumed = 0;
			bool bHandled = false;

			// <speed=1.0>
			consumed = ParseTag(&m_szTypewriterBuffer[m_iTypewriterPos], "speed", tagValue, sizeof(tagValue));
			if (consumed > 0)
			{
				float speed = (float)atof(tagValue);
				if (speed > 0.0f)
					m_flTypewriterSpeed = speed;
				m_iTypewriterPos += consumed;
				bHandled = true;
			}

			if (!bHandled)
			{
				consumed = ParseTag(&m_szTypewriterBuffer[m_iTypewriterPos], "color", tagValue, sizeof(tagValue));
				if (consumed > 0)
				{
					Color clr;
					if (ParseColorValue(tagValue, clr))
						m_pDialogueText->InsertColorChange(clr);
					m_iTypewriterPos += consumed;
					bHandled = true;
				}
			}

			if (!bHandled)
			{
				consumed = ParseTag(&m_szTypewriterBuffer[m_iTypewriterPos], "focus", tagValue, sizeof(tagValue));
				if (consumed > 0) { LookAtTarget(tagValue); m_iTypewriterPos += consumed; bHandled = true; }
			}

			if (!bHandled)
			{
				consumed = ParseTag(&m_szTypewriterBuffer[m_iTypewriterPos], "anim", tagValue, sizeof(tagValue));
				if (consumed > 0) { PlayNPCAnimation(tagValue); m_iTypewriterPos += consumed; bHandled = true; }
			}

			if (!bHandled)
			{
				consumed = ParseTag(&m_szTypewriterBuffer[m_iTypewriterPos], "command", tagValue, sizeof(tagValue));
				if (consumed > 0) { ExecuteCommand(tagValue); m_iTypewriterPos += consumed; bHandled = true; }
			}

			if (!bHandled)
			{
				consumed = ParseTag(&m_szTypewriterBuffer[m_iTypewriterPos], "sound_npc", tagValue, sizeof(tagValue));
				if (consumed > 0) { PlayNPCSound(tagValue); m_iTypewriterPos += consumed; bHandled = true; }
			}

			if (!bHandled)
			{
				consumed = ParseTag(&m_szTypewriterBuffer[m_iTypewriterPos], "sound_world", tagValue, sizeof(tagValue));
				if (consumed > 0) { PlayGameSound(tagValue); m_iTypewriterPos += consumed; bHandled = true; }
			}

			if (!bHandled)
			{
				consumed = ParseTag(&m_szTypewriterBuffer[m_iTypewriterPos], "sound_typewriter", tagValue, sizeof(tagValue));
				if (consumed > 0)
				{
					Q_strncpy(m_szTypewriterSound, tagValue, sizeof(m_szTypewriterSound));
					m_iTypewriterPos += consumed;
					bHandled = true;
				}
			}

			// Not a recognized tag — break out and print '<' as a character
			if (!bHandled)
				break;

			// After consuming a tag, check if we've reached the end
			if (m_szTypewriterBuffer[m_iTypewriterPos] == '\0')
			{
				m_bTypewriterActive = false;
				BeginButtonsFade();
				return;
			}
		}

		// Check if enough time has passed to print the next character
		float flCharInterval = (float)TYPEWRITER_BASE_INTERVAL_MS / (m_flTypewriterSpeed * 1000.0f);
		float flNow = gpGlobals->realtime;

		if (flNow - m_flTypewriterLastCharTime >= flCharInterval)
		{
			// Print exactly one character
			if (m_szTypewriterBuffer[m_iTypewriterPos] != '\0')
			{
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
				m_flTypewriterLastCharTime = flNow;

				// Play typewriter sound — exactly one sound per character
				if (m_szTypewriterSound[0])
					vgui::surface()->PlaySound(m_szTypewriterSound);

				// Check if we've finished
				if (m_szTypewriterBuffer[m_iTypewriterPos] == '\0')
				{
					m_bTypewriterActive = false;
					BeginButtonsFade();
				}
			}
		}
	}
	else if (m_bTypewriterActive)
	{
		m_bTypewriterActive = false;
		BeginButtonsFade();
	}
}

void CDialoguePanel::OnCommand(const char* pcCommand)
{
	if (!Q_stricmp(pcCommand, "Close"))
	{
		// Defer close too, so close button sound can play
		m_bHidePending = true;
		m_flHideTime = gpGlobals->curtime + DIALOGUE_HIDE_DELAY;
		return;
	}

	// Don't call BaseClass::OnCommand — we handle all commands ourselves.
	// BaseClass would forward unrecognized commands to the parent, which is unnecessary.

	// Split on semicolons to support composite commands (e.g. "cmd x;gotonode y;turnoff")
	char cmdBuf[512];
	Q_strncpy(cmdBuf, pcCommand, sizeof(cmdBuf));

	bool bWantsHide = false;

	char* ctx = NULL;
	char* token = strtok_s(cmdBuf, ";", &ctx);
while (token)
	{
		// Trim leading spaces
		while (*token == ' ')
			token++;

		if (!Q_stricmp(token, "turnoff"))
		{
			bWantsHide = true;
		}
		else if (!Q_strnicmp(token, "gotonode ", 9))
		{
			ShowNode(token + 9);
		}
		else if (!Q_strnicmp(token, "startdiag ", 10))
		{
			LoadFile(token + 10);
		}
		else if (!Q_strnicmp(token, "cmd ", 4))
		{
			engine->ClientCmd_Unrestricted(token + 4);
		}

		token = strtok_s(NULL, ";", &ctx);
	}

	// Defer hide so button release sound has time to play
	if (bWantsHide)
	{
		m_bHidePending = true;
		m_flHideTime = gpGlobals->curtime + DIALOGUE_HIDE_DELAY;
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

	// When switching nodes, stop any ongoing button fade and hide buttons
	if (m_eAnimPhase == ANIM_BUTTONS_FADE)
		m_eAnimPhase = ANIM_NONE;

	// Hide buttons for the new node — they'll fade in after text finishes
	for (int i = 0; i < 5; i++)
	{
		m_pOptions[i]->SetVisible(false);
		m_pOptions[i]->SetAlpha(0);
		m_pOptions[i]->SetEnabled(true);
		m_pOptions[i]->SetText("");
		m_pOptions[i]->SetCommand("");
		m_pOptions[i]->SetAsDefaultButton(false);
		m_pOptions[i]->SetArmedSound("ui/buttonrollover.wav");
		m_pOptions[i]->SetReleasedSound("common/bugreporter_succeeded.wav");
		m_szOptionCondition[i][0] = '\0';
	}

	SetCloseButtonVisible(false);

	// =========================================================
	// Apply new node data
	// =========================================================

	// --- Character name ---
	const char* speaker = pNode->GetString("speaker", NULL);
	if (speaker)
		m_pCharacterName->SetText(speaker);

	// --- Focus camera on entity (NPC or info_target, before other actions so m_szFocusTargetName is set) ---
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
	float nodeSpeed = pNode->GetFloat("speed", m_flDefaultSpeed);
	m_flTypewriterSpeed = nodeSpeed;

	// --- Node-level typewriter sound (can be overridden by inline <sound_typewriter=...>) ---
	const char* nodeTwSound = pNode->GetString("sound_typewriter", "");
	if (nodeTwSound && nodeTwSound[0] != '\0')
		Q_strncpy(m_szTypewriterSound, nodeTwSound, sizeof(m_szTypewriterSound));

	// --- Node-level action sound (can be overridden by inline <sound_action=...>) ---
	const char* nodeActionSound = pNode->GetString("sound_action", "");
	if (nodeActionSound && nodeActionSound[0] != '\0')
		Q_strncpy(m_szCloseSound, nodeActionSound, sizeof(m_szCloseSound));

	// --- Typewriter colored text output ---
	const char* text = pNode->GetString("text", NULL);
	if (text)
	{
		bool bTypewriter = pNode->GetBool("typewriter", m_bDefaultTypewriter);

		if (bTypewriter)
		{
			Q_strncpy(m_szTypewriterBuffer, text, sizeof(m_szTypewriterBuffer));
			m_iTypewriterPos = 0;

			// If slide-in animation is still playing, typewriter starts when it finishes (in OnTick).
			// Otherwise start immediately.
			if (m_eAnimPhase != ANIM_SLIDE_IN)
			{
				m_bTypewriterActive = true;
				m_flTypewriterLastCharTime = gpGlobals->realtime;
				UpdateTickInterval();
			}
		}
		else
		{
			// Instant mode: parse tags and insert text instantly
			const char* p = text;
			while (*p)
			{
				if (*p == '<')
				{
					char tagValue[256];
					int consumed = 0;

					consumed = ParseTag(p, "speed", tagValue, sizeof(tagValue));
					if (consumed > 0) { p += consumed; continue; }

					consumed = ParseTag(p, "color", tagValue, sizeof(tagValue));
					if (consumed > 0)
					{
						Color clr;
						if (ParseColorValue(tagValue, clr))
							m_pDialogueText->InsertColorChange(clr);
						p += consumed;
					continue;
					}

					consumed = ParseTag(p, "focus", tagValue, sizeof(tagValue));
					if (consumed > 0) { LookAtTarget(tagValue); p += consumed; continue; }

					consumed = ParseTag(p, "anim", tagValue, sizeof(tagValue));
					if (consumed > 0) { PlayNPCAnimation(tagValue); p += consumed; continue; }

					consumed = ParseTag(p, "command", tagValue, sizeof(tagValue));
					if (consumed > 0) { ExecuteCommand(tagValue); p += consumed; continue; }

					consumed = ParseTag(p, "sound_npc", tagValue, sizeof(tagValue));
					if (consumed > 0) { PlayNPCSound(tagValue); p += consumed; continue; }

					consumed = ParseTag(p, "sound_world", tagValue, sizeof(tagValue));
					if (consumed > 0) { PlayGameSound(tagValue); p += consumed; continue; }

					consumed = ParseTag(p, "sound_typewriter", tagValue, sizeof(tagValue));
					if (consumed > 0) { p += consumed; continue; }
				}

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
		m_pOptions[i]->SetArmedSound(pNodeOption->GetString("sound_hover", "ui/buttonrollover.wav"));
		m_pOptions[i]->SetReleasedSound(pNodeOption->GetString("sound_press", "common/bugreporter_succeeded.wav"));
		m_pOptions[i]->SetText(pNodeOption->GetString("text", "..."));

		// "enabled" key: absent/empty = always enabled, "0" = always disabled, other string = condition name
		const char* enabledVal = pNodeOption->GetString("enabled", "");
		if (!enabledVal[0])
		{
			m_pOptions[i]->SetEnabled(true);
			m_szOptionCondition[i][0] = '\0';
		}
		else if (!Q_stricmp(enabledVal, "0"))
		{
			m_pOptions[i]->SetEnabled(false);
			m_szOptionCondition[i][0] = '\0';
		}
		else
		{
			Q_strncpy(m_szOptionCondition[i], enabledVal, sizeof(m_szOptionCondition[i]));
			m_pOptions[i]->SetEnabled(IsConditionUnlocked(enabledVal));
		}

		if (pNodeOption->FindKey("exit", false))
			m_pOptions[i]->SetAsDefaultButton(true);

		const char* choiceCmd = pNodeOption->GetString("command", "");
		const char* choiceNext = pNodeOption->GetString("next", "");
		bool bExit = pNodeOption->FindKey("exit", false) != NULL;

		char compositeCmd[512];
		compositeCmd[0] = '\0';

		if (choiceCmd[0])
			Q_snprintf(compositeCmd, sizeof(compositeCmd), "cmd %s", choiceCmd);

		if (choiceNext[0])
		{
			if (compositeCmd[0])
			{
				char temp[512];
				Q_snprintf(temp, sizeof(temp), "%s;gotonode %s", compositeCmd, choiceNext);
				Q_strncpy(compositeCmd, temp, sizeof(compositeCmd));
			}
			else
			{
				Q_snprintf(compositeCmd, sizeof(compositeCmd), "gotonode %s", choiceNext);
			}
		}
		else if (bExit && !compositeCmd[0])
		{
		 Q_strncpy(compositeCmd, "turnoff", sizeof(compositeCmd));
		}
		else if (bExit)
		{
			char temp[512];
			Q_snprintf(temp, sizeof(temp), "%s;turnoff", compositeCmd);
		 Q_strncpy(compositeCmd, temp, sizeof(compositeCmd));
		}

		m_pOptions[i]->SetCommand(compositeCmd);
	}

	// --- Closable (close X button) ---
	if (pNode->FindKey("closable", false))
		SetCloseButtonVisible(true);

	// If text was instant (no typewriter) and slide-in is done,
	// fade buttons in immediately.
	if (!m_bTypewriterActive && m_eAnimPhase == ANIM_NONE)
		BeginButtonsFade();

	// Re-layout so buttons resize based on how many are visible
	InvalidateLayout();
}

void CDialoguePanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	// Apply custom font after scheme is fully loaded
	vgui::HFont hFont = pScheme->GetFont("DialogueFont");
	if (hFont)
	{
		m_pCharacterName->SetFont(hFont);
		m_pDialogueText->SetFont(hFont);
		for (int i = 0; i < 5; i++)
			m_pOptions[i]->SetFont(hFont);
	}

	// Re-apply colors that scheme would override
	m_pCharacterName->SetFgColor(Color(255, 255, 255, 255));
	m_pDialogueText->SetFgColor(Color(255, 255, 255, 255));

	// BaseClass::ApplySchemeSettings resets child alpha to 255.
	// Keep buttons hidden if typewriter is still printing or hasn't started yet.
	if (m_eAnimPhase == ANIM_SLIDE_IN || m_bTypewriterActive ||
		(m_eAnimPhase == ANIM_NONE && m_szTypewriterBuffer[0] != '\0' && m_iTypewriterPos == 0))
	{
		for (int i = 0; i < 5; i++)
			m_pOptions[i]->SetAlpha(0);
	}
}
