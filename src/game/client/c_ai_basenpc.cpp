//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_ai_basenpc.h"
#include "engine/ivdebugoverlay.h"

#if defined( HL2_DLL ) || defined( HL2_EPISODIC )
#include "c_basehlplayer.h"
#endif

#ifdef MAPBASE_MP
#include "takedamageinfo.h"
#ifdef HL2MP
#include "c_hl2mp_playerresource.h"
#endif
#endif

#include "death_pose.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define PING_MAX_TIME	2.0

#ifdef MAPBASE_MP
BEGIN_RECV_TABLE_NOBASE( C_AI_BaseNPC, DT_BaseNPCGameData )
	RecvPropInt( RECVINFO( m_iHealth ) ),
	RecvPropInt( RECVINFO( m_takedamage ) ),
	RecvPropInt( RECVINFO( m_bloodColor ) ),
	//RecvPropString( RECVINFO( m_szNetname ) ),	// Transmitted by player resource now
	RecvPropInt( RECVINFO( m_nDefaultPlayerRelationship ) ),
END_RECV_TABLE();
#endif

#ifdef CAI_BaseNPC
#undef CAI_BaseNPC
#endif

IMPLEMENT_CLIENTCLASS_DT( C_AI_BaseNPC, DT_AI_BaseNPC, CAI_BaseNPC )
	RecvPropInt( RECVINFO( m_lifeState ) ),
	RecvPropBool( RECVINFO( m_bPerformAvoidance ) ),
	RecvPropBool( RECVINFO( m_bIsMoving ) ),
	RecvPropBool( RECVINFO( m_bFadeCorpse ) ),
	RecvPropInt( RECVINFO ( m_iDeathPose) ),
	RecvPropInt( RECVINFO( m_iDeathFrame) ),
	RecvPropInt( RECVINFO( m_iSpeedModRadius ) ),
	RecvPropInt( RECVINFO( m_iSpeedModSpeed ) ),
	RecvPropInt( RECVINFO( m_bSpeedModActive ) ),
	RecvPropBool( RECVINFO( m_bImportanRagdoll ) ),
	RecvPropFloat( RECVINFO( m_flTimePingEffect ) ),
#ifdef MAPBASE_MP
	RecvPropDataTable( "npc_gamedata", 0, 0, &REFERENCE_RECV_TABLE( DT_BaseNPCGameData ) ),
#endif
END_RECV_TABLE()

extern ConVar cl_npc_speedmod_intime;

bool NPC_IsImportantNPC( C_BaseAnimating *pAnimating )
{
	C_AI_BaseNPC *pBaseNPC = dynamic_cast < C_AI_BaseNPC* > ( pAnimating );

	if ( pBaseNPC == NULL )
		return false;

	return pBaseNPC->ImportantRagdoll();
}

C_AI_BaseNPC::C_AI_BaseNPC()
{
#ifdef MAPBASE_MP
	SetBloodColor( DONT_BLEED );
#endif
}

//-----------------------------------------------------------------------------
// Makes ragdolls ignore npcclip brushes
//-----------------------------------------------------------------------------
unsigned int C_AI_BaseNPC::PhysicsSolidMaskForEntity( void ) const 
{
	// This allows ragdolls to move through npcclip brushes
	if ( !IsRagdoll() )
	{
		return MASK_NPCSOLID; 
	}
	return MASK_SOLID;
}


void C_AI_BaseNPC::ClientThink( void )
{
	BaseClass::ClientThink();

#ifdef HL2_DLL
	C_BaseHLPlayer *pPlayer = dynamic_cast<C_BaseHLPlayer*>( C_BasePlayer::GetLocalPlayer() );

	if ( ShouldModifyPlayerSpeed() == true )
	{
		if ( pPlayer )
		{
			float flDist = (GetAbsOrigin() - pPlayer->GetAbsOrigin()).LengthSqr();

			if ( flDist <= GetSpeedModifyRadius() )
			{
				if ( pPlayer->m_hClosestNPC )
				{
					if ( pPlayer->m_hClosestNPC != this )
					{
						float flDistOther = (pPlayer->m_hClosestNPC->GetAbsOrigin() - pPlayer->GetAbsOrigin()).Length();

						//If I'm closer than the other NPC then replace it with myself.
						if ( flDist < flDistOther )
						{
							pPlayer->m_hClosestNPC = this;
							pPlayer->m_flSpeedModTime = gpGlobals->curtime + cl_npc_speedmod_intime.GetFloat();
						}
					}
				}
				else
				{
					pPlayer->m_hClosestNPC = this;
					pPlayer->m_flSpeedModTime = gpGlobals->curtime + cl_npc_speedmod_intime.GetFloat();
				}
			}
		}
	}
#endif // HL2_DLL

#ifdef HL2_EPISODIC
	C_BaseHLPlayer *pPlayer = dynamic_cast<C_BaseHLPlayer*>( C_BasePlayer::GetLocalPlayer() );

	if ( pPlayer && m_flTimePingEffect > gpGlobals->curtime )
	{
		float fPingEffectTime = m_flTimePingEffect - gpGlobals->curtime;
		
		if ( fPingEffectTime > 0.0f )
		{
			Vector vRight, vUp;
			Vector vMins, vMaxs;

			float fFade;

			if( fPingEffectTime <= 1.0f )
			{
				fFade = 1.0f - (1.0f - fPingEffectTime);
			}
			else
			{
				fFade = 1.0f;
			}

			GetRenderBounds( vMins, vMaxs );
			AngleVectors (pPlayer->GetAbsAngles(), NULL, &vRight, &vUp );
			Vector p1 = GetAbsOrigin() + vRight * vMins.x + vUp * vMins.z;
			Vector p2 = GetAbsOrigin() + vRight * vMaxs.x + vUp * vMins.z;
			Vector p3 = GetAbsOrigin() + vUp * vMaxs.z;

			int r = 0 * fFade;
			int g = 255 * fFade;
			int b = 0 * fFade;

			if ( debugoverlay )
			{
				debugoverlay->AddLineOverlay( p1, p2, r, g, b, true, 0.05f );
				debugoverlay->AddLineOverlay( p2, p3, r, g, b, true, 0.05f );
				debugoverlay->AddLineOverlay( p3, p1, r, g, b, true, 0.05f );
			}
		}
	}
#endif
}

void C_AI_BaseNPC::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( ( ShouldModifyPlayerSpeed() == true ) || ( m_flTimePingEffect > gpGlobals->curtime ) )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
}

bool C_AI_BaseNPC::GetRagdollInitBoneArrays( matrix3x4_t *pDeltaBones0, matrix3x4_t *pDeltaBones1, matrix3x4_t *pCurrentBones, float boneDt )
{
	bool bRet = true;

	if ( !ForceSetupBonesAtTime( pDeltaBones0, gpGlobals->curtime - boneDt ) )
		bRet = false;

	GetRagdollCurSequenceWithDeathPose( this, pDeltaBones1, gpGlobals->curtime, m_iDeathPose, m_iDeathFrame );
	float ragdollCreateTime = PhysGetSyncCreateTime();
	if ( ragdollCreateTime != gpGlobals->curtime )
	{
		// The next simulation frame begins before the end of this frame
		// so initialize the ragdoll at that time so that it will reach the current
		// position at curtime.  Otherwise the ragdoll will simulate forward from curtime
		// and pop into the future a bit at this point of transition
		if ( !ForceSetupBonesAtTime( pCurrentBones, ragdollCreateTime ) )
			bRet = false;
	}
	else
	{
		if ( !SetupBones( pCurrentBones, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, gpGlobals->curtime ) )
			bRet = false;
	}

	return bRet;
}

#ifdef MAPBASE_MP
//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
const char *C_AI_BaseNPC::GetPlayerName( void ) const
{
#ifdef HL2MP
	if (g_HL2MP_PR)
	{
		return g_HL2MP_PR->GetNPCName( entindex() );
	}
#endif

	return BaseClass::GetPlayerName();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_AI_BaseNPC::DispatchTraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	m_fNoDamageDecal = false;

	if ( info.GetAttacker() && info.GetAttacker()->IsPlayer() )
	{
		if ( IsPlayerAlly( ToBasePlayer( info.GetAttacker() ) ) )
		{
			m_fNoDamageDecal = true;
			return;
		}
	}

	BaseClass::DispatchTraceAttack( info, vecDir, ptr, pAccumulator );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_AI_BaseNPC::DecalTrace( trace_t *pTrace, char const *decalName )
{
	if ( m_fNoDamageDecal )
	{
		// Don't do impact decals when we shouldn't
		// (adapts an existing hack from singleplayer HL2, see serverside counterpart)
		m_fNoDamageDecal = false;
		return;
	}
	BaseClass::DecalTrace( pTrace, decalName );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_AI_BaseNPC::ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName )
{
	if ( m_fNoDamageDecal )
	{
		// Don't do impact decals when we shouldn't
		// (adapts an existing hack from singleplayer HL2, see serverside counterpart)
		m_fNoDamageDecal = false;
		return;
	}
	BaseClass::ImpactTrace( pTrace, iDamageType, pCustomImpactName );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_AI_BaseNPC::IsPlayerAlly( C_BasePlayer *pPlayer )											
{ 
	if ( pPlayer == NULL )
	{
		pPlayer = C_BasePlayer::GetLocalPlayer();
	}

	if ( pPlayer->GetTeamNumber() == TEAM_UNASSIGNED )
	{
		// AI relationship code isn't available here, so we currently transmit a var from the server to determine if we're, at least generically, an ally
		return (m_nDefaultPlayerRelationship == GR_TEAMMATE);
	}
	else if (GetTeamNumber() == pPlayer->GetTeamNumber())
	{
		// Same team probably means allies
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_AI_BaseNPC::IsNeutralTo( C_BasePlayer *pPlayer )
{
	if ( IsPlayerAlly( pPlayer ) )
		return false;

	return (m_nDefaultPlayerRelationship == GR_NOTTEAMMATE);
}
#endif

