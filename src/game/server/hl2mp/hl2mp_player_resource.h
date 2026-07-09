//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose:		HL2MP's custom CPlayerResource
//
//				(Added by Mapbase for generalized use; most code will act as though
//				this is a default feature)
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef HL2MP_PLAYER_RESOURCE_H
#define HL2MP_PLAYER_RESOURCE_H
#ifdef _WIN32
#pragma once
#endif

#include "player_resource.h"
#include "hl2mp_player_shared.h"
#include "ai_basenpc_shared.h"

class CHL2MPPlayerResource : public CPlayerResource
{
	DECLARE_CLASS( CHL2MPPlayerResource, CPlayerResource );

public:
	DECLARE_SERVERCLASS();

	CHL2MPPlayerResource();
	~CHL2MPPlayerResource();

	virtual void UpdatePlayerData( void );
	virtual void Spawn( void );
	virtual void Init( int iIndex ) OVERRIDE;

#ifdef MAPBASE
	virtual void UpdateNPCData( void );
	void ForceNPCUpdate();

	int GetNPCIndex( int nEntIdx );
#endif

protected:
	virtual void UpdateConnectedPlayer( int iIndex, CBasePlayer *pPlayer ) OVERRIDE;
	virtual void UpdateDisconnectedPlayer( int iIndex ) OVERRIDE;

#ifdef MAPBASE
	// If non-zero, this player is involved with a bot taking over for an idle client.
	// This is a positive index to the bot if this is the client being taken over, and
	// a negative index if this is the bot in question. Both the client and the bot will
	// have this set.
	CNetworkArray( int,			m_iIdleBot,		MAX_PLAYERS_ARRAY_SAFE );

	//--------------------------------------------------------------------
	// NPCs in Player Resource
	// 
	// The HL2MP player resource is also effectively a "NPC resource".
	// NPC names are included in the player resource for two reasons:
	// 
	//		1.	NPC net names need to be transmitted to players outside of
	//			their PVS in case they're killed by a different player/NPC
	//		2.	Default NPC net names are usually shared and shouldn't all
	//			be transmitted individually by each NPC
	// 
	// We only track NPCs which are in the PVS and can reasonably expect
	// to "participate" in the game (i.e. no bullseyes or generic actors).
	//--------------------------------------------------------------------
	virtual bool ShouldConsiderNPC( CAI_BaseNPC *pNPC );
	int FindOrAddNPCIndex( CAI_BaseNPC *pNPC );
	virtual void InitNPC( int iIndex );
	virtual void UpdateConnectedNPC( int iIndex, CAI_BaseNPC *pNPC );
	virtual void UpdateDisconnectedNPC( int iIndex );

	CNetworkArray( int,			m_iNPCs,		MAX_PLAYER_RESOURCE_AIS );	// Ent indices for every NPC tracked by the player resource
	CNetworkArray( int,			m_iNPCNameIds,	MAX_PLAYER_RESOURCE_AIS );
	CNetworkArray( string_t,	m_iszNPCNames,	MAX_PLAYER_RESOURCE_AI_NAMES );
	
	float	m_flNextNPCUpdate;
	bool	m_bForceNPCUpdate;
#endif
};

extern CHL2MPPlayerResource *g_HL2MP_PR;

#endif // TF_PLAYER_RESOURCE_H
