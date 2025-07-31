//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "decals.h"
#include "env_player_surface_trigger.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( env_player_surface_trigger, CEnvPlayerSurfaceTrigger );

BEGIN_DATADESC( CEnvPlayerSurfaceTrigger )
	DEFINE_KEYFIELD( m_iTargetGameMaterial, FIELD_INTEGER, "gamematerial" ),
#ifdef MAPBASE_MP
	DEFINE_AUTO_ARRAY( m_iCurrentGameMaterial, FIELD_INTEGER ),
	DEFINE_AUTO_ARRAY( m_iLastGameMaterial, FIELD_INTEGER ),
	DEFINE_FIELD( m_nNumOnMaterial, FIELD_INTEGER ),
#else
	DEFINE_FIELD( m_iCurrentGameMaterial, FIELD_INTEGER ),
#endif
	DEFINE_FIELD( m_bDisabled, FIELD_BOOLEAN ),

	DEFINE_THINKFUNC( UpdateMaterialThink ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),

	// Outputs
	DEFINE_OUTPUT(m_OnSurfaceChangedToTarget, "OnSurfaceChangedToTarget"),
	DEFINE_OUTPUT(m_OnSurfaceChangedFromTarget, "OnSurfaceChangedFromTarget"),
#ifdef MAPBASE
	// Used in MP
	DEFINE_OUTPUT( m_OnSurfaceChangedToTargetAll, "OnSurfaceChangedToTargetAll" ),
	DEFINE_OUTPUT( m_OnSurfaceChangedFromTargetAll, "OnSurfaceChangedFromTargetAll" ),
#endif
END_DATADESC()

// Global list of surface triggers
CUtlVector< CHandle<CEnvPlayerSurfaceTrigger> >	g_PlayerSurfaceTriggers;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEnvPlayerSurfaceTrigger::~CEnvPlayerSurfaceTrigger( void )
{
	g_PlayerSurfaceTriggers.FindAndRemove( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvPlayerSurfaceTrigger::Spawn( void )
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );

#ifdef MAPBASE_MP
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		m_iCurrentGameMaterial[i] = 0;
		m_iLastGameMaterial[i] = 0;
	}
#else
	m_iCurrentGameMaterial = 0;
#endif
	m_bDisabled = false;

	g_PlayerSurfaceTriggers.AddToTail( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvPlayerSurfaceTrigger::OnRestore( void )
{
	BaseClass::OnRestore();

	g_PlayerSurfaceTriggers.AddToTail( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvPlayerSurfaceTrigger::SetPlayerSurface( CBasePlayer *pPlayer, char gameMaterial )
{
	// Ignore players in the air (stops bunny hoppers escaping triggers)
	if ( gameMaterial == 0 )
		return;

	// Loop through the surface triggers and tell them all about the change
	int iCount = g_PlayerSurfaceTriggers.Count();
	for ( int i = 0; i < iCount; i++ )
	{
		g_PlayerSurfaceTriggers[i]->PlayerSurfaceChanged( pPlayer, gameMaterial );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvPlayerSurfaceTrigger::PlayerSurfaceChanged( CBasePlayer *pPlayer, char gameMaterial )
{
	if ( m_bDisabled )
		return;

	// Fire the output if we've changed, but only if it involves the target material
#ifdef MAPBASE_MP
	int idx = pPlayer->entindex();
	if ( gameMaterial != (char)(m_iCurrentGameMaterial[idx]) &&
	     ( gameMaterial == m_iTargetGameMaterial || m_iCurrentGameMaterial[idx] == m_iTargetGameMaterial ) )
	{
		DevMsg( 2, "Player changed material to %d (was %d)\n", gameMaterial, m_iCurrentGameMaterial[idx] );

		m_iCurrentGameMaterial[idx] = (int)gameMaterial;

		SetThink( &CEnvPlayerSurfaceTrigger::UpdateMaterialThink );
		SetNextThink( gpGlobals->curtime );
	}
#else
	if ( gameMaterial != (char)m_iCurrentGameMaterial &&
	     ( gameMaterial == m_iTargetGameMaterial || m_iCurrentGameMaterial == m_iTargetGameMaterial ) )
	{
		DevMsg( 2, "Player changed material to %d (was %d)\n", gameMaterial, m_iCurrentGameMaterial );

		m_iCurrentGameMaterial = (int)gameMaterial;

		SetThink( &CEnvPlayerSurfaceTrigger::UpdateMaterialThink );
		SetNextThink( gpGlobals->curtime );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Think function to fire outputs. Done this way so that sv_alternate ticks
//			doesn't allow multiple surface changes in the same tick to fire outputs.
//-----------------------------------------------------------------------------
void CEnvPlayerSurfaceTrigger::UpdateMaterialThink( void )
{
#ifdef MAPBASE_MP
	CBasePlayer *pFirstPlayer = NULL;
	int nNumOnMaterialLast = m_nNumOnMaterial;

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if (!pFirstPlayer)
			pFirstPlayer = pPlayer;

		if ( m_iCurrentGameMaterial[i] != m_iLastGameMaterial[i] )
		{
			if ( m_iCurrentGameMaterial[i] == m_iTargetGameMaterial )
			{
				m_OnSurfaceChangedToTarget.FireOutput( pPlayer, this );
				m_nNumOnMaterial++;
			}
			else 
			{
				m_OnSurfaceChangedFromTarget.FireOutput( pPlayer, this );

				if (m_nNumOnMaterial > 0)
					m_nNumOnMaterial--;
			}
		}

		m_iLastGameMaterial[i] = m_iCurrentGameMaterial[i];
	}

	if ( nNumOnMaterialLast == 0 && m_nNumOnMaterial > 0 )
	{
		m_OnSurfaceChangedToTargetAll.FireOutput( pFirstPlayer, this );
	}
	else if ( nNumOnMaterialLast > 0 && m_nNumOnMaterial == 0 )
	{
		m_OnSurfaceChangedFromTargetAll.FireOutput( pFirstPlayer, this );
	}
#else
	if ( m_iCurrentGameMaterial == m_iTargetGameMaterial )
	{
		m_OnSurfaceChangedToTarget.FireOutput( NULL, this );

#ifdef MAPBASE
		// This is used in MP, but SP has only one player, so just fire it here
		m_OnSurfaceChangedToTargetAll.FireOutput( NULL, this );
#endif
	}
	else 
	{
		m_OnSurfaceChangedFromTarget.FireOutput( NULL, this );

#ifdef MAPBASE
		// This is used in MP, but SP has only one player, so just fire it here
		m_OnSurfaceChangedFromTargetAll.FireOutput( NULL, this );
#endif
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvPlayerSurfaceTrigger::InputDisable( inputdata_t &inputdata )
{
	m_bDisabled = true;
#ifdef MAPBASE_MP
	m_nNumOnMaterial = 0;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvPlayerSurfaceTrigger::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
}
