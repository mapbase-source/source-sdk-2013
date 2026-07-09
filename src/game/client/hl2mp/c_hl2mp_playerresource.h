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

#ifndef C_HL2MP_PLAYERRESOURCE_H
#define C_HL2MP_PLAYERRESOURCE_H
#ifdef _WIN32
#pragma once
#endif

#include "c_playerresource.h"
#include "hl2mp_player_shared.h"
#include "ai_basenpc_shared.h"

class C_HL2MP_PlayerResource : public C_PlayerResource
{
	DECLARE_CLASS( C_HL2MP_PlayerResource, C_PlayerResource );
public:
	DECLARE_CLIENTCLASS();

	C_HL2MP_PlayerResource();
	virtual ~C_HL2MP_PlayerResource();

	virtual bool	IsConnected( int index );
	virtual bool	IsAlive( int index );
	
	virtual const char *GetPlayerName( int index );
	virtual int		GetPing( int index );
	virtual int		GetPlayerScore( int index );
	virtual int		GetDeaths( int index );
	virtual int		GetTeam( int index );
	virtual int		GetFrags( int index );
	virtual int		GetHealth( int index );
	
#ifdef MAPBASE
	int				GetIdleBotTarget( int index );
	inline bool			IsIdleBot( int index )			{ return m_iIdleBot[index] < 0; }
	inline bool			IsUsingIdleBot( int index )		{ return m_iIdleBot[index] > 0; }

	const char *GetNPCName( int nEntIdx );

protected:


	int m_iIdleBot[MAX_PLAYERS_ARRAY_SAFE];

	//-----------------------------------------

	int GetNPCIndex( int nEntIdx );

	int m_iNPCs[MAX_PLAYER_RESOURCE_AIS];
	int m_iNPCNameIds[MAX_PLAYER_RESOURCE_AIS];
	char m_iszNPCNames[MAX_PLAYER_RESOURCE_AI_NAMES][MAX_PLAYER_NAME_LENGTH];
#endif
};

extern C_HL2MP_PlayerResource *g_HL2MP_PR;

#endif // C_HL2MP_PLAYERRESOURCE_H
