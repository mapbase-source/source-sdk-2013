//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "in_buttons.h"
#include "takedamageinfo.h"
#include "ammodef.h"
#include "hl2mp_gamerules.h"
#ifdef MAPBASE
#include "mapbase/protagonist_system.h"
#include "eventlist.h"
#endif


#ifdef CLIENT_DLL
extern IVModelInfoClient* modelinfo;
#else
extern IVModelInfo* modelinfo;
#endif


#if defined( CLIENT_DLL )

	#include "vgui/ISurface.h"
	#include "vgui_controls/Controls.h"
	#include "c_hl2mp_player.h"
	#include "hud_crosshair.h"

#else

	#include "hl2mp_player.h"
	#include "vphysics/constraints.h"

#endif

#include "weapon_hl2mpbase.h"


// ----------------------------------------------------------------------------- //
// Global functions.
// ----------------------------------------------------------------------------- //

bool IsAmmoType( int iAmmoType, const char *pAmmoName )
{
	return GetAmmoDef()->Index( pAmmoName ) == iAmmoType;
}

static const char * s_WeaponAliasInfo[] = 
{
	"none",	//	WEAPON_NONE = 0,

	//Melee
	"shotgun",	//WEAPON_AMERKNIFE,
	
	NULL,		// end of list marker
};


// ----------------------------------------------------------------------------- //
// CWeaponHL2MPBase tables.
// ----------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponHL2MPBase, DT_WeaponHL2MPBase )

BEGIN_NETWORK_TABLE( CWeaponHL2MPBase, DT_WeaponHL2MPBase )

#ifdef CLIENT_DLL
  
#else
	// world weapon models have no aminations
  //	SendPropExclude( "DT_AnimTimeMustBeFirst", "m_flAnimTime" ),
//	SendPropExclude( "DT_BaseAnimating", "m_nSequence" ),
//	SendPropExclude( "DT_LocalActiveWeaponData", "m_flTimeWeaponIdle" ),
#endif
	
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponHL2MPBase ) 
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_hl2mp_base, CWeaponHL2MPBase );


#ifdef GAME_DLL

	BEGIN_DATADESC( CWeaponHL2MPBase )

	END_DATADESC()

#endif

// ----------------------------------------------------------------------------- //
// CWeaponHL2MPBase implementation. 
// ----------------------------------------------------------------------------- //
CWeaponHL2MPBase::CWeaponHL2MPBase()
{
	SetPredictionEligible( true );
	AddSolidFlags( FSOLID_TRIGGER ); // Nothing collides with these but it gets touches.

	m_flNextResetCheckTime = 0.0f;
}


bool CWeaponHL2MPBase::IsPredicted() const
{ 
	return true;
}

void CWeaponHL2MPBase::WeaponSound( WeaponSound_t sound_type, float soundtime /* = 0.0f */ )
{
#ifdef CLIENT_DLL

		// If we have some sounds from the weapon classname.txt file, play a random one of them
		const char *shootsound = GetWpnData().aShootSounds[ sound_type ]; 
		if ( !shootsound || !shootsound[0] )
			return;

		CBroadcastRecipientFilter filter; // this is client side only
		if ( !te->CanPredict() )
			return;
				
		CBaseEntity::EmitSound( filter, GetPlayerOwner()->entindex(), shootsound, &GetPlayerOwner()->GetAbsOrigin() ); 
#else
		BaseClass::WeaponSound( sound_type, soundtime );
#endif
}


CBasePlayer* CWeaponHL2MPBase::GetPlayerOwner() const
{
	return dynamic_cast< CBasePlayer* >( GetOwner() );
}

CHL2MP_Player* CWeaponHL2MPBase::GetHL2MPPlayerOwner() const
{
	return dynamic_cast< CHL2MP_Player* >( GetOwner() );
}

#ifdef CLIENT_DLL
	
void CWeaponHL2MPBase::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( GetPredictable() && !ShouldPredict() )
		ShutdownPredictable();
}


bool CWeaponHL2MPBase::ShouldPredict()
{
	if ( GetOwner() && GetOwner() == C_BasePlayer::GetLocalPlayer() )
		return true;

	return BaseClass::ShouldPredict();
}


#else
	
void CWeaponHL2MPBase::Spawn()
{
	BaseClass::Spawn();

	// Set this here to allow players to shoot dropped weapons
	SetCollisionGroup( COLLISION_GROUP_WEAPON );
}

void CWeaponHL2MPBase::Materialize( void )
{
	if ( IsEffectActive( EF_NODRAW ) )
	{
		// changing from invisible state to visible.
		EmitSound( "AlyxEmp.Charge" );
		
		RemoveEffects( EF_NODRAW );
		DoMuzzleFlash();
	}

	if ( HasSpawnFlags( SF_NORESPAWN ) == false )
	{
		VPhysicsInitNormal( SOLID_BBOX, GetSolidFlags() | FSOLID_TRIGGER, false );
		SetMoveType( MOVETYPE_VPHYSICS );

		HL2MPRules()->AddLevelDesignerPlacedObject( this );
	}

	if ( HasSpawnFlags( SF_NORESPAWN ) == false )
	{
		if ( GetOriginalSpawnOrigin() == vec3_origin )
		{
			m_vOriginalSpawnOrigin = GetAbsOrigin();
			m_vOriginalSpawnAngles = GetAbsAngles();
		}
	}

	SetPickupTouch();

	SetThink (NULL);
}

int CWeaponHL2MPBase::ObjectCaps()
{
	return BaseClass::ObjectCaps() & ~FCAP_IMPULSE_USE;
}

#endif

void CWeaponHL2MPBase::FallInit( void )
{
#ifndef CLIENT_DLL
	SetModel( GetWorldModel() );
	VPhysicsDestroyObject();

	if ( HasSpawnFlags( SF_NORESPAWN ) == false )
	{
		SetMoveType( MOVETYPE_NONE );
		SetSolid( SOLID_BBOX );
		AddSolidFlags( FSOLID_TRIGGER );

		UTIL_DropToFloor( this, MASK_SOLID );
	}
	else
	{
		if ( !VPhysicsInitNormal( SOLID_BBOX, GetSolidFlags() | FSOLID_TRIGGER, false ) )
		{
			SetMoveType( MOVETYPE_NONE );
			SetSolid( SOLID_BBOX );
			AddSolidFlags( FSOLID_TRIGGER );
		}
		else
		{
	#if !defined( CLIENT_DLL )
			// Constrained start?
			if ( HasSpawnFlags( SF_WEAPON_START_CONSTRAINED ) )
			{
				//Constrain the weapon in place
				IPhysicsObject *pReferenceObject, *pAttachedObject;
				
				pReferenceObject = g_PhysWorldObject;
				pAttachedObject = VPhysicsGetObject();

				if ( pReferenceObject && pAttachedObject )
				{
					constraint_fixedparams_t fixed;
					fixed.Defaults();
					fixed.InitWithCurrentObjectState( pReferenceObject, pAttachedObject );
					
					fixed.constraint.forceLimit	= lbs2kg( 10000 );
					fixed.constraint.torqueLimit = lbs2kg( 10000 );

					IPhysicsConstraint *pConstraint = GetConstraint();

					pConstraint = physenv->CreateFixedConstraint( pReferenceObject, pAttachedObject, NULL, fixed );

					pConstraint->SetGameData( (void *) this );
				}
			}
	#endif //CLIENT_DLL
		}
	}

	SetPickupTouch();
	
	SetThink( &CBaseCombatWeapon::FallThink );

	SetNextThink( gpGlobals->curtime + 0.1f );

#endif
}

const CHL2MPSWeaponInfo &CWeaponHL2MPBase::GetHL2MPWpnData() const
{
	const FileWeaponInfo_t *pWeaponInfo = &GetWpnData();
	const CHL2MPSWeaponInfo *pHL2MPInfo;

	#ifdef _DEBUG
		pHL2MPInfo = dynamic_cast< const CHL2MPSWeaponInfo* >( pWeaponInfo );
		Assert( pHL2MPInfo );
	#else
		pHL2MPInfo = static_cast< const CHL2MPSWeaponInfo* >( pWeaponInfo );
	#endif

	return *pHL2MPInfo;
}
void CWeaponHL2MPBase::FireBullets( const FireBulletsInfo_t &info )
{
	FireBulletsInfo_t modinfo = info;

	modinfo.m_iPlayerDamage = GetHL2MPWpnData().m_iPlayerDamage;

	BaseClass::FireBullets( modinfo );
}

#ifdef MAPBASE
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CWeaponHL2MPBase::GetViewModel( int viewmodelindex ) const
{
	if (GetOwner() && GetOwner()->IsPlayer() && viewmodelindex == 0)
	{
		const char *pszProtagVM = g_ProtagonistSystem.GetProtagonist_ViewModel( static_cast<CBasePlayer *>(GetOwner()), this );
		if (pszProtagVM)
			return pszProtagVM;
	}

	return BaseClass::GetViewModel( viewmodelindex );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponHL2MPBase::GetViewmodelFOVOverride() const
{
	if (GetOwner() && GetOwner()->IsPlayer())
	{
		float *flVMFOV = g_ProtagonistSystem.GetProtagonist_ViewModelFOV( static_cast<CBasePlayer *>(GetOwner()), this );
		if (flVMFOV)
			return *flVMFOV;
	}

	return BaseClass::GetViewmodelFOVOverride();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponHL2MPBase::UsesHands() const
{
	if (GetOwner() && GetOwner()->IsPlayer())
	{
		bool *bProtagUsesHands = g_ProtagonistSystem.GetProtagonist_UsesHands( static_cast<CBasePlayer *>(GetOwner()), this );
		if (bProtagUsesHands)
			return *bProtagUsesHands;
	}

	return BaseClass::UsesHands();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CWeaponHL2MPBase::GetHandRig() const
{
	if (GetOwner() && GetOwner()->IsPlayer())
	{
		int *nProtagHandRig = g_ProtagonistSystem.GetProtagonist_HandRig( static_cast<CBasePlayer *>(GetOwner()), this );
		if (nProtagHandRig)
			return *nProtagHandRig;
	}

	return BaseClass::GetHandRig();
}
#endif


#if defined( CLIENT_DLL )

#include "c_te_effect_dispatch.h"

#define NUM_MUZZLE_FLASH_TYPES 4

#ifdef MAPBASE_MP
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//			*pOperator - 
//-----------------------------------------------------------------------------
void CWeaponHL2MPBase::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	if ( (pEvent->type & AE_TYPE_NEWEVENTSYSTEM) && (pEvent->type & AE_TYPE_CLIENT) )
	{
		/*if ( pEvent->event == AE_NPC_WEAPON_FIRE )
		{
			bool bSecondary = (atoi( pEvent->options ) != 0);
			Operator_ForceNPCFire( pOperator, bSecondary );
			return;
		}
		else*/ if ( pEvent->event == AE_WPN_PLAYWPNSOUND )
		{
			int iSnd = GetWeaponSoundFromString(pEvent->options);
			if ( iSnd != -1 )
			{
				WeaponSound( (WeaponSound_t)iSnd );
			}
		}
	}

	//DevWarning( 2, "Unhandled animation event %d from %s --> %s\n", pEvent->event, pOperator->GetClassname(), GetClassname() );
}
#endif

bool CWeaponHL2MPBase::OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options )
{
#ifdef MAPBASE_MP
	bool bBase = BaseClass::OnFireEvent( pViewModel, origin, angles, event, options );

	if (!bBase)
	{
		//If the player is receiving this message, pass it through
		C_BasePlayer *pOwner = ToBasePlayer( GetOwner() );

		if ( pOwner != NULL )
		{
			animevent_t animEvent;
			animEvent.event = event;
			animEvent.options = options;
			Operator_HandleAnimEvent( &animEvent, pOwner );
			return true;
		}
	}

	return bBase;
#else
	return BaseClass::OnFireEvent( pViewModel, origin, angles, event, options );
#endif
}


void UTIL_ClipPunchAngleOffset( QAngle &in, const QAngle &punch, const QAngle &clip )
{
	QAngle	final = in + punch;

	//Clip each component
	for ( int i = 0; i < 3; i++ )
	{
		if ( final[i] > clip[i] )
		{
			final[i] = clip[i];
		}
		else if ( final[i] < -clip[i] )
		{
			final[i] = -clip[i];
		}

		//Return the result
		in[i] = final[i] - punch[i];
	}
}

#endif

