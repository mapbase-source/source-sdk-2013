//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 =================
//
// Purpose: Server and client implementation of Mapbase's console groups.
// 
//=============================================================================

#ifndef GAME_CON_GROUPS_H
#define GAME_CON_GROUPS_H
#ifdef _WIN32
#pragma once
#endif

#include "mapbase_con_groups.h"

extern CUtlVector<ConGroup_t *> g_GameConGroups;

extern ConGroup_t ConGroup_Physics;
extern ConGroup_t ConGroup_EntityIO;
extern ConGroup_t ConGroup_NPC_AI;
extern ConGroup_t ConGroup_NPC_Scripts;
extern ConGroup_t ConGroup_SpeechAI;
extern ConGroup_t ConGroup_Choreo;
extern ConGroup_t ConGroup_DeathNotice;
#if defined( TF_DLL ) || defined( TF_CLIENT_DLL )
extern ConGroup_t ConGroup_GC;
#endif

// Preprocessors for old naming convention

#define CON_GROUP_PHYSICS			ConGroup_Physics
#define CON_GROUP_IO_SYSTEM			ConGroup_EntityIO
#define CON_GROUP_NPC_AI			ConGroup_NPC_AI
#define CON_GROUP_NPC_SCRIPTS		ConGroup_NPC_Scripts
#define CON_GROUP_SPEECH_AI			ConGroup_SpeechAI
#define CON_GROUP_CHOREO			ConGroup_Choreo

#endif
