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
#include "cbase.h"
#include "c_hl2mp_playerresource.h"
#include "hl2mp_gamerules.h"
#ifdef MAPBASE
#include "vgui/ILocalize.h"
#include "iclientmode.h"
#include "gamestringpool.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

C_HL2MP_PlayerResource *g_HL2MP_PR;

IMPLEMENT_CLIENTCLASS_DT( C_HL2MP_PlayerResource, DT_HL2MPPlayerResource, CHL2MPPlayerResource )
#ifdef MAPBASE
	RecvPropArray3( RECVINFO_ARRAY( m_iIdleBot ), RecvPropInt( RECVINFO( m_iIdleBot[0] ) ) ),

	// NPCs
	RecvPropArray3( RECVINFO_ARRAY( m_iNPCs ), RecvPropInt( RECVINFO( m_iNPCs[0] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_iNPCNameIds ), RecvPropInt( RECVINFO( m_iNPCNameIds[0] ) ) ),
	RecvPropArray( RecvPropString( RECVINFO( m_iszNPCNames[0] ) ), m_iszNPCNames ),
#endif
END_RECV_TABLE()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_HL2MP_PlayerResource::C_HL2MP_PlayerResource()
{
	g_HL2MP_PR = this;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_HL2MP_PlayerResource::~C_HL2MP_PlayerResource()
{
	g_HL2MP_PR = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_HL2MP_PlayerResource::IsConnected( int index )
{
#ifdef MAPBASE
	if ( IsIdleBot( index ) )
		return false;
		//index = GetIdleBotTarget( index );
#endif

	return BaseClass::IsConnected( index );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_HL2MP_PlayerResource::IsAlive( int index )
{
#ifdef MAPBASE
	if ( IsUsingIdleBot( index ) )
		index = GetIdleBotTarget( index );
#endif

	return BaseClass::IsAlive( index );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *C_HL2MP_PlayerResource::GetPlayerName( int index )
{
#ifdef MAPBASE
	if ( IsUsingIdleBot( index ) )
	{
		// The player resource does not update names for players which are unconnected but valid.
		// We can't override player name updating without changing base behavior, so just test it here.
		int nBotIdx = GetIdleBotTarget( index );
		if (IsValid( nBotIdx ))
		{
			if (FStrEq( m_szName[nBotIdx], PLAYER_UNCONNECTED_NAME ))
			{
				wchar_t *pIdleLabel = g_pVGuiLocalize->Find( "#game_idle" );
				if (!pIdleLabel)
					pIdleLabel = L"IDLE";

				char szNewName[MAX_PLAYER_NAME_LENGTH];
				V_snprintf( szNewName, MAX_PLAYER_NAME_LENGTH, "%s [%ls]", BaseClass::GetPlayerName( index ), pIdleLabel );

				m_szName[nBotIdx] = AllocPooledString( szNewName );
				return m_szName[nBotIdx];
			}

			return BaseClass::GetPlayerName( nBotIdx );
		}
	}
#endif

	return BaseClass::GetPlayerName( index );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_HL2MP_PlayerResource::GetPing( int index )
{
	return BaseClass::GetPing( index );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_HL2MP_PlayerResource::GetPlayerScore( int index )
{
#ifdef MAPBASE
	if ( IsUsingIdleBot( index ) )
		index = GetIdleBotTarget( index );
#endif

	return BaseClass::GetPlayerScore( index );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_HL2MP_PlayerResource::GetDeaths( int index )
{
#ifdef MAPBASE
	if ( IsUsingIdleBot( index ) )
		index = GetIdleBotTarget( index );
#endif

	return BaseClass::GetDeaths( index );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_HL2MP_PlayerResource::GetTeam( int index )
{
#ifdef MAPBASE
	if ( IsUsingIdleBot( index ) )
		index = GetIdleBotTarget( index );
#endif

	return BaseClass::GetTeam( index );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_HL2MP_PlayerResource::GetFrags( int index )
{
#ifdef MAPBASE
	if ( IsUsingIdleBot( index ) )
		index = GetIdleBotTarget( index );
#endif

	return BaseClass::GetFrags( index );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_HL2MP_PlayerResource::GetHealth( int index )
{
#ifdef MAPBASE
	if ( IsUsingIdleBot( index ) )
		index = GetIdleBotTarget( index );
#endif

	return BaseClass::GetHealth( index );
}

#ifdef MAPBASE
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_HL2MP_PlayerResource::GetIdleBotTarget( int index )
{
	return abs( m_iIdleBot[index] );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_HL2MP_PlayerResource::GetNPCIndex( int nEntIdx )
{
	for (int i = 0; i < MAX_PLAYER_RESOURCE_AIS; i++)
	{
		if (m_iNPCs[i] == nEntIdx)
			return i;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *C_HL2MP_PlayerResource::GetNPCName( int nEntIdx )
{
	int nIdx = GetNPCIndex( nEntIdx );
	if (nIdx == -1)
		return NULL;

	return STRING( m_iszNPCNames[m_iNPCNameIds[nIdx]] );
}
#endif
