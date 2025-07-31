//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef ENV_PLAYER_SURFACE_TRIGGER_H
#define ENV_PLAYER_SURFACE_TRIGGER_H
#ifdef _WIN32
#pragma once
#endif

#include "baseentity.h"
#include "entityoutput.h"

//-----------------------------------------------------------------------------
// Purpose: Entity that fires outputs whenever the player stands on a different surface
//-----------------------------------------------------------------------------
class CEnvPlayerSurfaceTrigger : public CPointEntity
{
	DECLARE_CLASS( CEnvPlayerSurfaceTrigger, CPointEntity );
public:
	DECLARE_DATADESC();

	~CEnvPlayerSurfaceTrigger( void );
	void	Spawn( void );
	void	OnRestore( void );

	// Main interface to all surface triggers
	static void	SetPlayerSurface( CBasePlayer *pPlayer, char gameMaterial );

	void	UpdateMaterialThink( void );

private:
	void	PlayerSurfaceChanged( CBasePlayer *pPlayer, char gameMaterial );
	void	InputDisable( inputdata_t &inputdata );
	void	InputEnable( inputdata_t &inputdata );

private:
	int		m_iTargetGameMaterial;
#ifdef MAPBASE_MP
	int		m_iCurrentGameMaterial[MAX_PLAYERS];
	int		m_iLastGameMaterial[MAX_PLAYERS];
	int		m_nNumOnMaterial;
#else
	int		m_iCurrentGameMaterial;
#endif
	bool	m_bDisabled;

	// Outputs
	COutputEvent m_OnSurfaceChangedToTarget;
	COutputEvent m_OnSurfaceChangedFromTarget;
#ifdef MAPBASE
	COutputEvent m_OnSurfaceChangedToTargetAll;
	COutputEvent m_OnSurfaceChangedFromTargetAll;
#endif
};

#endif // ENV_PLAYER_SURFACE_TRIGGER_H
