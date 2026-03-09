//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ========
//
// Purpose: Simple logical entity that counts up to a threshold value, then
//			fires an output when reached.
//
//=============================================================================

#include "cbase.h"
#include "ai_baseactor.h"
#include "activitylist.h"
#include "engine/IEngineSound.h"

// Message types sent to the client
#define DIALOGUE_MSG_START    0
#define DIALOGUE_MSG_STOP     1
#define DIALOGUE_MSG_NODE     2
#define DIALOGUE_MSG_SETTINGS 3
#define DIALOGUE_MSG_FOCUS    4
#define DIALOGUE_MSG_UNLOCK   5
#define DIALOGUE_MSG_LOCK     6
#define DIALOGUE_MSG_DEFAULT_NPC 7 // <--- new

class CLogicDialogue : public CLogicalEntity
{
public:
	DECLARE_CLASS(CLogicDialogue, CLogicalEntity);
	DECLARE_DATADESC();

	void InputStartDialogue(inputdata_t& inputData);
	void InputStopDialogue(inputdata_t& inputData);
	void InputUnlockChoice(inputdata_t& inputData);
	void InputLockChoice(inputdata_t& inputData);

	void Think(void);

private:
	// Helper functions
	void SendDialogueMsg(CBasePlayer* pPlayer, int type, const char* str1 = "", const char* str2 = "");
	void SendDialogueSettings(CBasePlayer* pPlayer);
	void StopDialogueInternal(void);

	string_t m_iszDialogueFile;
	string_t m_iszStartNode;
	string_t m_iszDefaultNPC;          // Default NPC targetname — dialogue ends if this NPC dies

	// Typewriter settings
	bool     m_bTypewriterEnabled;     // Default typewriter on/off (overridden by node "typewriter" or inline <speed=>)
	float    m_flTypewriterSpeed;      // Default typewriter speed multiplier
	string_t m_iszTypewriterSound;     // Looping sound while typewriter is printing

	// Panel sounds
	string_t m_iszOpenSound;           // Sound when dialogue opens
	string_t m_iszCloseSound;          // Sound when dialogue closes

	bool     m_bDialogueActive;        // Whether dialogue is currently running

	COutputEvent m_OnDialogueStarted;
	COutputEvent m_OnDialogueStopped;
};

LINK_ENTITY_TO_CLASS(logic_dialogue, CLogicDialogue);

BEGIN_DATADESC(CLogicDialogue)

	DEFINE_KEYFIELD(m_iszDialogueFile, FIELD_STRING, "dialogue_file"),
	DEFINE_KEYFIELD(m_iszStartNode, FIELD_STRING, "start_node"),
	DEFINE_KEYFIELD(m_iszDefaultNPC, FIELD_STRING, "default_npc"),

	DEFINE_KEYFIELD(m_bTypewriterEnabled, FIELD_BOOLEAN, "typewriter_enabled"),
	DEFINE_KEYFIELD(m_flTypewriterSpeed, FIELD_FLOAT, "typewriter_speed"),
	DEFINE_KEYFIELD(m_iszTypewriterSound, FIELD_STRING, "typewriter_sound"),
	DEFINE_KEYFIELD(m_iszOpenSound, FIELD_STRING, "open_sound"),
	DEFINE_KEYFIELD(m_iszCloseSound, FIELD_STRING, "close_sound"),

	DEFINE_FIELD(m_bDialogueActive, FIELD_BOOLEAN),

	DEFINE_INPUTFUNC(FIELD_VOID, "StartDialogue", InputStartDialogue),
	DEFINE_INPUTFUNC(FIELD_VOID, "StopDialogue", InputStopDialogue),
	DEFINE_INPUTFUNC(FIELD_STRING, "UnlockChoice", InputUnlockChoice),
	DEFINE_INPUTFUNC(FIELD_STRING, "LockChoice", InputLockChoice),

	DEFINE_THINKFUNC(Think),

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

	// Send default_npc as a separate message if set
	if (m_iszDefaultNPC != NULL_STRING && STRING(m_iszDefaultNPC)[0])
	{
		UserMessageBegin(filter, "DialogueMsg");
			WRITE_BYTE(DIALOGUE_MSG_DEFAULT_NPC);
			WRITE_STRING(STRING(m_iszDefaultNPC));
		MessageEnd();
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

	// Send settings first, then start — client applies settings before opening
	SendDialogueSettings(pPlayer);
	SendDialogueMsg(pPlayer, DIALOGUE_MSG_START, filePath, startNode);

	m_bDialogueActive = true;

	// Start thinking to monitor NPC death
	if (m_iszDefaultNPC != NULL_STRING)
	{
		SetThink(&CLogicDialogue::Think);
		SetNextThink(gpGlobals->curtime + 0.1f);
	}

	m_OnDialogueStarted.FireOutput(inputData.pActivator, this);
}

void CLogicDialogue::InputStopDialogue(inputdata_t& inputData)
{
	StopDialogueInternal();
}

void CLogicDialogue::StopDialogueInternal(void)
{
	if (!m_bDialogueActive)
		return;

	m_bDialogueActive = false;
	SetThink(NULL);

	CBasePlayer* pPlayer = UTIL_GetLocalPlayer();
	if (!pPlayer)
		return;

	SendDialogueMsg(pPlayer, DIALOGUE_MSG_STOP);

	m_OnDialogueStopped.FireOutput(NULL, this);
}

void CLogicDialogue::InputUnlockChoice(inputdata_t& inputData)
{
	CBasePlayer* pPlayer = UTIL_GetLocalPlayer();
	if (!pPlayer)
		return;

	const char* condName = inputData.value.String();
	if (!condName || !condName[0])
		return;

	CSingleUserRecipientFilter filter(pPlayer);
	filter.MakeReliable();

	UserMessageBegin(filter, "DialogueMsg");
		WRITE_BYTE(DIALOGUE_MSG_UNLOCK);
		WRITE_STRING(condName);
	MessageEnd();
}

void CLogicDialogue::InputLockChoice(inputdata_t& inputData)
{
	CBasePlayer* pPlayer = UTIL_GetLocalPlayer();
	if (!pPlayer)
		return;

	const char* condName = inputData.value.String();
	if (!condName || !condName[0])
		return;

	CSingleUserRecipientFilter filter(pPlayer);
	filter.MakeReliable();

	UserMessageBegin(filter, "DialogueMsg");
		WRITE_BYTE(DIALOGUE_MSG_LOCK);
		WRITE_STRING(condName);
	MessageEnd();
}

void CLogicDialogue::Think(void)
{
	if (!m_bDialogueActive)
		return;

	if (m_iszDefaultNPC != NULL_STRING)
	{
		CBaseEntity* pNPC = gEntList.FindEntityByName(NULL, STRING(m_iszDefaultNPC));
		if (!pNPC || !pNPC->IsAlive())
		{
			StopDialogueInternal();
			return;
		}
	}

	SetNextThink(gpGlobals->curtime + 0.1f);
}

//-----------------------------------------------------------------------------
// Helper: get the head/center position of an entity on the server.
//-----------------------------------------------------------------------------
static Vector GetEntityFocusPosition(CBaseEntity* pEnt)
{
	CBaseAnimating* pAnimating = pEnt->GetBaseAnimating();
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
	return pEnt->GetAbsOrigin();
}

// Server command: client requests focus on an entity by targetname.
// Finds the entity, handles NPC look-at-player, and sends position back to client.
CON_COMMAND_F(internal_dialogue_focus, "Focuses dialogue camera on the named entity", FCVAR_HIDDEN)
{
	if (args.ArgC() < 2)
		return;

	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	if (!pPlayer)
		pPlayer = UTIL_GetLocalPlayer();
	if (!pPlayer)
		return;

	const char* targetName = args[1];
	CBaseEntity* pEnt = gEntList.FindEntityByName(NULL, targetName);
	if (!pEnt)
	{
		Warning("sv_dialogue_focus: Entity '%s' not found!\n", targetName);
		return;
	}

	// Turn NPC body to face the player (only affects NPCs)
	CAI_BaseNPC* pNPC = pEnt->MyNPCPointer();
	if (pNPC)
	{
		Vector vecDir = pPlayer->EyePosition() - pNPC->GetAbsOrigin();
		vecDir.z = 0;
		VectorNormalize(vecDir);

		QAngle angFacing;
		VectorAngles(vecDir, angFacing);
		pNPC->Teleport(NULL, &angFacing, NULL);

		if (pNPC->GetMotor())
		{
			pNPC->GetMotor()->SetIdealYawAndUpdate(angFacing[YAW]);
		}
	}

	// Make NPC look at the player with head/eyes (only affects actors)
	CAI_BaseActor* pActor = dynamic_cast<CAI_BaseActor*>(pEnt);
	if (pActor)
	{
		pActor->AddLookTarget(pPlayer, 1.0f, 10.0f, 0.2f);
	}

	// Send focus position back to client
	Vector vecFocus = GetEntityFocusPosition(pEnt);

	CSingleUserRecipientFilter filter(pPlayer);
	filter.MakeReliable();

	UserMessageBegin(filter, "DialogueMsg");
		WRITE_BYTE(DIALOGUE_MSG_FOCUS);
		WRITE_FLOAT(vecFocus.x);
		WRITE_FLOAT(vecFocus.y);
		WRITE_FLOAT(vecFocus.z);
	MessageEnd();
}

// Server command: lightweight position update for server-only entities (info_target, etc.)
// Called each tick by the client when the focused entity doesn't exist client-side.
CON_COMMAND_F(internal_dialogue_focus_update, "Updates dialogue focus position for server-only entities", FCVAR_HIDDEN)
{
	if (args.ArgC() < 2)
		return;

	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	if (!pPlayer)
		pPlayer = UTIL_GetLocalPlayer();
	if (!pPlayer)
		return;

	const char* targetName = args[1];
	CBaseEntity* pEnt = gEntList.FindEntityByName(NULL, targetName);
	if (!pEnt)
		return;

	Vector vecFocus = GetEntityFocusPosition(pEnt);

	CSingleUserRecipientFilter filter(pPlayer);
	filter.MakeReliable();

	UserMessageBegin(filter, "DialogueMsg");
		WRITE_BYTE(DIALOGUE_MSG_FOCUS);
		WRITE_FLOAT(vecFocus.x);
		WRITE_FLOAT(vecFocus.y);
		WRITE_FLOAT(vecFocus.z);
	MessageEnd();
}

// Server command: play sound from the named entity (with lip sync).
// Uses enginesound->PrecacheSound for runtime precache (bypasses CBaseEntity
// wrapper warnings) then EmitSound from the entity so lip sync works on NPCs.
CON_COMMAND_F(internal_dialogue_sound, "Plays a sound from the named entity", FCVAR_HIDDEN)
{
	if (args.ArgC() < 3)
		return;

	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	if (!pPlayer)
		pPlayer = UTIL_GetLocalPlayer();
	if (!pPlayer)
		return;

	const char* targetName = args[1];
	const char* soundName = args[2];

	CBaseEntity* pEnt = gEntList.FindEntityByName(NULL, targetName);
	if (!pEnt)
	{
		Warning("internal_dialogue_sound: Entity '%s' not found!\n", targetName);
		return;
	}

	// Runtime precache via engine directly (no "Direct precache" warning).
	// In SP, late additions to the sound precache string table still work.
	enginesound->PrecacheSound(soundName, false);

	CPASAttenuationFilter sndFilter(pEnt, SNDLVL_TALKING);
	EmitSound_t ep;
	ep.m_nChannel = CHAN_VOICE;
	ep.m_pSoundName = soundName;
	ep.m_flVolume = 1.0f;
	ep.m_SoundLevel = SNDLVL_TALKING;
	pEnt->EmitSound(sndFilter, pEnt->entindex(), ep);
}

// Server command for client to request FOV zoom during dialogue
// Usage: sv_dialogue_zoom <fov> <rate>
// fov=0 resets to default
CON_COMMAND_F(internal_dialogue_zoom, "Sets the player FOV for dialogue zoom", FCVAR_HIDDEN)
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

	pPlayer->SetFOV(pPlayer, iFOV, flRate);
}

// Server command for client to request NPC animation during dialogue
// Usage: sv_dialogue_animate <npc_name> <activity_name>
CON_COMMAND_F(internal_dialogue_animate, "Makes the named NPC play an activity", FCVAR_HIDDEN)
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
		return;

	CBaseAnimating* pAnimating = pEnt->GetBaseAnimating();
	if (!pAnimating)
		return;

	int iActivity = ActivityList_IndexForName(actName);
	if (iActivity == kActivityLookup_Missing)
		return;

	// Force the sequence directly so a new animation always starts immediately,
	// even if the previous one hasn't finished yet.
	int iSequence = pAnimating->SelectWeightedSequence((Activity)iActivity);
	if (iSequence == ACTIVITY_NOT_AVAILABLE)
		return;

	pAnimating->SetSequence(iSequence);
	pAnimating->ResetSequenceInfo();

	// If this is an NPC, also update its activity state so the AI
	// doesn't immediately override our forced sequence.
	CAI_BaseNPC* pNPC = pEnt->MyNPCPointer();
	if (pNPC)
	{
		pNPC->SetActivity((Activity)iActivity);
		pNPC->SetIdealActivity((Activity)iActivity);
	}
}

// Server command for client to hide/show HUD during dialogue
// Usage: sv_dialogue_hud <0|1>  (0 = hide, 1 = show)
CON_COMMAND_F(internal_dialogue_hud, "Hides or shows the HUD for dialogue", FCVAR_HIDDEN)
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

//-----------------------------------------------------------------------------
// Save/Load blocking during dialogue
//
// When dialogue is active, save, load, quicksave and quickload commands are
// intercepted and blocked.  The original engine commands are found once on
// first use, and dynamic ConCommand objects are registered to shadow them.
// When dialogue is not active the originals are forwarded via Dispatch().
//-----------------------------------------------------------------------------

static bool  g_bDialogueSaveLocked = false;
static bool  s_bSaveLockInitialized = false;

static ConCommand *s_pOrigSave      = NULL;
static ConCommand *s_pOrigLoad      = NULL;
static ConCommand *s_pOrigQuickSave = NULL;
static ConCommand *s_pOrigQuickLoad = NULL;

// Dynamic commands that shadow the engine commands (allocated once, never freed)
static ConCommand *s_pOurSave      = NULL;
static ConCommand *s_pOurLoad      = NULL;
static ConCommand *s_pOurQuickSave = NULL;
static ConCommand *s_pOurQuickLoad = NULL;

static void SaveOverride_Callback(const CCommand &args)
{
	if (g_bDialogueSaveLocked)
	{
		Msg("Cannot save during dialogue.\n");
		return;
	}
	if (s_pOrigSave)
		s_pOrigSave->Dispatch(args);
}

static void LoadOverride_Callback(const CCommand &args)
{
	if (g_bDialogueSaveLocked)
	{
		Msg("Cannot load during dialogue.\n");
		return;
	}
	if (s_pOrigLoad)
		s_pOrigLoad->Dispatch(args);
}

static void QuickSaveOverride_Callback(const CCommand &args)
{
	if (g_bDialogueSaveLocked)
	{
		Msg("Cannot quicksave during dialogue.\n");
		return;
	}
	if (s_pOrigQuickSave)
		s_pOrigQuickSave->Dispatch(args);
}

static void QuickLoadOverride_Callback(const CCommand &args)
{
	if (g_bDialogueSaveLocked)
	{
		Msg("Cannot quickload during dialogue.\n");
		return;
	}
	if (s_pOrigQuickLoad)
		s_pOrigQuickLoad->Dispatch(args);
}

static void InitSaveLockCommands()
{
	if (s_bSaveLockInitialized)
		return;
	s_bSaveLockInitialized = true;

	// Grab pointers to the original engine commands BEFORE we shadow them
	s_pOrigSave      = g_pCVar->FindCommand("save");
	s_pOrigLoad      = g_pCVar->FindCommand("load");
	s_pOrigQuickSave = g_pCVar->FindCommand("quicksave");
	s_pOrigQuickLoad = g_pCVar->FindCommand("quickload");

	// Register our overrides — these shadow the engine commands in Mapbase
	s_pOurSave      = new ConCommand("save",      SaveOverride_Callback,      "Save the game", 0);
	s_pOurLoad      = new ConCommand("load",      LoadOverride_Callback,      "Load a saved game", 0);
	s_pOurQuickSave = new ConCommand("quicksave", QuickSaveOverride_Callback, "Quick save", 0);
	s_pOurQuickLoad = new ConCommand("quickload", QuickLoadOverride_Callback, "Quick load", 0);
}

CON_COMMAND_F(internal_dialogue_savelock, "Blocks or unblocks save/load during dialogue", FCVAR_HIDDEN)
{
	if (args.ArgC() < 2)
		return;

	InitSaveLockCommands();

	g_bDialogueSaveLocked = (atoi(args[1]) != 0);
}
