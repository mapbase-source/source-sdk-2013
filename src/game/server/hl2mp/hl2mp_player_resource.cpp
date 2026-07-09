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
#include "hl2mp_player.h"
#include "hl2mp_player_resource.h"
#include "hl2mp_gamerules.h"

#ifdef MAPBASE
// Don't need to do this much since we only keep track of names and we're forced to update when an NPC enters PVS
// Decrease this if NPCs start tracking more stats
#define NPC_SEND_FREQUENCY 5.0f
#endif

CHL2MPPlayerResource *g_HL2MP_PR;

// Datatable
IMPLEMENT_SERVERCLASS_ST( CHL2MPPlayerResource, DT_HL2MPPlayerResource )
#ifdef MAPBASE
	SendPropArray3( SENDINFO_ARRAY3( m_iIdleBot ), SendPropInt( SENDINFO_ARRAY( m_iIdleBot ), -1, SPROP_VARINT ) ),

	// NPCs
	SendPropArray3( SENDINFO_ARRAY3( m_iNPCs ), SendPropInt( SENDINFO_ARRAY( m_iNPCs ), -1, SPROP_UNSIGNED | SPROP_VARINT ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_iNPCNameIds ), SendPropInt( SENDINFO_ARRAY( m_iNPCNameIds ), -1, SPROP_UNSIGNED | SPROP_VARINT ) ),
	SendPropArray( SendPropStringT( SENDINFO_ARRAY( m_iszNPCNames ) ), m_iszNPCNames ),
#endif
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( hl2mp_player_manager, CHL2MPPlayerResource );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHL2MPPlayerResource::CHL2MPPlayerResource( void )
{
	g_HL2MP_PR = this;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHL2MPPlayerResource::~CHL2MPPlayerResource( void )
{
	g_HL2MP_PR = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2MPPlayerResource::UpdatePlayerData( void )
{
	BaseClass::UpdatePlayerData();

#ifdef MAPBASE
	if (m_flNextNPCUpdate < gpGlobals->curtime)
	{
		UpdateNPCData();
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2MPPlayerResource::UpdateConnectedPlayer( int iIndex, CBasePlayer *pPlayer )
{
	BaseClass::UpdateConnectedPlayer( iIndex, pPlayer );

#ifdef MAPBASE
	CHL2MP_Player *pHL2MPPlayer = ToHL2MPPlayer( pPlayer );
	if ( pHL2MPPlayer && pHL2MPPlayer->GetBotTakeOverAvatar() )
	{
		// Stores the reciprocal takeover client.
		// This index is positive if this client is the real player, negative if this client is the bot.
		int nAvatarIdx = pHL2MPPlayer->GetBotTakeOverAvatar()->entindex();
		m_iIdleBot.Set( iIndex, pHL2MPPlayer->IsFakeClient() ? -nAvatarIdx : nAvatarIdx );
	}
	else
	{
		m_iIdleBot.Set( iIndex, 0 );
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2MPPlayerResource::UpdateDisconnectedPlayer( int iIndex )
{
	BaseClass::UpdateDisconnectedPlayer( iIndex );
}


#ifdef MAPBASE
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2MPPlayerResource::UpdateNPCData()
{
	if (g_AI_Manager.NumAIs() > 0)
	{
		// Keep track of who was updated
		bool bUpdated[MAX_PLAYER_RESOURCE_AIS];
		memset( bUpdated, 0, sizeof( bool ) * MAX_PLAYER_RESOURCE_AIS );

		// Update NPCs
		CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
		for ( int i = 0; i < g_AI_Manager.NumAIs(); i++ )
		{
			if ( ppAIs[i] == NULL || !ShouldConsiderNPC( ppAIs[i] ) )
				continue;
	
			const char *pszNetName = ppAIs[i]->GetNetName();
			if ( pszNetName && *pszNetName )
			{
				int nIdx = FindOrAddNPCIndex( ppAIs[i] );
				if (nIdx != -1)
				{
					UpdateConnectedNPC( nIdx, ppAIs[i] );
					bUpdated[nIdx] = true;
				}
			}
		}

		// Clean up stale NPCs
		for ( int i = 0; i < MAX_PLAYER_RESOURCE_AIS; i++ )
		{
			if (!bUpdated[i] && m_iNPCs[i] != 0)
			{
				UpdateDisconnectedNPC( i );
			}
		}

		// Now clean up stale NPC names
		for (int i = 0; i < MAX_PLAYER_RESOURCE_AI_NAMES; i++)
		{
			if (m_iszNPCNames[i] != NULL_STRING)
			{
				bool bInUse = false;
				for ( int j = 0; j < MAX_PLAYER_RESOURCE_AIS; j++ )
				{
					if (m_iNPCNameIds[j] == i)
					{
						bInUse = true;
						break;
					}
				}

				if (!bInUse)
				{
					// Clear this name
					m_iszNPCNames.Set( i, NULL_STRING );
				}
			}
		}
	}

	m_flNextNPCUpdate = gpGlobals->curtime + NPC_SEND_FREQUENCY;
	m_bForceNPCUpdate = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2MPPlayerResource::ForceNPCUpdate()
{
	if (!m_bForceNPCUpdate)
	{
		m_flNextNPCUpdate = gpGlobals->curtime + 0.1f;
		m_bForceNPCUpdate = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CHL2MPPlayerResource::GetNPCIndex( int nEntIdx )
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
bool CHL2MPPlayerResource::ShouldConsiderNPC( CAI_BaseNPC *pNPC )
{
	if (pNPC->Classify() == CLASS_NONE || pNPC->IsMarkedForDeletion())
		return false;

	if (!pNPC->HasCondition( COND_IN_PVS ) && !pNPC->ShouldAlwaysThink()
		&& pNPC->GetState() != NPC_STATE_COMBAT && pNPC->GetState() != NPC_STATE_SCRIPT)
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CHL2MPPlayerResource::FindOrAddNPCIndex( CAI_BaseNPC *pNPC )
{
	int nIdx = pNPC->entindex();
	int nFirstEmpty = -1;
	for (int i = 0; i < MAX_PLAYER_RESOURCE_AIS; i++)
	{
		if (m_iNPCs[i] == nIdx)
			return i;

		if (nFirstEmpty == -1 && m_iNPCs[i] == 0)
			nFirstEmpty = i;
	}

	// New index
	if (nFirstEmpty != -1)
	{
		m_iNPCs.Set( nFirstEmpty, nIdx );
		return nFirstEmpty;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2MPPlayerResource::InitNPC( int iIndex )
{
	m_iNPCs.Set( iIndex, 0 );
	m_iNPCNameIds.Set( iIndex, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2MPPlayerResource::UpdateConnectedNPC( int iIndex, CAI_BaseNPC *pNPC )
{
	// A direct comparison should be fine since these are pooled strings
	string_t netName = pNPC->GetNetNameID();
	if (netName != m_iszNPCNames.Get( m_iNPCNameIds[iIndex] ))
	{
		// Find this name in our list
		m_iNPCNameIds.Set( iIndex, 0 );
		int nFirstEmpty = -1;
		for (int i = 0; i < MAX_PLAYER_RESOURCE_AI_NAMES; i++)
		{
			if (m_iszNPCNames[i] == netName)
			{
				m_iNPCNameIds.Set( iIndex, i );
				break;
			}

			if (nFirstEmpty == -1 && m_iszNPCNames[i] == NULL_STRING)
				nFirstEmpty = i;
		}

		if (m_iNPCNameIds[iIndex] == 0)
		{
			if (nFirstEmpty != -1)
			{
				m_iszNPCNames.Set( nFirstEmpty, netName );
				m_iNPCNameIds.Set( iIndex, nFirstEmpty );
			}
			else
			{
				// TODO: Better error case?
				Warning("CHL2MPPlayerResource::UpdateConnectedNPC: No free names (tried to add %s)\n", STRING(netName));
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2MPPlayerResource::UpdateDisconnectedNPC( int iIndex )
{
	InitNPC( iIndex );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2MPPlayerResource::Spawn( void )
{
	BaseClass::Spawn();

#ifdef MAPBASE
	for ( int i = 0; i < MAX_PLAYER_RESOURCE_AIS; i++ )
	{
		InitNPC( i );
	}

	for ( int i = 0; i < MAX_PLAYER_RESOURCE_AI_NAMES; i++ )
	{
		m_iszNPCNames.Set( i, NULL_STRING );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2MPPlayerResource::Init( int iIndex )
{
	BaseClass::Init( iIndex );
}


