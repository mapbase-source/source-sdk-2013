//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 =================
//
// Purpose: Server and client implementation of Mapbase's console groups.
//
//=============================================================================

#include "cbase.h"
#include "mapbase_con_groups.h"

CUtlVector<ConGroup_t *> g_GameConGroups;

//-----------------------------------------------------------------------------

// Game console groups
ConGroup_t ConGroup_Physics			( g_GameConGroups, "Physics" );
ConGroup_t ConGroup_EntityIO		( g_GameConGroups, "Entity IO" );
ConGroup_t ConGroup_NPC_AI		( g_GameConGroups, "NPC AI" );
ConGroup_t ConGroup_NPC_Scripts	( g_GameConGroups, "NPC scripts" );
ConGroup_t ConGroup_SpeechAI	( g_GameConGroups, "Speech AI" );
ConGroup_t ConGroup_Choreo		( g_GameConGroups, "Choreo" );
ConGroup_t ConGroup_DeathNotice	( g_GameConGroups, "Death Notice" );
#if defined( TF_DLL ) || defined( TF_CLIENT_DLL )
ConGroup_t ConGroup_GC			( g_GameConGroups, "Game Coordinator" );
#endif
