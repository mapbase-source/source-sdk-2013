//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_AI_BASENPC_H
#define C_AI_BASENPC_H
#ifdef _WIN32
#pragma once
#endif


#include "c_basecombatcharacter.h"
#ifdef MAPBASE_MP
#include "ai_debug_shared.h"
#endif

// NOTE: Moved all controller code into c_basestudiomodel
class C_AI_BaseNPC : public C_BaseCombatCharacter
{
	DECLARE_CLASS( C_AI_BaseNPC, C_BaseCombatCharacter );

public:
	DECLARE_CLIENTCLASS();

	C_AI_BaseNPC();
	virtual unsigned int	PhysicsSolidMaskForEntity( void ) const;
	virtual bool			IsNPC( void ) { return true; }
	bool					IsMoving( void ){ return m_bIsMoving; }
	bool					ShouldAvoidObstacle( void ){ return m_bPerformAvoidance; }
	virtual bool			AddRagdollToFadeQueue( void ) { return m_bFadeCorpse; }

	virtual bool			GetRagdollInitBoneArrays( matrix3x4_t *pDeltaBones0, matrix3x4_t *pDeltaBones1, matrix3x4_t *pCurrentBones, float boneDt ) OVERRIDE;

	int						GetDeathPose( void ) { return m_iDeathPose; }

	bool					ShouldModifyPlayerSpeed( void ) { return m_bSpeedModActive;	}
	int						GetSpeedModifyRadius( void ) { return m_iSpeedModRadius; }
	int						GetSpeedModifySpeed( void ) { return m_iSpeedModSpeed;	}

	void					ClientThink( void );
	void					OnDataChanged( DataUpdateType_t type );
	bool					ImportantRagdoll( void ) { return m_bImportanRagdoll;	}

#ifdef MAPBASE_MP
	virtual int				GetHealth() const { return m_iHealth; }

	virtual const char		*GetPlayerName( void ) const;
	virtual void		DispatchTraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator = NULL );
	void				DecalTrace( trace_t *pTrace, char const *decalName );
	void				ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName );
	virtual bool		IsPlayerAlly( C_BasePlayer *pPlayer = NULL );
	virtual bool		IsNeutralTo( C_BasePlayer *pPlayer = NULL );
#endif

private:
	C_AI_BaseNPC( const C_AI_BaseNPC & ); // not defined, not accessible
	float m_flTimePingEffect;
	int  m_iDeathPose;
	int	 m_iDeathFrame;

	int m_iSpeedModRadius;
	int m_iSpeedModSpeed;

	bool m_bPerformAvoidance;
	bool m_bIsMoving;
	bool m_bFadeCorpse;
	bool m_bSpeedModActive;
	bool m_bImportanRagdoll;

#ifdef MAPBASE_MP
	// User-friendly name used for death notices, etc.
	// Now transmitted by player resource
	//char m_szNetname[32];
	
	// Used to determine whether to draw blood, target ID, etc. on the client
	// Uses first 3 gamerules relationship return codes (GR_TEAMMATE, GR_NOTTEAMMATE, and GR_ENEMY)
	int m_nDefaultPlayerRelationship;

	// Based on the existing decal hack from singleplayer HL2
	bool m_fNoDamageDecal;
#endif
};


#endif // C_AI_BASENPC_H
