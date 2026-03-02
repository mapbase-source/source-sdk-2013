//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ========
//
// Purpose: Simple logical entity that counts up to a threshold value, then
//			fires an output when reached.
//
//=============================================================================

#include "cbase.h"
#include "ai_baseactor.h"
#include "activitylist.h"

// Message types sent to the client
#define DIALOGUE_MSG_START    0
#define DIALOGUE_MSG_STOP     1
#define DIALOGUE_MSG_NODE     2
#define DIALOGUE_MSG_SETTINGS 3

class CLogicDialogue : public CLogicalEntity
{
public:
	DECLARE_CLASS(CLogicDialogue, CLogicalEntity);
	DECLARE_DATADESC();

	void InputStartDialogue(inputdata_t& inputData);
	void InputStopDialogue(inputdata_t& inputData);

private:
	// Helper functions
	void SendDialogueMsg(CBasePlayer* pPlayer, int type, const char* str1 = "", const char* str2 = "");
	void SendDialogueSettings(CBasePlayer* pPlayer);

	string_t m_iszDialogueFile;
	string_t m_iszStartNode;

	// Typewriter settings
	bool     m_bTypewriterEnabled;     // Default typewriter on/off (overridden by node "typewriter" or inline <speed=>)
	float    m_flTypewriterSpeed;      // Default typewriter speed multiplier
	string_t m_iszTypewriterSound;     // Looping sound while typewriter is printing

	// Panel sounds
	string_t m_iszOpenSound;           // Sound when dialogue opens
	string_t m_iszCloseSound;          // Sound when dialogue closes

	COutputEvent m_OnDialogueStarted;
	COutputEvent m_OnDialogueStopped;
};

LINK_ENTITY_TO_CLASS(logic_dialogue, CLogicDialogue);

BEGIN_DATADESC(CLogicDialogue)

	DEFINE_KEYFIELD(m_iszDialogueFile, FIELD_STRING, "dialogue_file"),
	DEFINE_KEYFIELD(m_iszStartNode, FIELD_STRING, "start_node"),

	DEFINE_KEYFIELD(m_bTypewriterEnabled, FIELD_BOOLEAN, "typewriter_enabled"),
	DEFINE_KEYFIELD(m_flTypewriterSpeed, FIELD_FLOAT, "typewriter_speed"),
	DEFINE_KEYFIELD(m_iszTypewriterSound, FIELD_STRING, "typewriter_sound"),
	DEFINE_KEYFIELD(m_iszOpenSound, FIELD_STRING, "open_sound"),
	DEFINE_KEYFIELD(m_iszCloseSound, FIELD_STRING, "close_sound"),

	DEFINE_INPUTFUNC(FIELD_VOID, "StartDialogue", InputStartDialogue),
	DEFINE_INPUTFUNC(FIELD_VOID, "StopDialogue", InputStopDialogue),

	DEFINE_OUTPUT(m_OnDialogueStarted, "OnDialogueStarted"),
	DEFINE_OUTPUT(m_OnDialogueStopped, "OnDialogueStopped"),

END_DATADESC()

void CLogicDialogue::SendDialogueMsg(CBasePlayer* pPlayer, int type, const char* str1, const char* str2)
{
	CSingleUserRecipientFilter filter(pPlayer);
	filter.MakeReliable();

	UserMessageBegin(filter, "DialogueMsg");
		WRITE_BYTE(type);
		WRITE_STRING(str1);
		WRITE_STRING(str2);
	MessageEnd();
}

void CLogicDialogue::SendDialogueSettings(CBasePlayer* pPlayer)
{
	CSingleUserRecipientFilter filter(pPlayer);
	filter.MakeReliable();

	UserMessageBegin(filter, "DialogueMsg");
		WRITE_BYTE(DIALOGUE_MSG_SETTINGS);
		WRITE_BYTE(m_bTypewriterEnabled ? 1 : 0);
		WRITE_FLOAT(m_flTypewriterSpeed > 0.0f ? m_flTypewriterSpeed : 1.0f);
		WRITE_STRING(m_iszTypewriterSound != NULL_STRING ? STRING(m_iszTypewriterSound) : "");
		WRITE_STRING(m_iszOpenSound != NULL_STRING ? STRING(m_iszOpenSound) : "");
		WRITE_STRING(m_iszCloseSound != NULL_STRING ? STRING(m_iszCloseSound) : "");
	MessageEnd();
}

void CLogicDialogue::InputStartDialogue(inputdata_t& inputData)
{
	CBasePlayer* pPlayer = UTIL_GetLocalPlayer();
	if (!pPlayer)
		return;

	const char* filePath = STRING(m_iszDialogueFile);
	const char* startNode = STRING(m_iszStartNode);

	if (!startNode || !startNode[0])
		startNode = "node_start";

	// Send settings first, then start — client applies settings before opening
	SendDialogueSettings(pPlayer);
	SendDialogueMsg(pPlayer, DIALOGUE_MSG_START, filePath, startNode);

	m_OnDialogueStarted.FireOutput(inputData.pActivator, this);
}

void CLogicDialogue::InputStopDialogue(inputdata_t& inputData)
{
	CBasePlayer* pPlayer = UTIL_GetLocalPlayer();
	if (!pPlayer)
		return;

	SendDialogueMsg(pPlayer, DIALOGUE_MSG_STOP);

	m_OnDialogueStopped.FireOutput(inputData.pActivator, this);
}

// Server command for client to request NPC look-at-player (called from ShowNode)
CON_COMMAND_F(sv_dialogue_lookatplayer, "Makes the named NPC look at the player", FCVAR_HIDDEN)
{
	if (args.ArgC() < 2)
		return;

	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	if (!pPlayer)
		pPlayer = UTIL_GetLocalPlayer();
	if (!pPlayer)
		return;

	const char* npcName = args[1];
	CBaseEntity* pEnt = gEntList.FindEntityByName(NULL, npcName);
	if (!pEnt)
		return;

	// Turn NPC body to face the player
	CAI_BaseNPC* pNPC = pEnt->MyNPCPointer();
	if (pNPC)
	{
		Vector vecDir = pPlayer->EyePosition() - pNPC->GetAbsOrigin();
		vecDir.z = 0; // only yaw, no pitch for body
		VectorNormalize(vecDir);

		QAngle angFacing;
		VectorAngles(vecDir, angFacing);
		pNPC->Teleport(NULL, &angFacing, NULL);

		// Also set the ideal yaw so the AI doesn't immediately turn away
		if (pNPC->GetMotor())
		{
			pNPC->GetMotor()->SetIdealYawAndUpdate(angFacing[YAW]);
		}
	}

	// Make NPC look at the player with head/eyes
	CAI_BaseActor* pActor = dynamic_cast<CAI_BaseActor*>(pEnt);
	if (pActor)
	{
		pActor->AddLookTarget(pPlayer, 1.0f, 10.0f, 0.2f);
	}
}

// Server command for client to request FOV zoom during dialogue
// Usage: sv_dialogue_zoom <fov> <rate>
// fov=0 resets to default
CON_COMMAND_F(sv_dialogue_zoom, "Sets the player FOV for dialogue zoom", FCVAR_HIDDEN)
{
	if (args.ArgC() < 2)
		return;

	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	if (!pPlayer)
		pPlayer = UTIL_GetLocalPlayer();
	if (!pPlayer)
		return;

	int iFOV = atoi(args[1]);
	float flRate = (args.ArgC() >= 3) ? atof(args[2]) : 0.3f;

	// Clear any existing zoom owner so our request isn't rejected
	if (pPlayer->GetFOVOwner() && pPlayer->GetFOVOwner() != pPlayer)
	{
		pPlayer->SetFOV(pPlayer->GetFOVOwner(), 0, 0.0f);
	}

	bool bResult = pPlayer->SetFOV(pPlayer, iFOV, flRate);
	Msg("sv_dialogue_zoom: FOV=%d rate=%.1f result=%s\n", iFOV, flRate, bResult ? "OK" : "FAILED");
}

// Server command for client to request NPC animation during dialogue
// Usage: sv_dialogue_animate <npc_name> <activity_name>
CON_COMMAND_F(sv_dialogue_animate, "Makes the named NPC play an activity", FCVAR_HIDDEN)
{
	if (args.ArgC() < 3)
		return;

	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	if (!pPlayer)
		pPlayer = UTIL_GetLocalPlayer();
	if (!pPlayer)
		return;

	const char* npcName = args[1];
	const char* actName = args[2];

	CBaseEntity* pEnt = gEntList.FindEntityByName(NULL, npcName);
	if (!pEnt)
	{
		Warning("sv_dialogue_animate: NPC '%s' not found!\n", npcName);
		return;
	}

	CAI_BaseNPC* pNPC = pEnt->MyNPCPointer();
	if (!pNPC)
	{
		Warning("sv_dialogue_animate: '%s' is not an NPC!\n", npcName);
		return;
	}

	int iActivity = ActivityList_IndexForName(actName);
	if (iActivity == kActivityLookup_Missing)
	{
		Warning("sv_dialogue_animate: Activity '%s' not found!\n", actName);
		return;
	}

	pNPC->SetIdealActivity((Activity)iActivity);
}

// Server command for client to hide/show HUD during dialogue
// Usage: sv_dialogue_hud <0|1>  (0 = hide, 1 = show)
CON_COMMAND_F(sv_dialogue_hud, "Hides or shows the HUD for dialogue", FCVAR_HIDDEN)
{
	if (args.ArgC() < 2)
		return;

	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	if (!pPlayer)
		pPlayer = UTIL_GetLocalPlayer();
	if (!pPlayer)
		return;

	int iShow = atoi(args[1]);
	if (iShow)
	{
		pPlayer->m_Local.m_iHideHUD &= ~HIDEHUD_ALL;
	}
	else
	{
		pPlayer->m_Local.m_iHideHUD |= HIDEHUD_ALL;
	}
}
