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

#define DIALOGUE_FILE_PATH "resource/dialogues/sewers/blackguy_1.txt"
#define TYPEWRITER_BASE_SPEED 2.0f // Base characters per tick at speed multiplier 1.0
#define DIALOGUE_ZOOM_FOV 45       // FOV to zoom to during dialogue (default is ~75)
#define DIALOGUE_ZOOM_RATE 0.3f    // How fast to zoom in/out (seconds)

using namespace vgui;

// Message types matching the server
#define DIALOGUE_MSG_START  0
#define DIALOGUE_MSG_STOP   1
#define DIALOGUE_MSG_NODE   2

void __MsgFunc_DialogueMsg(bf_read &msg)
{
	if (!g_pDialoguePanel)
		return;

	int type = msg.ReadByte();
	char str1[256], str2[64];
	msg.ReadString(str1, sizeof(str1));
	msg.ReadString(str2, sizeof(str2));

	switch (type)
	{
	case DIALOGUE_MSG_START:
		g_pDialoguePanel->LoadFile(str1);
		if (str2[0])
			g_pDialoguePanel->ShowNode(str2);
		g_pDialoguePanel->Show();
		break;

	case DIALOGUE_MSG_STOP:
		g_pDialoguePanel->Hide();
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

class CDialoguePanel : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE(CDialoguePanel, vgui::Frame);

	CDialoguePanel(vgui::VPANEL parent);
	~CDialoguePanel();
	virtual void LoadFile(const char* filePath);
	virtual void ShowNode(const char* nodeName);
	void ShowPanel(void);
	void HidePanel(void);

	protected:
	virtual void OnTick();
	virtual void OnCommand(const char* pcCommand);
	virtual void PerformLayout();

	private:
		void LookAtNPC(const char* npcName);
		void PlayNPCAnimation(const char* actName);
		void ExecuteCommand(const char* cmdText);
		void PlayNPCSound(const char* soundName);
		void PlayGameSound(const char* soundName);

	KeyValues* m_pDialogueKV;
	RichText* m_pDialogueText;
	Button* m_pOptions[5];
	Label* m_pCharacterName;

	char m_szTypewriterBuffer[2048];
	int m_iTypewriterPos;
	bool m_bTypewriterActive;
	float m_flTypewriterSpeed;
	float m_flTypewriterAccum;

	// NPC focus tracking
	EHANDLE m_hFocusNPC;           // Handle to the NPC we're focusing on
	bool m_bShouldTrackNPC;        // Whether to keep tracking the NPC each tick
	bool m_bZoomActive;            // Whether we've applied a zoom

	bool m_bIsDialogueActive;      // Whether the dialogue panel is currently shown
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
	m_hFocusNPC = NULL;
	m_bShouldTrackNPC = false;
	m_bZoomActive = false;

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

	SetScheme(vgui::scheme()->LoadSchemeFromFile("resource/SourceScheme.res", "SourceScheme"));

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
	m_pOptions[3]->SetText("StartDiag");
	m_pOptions[3]->SetCommand(VarArgs("startdiag %s", DIALOGUE_FILE_PATH));

	// Exit button
	m_pOptions[4]->SetContentAlignment(Label::a_west);
	m_pOptions[4]->SetText("Exit");
	m_pOptions[4]->SetTextInset(6, 0);
	m_pOptions[4]->SetVisible(true);
	m_pOptions[4]->SetEnabled(true);
	m_pOptions[4]->SetAsDefaultButton(true);
	m_pOptions[4]->SetArmedSound("ui/buttonrollover.wav");
	m_pOptions[4]->SetReleasedSound("common/bugreporter_failed.wav");
	m_pOptions[4]->SetCommand("turnoff");

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

	// Hide the HUD during dialogue
	engine->ClientCmd_Unrestricted("sv_dialogue_hud 0");

	// Start receiving ticks only when dialogue is active
	vgui::ivgui()->AddTickSignal(GetVPanel(), 100);
}

void CDialoguePanel::HidePanel(void)
{
	m_bIsDialogueActive = false;
	m_bShouldTrackNPC = false;
	m_hFocusNPC = NULL;

	// Stop typewriter if still running
	m_bTypewriterActive = false;
	m_szTypewriterBuffer[0] = '\0';
	m_iTypewriterPos = 0;

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
};
static CDialoguePanelInterface g_DialoguePanel;
IDialoguePanel* g_pDialoguePanel = (IDialoguePanel*)&g_DialoguePanel;

//-----------------------------------------------------------------------------
// Helper: get the head position of an NPC.
// Tries the "ValveBiped.Bip01_Head1" bone first (standard HL2 skeleton),
// then falls back to bounding-box center.
//-----------------------------------------------------------------------------
static Vector GetNPCHeadPosition(C_BaseEntity* pNPC)
{
	C_BaseAnimating* pAnimating = pNPC->GetBaseAnimating();
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
	return pNPC->WorldSpaceCenter();
}

void CDialoguePanel::LookAtNPC(const char* npcName)
{
	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	// Find the NPC by entity name
	C_BaseEntity* ent = NULL;
	C_BaseEntity* pFoundNPC = NULL;
	while ((ent = ClientEntityList().NextBaseEntity(ent)) != NULL)
	{
		const char* entName = ent->GetEntityName();
		if (entName && entName[0] && !Q_stricmp(entName, npcName))
		{
			pFoundNPC = ent;
			break;
		}
	}

	if (!pFoundNPC)
	{
		Warning("CDialoguePanel::LookAtNPC: NPC '%s' not found!\n", npcName);
		m_hFocusNPC = NULL;
		m_bShouldTrackNPC = false;
		return;
	}

	m_hFocusNPC = pFoundNPC;
	m_bShouldTrackNPC = true;

	Vector vecNPCTarget = GetNPCHeadPosition(pFoundNPC);

	Vector vecPlayerEye = pPlayer->EyePosition();
	Vector vecDir = vecNPCTarget - vecPlayerEye;
	VectorNormalize(vecDir);

	QAngle angLookAt;
	VectorAngles(vecDir, angLookAt);
	engine->SetViewAngles(angLookAt);

	// Apply zoom
	if (!m_bZoomActive)
	{
		engine->ClientCmd_Unrestricted(VarArgs("sv_dialogue_zoom %d %.1f", DIALOGUE_ZOOM_FOV, DIALOGUE_ZOOM_RATE));
		m_bZoomActive = true;
	}

	// Ask server to make the NPC face and look at the player
	char szCmd[256];
	Q_snprintf(szCmd, sizeof(szCmd), "sv_dialogue_lookatplayer %s", npcName);
	engine->ClientCmd_Unrestricted(szCmd);
}

void CDialoguePanel::PlayNPCAnimation(const char* actName)
{
	// Send animation request to the server for the currently focused NPC
	C_BaseEntity* pNPC = m_hFocusNPC.Get();
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

	C_BaseEntity* pNPC = m_hFocusNPC.Get();
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

	// Track NPC: keep camera focused on the NPC while dialogue is open
	if (m_bShouldTrackNPC)
	{
		C_BaseEntity* pNPC = m_hFocusNPC.Get();
		C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
		if (pNPC && pPlayer)
		{
			Vector vecNPCTarget = GetNPCHeadPosition(pNPC);

			Vector vecPlayerEye = pPlayer->EyePosition();
			Vector vecDir = vecNPCTarget - vecPlayerEye;
			VectorNormalize(vecDir);

			QAngle angLookAt;
			VectorAngles(vecDir, angLookAt);
			engine->SetViewAngles(angLookAt);
		}
		else
		{
			m_bShouldTrackNPC = false;
			m_hFocusNPC = NULL;
		}
	}

	// Typewriter effect: reveal characters gradually
	if (m_bTypewriterActive && m_szTypewriterBuffer[m_iTypewriterPos] != '\0')
	{
		m_flTypewriterAccum += m_flTypewriterSpeed * TYPEWRITER_BASE_SPEED;

		while (m_flTypewriterAccum >= 1.0f)
		{
			if (m_szTypewriterBuffer[m_iTypewriterPos] == '\0')
			{
				m_bTypewriterActive = false;
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

				// <animate=ACT_TEMPLATE>
				consumed = ParseTag(&m_szTypewriterBuffer[m_iTypewriterPos], "animate", tagValue, sizeof(tagValue));
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

				// <sndnpc=sound/path.wav>
				consumed = ParseTag(&m_szTypewriterBuffer[m_iTypewriterPos], "sndnpc", tagValue, sizeof(tagValue));
				if (consumed > 0)
				{
					PlayNPCSound(tagValue);
					m_iTypewriterPos += consumed;
					continue;
				}

				// <sndgame=sound/path.wav>
				consumed = ParseTag(&m_szTypewriterBuffer[m_iTypewriterPos], "sndgame", tagValue, sizeof(tagValue));
				if (consumed > 0)
				{
					PlayGameSound(tagValue);
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

	// --- Focus camera on NPC (before other actions so m_hFocusNPC is set) ---
	const char* npc = pNode->GetString("npc", "");
	if (npc && npc[0] != '\0')
		LookAtNPC(npc);

	// --- Node-level actions (executed once, before typewriter starts) ---
	const char* nodeAnim = pNode->GetString("animate", "");
	if (nodeAnim && nodeAnim[0] != '\0')
		PlayNPCAnimation(nodeAnim);

	const char* nodeSndNpc = pNode->GetString("sndnpc", "");
	if (nodeSndNpc && nodeSndNpc[0] != '\0')
		PlayNPCSound(nodeSndNpc);

	const char* nodeSndGame = pNode->GetString("sndgame", "");
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
	float nodeSpeed = pNode->GetFloat("speed", 1.0f);
	m_flTypewriterSpeed = nodeSpeed;

	// --- Typewriter colored text output ---
	const char* text = pNode->GetString("text", NULL);
	if (text)
	{
		bool bTypewriter = pNode->GetBool("typewriter", true);

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

					// <animate=ACT_TEMPLATE>
					consumed = ParseTag(p, "animate", tagValue, sizeof(tagValue));
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

					// <sndnpc=sound/path.wav>
					consumed = ParseTag(p, "sndnpc", tagValue, sizeof(tagValue));
					if (consumed > 0)
					{
						PlayNPCSound(tagValue);
						p += consumed;
						continue;
					}

					// <sndgame=sound/path.wav>
					consumed = ParseTag(p, "sndgame", tagValue, sizeof(tagValue));
					if (consumed > 0)
					{
						PlayGameSound(tagValue);
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
		m_pOptions[i]->SetArmedSound(pNodeOption->GetString("hover", "ui/buttonrollover.wav"));
		m_pOptions[i]->SetReleasedSound(pNodeOption->GetString("release", "common/bugreporter_succeeded.wav"));

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

	// --- Leave button (close X) ---
	if (pNode->FindKey("leavebutton", false))
		SetCloseButtonVisible(true);
}
