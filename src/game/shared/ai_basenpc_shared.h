//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef AI_BASENPC_SHARED_H
#define AI_BASENPC_SHARED_H

#ifdef CLIENT_DLL
#include "c_ai_basenpc.h"
#else
#include "ai_basenpc.h"
#endif

#ifdef CLIENT_DLL
#define CAI_BaseNPC C_AI_BaseNPC
#endif

#ifdef MAPBASE
// How many NPCs and net names the player resource can keep track of in multiplayer.
// See hl2mp_player_resource.h for more information.
enum
{
	MAX_PLAYER_RESOURCE_AIS = 48,		// Should be big enough for all participating NPCs that can be in any player's PVS and active at once in a level. Invisible NPCs and actors are not considered.
	MAX_PLAYER_RESOURCE_AI_NAMES = 16	// Should be big enough for all unique NPC classes plus any potential map-defined net names.
};
#endif

#endif // AI_BASENPC_SHARED_H
