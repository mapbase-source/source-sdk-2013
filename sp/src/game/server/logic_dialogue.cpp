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
#define DIALOGUE_MSG_START  0
#define DIALOGUE_MSG_STOP   1
#define DIALOGUE_MSG_NODE   2

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
	void MakeNPCLookAtPlayer(const char* npcName, CBaseEntity* pActivator);

	string_t m_iszDialogueFile;
	string_t m_iszStartNode;

	COutputEvent m_OnDialogueStarted;
	COutputEvent m_OnDialogueStopped;
};

LINK_ENTITY_TO_CLASS(logic_dialogue, CLogicDialogue);

BEGIN_DATADESC(CLogicDialogue)

	DEFINE_KEYFIELD(m_iszDialogueFile, FIELD_STRING, "dialogue_file"),
	DEFINE_KEYFIELD(m_iszStartNode, FIELD_STRING, "start_node"),

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

void CLogicDialogue::MakeNPCLookAtPlayer(const char* npcName, CBaseEntity* pActivator)
{
	if (!npcName || !npcName[0])
		return;

	CBasePlayer* pPlayer = UTIL_GetLocalPlayer();
	if (!pPlayer)
		return;

	CBaseEntity* pEnt = gEntList.FindEntityByName(NULL, npcName, this, pActivator, this);
	if (!pEnt)
		return;

	CAI_BaseActor* pActor = dynamic_cast<CAI_BaseActor*>(pEnt);
	if (pActor)
	{
		pActor->AddLookTarget(pPlayer, 1.0f, 10.0f, 0.2f);
	}
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

	// Tell client: open panel and load file
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
CON_COMMAND(sv_dialogue_lookatplayer, "Makes the named NPC look at the player")
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
CON_COMMAND(sv_dialogue_zoom, "Sets the player FOV for dialogue zoom")
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

	// FOV 0 means reset to default
	pPlayer->SetFOV(pPlayer, iFOV, flRate);
}

// Server command for client to request NPC animation during dialogue
// Usage: sv_dialogue_animate <npc_name> <activity_name>
CON_COMMAND(sv_dialogue_animate, "Makes the named NPC play an activity")
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
