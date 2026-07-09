//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Physics cannon
//
//=============================================================================//

#include "cbase.h"

#ifdef CLIENT_DLL
	#include "c_hl2mp_player.h"
	#include "vcollide_parse.h"
	#include "engine/ivdebugoverlay.h"
	#include "iviewrender_beams.h"
	#include "beamdraw.h"
	#include "c_te_effect_dispatch.h"
	#include "clienteffectprecachesystem.h"
	#include "fx_interpvalue.h"
#else
	#include "hl2mp_player.h"
	#include "soundent.h"
	#include "ndebugoverlay.h"
	#include "ai_basenpc.h"
	#include "player_pickup.h"
	#include "physics_prop_ragdoll.h"
	#include "globalstate.h"
	#include "props.h"
	#include "te_effect_dispatch.h"
	#include "util.h"
	#include "saverestore_utlvector.h"
	#include "prop_combine_ball.h"
	#include "physobj.h"
	#include "citadel_effects_shared.h"
	#include "eventqueue.h"
	#include "player_pickup.h"
#ifdef MAPBASE
	#include "mapbase/GlobalStrings.h"
#endif
#endif

#include "gamerules.h"
#include "soundenvelope.h"
#include "engine/IEngineSound.h"
#include "physics.h"
#include "in_buttons.h"
#include "IEffects.h"
#include "shake.h"
#include "beam_shared.h"
#include "Sprite.h"
#include "physics_saverestore.h"
#include "movevars_shared.h"
#include "weapon_hl2mpbasehlmpcombatweapon.h"
#include "vphysics/friction.h"
#include "weapon_physcannon.h"
#include "debugoverlay_shared.h"
#include "model_types.h"
#include "hl2mp_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	SPRITE_SCALE	128.0f

static const char *s_pWaitForUpgradeContext = "WaitForUpgrade";

ConVar	g_debug_physcannon( "g_debug_physcannon", "0", FCVAR_REPLICATED | FCVAR_CHEAT );

ConVar physcannon_minforce( "physcannon_minforce", "700", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar physcannon_maxforce( "physcannon_maxforce", "1500", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar physcannon_maxmass( "physcannon_maxmass", "250", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar physcannon_mega_tracelength( "physcannon_mega_tracelength", "850", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar physcannon_tracelength( "physcannon_tracelength", "250", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar physcannon_chargetime("physcannon_chargetime", "2", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar physcannon_pullforce( "physcannon_pullforce", "4000", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar physcannon_mega_pullforce( "physcannon_mega_pullforce", "8000", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar physcannon_cone( "physcannon_cone", "0.97", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar physcannon_ball_cone( "physcannon_ball_cone", "0.997", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar player_throwforce( "player_throwforce", "1000", FCVAR_REPLICATED | FCVAR_CHEAT );

#ifndef CLIENT_DLL
#ifdef MAPBASE
ConVar sv_player_enable_propsprint("sv_player_enable_propsprint", "1", FCVAR_NONE, "If enabled, allows the player to sprint while holding a physics object" );
ConVar sv_player_enable_gravgun_sprint("sv_player_enable_gravgun_sprint", "1", FCVAR_NONE, "Enables the player to sprint while holding a phys. object with the gravity gun" );
#endif
extern ConVar hl2_normspeed;
extern ConVar hl2_walkspeed;
#endif

#ifdef CLIENT_DLL

	//Precahce the effects
	CLIENTEFFECT_REGISTER_BEGIN( PrecacheEffectPhysCannon )
	CLIENTEFFECT_MATERIAL( "sprites/orangelight1" )
	CLIENTEFFECT_MATERIAL( "sprites/orangelight1_noz" )
	CLIENTEFFECT_MATERIAL( PHYSCANNON_GLOW_SPRITE )
	CLIENTEFFECT_MATERIAL( PHYSCANNON_ENDCAP_SPRITE )
	CLIENTEFFECT_MATERIAL( PHYSCANNON_CENTER_GLOW )
	CLIENTEFFECT_MATERIAL( PHYSCANNON_BLAST_SPRITE )
	CLIENTEFFECT_REGISTER_END()

#endif	// CLIENT_DLL

// -------------------------------------------------------------------------
//  Physcannon trace filter to handle special cases
// -------------------------------------------------------------------------

class CTraceFilterPhyscannon : public CTraceFilterSimple
{
public:
	DECLARE_CLASS( CTraceFilterPhyscannon, CTraceFilterSimple );

	CTraceFilterPhyscannon( const IHandleEntity *passentity, int collisionGroup )
		: CTraceFilterSimple( NULL, collisionGroup ), m_pTraceOwner( passentity ) {	}

	// For this test, we only test against entities (then world brushes afterwards)
	virtual TraceType_t	GetTraceType() const { return TRACE_ENTITIES_ONLY; }

	bool HasContentsGrate( CBaseEntity *pEntity )
	{
		// FIXME: Move this into the GetModelContents() function in base entity

		// Find the contents based on the model type
		int nModelType = modelinfo->GetModelType( pEntity->GetModel() );
		if ( nModelType == mod_studio )
		{
			CBaseAnimating *pAnim = dynamic_cast<CBaseAnimating *>(pEntity);
			if ( pAnim != NULL )
			{
				CStudioHdr *pStudioHdr = pAnim->GetModelPtr();
				if ( pStudioHdr != NULL && (pStudioHdr->contents() & CONTENTS_GRATE) )
					return true;
			}
		}
		else if ( nModelType == mod_brush )
		{
			// Brushes poll their contents differently
			int contents = modelinfo->GetModelContents( pEntity->GetModelIndex() );
			if ( contents & CONTENTS_GRATE )
				return true;
		}

		return false;
	}

	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		// Only skip ourselves (not things we own)
		if ( pHandleEntity == m_pTraceOwner )
			return false;

		// Get the entity referenced by this handle
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
		if ( pEntity == NULL )
			return false;

		// Handle grate entities differently
		if ( HasContentsGrate( pEntity ) )
		{
#ifndef CLIENT_DLL
#ifdef MAPBASE
			if (pEntity->CanBePickedUpByPhyscannon())
				return true;
#else
			// See if it's a grabbable physics prop
			CPhysicsProp *pPhysProp = dynamic_cast<CPhysicsProp *>(pEntity);
			if ( pPhysProp != NULL )
				return pPhysProp->CanBePickedUpByPhyscannon();

			// See if it's a grabbable physics prop
			if ( FClassnameIs( pEntity, "prop_physics" ) )
			{
				CPhysicsProp *pPhysProp = dynamic_cast<CPhysicsProp *>(pEntity);
				if ( pPhysProp != NULL )
					return pPhysProp->CanBePickedUpByPhyscannon();

				// Somehow had a classname that didn't match the class!
				Assert(0);
			}
			else if ( FClassnameIs( pEntity, "func_physbox" ) )
			{
				// Must be a moveable physbox
				CPhysBox *pPhysBox = dynamic_cast<CPhysBox *>(pEntity);
				if ( pPhysBox )
					return pPhysBox->CanBePickedUpByPhyscannon();

				// Somehow had a classname that didn't match the class!
				Assert(0);
			}
#endif
#endif

			// Don't bother with any other sort of grated entity
			return false;
		}

		// Use the default rules
		return BaseClass::ShouldHitEntity( pHandleEntity, contentsMask );
	}

protected:
	const IHandleEntity *m_pTraceOwner;
};

// We want to test against brushes alone
class CTraceFilterOnlyBrushes : public CTraceFilterSimple
{
public:
	DECLARE_CLASS( CTraceFilterOnlyBrushes, CTraceFilterSimple );
	CTraceFilterOnlyBrushes( int collisionGroup ) : CTraceFilterSimple( NULL, collisionGroup ) {}
	virtual TraceType_t	GetTraceType() const { return TRACE_WORLD_ONLY; }
};

//-----------------------------------------------------------------------------
// this will hit skip the pass entity, but not anything it owns 
// (lets player grab own grenades)
class CTraceFilterNoOwnerTest : public CTraceFilterSimple
{
public:
	DECLARE_CLASS( CTraceFilterNoOwnerTest, CTraceFilterSimple );

	CTraceFilterNoOwnerTest( const IHandleEntity *passentity, int collisionGroup )
		: CTraceFilterSimple( NULL, collisionGroup ), m_pPassNotOwner(passentity)
	{
	}

	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		if ( pHandleEntity != m_pPassNotOwner )
			return BaseClass::ShouldHitEntity( pHandleEntity, contentsMask );

		return false;
	}

protected:
	const IHandleEntity *m_pPassNotOwner;
};



//-----------------------------------------------------------------------------
// Purpose: Trace a line the special physcannon way!
//-----------------------------------------------------------------------------
void UTIL_PhyscannonTraceLine( const Vector &vecAbsStart, const Vector &vecAbsEnd, CBaseEntity *pTraceOwner, trace_t *pTrace )
{
	// Default to HL2 vanilla
	if ( hl2_episodic.GetBool() == false )
	{
		CTraceFilterNoOwnerTest filter( pTraceOwner, COLLISION_GROUP_NONE );
		UTIL_TraceLine( vecAbsStart, vecAbsEnd, (MASK_SHOT|CONTENTS_GRATE), &filter, pTrace );
		return;
	}

	// First, trace against entities
	CTraceFilterPhyscannon filter( pTraceOwner, COLLISION_GROUP_NONE );
	UTIL_TraceLine( vecAbsStart, vecAbsEnd, (MASK_SHOT|CONTENTS_GRATE), &filter, pTrace );

	// If we've hit something, test again to make sure no brushes block us
	if ( pTrace->m_pEnt != NULL )
	{
		trace_t testTrace;
		CTraceFilterOnlyBrushes brushFilter( COLLISION_GROUP_NONE );
		UTIL_TraceLine( pTrace->startpos, pTrace->endpos, MASK_SHOT, &brushFilter, &testTrace );

		// If we hit a brush, replace the trace with that result
		if ( testTrace.fraction < 1.0f || testTrace.startsolid || testTrace.allsolid )
		{
			*pTrace = testTrace;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Trace a hull for the physcannon
//-----------------------------------------------------------------------------
void UTIL_PhyscannonTraceHull( const Vector &vecAbsStart, const Vector &vecAbsEnd, const Vector &vecAbsMins, const Vector &vecAbsMaxs, CBaseEntity *pTraceOwner, trace_t *pTrace )
{
	// Default to HL2 vanilla
	if ( hl2_episodic.GetBool() == false )
	{
		CTraceFilterNoOwnerTest filter( pTraceOwner, COLLISION_GROUP_NONE );
		UTIL_TraceHull( vecAbsStart, vecAbsEnd, vecAbsMins, vecAbsMaxs, (MASK_SHOT|CONTENTS_GRATE), &filter, pTrace );
		return;
	}

	// First, trace against entities
	CTraceFilterPhyscannon filter( pTraceOwner, COLLISION_GROUP_NONE );
	UTIL_TraceHull( vecAbsStart, vecAbsEnd, vecAbsMins, vecAbsMaxs, (MASK_SHOT|CONTENTS_GRATE), &filter, pTrace );

	// If we've hit something, test again to make sure no brushes block us
	if ( pTrace->m_pEnt != NULL )
	{
		trace_t testTrace;
		CTraceFilterOnlyBrushes brushFilter( COLLISION_GROUP_NONE );
		UTIL_TraceHull( pTrace->startpos, pTrace->endpos, vecAbsMins, vecAbsMaxs, MASK_SHOT, &brushFilter, &testTrace );

		// If we hit a brush, replace the trace with that result
		if ( testTrace.fraction < 1.0f || testTrace.startsolid || testTrace.allsolid )
		{
			*pTrace = testTrace;
		}
	}
}

static void MatrixOrthogonalize( matrix3x4_t &matrix, int column )
{
	Vector columns[3];
	int i;

	for ( i = 0; i < 3; i++ )
	{
		MatrixGetColumn( matrix, i, columns[i] );
	}

	int index0 = column;
	int index1 = (column+1)%3;
	int index2 = (column+2)%3;

	columns[index2] = CrossProduct( columns[index0], columns[index1] );
	columns[index1] = CrossProduct( columns[index2], columns[index0] );
	VectorNormalize( columns[index2] );
	VectorNormalize( columns[index1] );
	MatrixSetColumn( columns[index1], index1, matrix );
	MatrixSetColumn( columns[index2], index2, matrix );
}

#define SIGN(x) ( (x) < 0 ? -1 : 1 )

static QAngle AlignAngles( const QAngle &angles, float cosineAlignAngle )
{
	matrix3x4_t alignMatrix;
	AngleMatrix( angles, alignMatrix );

	// NOTE: Must align z first
	for ( int j = 3; --j >= 0; )
	{
		Vector vec;
		MatrixGetColumn( alignMatrix, j, vec );
		for ( int i = 0; i < 3; i++ )
		{
			if ( fabs(vec[i]) > cosineAlignAngle )
			{
				vec[i] = SIGN(vec[i]);
				vec[(i+1)%3] = 0;
				vec[(i+2)%3] = 0;
				MatrixSetColumn( vec, j, alignMatrix );
				MatrixOrthogonalize( alignMatrix, j );
				break;
			}
		}
	}

	QAngle out;
	MatrixAngles( alignMatrix, out );
	return out;
}


static void TraceCollideAgainstBBox( const CPhysCollide *pCollide, const Vector &start, const Vector &end, const QAngle &angles, const Vector &boxOrigin, const Vector &mins, const Vector &maxs, trace_t *ptr )
{
	physcollision->TraceBox( boxOrigin, boxOrigin + (start-end), mins, maxs, pCollide, start, angles, ptr );

	if ( ptr->DidHit() )
	{
		ptr->endpos = start * (1-ptr->fraction) + end * ptr->fraction;
		ptr->startpos = start;
		ptr->plane.dist = -ptr->plane.dist;
		ptr->plane.normal *= -1;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Finds the nearest ragdoll sub-piece to a location and returns it
// Input  : *pTarget - entity that is the potential ragdoll
//			&position - position we're testing against
// Output : IPhysicsObject - sub-object (if any)
//-----------------------------------------------------------------------------
IPhysicsObject *GetRagdollChildAtPosition( CBaseEntity *pTarget, const Vector &position )
{
#ifdef CLIENT_DLL
	return NULL;
#else
	// Check for a ragdoll
	if ( dynamic_cast<CRagdollProp*>( pTarget ) == NULL )
		return NULL;

	// Get the root
	IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	int count = pTarget->VPhysicsGetObjectList( pList, ARRAYSIZE( pList ) );
	
	IPhysicsObject *pBestChild = NULL;
	float			flBestDist = 99999999.0f;
	float			flDist;
	Vector			vPos;

	// Find the nearest child to where we're looking
	for ( int i = 0; i < count; i++ )
	{
		pList[i]->GetPosition( &vPos, NULL );
		
		flDist = ( position - vPos ).LengthSqr();

		if ( flDist < flBestDist )
		{
			pBestChild = pList[i];
			flBestDist = flDist;
		}
	}

	// Make this our base now
	pTarget->VPhysicsSwapObject( pBestChild );

	return pTarget->VPhysicsGetObject();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Computes a local matrix for the player clamped to valid carry ranges
//-----------------------------------------------------------------------------
// when looking level, hold bottom of object 8 inches below eye level
#define PLAYER_HOLD_LEVEL_EYES	-8

// when looking down, hold bottom of object 0 inches from feet
#define PLAYER_HOLD_DOWN_FEET	2

// when looking up, hold bottom of object 24 inches above eye level
#define PLAYER_HOLD_UP_EYES		24

// use a +/-30 degree range for the entire range of motion of pitch
#define PLAYER_LOOK_PITCH_RANGE	30

// player can reach down 2ft below his feet (otherwise he'll hold the object above the bottom)
#define PLAYER_REACH_DOWN_DISTANCE	24

static void ComputePlayerMatrix( CBasePlayer *pPlayer, matrix3x4_t &out )
{
	if ( !pPlayer )
		return;

	QAngle angles = pPlayer->EyeAngles();
	Vector origin = pPlayer->EyePosition();
	
	// 0-360 / -180-180
	//angles.x = init ? 0 : AngleDistance( angles.x, 0 );
	//angles.x = clamp( angles.x, -PLAYER_LOOK_PITCH_RANGE, PLAYER_LOOK_PITCH_RANGE );
	angles.x = 0;

	float feet = pPlayer->GetAbsOrigin().z + pPlayer->WorldAlignMins().z;
	float eyes = origin.z;
	float zoffset = 0;
	// moving up (negative pitch is up)
	if ( angles.x < 0 )
	{
		zoffset = RemapVal( angles.x, 0, -PLAYER_LOOK_PITCH_RANGE, PLAYER_HOLD_LEVEL_EYES, PLAYER_HOLD_UP_EYES );
	}
	else
	{
		zoffset = RemapVal( angles.x, 0, PLAYER_LOOK_PITCH_RANGE, PLAYER_HOLD_LEVEL_EYES, PLAYER_HOLD_DOWN_FEET + (feet - eyes) );
	}
	origin.z += zoffset;
	angles.x = 0;
	AngleMatrix( angles, origin, out );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

BEGIN_SIMPLE_DATADESC( game_shadowcontrol_params_t )
	
	DEFINE_FIELD( targetPosition,		FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( targetRotation,		FIELD_VECTOR ),
	DEFINE_FIELD( maxAngular, FIELD_FLOAT ),
	DEFINE_FIELD( maxDampAngular, FIELD_FLOAT ),
	DEFINE_FIELD( maxSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( maxDampSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( dampFactor, FIELD_FLOAT ),
	DEFINE_FIELD( teleportDistance,	FIELD_FLOAT ),

END_DATADESC()

const float DEFAULT_MAX_ANGULAR = 360.0f * 10.0f;
const float REDUCED_CARRY_MASS = 1.0f;

CGrabController::CGrabController( void )
{
	m_shadow.dampFactor = 1.0;
	m_shadow.teleportDistance = 0;
	m_errorTime = 0;
	m_error = 0;
	// make this controller really stiff!
	m_shadow.maxSpeed = 1000;
	m_shadow.maxAngular = DEFAULT_MAX_ANGULAR;
	m_shadow.maxDampSpeed = m_shadow.maxSpeed*2;
	m_shadow.maxDampAngular = m_shadow.maxAngular;
	m_attachedEntity = NULL;
	m_vecPreferredCarryAngles = vec3_angle;
	m_bHasPreferredCarryAngles = false;
	m_flDistanceOffset = 0;
#ifdef MAPBASE
	m_bDontUseListMass = false;
#endif
}

CGrabController::~CGrabController( void )
{
	DetachEntity( false );
}

void CGrabController::OnRestore()
{
	if ( m_controller )
	{
		m_controller->SetEventHandler( this );
	}
}

void CGrabController::SetTargetPosition( const Vector &target, const QAngle &targetOrientation )
{
	m_shadow.targetPosition = target;
	m_shadow.targetRotation = targetOrientation;

	m_timeToArrive = gpGlobals->frametime;

	CBaseEntity *pAttached = GetAttached();
	if ( pAttached )
	{
		IPhysicsObject *pObj = pAttached->VPhysicsGetObject();
		
		if ( pObj != NULL )
		{
			pObj->Wake();
		}
		else
		{
			DetachEntity( false );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CGrabController::ComputeError()
{
	if ( m_errorTime <= 0 )
		return 0;

	CBaseEntity *pAttached = GetAttached();
	if ( pAttached )
	{
		Vector pos;
		IPhysicsObject *pObj = pAttached->VPhysicsGetObject();
		
		if ( pObj )
		{	
			pObj->GetShadowPosition( &pos, NULL );

			float error = (m_shadow.targetPosition - pos).Length();
			if ( m_errorTime > 0 )
			{
				if ( m_errorTime > 1 )
				{
					m_errorTime = 1;
				}
				float speed = error / m_errorTime;
				if ( speed > m_shadow.maxSpeed )
				{
					error *= 0.5;
				}
				m_error = (1-m_errorTime) * m_error + error * m_errorTime;
			}
		}
		else
		{
			DevMsg( "Object attached to Physcannon has no physics object\n" );
			DetachEntity( false );
			return 9999; // force detach
		}
	}
	
	if ( pAttached->IsEFlagSet( EFL_IS_BEING_LIFTED_BY_BARNACLE ) )
	{
		m_error *= 3.0f;
	}

	m_errorTime = 0;

	return m_error;
}


#define MASS_SPEED_SCALE	60
#define MAX_MASS			40

void CGrabController::ComputeMaxSpeed( CBaseEntity *pEntity, IPhysicsObject *pPhysics )
{
#ifndef CLIENT_DLL
	m_shadow.maxSpeed = 1000;
	m_shadow.maxAngular = DEFAULT_MAX_ANGULAR;

	// Compute total mass...
	float flMass = PhysGetEntityMass( pEntity );
	float flMaxMass = physcannon_maxmass.GetFloat();
	if ( flMass <= flMaxMass )
		return;

	float flLerpFactor = clamp( flMass, flMaxMass, 500.0f );
	flLerpFactor = SimpleSplineRemapVal( flLerpFactor, flMaxMass, 500.0f, 0.0f, 1.0f );

	float invMass = pPhysics->GetInvMass();
	float invInertia = pPhysics->GetInvInertia().Length();

	float invMaxMass = 1.0f / MAX_MASS;
	float ratio = invMaxMass / invMass;
	invMass = invMaxMass;
	invInertia *= ratio;

	float maxSpeed = invMass * MASS_SPEED_SCALE * 200;
	float maxAngular = invInertia * MASS_SPEED_SCALE * 360;

	m_shadow.maxSpeed = Lerp( flLerpFactor, m_shadow.maxSpeed, maxSpeed );
	m_shadow.maxAngular = Lerp( flLerpFactor, m_shadow.maxAngular, maxAngular );
#endif
}


QAngle CGrabController::TransformAnglesToPlayerSpace( const QAngle &anglesIn, CBasePlayer *pPlayer )
{
	if ( m_bIgnoreRelativePitch )
	{
		matrix3x4_t test;
		QAngle angleTest = pPlayer->EyeAngles();
		angleTest.x = 0;
		AngleMatrix( angleTest, test );
		return TransformAnglesToLocalSpace( anglesIn, test );
	}
	return TransformAnglesToLocalSpace( anglesIn, pPlayer->EntityToWorldTransform() );
}

QAngle CGrabController::TransformAnglesFromPlayerSpace( const QAngle &anglesIn, CBasePlayer *pPlayer )
{
	if ( m_bIgnoreRelativePitch )
	{
		matrix3x4_t test;
		QAngle angleTest = pPlayer->EyeAngles();
		angleTest.x = 0;
		AngleMatrix( angleTest, test );
		return TransformAnglesToWorldSpace( anglesIn, test );
	}
	return TransformAnglesToWorldSpace( anglesIn, pPlayer->EntityToWorldTransform() );
}


void CGrabController::AttachEntity( CBasePlayer *pPlayer, CBaseEntity *pEntity, IPhysicsObject *pPhys, bool bIsMegaPhysCannon, const Vector &vGrabPosition, bool bUseGrabPosition )
{
	// play the impact sound of the object hitting the player
	// used as feedback to let the player know he picked up the object
#ifndef CLIENT_DLL
	{
		// misyl: Disable pred filtering in this server-only section.
		CDisablePredictionFiltering disablePred;
		PhysicsImpactSound( pPlayer, pPhys, CHAN_STATIC, pPhys->GetMaterialIndex(), pPlayer->VPhysicsGetObject()->GetMaterialIndex(), 1.0, 64 );
	}
#endif
	Vector position;
	QAngle angles;
	pPhys->GetPosition( &position, &angles );
	// If it has a preferred orientation, use that instead.
#ifndef CLIENT_DLL
	Pickup_GetPreferredCarryAngles( pEntity, pPlayer, pPlayer->EntityToWorldTransform(), angles );
#endif

//	ComputeMaxSpeed( pEntity, pPhys );

	// If we haven't been killed by a grab, we allow the gun to grab the nearest part of a ragdoll
	if ( bUseGrabPosition )
	{
		IPhysicsObject *pChild = GetRagdollChildAtPosition( pEntity, vGrabPosition );
		
		if ( pChild )
		{
			pPhys = pChild;
		}
	}

	// Carried entities can never block LOS
	m_bCarriedEntityBlocksLOS = pEntity->BlocksLOS();
	pEntity->SetBlocksLOS( false );
	m_controller = physenv->CreateMotionController( this );
	m_controller->AttachObject( pPhys, true );
	// Don't do this, it's causing trouble with constraint solvers.
	//m_controller->SetPriority( IPhysicsMotionController::HIGH_PRIORITY );

	pPhys->Wake();
	PhysSetGameFlags( pPhys, FVPHYSICS_PLAYER_HELD );
	SetTargetPosition( position, angles );
	m_attachedEntity = pEntity;
	IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	int count = pEntity->VPhysicsGetObjectList( pList, ARRAYSIZE(pList) );
	m_flLoadWeight = 0;
	float damping = 10;
	float flFactor = count / 7.5f;
	if ( flFactor < 1.0f )
	{
		flFactor = 1.0f;
	}
	for ( int i = 0; i < count; i++ )
	{
		float mass = pList[i]->GetMass();
		pList[i]->GetDamping( NULL, &m_savedRotDamping[i] );
		m_savedMass[i] = mass;

#ifdef MAPBASE
		if (!m_bDontUseListMass || pList[i] == pPhys)
#endif
		{
			m_flLoadWeight += mass;

			// reduce the mass to prevent the player from adding crazy amounts of energy to the system
			pList[i]->SetMass( REDUCED_CARRY_MASS / flFactor );
			pList[i]->SetDamping( NULL, &damping );
		}
	}
	
	// Give extra mass to the phys object we're actually picking up
	pPhys->SetMass( REDUCED_CARRY_MASS );
	pPhys->EnableDrag( false );

	m_errorTime = bIsMegaPhysCannon ? -1.5f : -1.0f; // 1 seconds until error starts accumulating
	m_error = 0;
	m_contactAmount = 0;

	m_attachedAnglesPlayerSpace = TransformAnglesToPlayerSpace( angles, pPlayer );
	if ( m_angleAlignment != 0 )
	{
		m_attachedAnglesPlayerSpace = AlignAngles( m_attachedAnglesPlayerSpace, m_angleAlignment );
	}

#ifndef CLIENT_DLL
	// Ragdolls don't offset this way
	if ( dynamic_cast<CRagdollProp*>(pEntity) )
	{
		m_attachedPositionObjectSpace.Init();
	}
	else
#endif
	{
		VectorITransform( pEntity->WorldSpaceCenter(), pEntity->EntityToWorldTransform(), m_attachedPositionObjectSpace );
	}

#ifndef CLIENT_DLL
	// If it's a prop, see if it has desired carry angles
	CPhysicsProp *pProp = dynamic_cast<CPhysicsProp *>(pEntity);
	if ( pProp )
	{
#ifdef MAPBASE
		// If the prop has custom carry angles, don't override them
		// (regular PreferredCarryAngles() code should cover it)
		if (!pProp->m_bUsesCustomCarryAngles)
			m_bHasPreferredCarryAngles = pProp->GetPropDataAngles( "preferred_carryangles", m_vecPreferredCarryAngles );
#else
		m_bHasPreferredCarryAngles = pProp->GetPropDataAngles( "preferred_carryangles", m_vecPreferredCarryAngles );
#endif
		m_flDistanceOffset = pProp->GetCarryDistanceOffset();
	}
	else
	{
		m_bHasPreferredCarryAngles = false;
	}
#else

	m_bHasPreferredCarryAngles = false;
#endif

}

static void ClampPhysicsVelocity( IPhysicsObject *pPhys, float linearLimit, float angularLimit )
{
	Vector vel;
	AngularImpulse angVel;
	pPhys->GetVelocity( &vel, &angVel );
	float speed = VectorNormalize(vel) - linearLimit;
	float angSpeed = VectorNormalize(angVel) - angularLimit;
	speed = speed < 0 ? 0 : -speed;
	angSpeed = angSpeed < 0 ? 0 : -angSpeed;
	vel *= speed;
	angVel *= angSpeed;
	pPhys->AddVelocity( &vel, &angVel );
}

void CGrabController::DetachEntity( bool bClearVelocity )
{
	CBaseEntity *pEntity = GetAttached();
	if ( pEntity )
	{
		// Restore the LS blocking state
		pEntity->SetBlocksLOS( m_bCarriedEntityBlocksLOS );
		IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
		int count = pEntity->VPhysicsGetObjectList( pList, ARRAYSIZE(pList) );

		for ( int i = 0; i < count; i++ )
		{
			IPhysicsObject *pPhys = pList[i];
			if ( !pPhys )
				continue;

			// on the odd chance that it's gone to sleep while under anti-gravity
			pPhys->EnableDrag( true );
			pPhys->Wake();
			pPhys->SetMass( m_savedMass[i] );
			pPhys->SetDamping( NULL, &m_savedRotDamping[i] );
			PhysClearGameFlags( pPhys, FVPHYSICS_PLAYER_HELD );
			if ( bClearVelocity )
			{
				PhysForceClearVelocity( pPhys );
			}
			else
			{
#ifndef CLIENT_DLL
				ClampPhysicsVelocity( pPhys, hl2_normspeed.GetFloat() * 1.5f, 2.0f * 360.0f );
#endif
			}

		}
	}

	m_attachedEntity = NULL;
	if ( physenv )
	{
		physenv->DestroyMotionController( m_controller );
	}
	m_controller = NULL;
}

static bool InContactWithHeavyObject( IPhysicsObject *pObject, float heavyMass )
{
	bool contact = false;
	IPhysicsFrictionSnapshot *pSnapshot = pObject->CreateFrictionSnapshot();
	while ( pSnapshot->IsValid() )
	{
		IPhysicsObject *pOther = pSnapshot->GetObject( 1 );
		if ( !pOther->IsMoveable() || pOther->GetMass() > heavyMass )
		{
			contact = true;
			break;
		}
		pSnapshot->NextFrictionData();
	}
	pObject->DestroyFrictionSnapshot( pSnapshot );
	return contact;
}

IMotionEvent::simresult_e CGrabController::Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular )
{
	game_shadowcontrol_params_t shadowParams = m_shadow;
	if ( InContactWithHeavyObject( pObject, GetLoadWeight() ) )
	{
		m_contactAmount = Approach( 0.1f, m_contactAmount, deltaTime*2.0f );
	}
	else
	{
		m_contactAmount = Approach( 1.0f, m_contactAmount, deltaTime*2.0f );
	}
	shadowParams.maxAngular = m_shadow.maxAngular * m_contactAmount * m_contactAmount * m_contactAmount;
#ifndef CLIENT_DLL
	m_timeToArrive = pObject->ComputeShadowControl( shadowParams, m_timeToArrive, deltaTime );
#else
	m_timeToArrive = pObject->ComputeShadowControl( shadowParams, (TICK_INTERVAL*2), deltaTime );
#endif
	
	// Slide along the current contact points to fix bouncing problems
	Vector velocity;
	AngularImpulse angVel;
	pObject->GetVelocity( &velocity, &angVel );
	PhysComputeSlideDirection( pObject, velocity, angVel, &velocity, &angVel, GetLoadWeight() );
	pObject->SetVelocityInstantaneous( &velocity, NULL );

	linear.Init();
	angular.Init();
	m_errorTime += deltaTime;

	return SIM_LOCAL_ACCELERATION;
}

float CGrabController::GetSavedMass( IPhysicsObject *pObject )
{
	CBaseEntity *pHeld = m_attachedEntity;
	if ( pHeld )
	{
		if ( pObject->GetGameData() == (void*)pHeld )
		{
			IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
			int count = pHeld->VPhysicsGetObjectList( pList, ARRAYSIZE(pList) );
			for ( int i = 0; i < count; i++ )
			{
				if ( pList[i] == pObject )
					return m_savedMass[i];
			}
		}
	}
	return 0.0f;
}

//-----------------------------------------------------------------------------
// Player pickup controller
//-----------------------------------------------------------------------------

#ifdef CLIENT_DLL
#define CPlayerPickupController C_PlayerPickupController
#endif

class CPlayerPickupController : public CBaseEntity
{
	DECLARE_CLASS( CPlayerPickupController, CBaseEntity );
#ifdef MAPBASE_MP
	DECLARE_NETWORKCLASS();
#endif
public:
#ifndef CLIENT_DLL
	void Init( CBasePlayer *pPlayer, CBaseEntity *pObject );
#endif
	void Shutdown( bool bThrown = false );
	bool OnControls( CBaseEntity *pControls ) { return true; }
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void OnRestore()
	{
		m_grabController.OnRestore();
	}
	void VPhysicsUpdate( IPhysicsObject *pPhysics ){}
	void VPhysicsShadowUpdate( IPhysicsObject *pPhysics ) {}

#if defined(MAPBASE_MP) && defined(CLIENT_DLL)
	void OnDataChanged( DataUpdateType_t type );
	void ManagePredictedObject( void );
#endif

	bool IsHoldingEntity( CBaseEntity *pEnt );
	CGrabController &GetGrabController() { return m_grabController; }

private:
	CGrabController		m_grabController;

#ifdef MAPBASE_MP
	CNetworkHandle( CBaseEntity, m_hAttachedObject );
	CNetworkHandle( CBasePlayer, m_pPlayer );

#ifndef CLIENT_DLL
	CNetworkQAngle	( m_attachedAnglesPlayerSpace );
#else
	QAngle m_attachedAnglesPlayerSpace;

	EHANDLE m_hOldAttachedObject;
#endif

	CNetworkVector	( m_attachedPositionObjectSpace );
#else
	CBasePlayer			*m_pPlayer;
#endif
};

LINK_ENTITY_TO_CLASS( player_pickup, CPlayerPickupController );

#ifdef MAPBASE_MP
IMPLEMENT_NETWORKCLASS_ALIASED( PlayerPickupController, DT_PlayerPickupController )

BEGIN_NETWORK_TABLE( CPlayerPickupController, DT_PlayerPickupController )
#ifdef CLIENT_DLL
	RecvPropEHandle( RECVINFO( m_hAttachedObject ) ),
	RecvPropEHandle( RECVINFO( m_pPlayer ) ),
	RecvPropVector( RECVINFO( m_attachedPositionObjectSpace ) ),
	RecvPropFloat( RECVINFO( m_attachedAnglesPlayerSpace[0] ) ),
	RecvPropFloat( RECVINFO( m_attachedAnglesPlayerSpace[1] ) ),
	RecvPropFloat( RECVINFO( m_attachedAnglesPlayerSpace[2] ) ),
#else
	SendPropEHandle( SENDINFO( m_hAttachedObject ) ),
	SendPropEHandle( SENDINFO( m_pPlayer ) ),
	SendPropVector( SENDINFO( m_attachedPositionObjectSpace ), -1, SPROP_COORD ),
	SendPropAngle( SENDINFO_VECTORELEM( m_attachedAnglesPlayerSpace, 0 ), 11 ),
	SendPropAngle( SENDINFO_VECTORELEM( m_attachedAnglesPlayerSpace, 1 ), 11 ),
	SendPropAngle( SENDINFO_VECTORELEM( m_attachedAnglesPlayerSpace, 2 ), 11 ),
#endif
END_NETWORK_TABLE()
#endif

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//			*pObject - 
//-----------------------------------------------------------------------------
void CPlayerPickupController::Init( CBasePlayer *pPlayer, CBaseEntity *pObject )
{
	// Holster player's weapon
	if ( pPlayer->GetActiveWeapon() )
	{
		if ( !pPlayer->GetActiveWeapon()->Holster() )
		{
			Shutdown();
			return;
		}
	}

	CHL2MP_Player *pOwner = (CHL2MP_Player *)ToBasePlayer( pPlayer );
	if ( pOwner )
	{
#ifndef MAPBASE
		pOwner->EnableSprint( false );
#else
		if ( sv_player_enable_propsprint.GetBool() == false )
		{
			pOwner->EnableSprint( false );
		}
		else
		{
			pOwner->EnableSprint( true );	
		}
#endif
	}

	// If the target is debris, convert it to non-debris
	if ( pObject->GetCollisionGroup() == COLLISION_GROUP_DEBRIS )
	{
		// Interactive debris converts back to debris when it comes to rest
		pObject->SetCollisionGroup( COLLISION_GROUP_INTERACTIVE_DEBRIS );
	}

	// done so I'll go across level transitions with the player
	SetParent( pPlayer );
	m_grabController.SetIgnorePitch( true );
	m_grabController.SetAngleAlignment( DOT_30DEGREE );
	m_pPlayer = pPlayer;
	IPhysicsObject *pPhysics = pObject->VPhysicsGetObject();
	Pickup_OnPhysGunPickup( pObject, m_pPlayer );
	
#ifdef MAPBASE
	bool bUseGrabPos = false;
	Vector vecGrabPos;
	if ( dynamic_cast<CRagdollProp*>( pObject ) )
	{
		m_grabController.SetDontUseListMass( true );

		// Approximate where we're grabbing from
		vecGrabPos = pPlayer->EyePosition() + (pPlayer->EyeDirection3D() * 16.0f);
		bUseGrabPos = true;
	}
	else
		m_grabController.SetDontUseListMass( false );

	m_grabController.AttachEntity( pPlayer, pObject, pPhysics, false, vecGrabPos, bUseGrabPos );
#else
	m_grabController.AttachEntity( pPlayer, pObject, pPhysics, false, vec3_origin, false );
#endif

#ifdef MAPBASE_MP
	// The client needs to know that I'm the +USE entity for prediction
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
	m_hAttachedObject = pObject;

	m_attachedPositionObjectSpace = m_grabController.m_attachedPositionObjectSpace;
	m_attachedAnglesPlayerSpace = m_grabController.m_attachedAnglesPlayerSpace;
#endif
	
	m_pPlayer->m_Local.m_iHideHUD |= HIDEHUD_WEAPONSELECTION;
	m_pPlayer->SetUseEntity( this );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CPlayerPickupController::Shutdown( bool bThrown )
{
#ifndef CLIENT_DLL
	CBaseEntity *pObject = m_grabController.GetAttached();

	bool bClearVelocity = false;
	if ( !bThrown && pObject && pObject->VPhysicsGetObject() && pObject->VPhysicsGetObject()->GetContactPoint(NULL,NULL) )
	{
		bClearVelocity = true;
	}

	m_grabController.DetachEntity( bClearVelocity );
#ifdef MAPBASE_MP
	m_hAttachedObject = NULL;
#endif

	if ( pObject != NULL )
	{
		Pickup_OnPhysGunDrop( pObject, m_pPlayer, bThrown ? THROWN_BY_PLAYER : DROPPED_BY_PLAYER );
	}

	if ( m_pPlayer )
	{
		CHL2MP_Player *pOwner = (CHL2MP_Player *)ToBasePlayer( m_pPlayer );
#ifndef MAPBASE
		if ( pOwner )
		{
			pOwner->EnableSprint( true );
		}
#else
		if ( pOwner && sv_player_enable_propsprint.GetBool() == false )
		{
			pOwner->EnableSprint( true );
		}
#endif

		m_pPlayer->SetUseEntity( NULL );
		if ( m_pPlayer->GetActiveWeapon() )
		{
			if ( !m_pPlayer->GetActiveWeapon()->Deploy() )
			{
				// We tried to restore the player's weapon, but we couldn't.
				// This usually happens when they're holding an empty weapon that doesn't
				// autoswitch away when out of ammo. Switch to next best weapon.
				m_pPlayer->SwitchToNextBestWeapon( NULL );
			}
		}

		m_pPlayer->m_Local.m_iHideHUD &= ~HIDEHUD_WEAPONSELECTION;
	}
	Remove();

#elif defined(MAPBASE_MP)

	m_grabController.DetachEntity( bThrown );

#ifdef MAPBASE_MP
	if ( m_hAttachedObject )
	{
		m_hAttachedObject->VPhysicsDestroyObject();
		m_hAttachedObject = NULL;
	}
#endif

#endif
	
}


void CPlayerPickupController::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( ToBasePlayer(pActivator) == m_pPlayer )
	{
#if defined(CLIENT_DLL) && defined(MAPBASE_MP)
		if (m_pPlayer.Get() == C_BasePlayer::GetLocalPlayer() && !m_pPlayer->IsObserver())
			ManagePredictedObject();
		else
			return;
#endif

		CBaseEntity *pAttached = m_grabController.GetAttached();

		// UNDONE: Use vphysics stress to decide to drop objects
		// UNDONE: Must fix case of forcing objects into the ground you're standing on (causes stress) before that will work
		if ( !pAttached || useType == USE_OFF || (m_pPlayer->m_nButtons & IN_ATTACK2) || m_grabController.ComputeError() > 12 )
		{
			Shutdown();
			return;
		}
		
		//Adrian: Oops, our object became motion disabled, let go!
		IPhysicsObject *pPhys = pAttached->VPhysicsGetObject();
		if ( pPhys && pPhys->IsMoveable() == false )
		{
			Shutdown();
			return;
		}

#if STRESS_TEST
		vphysics_objectstress_t stress;
		CalculateObjectStress( pPhys, pAttached, &stress );
		if ( stress.exertedStress > 250 )
		{
			Shutdown();
			return;
		}
#endif
		// +ATTACK will throw phys objects
		if ( m_pPlayer->m_nButtons & IN_ATTACK )
		{
			Shutdown( true );
#ifndef CLIENT_DLL
			Vector vecLaunch;
			m_pPlayer->EyeVectors( &vecLaunch );
			// JAY: Scale this with mass because some small objects really go flying
#ifdef MAPBASE
			float massFactor = pPhys ? clamp( pPhys->GetMass(), 0.5, 15 ) : 7.5;
#else
			float massFactor = clamp( pPhys->GetMass(), 0.5, 15 );
#endif
			massFactor = RemapVal( massFactor, 0.5, 15, 0.5, 4 );
			vecLaunch *= player_throwforce.GetFloat() * massFactor;

			pPhys->ApplyForceCenter( vecLaunch );
			AngularImpulse aVel = RandomAngularImpulse( -10, 10 ) * massFactor;
			pPhys->ApplyTorqueCenter( aVel );
#endif
			return;
		}

		if ( useType == USE_SET )
		{
			// update position
			m_grabController.UpdateObject( m_pPlayer, 12 );
		}
	}
}

#if defined(MAPBASE_MP) && defined(CLIENT_DLL)
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerPickupController::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( m_pPlayer == NULL )
	{
		if ( m_hAttachedObject )
		{
			m_hAttachedObject->VPhysicsDestroyObject();
		}

		if ( m_hOldAttachedObject )
		{
			m_hOldAttachedObject->VPhysicsDestroyObject();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerPickupController::ManagePredictedObject( void )
{
	CBaseEntity *pAttachedObject = m_hAttachedObject.Get();

	if ( m_hAttachedObject )
	{
		// NOTE :This must happen after OnPhysGunPickup because that can change the mass
		if ( pAttachedObject != GetGrabController().GetAttached() )
		{
			IPhysicsObject *pPhysics = pAttachedObject->VPhysicsGetObject();

			if ( pPhysics == NULL )
			{
				solid_t tmpSolid;
				PhysModelParseSolid( tmpSolid, m_hAttachedObject, pAttachedObject->GetModelIndex() );

				pAttachedObject->VPhysicsInitNormal( SOLID_VPHYSICS, 0, false, &tmpSolid );
			}

			pPhysics = pAttachedObject->VPhysicsGetObject();

			if ( pPhysics )
			{
				m_grabController.SetIgnorePitch( true );
				m_grabController.SetAngleAlignment( 0.866025403784f ); // DOT_30DEGREE from player.h (is there really no client equivalent?)

				GetGrabController().AttachEntity( m_pPlayer, pAttachedObject, pPhysics, false, vec3_origin, false );
				GetGrabController().m_attachedPositionObjectSpace = m_attachedPositionObjectSpace;
				GetGrabController().m_attachedAnglesPlayerSpace = m_attachedAnglesPlayerSpace;
			}
		}
	}
	else
	{
		if ( m_hOldAttachedObject && m_hOldAttachedObject->VPhysicsGetObject() )
		{
			GetGrabController().DetachEntity( false );

			m_hOldAttachedObject->VPhysicsDestroyObject();
		}
	}

	m_hOldAttachedObject = m_hAttachedObject;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEnt - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPlayerPickupController::IsHoldingEntity( CBaseEntity *pEnt )
{
	return ( m_grabController.GetAttached() == pEnt );
}

void PlayerPickupObject( CBasePlayer *pPlayer, CBaseEntity *pObject )
{
	
#ifndef CLIENT_DLL
	
	//Don't pick up if we don't have a phys object.
	if ( pObject->VPhysicsGetObject() == NULL )
		 return;

	CPlayerPickupController *pController = (CPlayerPickupController *)CBaseEntity::Create( "player_pickup", pObject->GetAbsOrigin(), vec3_angle, pPlayer );
	
	if ( !pController )
		return;

	pController->Init( pPlayer, pObject );

#endif

}

//----------------------------------------------------------------------------------------------------------------------------------------------------------
//  CInterpolatedValue class
//---------------------------------------------------------------------------------------------------------------------------------------------------------

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponPhysCannon, DT_WeaponPhysCannon )

BEGIN_NETWORK_TABLE( CWeaponPhysCannon, DT_WeaponPhysCannon )
#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bActive ) ),
	RecvPropEHandle( RECVINFO( m_hAttachedObject ) ),
	RecvPropVector( RECVINFO( m_attachedPositionObjectSpace ) ),
	RecvPropFloat( RECVINFO( m_attachedAnglesPlayerSpace[0] ) ),
	RecvPropFloat( RECVINFO( m_attachedAnglesPlayerSpace[1] ) ),
	RecvPropFloat( RECVINFO( m_attachedAnglesPlayerSpace[2] ) ),
	RecvPropInt( RECVINFO( m_EffectState ) ),
	RecvPropBool( RECVINFO( m_bOpen ) ),
	RecvPropBool( RECVINFO( m_bIsCurrentlyUpgrading ) ),
#ifdef MAPBASE_MP
	RecvPropBool( RECVINFO( m_bDontPredictAttached ) ),
#endif
#else
	SendPropBool( SENDINFO( m_bActive ) ),
	SendPropEHandle( SENDINFO( m_hAttachedObject ) ),
	SendPropVector(SENDINFO( m_attachedPositionObjectSpace ), -1, SPROP_COORD),
	SendPropAngle( SENDINFO_VECTORELEM(m_attachedAnglesPlayerSpace, 0 ), 11 ),
	SendPropAngle( SENDINFO_VECTORELEM(m_attachedAnglesPlayerSpace, 1 ), 11 ),
	SendPropAngle( SENDINFO_VECTORELEM(m_attachedAnglesPlayerSpace, 2 ), 11 ),
	SendPropInt( SENDINFO( m_EffectState ) ),
	SendPropBool( SENDINFO( m_bOpen ) ),
	SendPropBool( SENDINFO( m_bIsCurrentlyUpgrading ) ),
#ifdef MAPBASE_MP
	SendPropBool( SENDINFO( m_bDontPredictAttached ) ),
#endif
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponPhysCannon )
	DEFINE_PRED_FIELD( m_EffectState,	FIELD_INTEGER,	FTYPEDESC_INSENDTABLE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_bOpen,			FIELD_BOOLEAN,	FTYPEDESC_INSENDTABLE | FTYPEDESC_NOERRORCHECK ),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( weapon_physcannon, CWeaponPhysCannon );
PRECACHE_WEAPON_REGISTER( weapon_physcannon );

#ifdef MAPBASE
acttable_t CWeaponPhysCannon::m_acttable[] =
{
	{ ACT_HL2MP_IDLE,                    ACT_HL2MP_IDLE_PHYSGUN,                    false },
	{ ACT_HL2MP_RUN,                    ACT_HL2MP_RUN_PHYSGUN,                    false },
	{ ACT_HL2MP_IDLE_CROUCH,            ACT_HL2MP_IDLE_CROUCH_PHYSGUN,            false },
	{ ACT_HL2MP_WALK_CROUCH,            ACT_HL2MP_WALK_CROUCH_PHYSGUN,            false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,    ACT_HL2MP_GESTURE_RANGE_ATTACK_PHYSGUN,    false },
	{ ACT_HL2MP_GESTURE_RELOAD,            ACT_HL2MP_GESTURE_RELOAD_PHYSGUN,        false },
	{ ACT_HL2MP_JUMP,                    ACT_HL2MP_JUMP_PHYSGUN,                    false },
	{ ACT_RANGE_ATTACK1,                ACT_RANGE_ATTACK_SLAM,                false },
#if EXPANDED_HL2DM_ACTIVITIES
	{ ACT_HL2MP_WALK,					ACT_HL2MP_WALK_PHYSGUN,					false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK2,	ACT_HL2MP_GESTURE_RANGE_ATTACK2_PHYSGUN,    false },
#endif

	{ ACT_ARM,						ACT_ARM_RIFLE,					false },
	{ ACT_DISARM,					ACT_DISARM_RIFLE,				false },
};

IMPLEMENT_ACTTABLE( CWeaponPhysCannon );
#endif


enum
{
	ELEMENT_STATE_NONE = -1,
	ELEMENT_STATE_OPEN,
	ELEMENT_STATE_CLOSED,
};

enum
{
	EFFECT_NONE,
	EFFECT_CLOSED,
	EFFECT_READY,
	EFFECT_HOLDING,
	EFFECT_LAUNCH,
};


//-----------------------------------------------------------------------------
// Do we have the super-phys gun?
//-----------------------------------------------------------------------------
bool PlayerHasMegaPhysCannon()
{
#ifdef CLIENT_DLL
	return false;
#else
	return ( HL2GameRules()->MegaPhyscannonActive() == true );
#endif
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CWeaponPhysCannon::CWeaponPhysCannon( void )
{
	m_bOpen					= false;
	m_nChangeState			= ELEMENT_STATE_NONE;
	m_flCheckSuppressTime	= 0.0f;
	m_EffectState			= (int)EFFECT_NONE;
	m_flLastDenySoundPlayed	= false;

#ifdef CLIENT_DLL
	m_nOldEffectState		= EFFECT_NONE;
	m_bOldOpen				= false;
#endif
}

CWeaponPhysCannon::~CWeaponPhysCannon()
{
	StopLoopingSounds();
}

//-----------------------------------------------------------------------------
// Purpose: Precache
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::Precache( void )
{
	PrecacheModel( PHYSCANNON_BEAM_SPRITE );
	PrecacheModel( PHYSCANNON_BEAM_SPRITE_NOZ );

	PrecacheScriptSound( "Weapon_PhysCannon.HoldSound" );
	PrecacheScriptSound( "Weapon_Physgun.Off" );

	PrecacheScriptSound( "Weapon_MegaPhysCannon.DryFire" );
	PrecacheScriptSound( "Weapon_MegaPhysCannon.Launch" );
	PrecacheScriptSound( "Weapon_MegaPhysCannon.Pickup");
	PrecacheScriptSound( "Weapon_MegaPhysCannon.Drop");
	PrecacheScriptSound( "Weapon_MegaPhysCannon.HoldSound");
	PrecacheScriptSound( "Weapon_MegaPhysCannon.ChargeZap");

	BaseClass::Precache();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::Spawn( void )
{
	BaseClass::Spawn();

	m_bPhyscannonState = IsMegaPhysCannon();

	// The megacannon uses a different skin
	if ( IsMegaPhysCannon() )
	{
		m_nSkin = MEGACANNON_SKIN;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Restore
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::OnRestore()
{
	BaseClass::OnRestore();
	m_grabController.OnRestore();

	m_bPhyscannonState = IsMegaPhysCannon();

	// Tracker 8106:  Physcannon effects disappear through level transition, so
	//  just recreate any effects here
	if ( m_EffectState != EFFECT_NONE )
	{
		DoEffect( m_EffectState, NULL );
	}
}

//-----------------------------------------------------------------------------
// On Remove
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::UpdateOnRemove(void)
{
	DestroyEffects( );
	BaseClass::UpdateOnRemove();
}

#ifdef CLIENT_DLL
void CWeaponPhysCannon::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );

		C_BaseAnimating::AutoAllowBoneAccess boneaccess( true, false );
		StartEffects();
	}

	if ( GetOwner() == NULL )
	{
		if ( m_hAttachedObject )
		{
			m_hAttachedObject->VPhysicsDestroyObject();
		}

		if ( m_hOldAttachedObject )
		{
			m_hOldAttachedObject->VPhysicsDestroyObject();
		}
	}

	// Update effect state when out of parity with the server
	if ( m_nOldEffectState != m_EffectState )
	{
		DoEffect( m_EffectState );
		m_nOldEffectState = m_EffectState;
	}

	// Update element state when out of parity
	if ( m_bOldOpen != m_bOpen )
	{
		if ( m_bOpen )
		{
			m_ElementParameter.InitFromCurrent( 1.0f, 0.2f, INTERP_SPLINE );
		}
		else
		{	
			m_ElementParameter.InitFromCurrent( 0.0f, 0.5f, INTERP_SPLINE );
		}

		m_bOldOpen = (bool) m_bOpen;
	}
}
#endif

//-----------------------------------------------------------------------------
// Sprite scale factor 
//-----------------------------------------------------------------------------
inline float CWeaponPhysCannon::SpriteScaleFactor() 
{
	return IsMegaPhysCannon() ? 1.5f : 1.0f;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponPhysCannon::Deploy( void )
{
	CloseElements();
	DoEffect( EFFECT_READY );

	bool bReturn = BaseClass::Deploy();

	m_flNextSecondaryAttack = m_flNextPrimaryAttack = gpGlobals->curtime;

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner )
	{
		pOwner->SetNextAttack( gpGlobals->curtime );
	}

	return bReturn;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::SetViewModel( void )
{
	if ( !IsMegaPhysCannon() )
	{
		BaseClass::SetViewModel();
		return;
	}

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner == NULL )
		return;

	CBaseViewModel *vm = pOwner->GetViewModel( m_nViewModelIndex );
	if ( vm == NULL )
		return;

	vm->SetWeaponModel( MEGACANNON_MODEL, this );
}

//-----------------------------------------------------------------------------
// Purpose: Force the cannon to drop anything it's carrying
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::ForceDrop( void )
{
	CloseElements();
	DetachObject();
	StopEffects();
}


//-----------------------------------------------------------------------------
// Purpose: Drops its held entity if it matches the entity passed in
// Input  : *pTarget - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponPhysCannon::DropIfEntityHeld( CBaseEntity *pTarget )
{
	if ( pTarget == NULL )
		return false;

	CBaseEntity *pHeld = m_grabController.GetAttached();
	
	if ( pHeld == NULL )
		return false;

	if ( pHeld == pTarget )
	{
		ForceDrop();
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::Drop( const Vector &vecVelocity )
{
	ForceDrop();

#ifndef CLIENT_DLL
	UTIL_Remove( this );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponPhysCannon::CanHolster( void ) 
{ 
	//Don't holster this weapon if we're holding onto something
	if ( m_bActive )
		return false;

	return BaseClass::CanHolster();
};

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponPhysCannon::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	//Don't holster this weapon if we're holding onto something
	if ( m_bActive )
		return false;

	ForceDrop();
	DestroyEffects();

	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DryFire( void )
{
	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	WeaponSound( EMPTY );

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner )
	{
#ifdef MAPBASE // TODO: Is this animation too dramatic?
		pOwner->SetAnimation( PLAYER_ATTACK1 );
#endif
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::PrimaryFireEffect( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	
	if ( pOwner == NULL )
		return;

	pOwner->ViewPunch( QAngle(-6, SharedRandomInt( "physcannonfire", -2,2) ,0) );
	
#ifndef CLIENT_DLL
	color32 white = { 245, 245, 255, 32 };
	UTIL_ScreenFade( pOwner, white, 0.1f, 0.0f, FFADE_IN );
#endif

	WeaponSound( SINGLE );
}

#define	MAX_KNOCKBACK_FORCE	128

//-----------------------------------------------------------------------------
// Punt non-physics
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::PuntNonVPhysics( CBaseEntity *pEntity, const Vector &forward, trace_t &tr )
{
	if ( m_hLastPuntedObject == pEntity && gpGlobals->curtime < m_flRepuntObjectTime )
		return;

#ifndef CLIENT_DLL
	{
		// misyl: Disable pred filtering in this server-only section.
		CDisablePredictionFiltering disablePred;
		CTakeDamageInfo	info;

		info.SetAttacker( GetOwner() );
		info.SetInflictor( this );
		info.SetDamage( 1.0f );
		info.SetDamageType( DMG_CRUSH | DMG_PHYSGUN );
		info.SetDamageForce( forward );	// Scale?
		info.SetDamagePosition( tr.endpos );

		m_hLastPuntedObject = pEntity;
		m_flRepuntObjectTime = gpGlobals->curtime + 0.5f;

		pEntity->DispatchTraceAttack( info, forward, &tr );

		ApplyMultiDamage();

		//Explosion effect
		DoEffect( EFFECT_LAUNCH, &tr.endpos );
	}
#endif
	
	PrimaryFireEffect();
	SendWeaponAnim( ACT_VM_SECONDARYATTACK );
#ifdef MAPBASE
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if (pPlayer)
		pPlayer->SetAnimation( PLAYER_ATTACK1 );
#endif

	m_nChangeState = ELEMENT_STATE_CLOSED;
	m_flElementDebounce = gpGlobals->curtime + 0.5f;
	m_flCheckSuppressTime = gpGlobals->curtime + 0.25f;
}


#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// What happens when the physgun picks up something 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::Physgun_OnPhysGunPickup( CBaseEntity *pEntity, CBasePlayer *pOwner, PhysGunPickup_t reason )
{
	// misyl: Disable pred filtering in this server-only section.
	CDisablePredictionFiltering disablePred;

	// If the target is debris, convert it to non-debris
	if ( pEntity->GetCollisionGroup() == COLLISION_GROUP_DEBRIS )
	{
		// Interactive debris converts back to debris when it comes to rest
		pEntity->SetCollisionGroup( COLLISION_GROUP_INTERACTIVE_DEBRIS );
	}

	float mass = 0.0f;
	if( pEntity->VPhysicsGetObject() )
	{
		mass = pEntity->VPhysicsGetObject()->GetMass();
	}

#ifdef HL2_EPISODIC
	// Warn Alyx if the player is punting a car around.
	if( hl2_episodic.GetBool() && mass > 250.0f )
	{
		CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
		int nAIs = g_AI_Manager.NumAIs();

		for ( int i = 0; i < nAIs; i++ )
		{
			if( ppAIs[ i ]->Classify() == CLASS_PLAYER_ALLY_VITAL )
			{
				ppAIs[ i ]->DispatchInteraction( g_interactionPlayerPuntedHeavyObject, pEntity, pOwner );
			}
		}
	}
#endif

	Pickup_OnPhysGunPickup( pEntity, pOwner, reason );
}
#endif

//-----------------------------------------------------------------------------
// Punt vphysics
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::PuntVPhysics( CBaseEntity *pEntity, const Vector &vecForward, trace_t &tr )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( m_hLastPuntedObject == pEntity && gpGlobals->curtime < m_flRepuntObjectTime )
		return;

	m_hLastPuntedObject = pEntity;
	m_flRepuntObjectTime = gpGlobals->curtime + 0.5f;

#ifndef CLIENT_DLL
	{
		// misyl: Disable pred filtering in this server-only section.
		CDisablePredictionFiltering disablePred;
		CTakeDamageInfo	info;

		Vector forward = vecForward;

		info.SetAttacker( GetOwner() );
		info.SetInflictor( this );
		info.SetDamage( 0.0f );
		info.SetDamageType( DMG_PHYSGUN );
		pEntity->DispatchTraceAttack( info, forward, &tr );
		ApplyMultiDamage();


		if ( Pickup_OnAttemptPhysGunPickup( pEntity, pOwner, PUNTED_BY_CANNON ) )
		{
			IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
			int listCount = pEntity->VPhysicsGetObjectList( pList, ARRAYSIZE(pList) );
			if ( !listCount )
			{
				//FIXME: Do we want to do this if there's no physics object?
				Physgun_OnPhysGunPickup( pEntity, pOwner, PUNTED_BY_CANNON );
				DryFire();
				return;
			}
				
			if( forward.z < 0 )
			{
				//reflect, but flatten the trajectory out a bit so it's easier to hit standing targets
				forward.z *= -0.65f;
			}
				
			// NOTE: Do this first to enable motion (if disabled) - so forces will work
			// Tell the object it's been punted
			Physgun_OnPhysGunPickup( pEntity, pOwner, PUNTED_BY_CANNON );

			// don't push vehicles that are attached to the world via fixed constraints
			// they will just wiggle...
			if ( (pList[0]->GetGameFlags() & FVPHYSICS_CONSTRAINT_STATIC) && pEntity->GetServerVehicle() )
			{
				forward.Init();
			}

			if ( !IsMegaPhysCannon() && !Pickup_ShouldPuntUseLaunchForces( pEntity, PHYSGUN_FORCE_PUNTED ) )
			{
				int i;

				// limit mass to avoid punting REALLY huge things
				float totalMass = 0;
				for ( i = 0; i < listCount; i++ )
				{
					totalMass += pList[i]->GetMass();
				}
				float maxMass = 250;
				IServerVehicle *pVehicle = pEntity->GetServerVehicle();
				if ( pVehicle )
				{
					maxMass *= 2.5;	// 625 for vehicles
				}
				float mass = MIN(totalMass, maxMass); // max 250kg of additional force

				// Put some spin on the object
				for ( i = 0; i < listCount; i++ )
				{
					const float hitObjectFactor = 0.5f;
					const float otherObjectFactor = 1.0f - hitObjectFactor;
  					// Must be light enough
					float ratio = pList[i]->GetMass() / totalMass;
					if ( pList[i] == pEntity->VPhysicsGetObject() )
					{
						ratio += hitObjectFactor;
						ratio = MIN(ratio,1.0f);
					}
					else
					{
						ratio *= otherObjectFactor;
					}
  					pList[i]->ApplyForceCenter( forward * 15000.0f * ratio );
  					pList[i]->ApplyForceOffset( forward * mass * 600.0f * ratio, tr.endpos );
				}
			}
			else
			{
				ApplyVelocityBasedForce( pEntity, vecForward, PHYSGUN_FORCE_PUNTED );
			}
		}
	}

#endif
	// Add recoil
	QAngle	recoil = QAngle( random->RandomFloat( 1.0f, 2.0f ), random->RandomFloat( -1.0f, 1.0f ), 0 );
	pOwner->ViewPunch( recoil );

	//Explosion effect
	DoEffect( EFFECT_LAUNCH, &tr.endpos );

	PrimaryFireEffect();
	SendWeaponAnim( ACT_VM_SECONDARYATTACK );

#ifdef MAPBASE
	pOwner->SetAnimation( PLAYER_ATTACK1 );
#endif

	m_nChangeState = ELEMENT_STATE_CLOSED;
	m_flElementDebounce = gpGlobals->curtime + 0.5f;
	m_flCheckSuppressTime = gpGlobals->curtime + 0.25f;

	// Don't allow the gun to regrab a thrown object!!
	m_flNextSecondaryAttack = m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;

#ifdef GAME_DLL
	if ( pOwner )
		pOwner->OnMyWeaponFired( this );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Applies velocity-based forces to throw the entity. This code is
//			called from both punt and launch carried code.
//			ASSUMES: that pEntity is a vphysics entity.
// Input  : - 
//-----------------------------------------------------------------------------
#ifdef CLIENT_DLL
void CWeaponPhysCannon::ApplyVelocityBasedForce( CBaseEntity *pEntity, const Vector &forward )
#else
void CWeaponPhysCannon::ApplyVelocityBasedForce( CBaseEntity *pEntity, const Vector &forward, PhysGunForce_t reason )
#endif
{
#ifndef CLIENT_DLL
	// Get the launch velocity
	Vector vVel = Pickup_PhysGunLaunchVelocity( pEntity, forward, reason );
	
	// Get the launch angular impulse
	AngularImpulse aVel = Pickup_PhysGunLaunchAngularImpulse( pEntity, reason );
		
	// Get the physics object (MUST have one)
	IPhysicsObject *pPhysicsObject = pEntity->VPhysicsGetObject();
	if ( pPhysicsObject == NULL )
	{
		Assert( 0 );
		return;
	}

	CRagdollProp *pRagdoll = dynamic_cast<CRagdollProp*>( pEntity );
	if ( pRagdoll != NULL )
	{
		Vector	vTempVel;
		AngularImpulse vTempAVel;

		ragdoll_t *pRagdollPhys = pRagdoll->GetRagdoll( );
		for ( int j = 0; j < pRagdollPhys->listCount; ++j )
		{
			pRagdollPhys->list[j].pObject->AddVelocity( &vVel, &aVel ); 
		}
		return;
	}

	float flForceMax = physcannon_maxforce.GetFloat();
	float flForce = flForceMax;

	float mass = pPhysicsObject->GetMass();
	if (mass > 100)
	{
		mass = MIN(mass, 1000);
		float flForceMin = physcannon_minforce.GetFloat();
		flForce = SimpleSplineRemapVal(mass, 100, 600, flForceMax, flForceMin);
	}

	//Vector vVel = forward * flForce;
	// FIXME: Josh needs to put a real value in for PHYSGUN_FORCE_PUNTED
	//AngularImpulse aVel = Pickup_PhysGunLaunchAngularImpulse( pEntity, PHYSGUN_FORCE_PUNTED );
		
	pPhysicsObject->AddVelocity( &vVel, &aVel );

#endif

}


//-----------------------------------------------------------------------------
// Punt non-physics
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::PuntRagdoll( CBaseEntity *pEntity, const Vector &vecForward, trace_t &tr )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( m_hLastPuntedObject == pEntity && gpGlobals->curtime < m_flRepuntObjectTime )
		return;

	m_hLastPuntedObject = pEntity;
	m_flRepuntObjectTime = gpGlobals->curtime + 0.5f;

#ifndef CLIENT_DLL
	Pickup_OnPhysGunDrop( pEntity, pOwner, LAUNCHED_BY_CANNON );

	CTakeDamageInfo	info;

	Vector forward = vecForward;
	info.SetAttacker( GetOwner() );
	info.SetInflictor( this );
	info.SetDamage( 0.0f );
	info.SetDamageType( DMG_PHYSGUN );
	pEntity->DispatchTraceAttack( info, forward, &tr );
	ApplyMultiDamage();

	if ( Pickup_OnAttemptPhysGunPickup( pEntity, pOwner, PUNTED_BY_CANNON ) )
	{
		Physgun_OnPhysGunPickup( pEntity, pOwner, PUNTED_BY_CANNON );

		if( forward.z < 0 )
		{
			//reflect, but flatten the trajectory out a bit so it's easier to hit standing targets
			forward.z *= -0.65f;
		}
		
		Vector			vVel = forward * 1500;
		AngularImpulse	aVel = Pickup_PhysGunLaunchAngularImpulse( pEntity, PHYSGUN_FORCE_PUNTED );

		CRagdollProp *pRagdoll = dynamic_cast<CRagdollProp*>( pEntity );
		ragdoll_t *pRagdollPhys = pRagdoll->GetRagdoll( );

		int j;
		for ( j = 0; j < pRagdollPhys->listCount; ++j )
		{
			pRagdollPhys->list[j].pObject->AddVelocity( &vVel, NULL ); 
		}
	}
#endif
	
	// Add recoil
	QAngle	recoil = QAngle( random->RandomFloat( 1.0f, 2.0f ), random->RandomFloat( -1.0f, 1.0f ), 0 );
	pOwner->ViewPunch( recoil );

	//Explosion effect
	DoEffect( EFFECT_LAUNCH, &tr.endpos );

	PrimaryFireEffect();
	SendWeaponAnim( ACT_VM_SECONDARYATTACK );

#ifdef MAPBASE
	pOwner->SetAnimation( PLAYER_ATTACK1 );
#endif

	m_nChangeState = ELEMENT_STATE_CLOSED;
	m_flElementDebounce = gpGlobals->curtime + 0.5f;
	m_flCheckSuppressTime = gpGlobals->curtime + 0.25f;

	// Don't allow the gun to regrab a thrown object!!
	m_flNextSecondaryAttack = m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;
}


//-----------------------------------------------------------------------------
// Trace length
//-----------------------------------------------------------------------------
float CWeaponPhysCannon::TraceLength()
{
	if ( !IsMegaPhysCannon() )
	{
		return physcannon_tracelength.GetFloat();
	}
	
	return physcannon_mega_tracelength.GetFloat();
}

//-----------------------------------------------------------------------------
// If there's any special rejection code you need to do per entity then do it here
// This is kinda nasty but I'd hate to move more physcannon related stuff into CBaseEntity
//-----------------------------------------------------------------------------
bool CWeaponPhysCannon::EntityAllowsPunts( CBaseEntity *pEntity )
{
#ifndef CLIENT_DLL
	if ( pEntity->HasSpawnFlags( SF_PHYSBOX_NEVER_PUNT ) )
	{
		CPhysBox *pPhysBox = dynamic_cast<CPhysBox*>(pEntity);

		if ( pPhysBox != NULL )
		{
			if ( pPhysBox->HasSpawnFlags( SF_PHYSBOX_NEVER_PUNT ) )
			{
				return false;
			}
		}
	}

	if ( pEntity->HasSpawnFlags( SF_WEAPON_NO_PHYSCANNON_PUNT ) )
	{
#ifdef MAPBASE
		if (pEntity->IsBaseCombatWeapon() || pEntity->IsCombatItem())
			return false;
#else
		CBaseCombatWeapon *pWeapon = dynamic_cast<CBaseCombatWeapon*>(pEntity);

		if ( pWeapon != NULL )
		{
			if ( pWeapon->HasSpawnFlags( SF_WEAPON_NO_PHYSCANNON_PUNT ) )
			{
				return false;
			}
		}
#endif
	}
#endif

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//
// This mode is a toggle. Primary fire one time to pick up a physics object.
// With an object held, click primary fire again to drop object.
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::PrimaryAttack( void )
{
	if( m_flNextPrimaryAttack > gpGlobals->curtime )
		return;

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	
	if ( pOwner == NULL )
		return;

	if( m_bActive )
	{
		// Punch the object being held!!
		Vector forward;
		pOwner->EyeVectors( &forward );

		// Validate the item is within punt range
		CBaseEntity *pHeld = m_grabController.GetAttached();
		Assert( pHeld != NULL );

		if ( pHeld != NULL )
		{
			float heldDist = ( pHeld->WorldSpaceCenter() - pOwner->WorldSpaceCenter() ).Length();

			if ( heldDist > physcannon_tracelength.GetFloat() )
			{
				// We can't punt this yet
				DryFire();
				return;
			}
		}

		LaunchObject( forward, physcannon_maxforce.GetFloat() );

		PrimaryFireEffect();
		SendWeaponAnim( ACT_VM_SECONDARYATTACK );
#ifdef MAPBASE
		pOwner->SetAnimation( PLAYER_ATTACK1 );
#endif
		return;
	}

	// If not active, just issue a physics punch in the world.
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;

	Vector forward;
	pOwner->EyeVectors( &forward );

	// NOTE: Notice we're *not* using the mega tracelength here
	// when you have the mega cannon. Punting has shorter range.
	Vector start, end;
	start = pOwner->Weapon_ShootPosition();
	float flPuntDistance = physcannon_tracelength.GetFloat();
	VectorMA( start, flPuntDistance, forward, end );

	CTraceFilterNoOwnerTest filter( pOwner, COLLISION_GROUP_NONE );
	trace_t tr;
	UTIL_PhyscannonTraceHull( start, end, -Vector(8,8,8), Vector(8,8,8), pOwner, &tr );
	bool bValid = true;
	CBaseEntity *pEntity = tr.m_pEnt;
	if ( tr.fraction == 1 || !tr.m_pEnt || tr.m_pEnt->IsEFlagSet( EFL_NO_PHYSCANNON_INTERACTION ) )
	{
		bValid = false;
	}
	else if ( (pEntity->GetMoveType() != MOVETYPE_VPHYSICS) && ( pEntity->m_takedamage == DAMAGE_NO ) )
	{
		bValid = false;
	}

	// If the entity we've hit is invalid, try a traceline instead
	if ( !bValid )
	{
		UTIL_PhyscannonTraceLine( start, end, pOwner, &tr );
		if ( tr.fraction == 1 || !tr.m_pEnt || tr.m_pEnt->IsEFlagSet( EFL_NO_PHYSCANNON_INTERACTION ) )
		{
#ifdef HL2_EPISODIC
			if( hl2_episodic.GetBool() )
			{
				// Try to find something in a very small cone. 
				CBaseEntity *pObject = FindObjectInCone( start, forward, physcannon_punt_cone.GetFloat() );

				if( pObject )
				{
					// Trace to the object.
					UTIL_PhyscannonTraceLine( start, pObject->WorldSpaceCenter(), pOwner, &tr );

					if( tr.m_pEnt && tr.m_pEnt == pObject && !(pObject->IsEFlagSet(EFL_NO_PHYSCANNON_INTERACTION)) )
					{
						bValid = true;
						pEntity = pObject;
					}
				}
			}
#endif
		}
		else
		{
			bValid = true;
			pEntity = tr.m_pEnt;
		}
	}

	if( !bValid )
	{
		DryFire();
		return;
	}

	// See if we hit something
	if ( pEntity->GetMoveType() != MOVETYPE_VPHYSICS )
	{
		if ( pEntity->m_takedamage == DAMAGE_NO )
		{
			DryFire();
			return;
		}

		if( GetOwner()->IsPlayer() && !IsMegaPhysCannon() )
		{
			// Don't let the player zap any NPC's except regular antlions and headcrabs.
#ifdef CLIENT_DLL
			if( pEntity->IsPlayer() || pEntity->IsNPC() )
#else
			if( pEntity->IsPlayer() || (pEntity->IsNPC() && pEntity->Classify() != CLASS_HEADCRAB && !FClassnameIs(pEntity, "npc_antlion")) )
#endif
			{
				DryFire();
				return;
			}
		}

#ifndef CLIENT_DLL
		if ( IsMegaPhysCannon() )
		{
			if ( pEntity->IsNPC() && !pEntity->IsEFlagSet( EFL_NO_MEGAPHYSCANNON_RAGDOLL ) && pEntity->MyNPCPointer()->CanBecomeRagdoll() )
			{
				CTakeDamageInfo info( pOwner, pOwner, 1.0f, DMG_GENERIC );
				CBaseEntity *pRagdoll = CreateServerRagdoll( pEntity->MyNPCPointer(), 0, info, COLLISION_GROUP_INTERACTIVE_DEBRIS, true );
				PhysSetEntityGameFlags( pRagdoll, FVPHYSICS_NO_SELF_COLLISIONS );
				pRagdoll->SetCollisionBounds( pEntity->CollisionProp()->OBBMins(), pEntity->CollisionProp()->OBBMaxs() );

				// Necessary to cause it to do the appropriate death cleanup
				CTakeDamageInfo ragdollInfo( pOwner, pOwner, 10000.0, DMG_PHYSGUN | DMG_REMOVENORAGDOLL );
				pEntity->TakeDamage( ragdollInfo );

				PuntRagdoll( pRagdoll, forward, tr );
				return;
			}
		}
#endif

		PuntNonVPhysics( pEntity, forward, tr );
	}
	else
	{
		if ( EntityAllowsPunts( pEntity) == false )
		{
			DryFire();
			return;
		}

		if ( !IsMegaPhysCannon() )
		{
			if ( pEntity->VPhysicsIsFlesh( ) )
			{
				DryFire();
				return;
			}
			PuntVPhysics( pEntity, forward, tr );
		}
		else
		{
#ifndef CLIENT_DLL
			if ( dynamic_cast<CRagdollProp*>(pEntity) )
			{
				PuntRagdoll( pEntity, forward, tr );
			}
			else
#endif
			{
				PuntVPhysics( pEntity, forward, tr );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Click secondary attack whilst holding an object to hurl it.
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::SecondaryAttack( void )
{
	if ( m_flNextSecondaryAttack > gpGlobals->curtime )
		return;

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	
	if ( pOwner == NULL )
		return;

	// See if we should drop a held item
	if ( ( m_bActive ) && ( pOwner->m_afButtonPressed & IN_ATTACK2 ) )
	{
		// Drop the held object
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.5;
		m_flNextSecondaryAttack = gpGlobals->curtime + 0.5;

		DetachObject();

		DoEffect( EFFECT_READY );

		SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	}
	else
	{
#ifndef CLIENT_DLL
		// misyl: Disable pred filtering in this server-only section.
		CDisablePredictionFiltering disablePred;

		// Otherwise pick it up
		FindObjectResult_t result = FindObject();
		switch ( result )
		{
		case OBJECT_FOUND:
			WeaponSound( SPECIAL1 );
			SendWeaponAnim( ACT_VM_PRIMARYATTACK );
			m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;

			// We found an object. Debounce the button
			m_nAttack2Debounce |= pOwner->m_nButtons;
			break;

		case OBJECT_NOT_FOUND:
			m_flNextSecondaryAttack = gpGlobals->curtime + 0.1f;
			CloseElements();
			break;

		case OBJECT_BEING_DETACHED:
			m_flNextSecondaryAttack = gpGlobals->curtime + 0.01f;
			break;
		}

		DoEffect( EFFECT_HOLDING );
#endif
	}
}	

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::WeaponIdle( void )
{
	if ( HasWeaponIdleTimeElapsed() )
	{
		if ( m_bActive )
		{
			//Shake when holding an item
			SendWeaponAnim( ACT_VM_RELOAD );
		}
		else
		{
			//Otherwise idle simply
			SendWeaponAnim( ACT_VM_IDLE );
		}
	}
}

#ifndef CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pObject - 
//-----------------------------------------------------------------------------
bool CWeaponPhysCannon::AttachObject( CBaseEntity *pObject, const Vector &vPosition )
{
	// misyl: Disable pred filtering in this server-only section.
	CDisablePredictionFiltering disablePred;

	if ( m_bActive )
		return false;

	if ( CanPickupObject( pObject ) == false )
		return false;

	m_grabController.SetIgnorePitch( false );
	m_grabController.SetAngleAlignment( 0 );

	bool bKilledByGrab = false;

	bool bIsMegaPhysCannon = IsMegaPhysCannon();
	if ( bIsMegaPhysCannon )
	{
		if ( pObject->IsNPC() && !pObject->IsEFlagSet( EFL_NO_MEGAPHYSCANNON_RAGDOLL ) )
		{
			Assert( pObject->MyNPCPointer()->CanBecomeRagdoll() );
			CTakeDamageInfo info( GetOwner(), GetOwner(), 1.0f, DMG_GENERIC );
			CBaseEntity *pRagdoll = CreateServerRagdoll( pObject->MyNPCPointer(), 0, info, COLLISION_GROUP_INTERACTIVE_DEBRIS, true );
			PhysSetEntityGameFlags( pRagdoll, FVPHYSICS_NO_SELF_COLLISIONS );

			pRagdoll->SetCollisionBounds( pObject->CollisionProp()->OBBMins(), pObject->CollisionProp()->OBBMaxs() );

			// Necessary to cause it to do the appropriate death cleanup
			CTakeDamageInfo ragdollInfo( GetOwner(), GetOwner(), 10000.0, DMG_PHYSGUN | DMG_REMOVENORAGDOLL );
			pObject->TakeDamage( ragdollInfo );

			// Now we act on the ragdoll for the remainder of the time
			pObject = pRagdoll;
			bKilledByGrab = true;
		}
	}

	IPhysicsObject *pPhysics = pObject->VPhysicsGetObject();

	// Must be valid
	if ( !pPhysics )
		return false;

	CHL2MP_Player *pOwner = (CHL2MP_Player *)ToBasePlayer( GetOwner() );

	m_bActive = true;
	if( pOwner )
	{
#ifdef HL2_EPISODIC
		CBreakableProp *pProp = dynamic_cast< CBreakableProp * >( pObject );

		if ( pProp && pProp->HasInteraction( PROPINTER_PHYSGUN_CREATE_FLARE ) )
		{
			pOwner->FlashlightTurnOff();
		}
#endif

		// NOTE: This can change the mass; so it must be done before max speed setting
		Physgun_OnPhysGunPickup( pObject, pOwner, PICKED_UP_BY_CANNON );
	}

	// NOTE :This must happen after OnPhysGunPickup because that can change the mass
	m_grabController.AttachEntity( pOwner, pObject, pPhysics, bIsMegaPhysCannon, vPosition, (!bKilledByGrab) );
	m_hAttachedObject = pObject;
	m_attachedPositionObjectSpace = m_grabController.m_attachedPositionObjectSpace;
	m_attachedAnglesPlayerSpace = m_grabController.m_attachedAnglesPlayerSpace;

	m_bResetOwnerEntity = false;

	if ( m_hAttachedObject->GetOwnerEntity() == NULL )
	{
		m_hAttachedObject->SetOwnerEntity( pOwner );
		m_bResetOwnerEntity = true;
	}

#ifdef MAPBASE_MP
	m_bDontPredictAttached = false;

	// HACKHACK
	if ( m_hAttachedObject->ClassMatches( "combine_mine" ) )
	{
		m_bDontPredictAttached = true;
	}

	if( pOwner && sv_player_enable_gravgun_sprint.GetBool() == false )
	{
		pOwner->EnableSprint( false );

		float	loadWeight = ( 1.0f - GetLoadPercentage() );
		float	maxSpeed = hl2_walkspeed.GetFloat() + ( ( hl2_normspeed.GetFloat() - hl2_walkspeed.GetFloat() ) * loadWeight );

		//Msg( "Load perc: %f -- Movement speed: %f/%f\n", loadWeight, maxSpeed, hl2_normspeed.GetFloat() );
		pOwner->SetMaxSpeed( maxSpeed );
	}
#endif

	// Don't drop again for a slight delay, in case they were pulling objects near them
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.4f;

	DoEffect( EFFECT_HOLDING );
	OpenElements();

	return true;
}

void CWeaponPhysCannon::FindObjectTrace( CBasePlayer *pPlayer, trace_t *pTraceResult )
{
	Vector forward;
	pPlayer->EyeVectors( &forward );

	// Setup our positions
	Vector	start = pPlayer->Weapon_ShootPosition();
	float	testLength = TraceLength() * 4.0f;
	Vector	end = start + forward * testLength;

	if( IsMegaPhysCannon() && hl2_episodic.GetBool() )
	{
		Vector vecAutoAimDir = pPlayer->GetAutoaimVector( 1.0f, testLength );
		end = start + vecAutoAimDir * testLength;
	}

	// Try to find an object by looking straight ahead
	UTIL_PhyscannonTraceLine( start, end, pPlayer, pTraceResult );

	// Try again with a hull trace
	if ( !pTraceResult->DidHitNonWorldEntity() )
	{
		UTIL_PhyscannonTraceHull( start, end, -Vector(4,4,4), Vector(4,4,4), pPlayer, pTraceResult );
	}
}


CWeaponPhysCannon::FindObjectResult_t CWeaponPhysCannon::FindObject( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	
	Assert( pPlayer );
	if ( pPlayer == NULL )
		return OBJECT_NOT_FOUND;
	
	trace_t tr;
	FindObjectTrace( pPlayer, &tr );
	CBaseEntity *pEntity = tr.m_pEnt ? tr.m_pEnt->GetRootMoveParent() : NULL;
	bool	bAttach = false;
	bool	bPull = false;

	// If we hit something, pick it up or pull it
	if ( ( tr.fraction != 1.0f ) && ( tr.m_pEnt ) && ( tr.m_pEnt->IsWorld() == false ) )
	{
		// Attempt to attach if within range
		if ( tr.fraction <= 0.25f )
		{
			bAttach = true;
		}
		else if ( tr.fraction > 0.25f )
		{
			bPull = true;
		}
	}
	
	Vector forward;
	pPlayer->EyeVectors( &forward );

	// Setup our positions
	Vector	start = pPlayer->Weapon_ShootPosition();
	float	testLength = TraceLength() * 4.0f;

	// Find anything within a general cone in front
	CBaseEntity *pConeEntity = NULL;
	if ( !IsMegaPhysCannon() )
	{
		if (!bAttach && !bPull)
		{
			pConeEntity = FindObjectInCone( start, forward, physcannon_cone.GetFloat() );
		}
	}
	else
	{
		pConeEntity = MegaPhysCannonFindObjectInCone( start, forward, 
			physcannon_cone.GetFloat(), physcannon_ball_cone.GetFloat(), bAttach || bPull );
	}

	if ( pConeEntity )
	{
		pEntity = pConeEntity;

		// If the object is near, grab it. Else, pull it a bit.
		if ( pEntity->WorldSpaceCenter().DistToSqr( start ) <= (testLength * testLength) )
		{
			bAttach = true;
		}
		else
		{
			bPull = true;
		}
	}

	if ( CanPickupObject( pEntity ) == false )
	{
		CBaseEntity *pNewObject = Pickup_OnFailedPhysGunPickup( pEntity, start );

		if ( pNewObject && CanPickupObject( pNewObject ) )
		{
			pEntity = pNewObject;
		}
		else
		{
			// Make a noise to signify we can't pick this up
			if ( !m_flLastDenySoundPlayed )
			{
				m_flLastDenySoundPlayed = true;
				WeaponSound( SPECIAL3 );
			}

			return OBJECT_NOT_FOUND;
		}
	}

	// Check to see if the object is constrained + needs to be ripped off...
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !Pickup_OnAttemptPhysGunPickup( pEntity, pOwner, PICKED_UP_BY_CANNON ) )
		return OBJECT_BEING_DETACHED;

	if ( bAttach )
	{
		return AttachObject( pEntity, tr.endpos ) ? OBJECT_FOUND : OBJECT_NOT_FOUND;
	}

	if ( !bPull )
		return OBJECT_NOT_FOUND;

	// FIXME: This needs to be run through the CanPickupObject logic
	IPhysicsObject *pObj = pEntity->VPhysicsGetObject();
	if ( !pObj )
		return OBJECT_NOT_FOUND;

	// If we're too far, simply start to pull the object towards us
	Vector	pullDir = start - pEntity->WorldSpaceCenter();
	VectorNormalize( pullDir );
	pullDir *= IsMegaPhysCannon() ? physcannon_mega_pullforce.GetFloat() : physcannon_pullforce.GetFloat();
	
	float mass = PhysGetEntityMass( pEntity );
	if ( mass < 50.0f )
	{
		pullDir *= (mass + 0.5) * (1/50.0f);
	}

	CPhysicsProp* pProp = dynamic_cast<CPhysicsProp*>(pEntity);
	if (pProp) {
		pProp->OnPhysGunPull( pOwner );
	}

	// Nudge it towards us
	pObj->ApplyForceCenter( pullDir );
	return OBJECT_NOT_FOUND;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CBaseEntity *CWeaponPhysCannon::MegaPhysCannonFindObjectInCone( const Vector &vecOrigin, 
   const Vector &vecDir, float flCone, float flCombineBallCone, bool bOnlyCombineBalls )
{
	// Find the nearest physics-based item in a cone in front of me.
	CBaseEntity *list[1024];
	float flMaxDist = TraceLength() + 1.0;
	float flNearestDist = flMaxDist;
	bool bNearestIsCombineBall = bOnlyCombineBalls ? true : false;
	Vector mins = vecOrigin - Vector( flNearestDist, flNearestDist, flNearestDist );
	Vector maxs = vecOrigin + Vector( flNearestDist, flNearestDist, flNearestDist );

	CBaseEntity *pNearest = NULL;

	int count = UTIL_EntitiesInBox( list, 1024, mins, maxs, 0 );
	for( int i = 0 ; i < count ; i++ )
	{
		if ( !list[ i ]->VPhysicsGetObject() )
			continue;

		bool bIsCombineBall = FClassnameIs( list[ i ], "prop_combine_ball" );
		if ( !bIsCombineBall && bNearestIsCombineBall )
			continue;

		// Closer than other objects
		Vector los;
		VectorSubtract( list[ i ]->WorldSpaceCenter(), vecOrigin, los );
		float flDist = VectorNormalize( los );

		if ( !bIsCombineBall || bNearestIsCombineBall )
		{
			// Closer than other objects
			if( flDist >= flNearestDist )
				continue;

			// Cull to the cone
			if ( DotProduct( los, vecDir ) <= flCone )
				continue;
		}
		else
		{
			// Close enough?
			if ( flDist >= flMaxDist )
				continue;

			// Cull to the cone
			if ( DotProduct( los, vecDir ) <= flCone )
				continue;

			// NOW: If it's either closer than nearest dist or within the ball cone, use it!
			if ( (flDist > flNearestDist) && (DotProduct( los, vecDir ) <= flCombineBallCone) )
				continue;
		}

		// Make sure it isn't occluded!
		trace_t tr;
		UTIL_PhyscannonTraceLine( vecOrigin, list[ i ]->WorldSpaceCenter(), GetOwner(), &tr );
		if( tr.m_pEnt == list[ i ] )
		{
			flNearestDist = flDist;
			pNearest = list[ i ];
			bNearestIsCombineBall = bIsCombineBall;
		}
	}

	return pNearest;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CBaseEntity *CWeaponPhysCannon::FindObjectInCone( const Vector &vecOrigin, const Vector &vecDir, float flCone )
{
	// Find the nearest physics-based item in a cone in front of me.
	CBaseEntity *list[256];
	float flNearestDist = physcannon_tracelength.GetFloat() + 1.0; //Use regular distance.
	Vector mins = vecOrigin - Vector( flNearestDist, flNearestDist, flNearestDist );
	Vector maxs = vecOrigin + Vector( flNearestDist, flNearestDist, flNearestDist );

	CBaseEntity *pNearest = NULL;

	int count = UTIL_EntitiesInBox( list, 256, mins, maxs, 0 );
	for( int i = 0 ; i < count ; i++ )
	{
		if ( !list[ i ]->VPhysicsGetObject() )
			continue;

		// Closer than other objects
		Vector los = ( list[ i ]->WorldSpaceCenter() - vecOrigin );
		float flDist = VectorNormalize( los );
		if( flDist >= flNearestDist )
			continue;

		// Cull to the cone
		if ( DotProduct( los, vecDir ) <= flCone )
			continue;

		// Make sure it isn't occluded!
		trace_t tr;
		UTIL_PhyscannonTraceLine( vecOrigin, list[ i ]->WorldSpaceCenter(), GetOwner(), &tr );
		if( tr.m_pEnt == list[ i ] )
		{
			flNearestDist = flDist;
			pNearest = list[ i ];
		}
	}

	return pNearest;
}

#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CGrabController::UpdateObject( CBasePlayer *pPlayer, float flError )
{
	CBaseEntity *pEntity = GetAttached();
	if ( !pEntity )
		return false;
	if ( ComputeError() > flError )
		return false;
	if ( pPlayer->GetGroundEntity() == pEntity )
		return false;
	if (!pEntity->VPhysicsGetObject() )
		return false;    

	//Adrian: Oops, our object became motion disabled, let go!
	IPhysicsObject *pPhys = pEntity->VPhysicsGetObject();
	if ( pPhys && pPhys->IsMoveable() == false )
	{
		return false;
	}

	if ( m_frameCount == gpGlobals->framecount )
	{
		return true;
	}
	m_frameCount = gpGlobals->framecount;
	Vector forward, right, up;
	QAngle playerAngles = pPlayer->EyeAngles();

	float pitch = AngleDistance(playerAngles.x,0);
	playerAngles.x = clamp( pitch, -75, 75 );
	AngleVectors( playerAngles, &forward, &right, &up );

	if ( HL2MPRules()->MegaPhyscannonActive() )
	{
		Vector los = ( pEntity->WorldSpaceCenter() - pPlayer->Weapon_ShootPosition() );
		VectorNormalize( los );

		float flDot = DotProduct( los, forward );

		//Let go of the item if we turn around too fast.
		if ( flDot <= 0.35f )
			return false;
	}

	// Now clamp a sphere of object radius at end to the player's bbox
	Vector radial = physcollision->CollideGetExtent( pPhys->GetCollide(), vec3_origin, pEntity->GetAbsAngles(), -forward );
	Vector player2d = pPlayer->CollisionProp()->OBBMaxs();
	float playerRadius = player2d.Length2D();
	float flDot = DotProduct( forward, radial );

	float radius = playerRadius + fabs( flDot );

	float distance = 24 + ( radius * 2.0f );

	// Add the prop's distance offset
	distance += m_flDistanceOffset;

	Vector start = pPlayer->Weapon_ShootPosition();
	Vector end = start + ( forward * distance );

	trace_t	tr;
	CTraceFilterSkipTwoEntities traceFilter( pPlayer, pEntity, COLLISION_GROUP_NONE );
	Ray_t ray;
	ray.Init( start, end );
	enginetrace->TraceRay( ray, MASK_SOLID_BRUSHONLY, &traceFilter, &tr );

	if ( tr.fraction < 0.5 )
	{
		end = start + forward * (radius*0.5f);
	}
	else if ( tr.fraction <= 1.0f )
	{
		end = start + forward * ( distance - radius );
	}

	Vector playerMins, playerMaxs, nearest;
	pPlayer->CollisionProp()->WorldSpaceAABB( &playerMins, &playerMaxs );
	Vector playerLine = pPlayer->CollisionProp()->WorldSpaceCenter();
	CalcClosestPointOnLine( end, playerLine+Vector(0,0,playerMins.z), playerLine+Vector(0,0,playerMaxs.z), nearest, NULL );

	Vector delta = end - nearest;
	float len = VectorNormalize(delta);
	if ( len < radius )
	{
		end = nearest + radius * delta;
	}

	QAngle angles = TransformAnglesFromPlayerSpace( m_attachedAnglesPlayerSpace, pPlayer );

	//Show overlays of radius
	if ( g_debug_physcannon.GetBool() )
	{

#ifdef CLIENT_DLL

		debugoverlay->AddBoxOverlay( end, -Vector( 2,2,2 ), Vector(2,2,2), angles, 0, 255, 255, true, 0 );

		debugoverlay->AddBoxOverlay( GetAttached()->WorldSpaceCenter(), 
							-Vector( radius, radius, radius), 
							Vector( radius, radius, radius ),
							angles,
							255, 255, 0,
							true,
							0.0f );

#else

		NDebugOverlay::Box( end, -Vector( 2,2,2 ), Vector(2,2,2), 0, 255, 0, true, 0 );

		NDebugOverlay::Box( GetAttached()->WorldSpaceCenter(), 
							-Vector( radius+5, radius+5, radius+5), 
							Vector( radius+5, radius+5, radius+5 ),
							255, 0, 0,
							true,
							0.0f );
#endif
	}
	
#ifndef CLIENT_DLL
	// If it has a preferred orientation, update to ensure we're still oriented correctly.
	Pickup_GetPreferredCarryAngles( pEntity, pPlayer, pPlayer->EntityToWorldTransform(), angles );


	// We may be holding a prop that has preferred carry angles
	if ( m_bHasPreferredCarryAngles )
	{
		matrix3x4_t tmp;
		ComputePlayerMatrix( pPlayer, tmp );
		angles = TransformAnglesToWorldSpace( m_vecPreferredCarryAngles, tmp );
	}

#endif

	matrix3x4_t attachedToWorld;
	Vector offset;
	AngleMatrix( angles, attachedToWorld );
	VectorRotate( m_attachedPositionObjectSpace, attachedToWorld, offset );

	SetTargetPosition( end - offset, angles );

	return true;
}

void CWeaponPhysCannon::UpdateObject( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	Assert( pPlayer );

	float flError = IsMegaPhysCannon() ? 18 : 12;
	if ( !m_grabController.UpdateObject( pPlayer, flError ) )
	{
		DetachObject();
		return;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DetachObject( bool playSound, bool wasLaunched )
{
#ifndef CLIENT_DLL
	// misyl: Disable pred filtering in this server-only section.
	CDisablePredictionFiltering disablePred;

	if ( m_bActive == false )
		return;

	CHL2MP_Player *pOwner = (CHL2MP_Player *)ToBasePlayer( GetOwner() );
	if( pOwner != NULL )
	{
#ifndef MAPBASE
		pOwner->EnableSprint( true );
		pOwner->SetMaxSpeed( hl2_normspeed.GetFloat() );
		
#else
		if (sv_player_enable_gravgun_sprint.GetBool() == false)
		{
			pOwner->EnableSprint( true );
			pOwner->SetMaxSpeed( hl2_normspeed.GetFloat() );
		}
#endif	
	}

	CBaseEntity *pObject = m_grabController.GetAttached();

	m_grabController.DetachEntity( wasLaunched );

	if ( pObject != NULL )
	{
		Pickup_OnPhysGunDrop( pObject, pOwner, wasLaunched ? LAUNCHED_BY_CANNON : DROPPED_BY_CANNON );
	}
	
	if ( pObject && m_bResetOwnerEntity == true )
	{
		pObject->SetOwnerEntity( NULL );
	}

	m_bActive = false;
	m_hAttachedObject = NULL;

	
	if ( playSound )
	{
		//Play the detach sound
		WeaponSound( MELEE_MISS );
	}
	
#else

	m_grabController.DetachEntity( wasLaunched );

	if ( m_hAttachedObject )
	{
		m_hAttachedObject->VPhysicsDestroyObject();
	}
#endif

	// Stop our looping sound
	if ( GetMotorSound() )
	{
		( CSoundEnvelopeController::GetController() ).SoundChangeVolume( GetMotorSound(), 0.0f, 1.0f );
		( CSoundEnvelopeController::GetController() ).SoundChangePitch( GetMotorSound(), 50, 1.0f );
	}
}


#ifdef CLIENT_DLL
void CWeaponPhysCannon::ManagePredictedObject( void )
{
#ifdef MAPBASE_MP
	if ( m_bDontPredictAttached )
		return;
#endif

	CBaseEntity *pAttachedObject = m_hAttachedObject.Get();

	if ( m_hAttachedObject )
	{
		// NOTE :This must happen after OnPhysGunPickup because that can change the mass
		if ( pAttachedObject != GetGrabController().GetAttached() )
		{
			IPhysicsObject *pPhysics = pAttachedObject->VPhysicsGetObject();

			if ( pPhysics == NULL )
			{
				solid_t tmpSolid;
				PhysModelParseSolid( tmpSolid, m_hAttachedObject, pAttachedObject->GetModelIndex() );

				pAttachedObject->VPhysicsInitNormal( SOLID_VPHYSICS, 0, false, &tmpSolid );
			}

			pPhysics = pAttachedObject->VPhysicsGetObject();

			if ( pPhysics )
			{
				m_grabController.SetIgnorePitch( false );
				m_grabController.SetAngleAlignment( 0 );

				GetGrabController().AttachEntity( ToBasePlayer( GetOwner() ), pAttachedObject, pPhysics, false, vec3_origin, false );
				GetGrabController().m_attachedPositionObjectSpace = m_attachedPositionObjectSpace;
				GetGrabController().m_attachedAnglesPlayerSpace = m_attachedAnglesPlayerSpace;
			}
		}
	}
	else
	{
		if ( m_hOldAttachedObject && m_hOldAttachedObject->VPhysicsGetObject() )
		{
			GetGrabController().DetachEntity( false );

			m_hOldAttachedObject->VPhysicsDestroyObject();
		}
	}

	m_hOldAttachedObject = m_hAttachedObject;
}

#endif

#ifdef CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: Update the pose parameter for the gun
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::UpdateElementPosition( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	float flElementPosition = m_ElementParameter.Interp( gpGlobals->curtime );

	if ( ShouldDrawUsingViewModel() )
	{
		if ( pOwner != NULL )	
		{
			CBaseViewModel *vm = pOwner->GetViewModel();
			
			if ( vm != NULL )
			{
				vm->SetPoseParameter( "active", flElementPosition );
			}
		}
	}
	else
	{
		SetPoseParameter( "active", flElementPosition );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Think function for the client
//-----------------------------------------------------------------------------

void CWeaponPhysCannon::ClientThink( void )
{
	// Update our elements visually
	UpdateElementPosition();

	// Update our effects
	DoEffectIdle();
}

#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::ItemPreFrame()
{
	BaseClass::ItemPreFrame();

#ifdef CLIENT_DLL
	C_BasePlayer *localplayer = C_BasePlayer::GetLocalPlayer();

	if ( localplayer && !localplayer->IsObserver() )
		ManagePredictedObject();
#endif

	// Update the object if the weapon is switched on.
	if( m_bActive )
	{
		UpdateObject();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::CheckForTarget( void )
{
#ifndef CLIENT_DLL
	// misyl: Disable pred filtering in this server-only section.
	CDisablePredictionFiltering disablePred;

	//See if we're suppressing this
	if ( m_flCheckSuppressTime > gpGlobals->curtime )
		return;

	// holstered
	if ( IsEffectActive( EF_NODRAW ) )
		return;

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner == NULL )
		return;

	if ( m_bActive )
		return;

	trace_t	tr;
	FindObjectTrace( pOwner, &tr );

	if ( ( tr.fraction != 1.0f ) && ( tr.m_pEnt != NULL ) )
	{
		// FIXME: Try just having the elements always open when pointed at a physics object
		if ( CanPickupObject( tr.m_pEnt ) || Pickup_ForcePhysGunOpen( tr.m_pEnt, pOwner ) )
		// if ( ( tr.m_pEnt->VPhysicsGetObject() != NULL ) && ( tr.m_pEnt->GetMoveType() == MOVETYPE_VPHYSICS ) )
		{
			m_nChangeState = ELEMENT_STATE_NONE;
			OpenElements();
			return;
		}
	}

	// Close the elements after a delay to prevent overact state switching
	if ( ( m_flElementDebounce < gpGlobals->curtime ) && ( m_nChangeState == ELEMENT_STATE_NONE ) )
	{
		m_nChangeState = ELEMENT_STATE_CLOSED;
		m_flElementDebounce = gpGlobals->curtime + 0.5f;
	}
#endif
}


#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Begin upgrading!
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::BeginUpgrade()
{
	if ( IsMegaPhysCannon() )
		return;
	
	if ( m_bIsCurrentlyUpgrading )
		return;

	SetSequence( SelectWeightedSequence( ACT_PHYSCANNON_UPGRADE ) );
	ResetSequenceInfo();

	m_bIsCurrentlyUpgrading = true;

	SetContextThink( &CWeaponPhysCannon::WaitForUpgradeThink, gpGlobals->curtime + 6.0f, s_pWaitForUpgradeContext );

	EmitSound( "WeaponDissolve.Charge" );

	// Bloat our bounds
	CollisionProp()->UseTriggerBounds( true, 32.0f );

	// Turn on the new skin
	m_nSkin = MEGACANNON_SKIN;
}


//-----------------------------------------------------------------------------
// Wait until we're done upgrading
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::WaitForUpgradeThink()
{
	Assert( m_bIsCurrentlyUpgrading );

	StudioFrameAdvance();
	if ( !IsActivityFinished() )
	{
		SetContextThink( &CWeaponPhysCannon::WaitForUpgradeThink, gpGlobals->curtime + 0.1f, s_pWaitForUpgradeContext );
		return;
	}

	if ( !GlobalEntity_IsInTable( "super_phys_gun" ) )
	{
		GlobalEntity_Add( MAKE_STRING("super_phys_gun"), gpGlobals->mapname, GLOBAL_ON );
	}
	else
	{
		GlobalEntity_SetState( MAKE_STRING("super_phys_gun"), GLOBAL_ON );
	}
	m_bIsCurrentlyUpgrading = false;

	// This is necessary to get the effects to look different
	DestroyEffects();

	// HACK: Hacky notification back to the level that we've finish upgrading
	CBaseEntity *pEnt = gEntList.FindEntityByName( NULL, "script_physcannon_upgrade" );
	if ( pEnt )
	{
		variant_t emptyVariant;
		pEnt->AcceptInput( "Trigger", this, this, emptyVariant, 0 );
	}

	StopSound( "WeaponDissolve.Charge" );

	// Re-enable weapon pickup
	AddSolidFlags( FSOLID_TRIGGER );

	SetContextThink( NULL, gpGlobals->curtime, s_pWaitForUpgradeContext );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Idle effect (pulsing)
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DoEffectIdle( void )
{
#ifdef CLIENT_DLL

	StartEffects();

	//if ( ShouldDrawUsingViewModel() )
	{
		// Turn on the glow sprites
		for ( int i = PHYSCANNON_GLOW1; i < (PHYSCANNON_GLOW1+NUM_GLOW_SPRITES); i++ )
		{
			m_Parameters[i].GetScale().SetAbsolute( random->RandomFloat( 0.075f, 0.05f ) * SPRITE_SCALE );
			m_Parameters[i].GetAlpha().SetAbsolute( random->RandomInt( 24, 32 ) );
		}

		// Turn on the glow sprites
		for ( int i = PHYSCANNON_ENDCAP1; i < (PHYSCANNON_ENDCAP1+NUM_ENDCAP_SPRITES); i++ )
		{
			m_Parameters[i].GetScale().SetAbsolute( random->RandomFloat( 3, 5 ) );
			m_Parameters[i].GetAlpha().SetAbsolute( random->RandomInt( 200, 255 ) );
		}

		if ( m_EffectState != EFFECT_HOLDING )
		{
			// Turn beams off
			m_Beams[0].SetVisible( false );
			m_Beams[1].SetVisible( false );
			m_Beams[2].SetVisible( false );
		}
	}
	/*
	else
	{
		// Turn on the glow sprites
		for ( int i = PHYSCANNON_GLOW1; i < (PHYSCANNON_GLOW1+NUM_GLOW_SPRITES); i++ )
		{
			m_Parameters[i].GetScale().SetAbsolute( random->RandomFloat( 0.075f, 0.05f ) * SPRITE_SCALE );
			m_Parameters[i].GetAlpha().SetAbsolute( random->RandomInt( 24, 32 ) );
		}

		// Turn on the glow sprites
		for ( i = PHYSCANNON_ENDCAP1; i < (PHYSCANNON_ENDCAP1+NUM_ENDCAP_SPRITES); i++ )
		{
			m_Parameters[i].GetScale().SetAbsolute( random->RandomFloat( 3, 5 ) );
			m_Parameters[i].GetAlpha().SetAbsolute( random->RandomInt( 200, 255 ) );
		}
		
		if ( m_EffectState != EFFECT_HOLDING )
		{
			// Turn beams off
			m_Beams[0].SetVisible( false );
			m_Beams[1].SetVisible( false );
			m_Beams[2].SetVisible( false );
		}
	}
	*/
#endif
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::ItemPostFrame()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner == NULL )
	{
		// We found an object. Debounce the button
		m_nAttack2Debounce = 0;
		return;
	}

#ifdef MAPBASE
	if (pOwner->HasSpawnFlags( SF_PLAYER_SUPPRESS_FIRING ))
	{
		m_nAttack2Debounce = 0;
		WeaponIdle();
		return;
	}
#endif

	//Check for object in pickup range
	if ( m_bActive == false )
	{
		CheckForTarget();

		if ( ( m_flElementDebounce < gpGlobals->curtime ) && ( m_nChangeState != ELEMENT_STATE_NONE ) )
		{
			if ( m_nChangeState == ELEMENT_STATE_OPEN )
			{
				OpenElements();
			}
			else if ( m_nChangeState == ELEMENT_STATE_CLOSED )
			{
				CloseElements();
			}

			m_nChangeState = ELEMENT_STATE_NONE;
		}
	}

	// NOTE: Attack2 will be considered to be pressed until the first item is picked up.
	int nAttack2Mask = pOwner->m_nButtons & (~m_nAttack2Debounce);
	if ( nAttack2Mask & IN_ATTACK2 )
	{
		SecondaryAttack();
	}
	else
	{
		// Reset our debouncer
		m_flLastDenySoundPlayed = false;

		if ( m_bActive == false )
		{
			DoEffect( EFFECT_READY );
		}
	}
	
	if (( pOwner->m_nButtons & IN_ATTACK2 ) == 0 )
	{
		m_nAttack2Debounce = 0;
	}

	if ( pOwner->m_nButtons & IN_ATTACK )
	{
		PrimaryAttack();
	}
	else 
	{
		WeaponIdle();
	}

	if ( hl2_episodic.GetBool() == true )
	{
		if ( IsMegaPhysCannon() )
		{
			if ( !( pOwner->m_nButtons & IN_ATTACK ) )
			{
				m_flNextPrimaryAttack = gpGlobals->curtime;
			}
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#define PHYSCANNON_DANGER_SOUND_RADIUS 128

void CWeaponPhysCannon::LaunchObject( const Vector &vecDir, float flForce )
{
	CBaseEntity *pObject = m_grabController.GetAttached();

	if ( !(m_hLastPuntedObject == pObject && gpGlobals->curtime < m_flRepuntObjectTime) )
	{
		// FIRE!!!
		if( pObject != NULL )
		{
			DetachObject( false, true );

			m_hLastPuntedObject = pObject;
			m_flRepuntObjectTime = gpGlobals->curtime + 0.5f;

			// Launch
#ifdef CLIENT_DLL
			ApplyVelocityBasedForce( pObject, vecDir );
#else
			ApplyVelocityBasedForce( pObject, vecDir, PHYSGUN_FORCE_LAUNCHED );
#endif

			// Don't allow the gun to regrab a thrown object!!
			m_flNextSecondaryAttack = m_flNextPrimaryAttack = gpGlobals->curtime + 0.5;
			
			Vector	center = pObject->WorldSpaceCenter();

			//Do repulse effect
			DoEffect( EFFECT_LAUNCH, &center );

			m_hAttachedObject = NULL;
			m_bActive = false;
		}
	}

	// Stop our looping sound
	if ( GetMotorSound() )
	{
		(CSoundEnvelopeController::GetController()).SoundChangeVolume( GetMotorSound(), 0.0f, 1.0f );
		(CSoundEnvelopeController::GetController()).SoundChangePitch( GetMotorSound(), 50, 1.0f );
	}

	//Close the elements and suppress checking for a bit
	m_nChangeState = ELEMENT_STATE_CLOSED;
	m_flElementDebounce = gpGlobals->curtime + 0.1f;
	m_flCheckSuppressTime = gpGlobals->curtime + 0.25f;
}

bool UTIL_IsCombineBall( CBaseEntity *pEntity );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTarget - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponPhysCannon::CanPickupObject( CBaseEntity *pTarget )
{
#ifndef CLIENT_DLL
	// misyl: Disable pred filtering in this server-only section.
	CDisablePredictionFiltering disablePred;

	if ( pTarget == NULL )
		return false;

	if ( pTarget->GetBaseAnimating() && pTarget->GetBaseAnimating()->IsDissolving() )
		return false;

	if ( pTarget->HasSpawnFlags( SF_PHYSBOX_ALWAYS_PICK_UP ) || pTarget->HasSpawnFlags( SF_PHYSBOX_NEVER_PICK_UP ) )
	{
		// It may seem strange to check this spawnflag before we know the class of this object, since the 
		// spawnflag only applies to func_physbox, but it can act as a filter of sorts to reduce the number 
		// of irrelevant entities that fall through to this next casting check, which is slower.
		CPhysBox *pPhysBox = dynamic_cast<CPhysBox*>(pTarget);

		if ( pPhysBox != NULL )
		{
			if ( pTarget->HasSpawnFlags( SF_PHYSBOX_NEVER_PICK_UP ) )
                return false;
			else
				return true;
		}
	}

	if ( pTarget->HasSpawnFlags(SF_PHYSPROP_ALWAYS_PICK_UP) )
	{
		// It may seem strange to check this spawnflag before we know the class of this object, since the 
		// spawnflag only applies to func_physbox, but it can act as a filter of sorts to reduce the number 
		// of irrelevant entities that fall through to this next casting check, which is slower.
		CPhysicsProp *pPhysProp = dynamic_cast<CPhysicsProp*>(pTarget);
		if ( pPhysProp != NULL )
			return true;
	}

	if ( pTarget->IsEFlagSet( EFL_NO_PHYSCANNON_INTERACTION ) )
		return false;

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner && pOwner->GetGroundEntity() == pTarget )
		return false;

#ifdef MAPBASE
	// The gravity gun can't pick up vehicles.
	if ( pTarget->GetServerVehicle() )
		return false;
#endif
	IPhysicsObject *pObj = pTarget->VPhysicsGetObject();	

	if ( pObj && pObj->GetGameFlags() & FVPHYSICS_PLAYER_HELD )
		return false;

	if ( UTIL_IsCombineBall( pTarget ) )
	{
		return CBasePlayer::CanPickupObject( pTarget, 0, 0 );
	}

	if ( !IsMegaPhysCannon() )
	{
		if ( pTarget->VPhysicsIsFlesh( ) )
			return false;
		return CBasePlayer::CanPickupObject( pTarget, physcannon_maxmass.GetFloat(), 0 );
	}

	if ( pTarget->IsNPC() && pTarget->MyNPCPointer()->CanBecomeRagdoll() )
		return true;

	if ( dynamic_cast<CRagdollProp*>(pTarget) )
		return true;

	return CBasePlayer::CanPickupObject( pTarget, 0, 0 );
#else
	return false;
#endif
	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::OpenElements( void )
{
	if ( m_bOpen )
		return;

	WeaponSound( SPECIAL2 );

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner == NULL )
		return;

	SendWeaponAnim( ACT_VM_IDLE );

	m_bOpen = true;

	DoEffect( EFFECT_READY );

#ifdef CLIENT_DLL
	// Element prediction 
	m_ElementParameter.InitFromCurrent( 1.0f, 0.2f, INTERP_SPLINE );
	m_bOldOpen = true;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::CloseElements( void )
{
	// The mega cannon cannot be closed!
	if ( IsMegaPhysCannon() )
	{
		OpenElements();
		return;
	}

	if ( m_bOpen == false )
		return;

	WeaponSound( MELEE_HIT );

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner == NULL )
		return;

	SendWeaponAnim( ACT_VM_IDLE );

	m_bOpen = false;

	if ( GetMotorSound() )
	{
		(CSoundEnvelopeController::GetController()).SoundChangeVolume( GetMotorSound(), 0.0f, 1.0f );
		(CSoundEnvelopeController::GetController()).SoundChangePitch( GetMotorSound(), 50, 1.0f );
	}
	
	DoEffect( EFFECT_CLOSED );

#ifdef CLIENT_DLL
	// Element prediction 
	m_ElementParameter.InitFromCurrent( 0.0f, 0.5f, INTERP_SPLINE );
	m_bOldOpen = false;
#endif
}

#define	PHYSCANNON_MAX_MASS		500


//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CWeaponPhysCannon::GetLoadPercentage( void )
{
	float loadWeight = m_grabController.GetLoadWeight();
	loadWeight /= physcannon_maxmass.GetFloat();	
	loadWeight = clamp( loadWeight, 0.0f, 1.0f );
	return loadWeight;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : CSoundPatch
//-----------------------------------------------------------------------------
CSoundPatch *CWeaponPhysCannon::GetMotorSound( void )
{
#ifdef CLIENT_DLL
	if ( m_sndMotor == NULL )
	{
		//CPASAttenuationFilter filter( this );
		CLocalPlayerFilter filter;
		
		if ( IsMegaPhysCannon() )
		{
			m_sndMotor = (CSoundEnvelopeController::GetController()).SoundCreate( filter, entindex(), CHAN_STATIC, "Weapon_MegaPhysCannon.HoldSound", ATTN_NORM );
		}
		else
		{
			m_sndMotor = (CSoundEnvelopeController::GetController()).SoundCreate( filter, entindex(), CHAN_STATIC, "Weapon_PhysCannon.HoldSound", ATTN_NORM );
		}
	}
#endif

	return m_sndMotor;
}


//-----------------------------------------------------------------------------
// Shuts down sounds
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::StopLoopingSounds()
{
	if ( m_sndMotor != NULL )
	{
		 (CSoundEnvelopeController::GetController()).SoundDestroy( m_sndMotor );
		 m_sndMotor = NULL;
	}

#ifndef CLIENT_DLL
	BaseClass::StopLoopingSounds();
#endif
	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DestroyEffects( void )
{
#ifdef CLIENT_DLL

	// Free our beams
	m_Beams[0].Release();
	m_Beams[1].Release();
	m_Beams[2].Release();

#endif

	// Stop everything
	StopEffects();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::StopEffects( bool stopSound )
{
	// Turn off our effect state
	DoEffect( EFFECT_NONE );

	//Shut off sounds
	if ( stopSound && GetMotorSound() != NULL )
	{
		(CSoundEnvelopeController::GetController()).SoundFadeOut( GetMotorSound(), 0.1f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::StartEffects( void )
{
#ifdef CLIENT_DLL

	// ------------------------------------------
	// Core
	// ------------------------------------------

	if ( m_Parameters[PHYSCANNON_CORE].GetMaterial() == NULL )
	{
		m_Parameters[PHYSCANNON_CORE].GetScale().Init( 0.0f, 1.0f, 0.1f );
		m_Parameters[PHYSCANNON_CORE].GetAlpha().Init( 255.0f, 255.0f, 0.1f );
		m_Parameters[PHYSCANNON_CORE].SetAttachment( 1 );
		
		if ( m_Parameters[PHYSCANNON_CORE].SetMaterial( PHYSCANNON_CENTER_GLOW ) == false )
		{
			// This means the texture was not found
			Assert( 0 );
		}
	}

	// ------------------------------------------
	// Blast
	// ------------------------------------------

	if ( m_Parameters[PHYSCANNON_BLAST].GetMaterial() == NULL )
	{
		m_Parameters[PHYSCANNON_BLAST].GetScale().Init( 0.0f, 1.0f, 0.1f );
		m_Parameters[PHYSCANNON_BLAST].GetAlpha().Init( 255.0f, 255.0f, 0.1f );
		m_Parameters[PHYSCANNON_BLAST].SetAttachment( 1 );
		m_Parameters[PHYSCANNON_BLAST].SetVisible( false );
		
		if ( m_Parameters[PHYSCANNON_BLAST].SetMaterial( PHYSCANNON_BLAST_SPRITE ) == false )
		{
			// This means the texture was not found
			Assert( 0 );
		}
	}

	// ------------------------------------------
	// Glows
	// ------------------------------------------

	const char *attachNamesGlowThirdPerson[NUM_GLOW_SPRITES] = 
	{
		"fork1m",
		"fork1t",
		"fork2m",
		"fork2t",
		"fork3m",
		"fork3t",
	};

	const char *attachNamesGlow[NUM_GLOW_SPRITES] = 
	{
		"fork1b",
		"fork1m",
		"fork1t",
		"fork2b",
		"fork2m",
		"fork2t"
	};

	//Create the glow sprites
	for ( int i = PHYSCANNON_GLOW1; i < (PHYSCANNON_GLOW1+NUM_GLOW_SPRITES); i++ )
	{
		if ( m_Parameters[i].GetMaterial() != NULL )
			continue;

		m_Parameters[i].GetScale().SetAbsolute( 0.05f * SPRITE_SCALE );
		m_Parameters[i].GetAlpha().SetAbsolute( 64.0f );
		
		// Different for different views
		if ( ShouldDrawUsingViewModel() )
		{
			m_Parameters[i].SetAttachment( LookupAttachment( attachNamesGlow[i-PHYSCANNON_GLOW1] ) );
		}
		else
		{
			m_Parameters[i].SetAttachment( LookupAttachment( attachNamesGlowThirdPerson[i-PHYSCANNON_GLOW1] ) );
		}
		m_Parameters[i].SetColor( Vector( 255, 128, 0 ) );
		
		if ( m_Parameters[i].SetMaterial( PHYSCANNON_GLOW_SPRITE ) == false )
		{
			// This means the texture was not found
			Assert( 0 );
		}
	}

	// ------------------------------------------
	// End caps
	// ------------------------------------------

	const char *attachNamesEndCap[NUM_ENDCAP_SPRITES] = 
	{
		"fork1t",
		"fork2t",
		"fork3t"
	};
	
	//Create the glow sprites
	for ( int i = PHYSCANNON_ENDCAP1; i < (PHYSCANNON_ENDCAP1+NUM_ENDCAP_SPRITES); i++ )
	{
		if ( m_Parameters[i].GetMaterial() != NULL )
			continue;

		m_Parameters[i].GetScale().SetAbsolute( 0.05f * SPRITE_SCALE );
		m_Parameters[i].GetAlpha().SetAbsolute( 255.0f );
		m_Parameters[i].SetAttachment( LookupAttachment( attachNamesEndCap[i-PHYSCANNON_ENDCAP1] ) );
		m_Parameters[i].SetVisible( false );
		
		if ( m_Parameters[i].SetMaterial( PHYSCANNON_ENDCAP_SPRITE ) == false )
		{
			// This means the texture was not found
			Assert( 0 );
		}
	}

#endif
}

//-----------------------------------------------------------------------------
// Purpose: Closing effects
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DoEffectClosed( void )
{
#ifdef CLIENT_DLL

	// Turn off the end-caps
	for ( int i = PHYSCANNON_ENDCAP1; i < (PHYSCANNON_ENDCAP1+NUM_ENDCAP_SPRITES); i++ )
	{
		m_Parameters[i].SetVisible( false );
	}
	
#endif
}

//-----------------------------------------------------------------------------
// Closing effects
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DoMegaEffectClosed( void )
{
	// TODO
}

//-----------------------------------------------------------------------------
// Purpose: Ready effects
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DoEffectReady( void )
{
#ifdef CLIENT_DLL

	// Special POV case
	if ( ShouldDrawUsingViewModel() )
	{
		//Turn on the center sprite
		m_Parameters[PHYSCANNON_CORE].GetScale().InitFromCurrent( 14.0f, 0.2f );
		m_Parameters[PHYSCANNON_CORE].GetAlpha().InitFromCurrent( 128.0f, 0.2f );
		m_Parameters[PHYSCANNON_CORE].SetVisible();
	}
	else
	{
		//Turn off the center sprite
		m_Parameters[PHYSCANNON_CORE].GetScale().InitFromCurrent( 8.0f, 0.2f );
		m_Parameters[PHYSCANNON_CORE].GetAlpha().InitFromCurrent( 0.0f, 0.2f );
		m_Parameters[PHYSCANNON_CORE].SetVisible();
	}

	// Turn on the glow sprites
	for ( int i = PHYSCANNON_GLOW1; i < (PHYSCANNON_GLOW1+NUM_GLOW_SPRITES); i++ )
	{
		m_Parameters[i].GetScale().InitFromCurrent( 0.4f * SPRITE_SCALE, 0.2f );
		m_Parameters[i].GetAlpha().InitFromCurrent( 64.0f, 0.2f );
		m_Parameters[i].SetVisible();
	}

	// Turn on the glow sprites
	for ( int i = PHYSCANNON_ENDCAP1; i < (PHYSCANNON_ENDCAP1+NUM_ENDCAP_SPRITES); i++ )
	{
		m_Parameters[i].SetVisible( false );
	}

	// Stop our looping sound
	if ( GetMotorSound() )
	{
		( CSoundEnvelopeController::GetController() ).SoundChangeVolume( GetMotorSound(), 0.0f, 1.0f );
		( CSoundEnvelopeController::GetController() ).SoundChangePitch( GetMotorSound(), 50, 1.0f );
	}

#endif
}


//-----------------------------------------------------------------------------
// Holding effects
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DoEffectHolding( void )
{
#ifdef CLIENT_DLL

	if ( ShouldDrawUsingViewModel() )
	{
		// Scale up the center sprite
		m_Parameters[PHYSCANNON_CORE].GetScale().InitFromCurrent( 16.0f, 0.2f );
		m_Parameters[PHYSCANNON_CORE].GetAlpha().InitFromCurrent( 255.0f, 0.1f );
		m_Parameters[PHYSCANNON_CORE].SetVisible();

		// Prepare for scale up
		m_Parameters[PHYSCANNON_BLAST].SetVisible( false );

		// Turn on the glow sprites
		for ( int i = PHYSCANNON_GLOW1; i < (PHYSCANNON_GLOW1+NUM_GLOW_SPRITES); i++ )
		{
			m_Parameters[i].GetScale().InitFromCurrent( 0.5f * SPRITE_SCALE, 0.2f );
			m_Parameters[i].GetAlpha().InitFromCurrent( 64.0f, 0.2f );
			m_Parameters[i].SetVisible();
		}

		// Turn on the glow sprites
		// NOTE: The last glow is left off for first-person
		for ( int i = PHYSCANNON_ENDCAP1; i < (PHYSCANNON_ENDCAP1+NUM_ENDCAP_SPRITES-1); i++ )
		{
			m_Parameters[i].SetVisible();
		}

		// Create our beams
		CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
		CBaseEntity *pBeamEnt = pOwner->GetViewModel();

		// Setup the beams
		m_Beams[0].Init( LookupAttachment( "fork1t" ), 1, pBeamEnt, true );
		m_Beams[1].Init( LookupAttachment( "fork2t" ), 1, pBeamEnt, true );

		// Set them visible
		m_Beams[0].SetVisible();
		m_Beams[1].SetVisible();
	}
	else
	{
		// Scale up the center sprite
		m_Parameters[PHYSCANNON_CORE].GetScale().InitFromCurrent( 14.0f, 0.2f );
		m_Parameters[PHYSCANNON_CORE].GetAlpha().InitFromCurrent( 255.0f, 0.1f );
		m_Parameters[PHYSCANNON_CORE].SetVisible();

		// Prepare for scale up
		m_Parameters[PHYSCANNON_BLAST].SetVisible( false );

		// Turn on the glow sprites
		for ( int i = PHYSCANNON_GLOW1; i < (PHYSCANNON_GLOW1+NUM_GLOW_SPRITES); i++ )
		{
			m_Parameters[i].GetScale().InitFromCurrent( 0.5f * SPRITE_SCALE, 0.2f );
			m_Parameters[i].GetAlpha().InitFromCurrent( 64.0f, 0.2f );
			m_Parameters[i].SetVisible();
		}

		// Turn on the glow sprites
		for ( int i = PHYSCANNON_ENDCAP1; i < (PHYSCANNON_ENDCAP1+NUM_ENDCAP_SPRITES); i++ )
		{
			m_Parameters[i].SetVisible();
		}

		// Setup the beams
		m_Beams[0].Init( LookupAttachment( "fork1t" ), 1, this, false );
		m_Beams[1].Init( LookupAttachment( "fork2t" ), 1, this, false );
		m_Beams[2].Init( LookupAttachment( "fork3t" ), 1, this, false );

		// Set them visible
		m_Beams[0].SetVisible();
		m_Beams[1].SetVisible();
		m_Beams[2].SetVisible();
	}

	if ( m_bOpen )
	{
		if ( GetMotorSound() )
		{
			( CSoundEnvelopeController::GetController() ).Play( GetMotorSound(), 0.0f, 50 );
			( CSoundEnvelopeController::GetController() ).SoundChangePitch( GetMotorSound(), 100, 0.5f );
			( CSoundEnvelopeController::GetController() ).SoundChangeVolume( GetMotorSound(), 0.8f, 0.5f );
		}
	}

#endif
}


//-----------------------------------------------------------------------------
// Launch effects
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DoEffectLaunch( Vector *pos )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner == NULL )
		return;

	Vector	endPos;
	Vector	shotDir;

	// See if we need to predict this position
	if ( pos == NULL )
	{
		// Hit an entity if we're holding one
		if ( m_hAttachedObject )
		{
			endPos = m_hAttachedObject->WorldSpaceCenter();
			
			shotDir = endPos - pOwner->Weapon_ShootPosition();
			VectorNormalize( shotDir );
		}
		else
		{
			// Otherwise try and find the right spot
			endPos = pOwner->Weapon_ShootPosition();
			pOwner->EyeVectors( &shotDir );

			trace_t	tr;
			UTIL_TraceLine( endPos, endPos + ( shotDir * MAX_TRACE_LENGTH ), MASK_SHOT, pOwner, COLLISION_GROUP_NONE, &tr );
			
			endPos = tr.endpos;
			shotDir = endPos - pOwner->Weapon_ShootPosition();
			VectorNormalize( shotDir );
		}
	}
	else
	{
		// Use what is supplied
		endPos = *pos;
		shotDir = ( endPos - pOwner->Weapon_ShootPosition() );
		VectorNormalize( shotDir );
	}

	// End hit
	CPVSFilter filter( endPos );

	// Don't send this to the owning player, they already had it predicted
	if ( IsPredicted() )
	{
		filter.UsePredictionRules();
	}

	// Do an impact hit
	CEffectData	data;
	data.m_vOrigin = endPos;
#ifdef CLIENT_DLL
	data.m_hEntity = GetRefEHandle();
#else
	data.m_nEntIndex = entindex();
#endif

	te->DispatchEffect( filter, 0.0, data.m_vOrigin, "PhyscannonImpact", data );

#ifdef CLIENT_DLL

	//Turn on the blast sprite and scale
	m_Parameters[PHYSCANNON_BLAST].GetScale().Init( 8.0f, 64.0f, 0.1f );
	m_Parameters[PHYSCANNON_BLAST].GetAlpha().Init( 255.0f, 0.0f, 0.2f );
	m_Parameters[PHYSCANNON_BLAST].SetVisible();

#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pos - 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DoMegaEffectLaunch( Vector *pos )
{
	// TODO
}

//-----------------------------------------------------------------------------
// Holding effects
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DoMegaEffectHolding( void )
{
	// TODO
}

//-----------------------------------------------------------------------------
// Ready effects
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DoMegaEffectReady( void )
{
	// TODO
}

//-----------------------------------------------------------------------------
// Purpose: Shutdown for the weapon when it's holstered
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DoEffectNone( void )
{
#ifdef CLIENT_DLL

	//Turn off main glows
	m_Parameters[PHYSCANNON_CORE].SetVisible( false );
	m_Parameters[PHYSCANNON_BLAST].SetVisible( false );

	for ( int i = PHYSCANNON_GLOW1; i < (PHYSCANNON_GLOW1+NUM_GLOW_SPRITES); i++ )
	{
		m_Parameters[i].SetVisible( false );
	}

	// Turn on the glow sprites
	for ( int i = PHYSCANNON_ENDCAP1; i < (PHYSCANNON_ENDCAP1+NUM_ENDCAP_SPRITES); i++ )
	{
		m_Parameters[i].SetVisible( false );
	}

	m_Beams[0].SetVisible( false );
	m_Beams[1].SetVisible( false );
	m_Beams[2].SetVisible( false );

	if ( GetMotorSound() )
	{
		( CSoundEnvelopeController::GetController() ).SoundFadeOut( GetMotorSound(), 0.1f );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : effectType - 
//			*pos - 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DoMegaEffect( int effectType, Vector *pos )
{
	switch( effectType )
	{
	case EFFECT_CLOSED:
		DoMegaEffectClosed();
		break;

	case EFFECT_READY:
		DoMegaEffectReady();
		break;

	case EFFECT_HOLDING:
		DoMegaEffectHolding();
		break;

	case EFFECT_LAUNCH:
		DoMegaEffectLaunch( pos );
		break;

	default:
	case EFFECT_NONE:
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : effectType - 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DoEffect( int effectType, Vector *pos )
{
	// Make sure we're active
	StartEffects();

	m_EffectState = effectType;

#ifdef CLIENT_DLL
	// Save predicted state
	m_nOldEffectState = m_EffectState;
#endif

	// Do different effects when upgraded
	if ( IsMegaPhysCannon() )
	{
		DoMegaEffect( effectType, pos );
		return;
	}

	switch( effectType )
	{
	case EFFECT_CLOSED:
		DoEffectClosed( );
		break;

	case EFFECT_READY:
		DoEffectReady( );
		break;

	case EFFECT_HOLDING:
		DoEffectHolding();
		break;

	case EFFECT_LAUNCH:
		DoEffectLaunch( pos );
		break;

	default:
	case EFFECT_NONE:
		DoEffectNone();
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iIndex - 
// Output : const char
//-----------------------------------------------------------------------------
const char *CWeaponPhysCannon::GetShootSound( int iIndex ) const
{
	// Just do this normally if we're a normal physcannon
	if ( PlayerHasMegaPhysCannon() == false )
		return BaseClass::GetShootSound( iIndex );

	// We override this if we're the charged up version
	switch( iIndex )
	{
	case EMPTY:
		return "Weapon_MegaPhysCannon.DryFire";
		break;

	case SINGLE:
		return "Weapon_MegaPhysCannon.Launch";
		break;

	case SPECIAL1:
		return "Weapon_MegaPhysCannon.Pickup";
		break;

	case MELEE_MISS:
		return "Weapon_MegaPhysCannon.Drop";
		break;

	default:
		break;
	}

	return BaseClass::GetShootSound( iIndex );
}

#ifdef CLIENT_DLL

extern void FormatViewModelAttachment( Vector &vOrigin, bool bInverse );

//-----------------------------------------------------------------------------
// Purpose: Gets the complete list of values needed to render an effect from an
//			effect parameter
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::GetEffectParameters( EffectType_t effectID, color32 &color, float &scale, IMaterial **pMaterial, Vector &vecAttachment )
{
	const float dt = gpGlobals->curtime;

	// Get alpha
	float alpha = m_Parameters[effectID].GetAlpha().Interp( dt );
	
	// Get scale
	scale = m_Parameters[effectID].GetScale().Interp( dt );
	
	// Get material
	*pMaterial = (IMaterial *) m_Parameters[effectID].GetMaterial();

	// Setup the color
	color.r = (int) m_Parameters[effectID].GetColor().x;
	color.g = (int) m_Parameters[effectID].GetColor().y;
	color.b = (int) m_Parameters[effectID].GetColor().z;
	color.a = (int) alpha;

	// Setup the attachment
	int		attachment = m_Parameters[effectID].GetAttachment();
	QAngle	angles;

	// Format for first-person
	if ( ShouldDrawUsingViewModel() )
	{
		CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
		
		if ( pOwner != NULL )
		{
			pOwner->GetViewModel()->GetAttachment( attachment, vecAttachment, angles );
			::FormatViewModelAttachment( vecAttachment, true );
		}
	}
	else
	{
		GetAttachment( attachment, vecAttachment, angles );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Whether or not an effect is set to display
//-----------------------------------------------------------------------------
bool CWeaponPhysCannon::IsEffectVisible( EffectType_t effectID )
{
	return m_Parameters[effectID].IsVisible();
}

//-----------------------------------------------------------------------------
// Purpose: Draws the effect sprite, given an effect parameter ID
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DrawEffectSprite( EffectType_t effectID )
{
	color32 color;
	float scale;
	IMaterial *pMaterial;
	Vector	vecAttachment;

	// Don't draw invisible effects
	if ( IsEffectVisible( effectID ) == false )
		return;

	// Get all of our parameters
	GetEffectParameters( effectID, color, scale, &pMaterial, vecAttachment );

	// Msg( "Scale: %.2f\tAlpha: %.2f\n", scale, alpha );

	// Don't render fully translucent objects
	if ( color.a <= 0.0f )
		return;

	// Draw the sprite
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( pMaterial, this );
	DrawSprite( vecAttachment, scale, scale, color );
}

//-----------------------------------------------------------------------------
// Purpose: Render our third-person effects
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DrawEffects( void )
{
#ifdef MAPBASE
	if ( !ShouldDrawUsingViewModel() )
	{
		// Don't duplicate the effects
		// (this can happen when drawing externally)
		C_BasePlayer *pPlayer = ToBasePlayer( GetOwner() );
		if ( pPlayer && pPlayer->IsLocalPlayer() && pPlayer->InFirstPersonView())
			return;
	}
#endif

	// Draw the core effects
	DrawEffectSprite( PHYSCANNON_CORE );
	DrawEffectSprite( PHYSCANNON_BLAST );
	
	// Draw the glows
	for ( int i = PHYSCANNON_GLOW1; i < (PHYSCANNON_GLOW1+NUM_GLOW_SPRITES); i++ )
	{
		DrawEffectSprite( (EffectType_t) i );
	}

	// Draw the endcaps
	for ( int i = PHYSCANNON_ENDCAP1; i < (PHYSCANNON_ENDCAP1+NUM_ENDCAP_SPRITES); i++ )
	{
		DrawEffectSprite( (EffectType_t) i );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Third-person function call to render world model
//-----------------------------------------------------------------------------
int CWeaponPhysCannon::DrawModel( int flags )
{
	// Only render these on the transparent pass
	if ( flags & STUDIO_TRANSPARENCY )
	{
		DrawEffects();
		return 1;
	}

	// Only do this on the opaque pass
	return BaseClass::DrawModel( flags );
}

//-----------------------------------------------------------------------------
// Purpose: First-person function call after viewmodel has been drawn
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::ViewModelDrawn( C_BaseViewModel *pBaseViewModel )
{
	// Render our effects
	DrawEffects();

	// Pass this back up
	BaseClass::ViewModelDrawn( pBaseViewModel );
}

//-----------------------------------------------------------------------------
// Purpose: We are always considered transparent
//-----------------------------------------------------------------------------
bool CWeaponPhysCannon::IsTransparent( void )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::NotifyShouldTransmit( ShouldTransmitState_t state )
{
	BaseClass::NotifyShouldTransmit(state);

	if ( state == SHOULDTRANSMIT_END )
	{
		DoEffect( EFFECT_NONE );
	}
}

#endif

//-----------------------------------------------------------------------------
// EXTERNAL API
//-----------------------------------------------------------------------------
void PhysCannonForceDrop( CBaseCombatWeapon *pActiveWeapon, CBaseEntity *pOnlyIfHoldingThis )
{
	CWeaponPhysCannon *pCannon = dynamic_cast<CWeaponPhysCannon *>(pActiveWeapon);
	if ( pCannon )
	{
		if ( pOnlyIfHoldingThis )
		{
			pCannon->DropIfEntityHeld( pOnlyIfHoldingThis );
		}
		else
		{
			pCannon->ForceDrop();
		}
	}
}

#ifndef CLIENT_DLL
void PhysCannonBeginUpgrade( CBaseAnimating *pAnim )
{
	CWeaponPhysCannon *pWeaponPhyscannon = assert_cast<	CWeaponPhysCannon* >( pAnim );
	pWeaponPhyscannon->BeginUpgrade();
}
#endif


bool PlayerPickupControllerIsHoldingEntity( CBaseEntity *pPickupControllerEntity, CBaseEntity *pHeldEntity )
{
	CPlayerPickupController *pController = dynamic_cast<CPlayerPickupController *>(pPickupControllerEntity);

	return pController ? pController->IsHoldingEntity( pHeldEntity ) : false;
}


float PhysCannonGetHeldObjectMass( CBaseCombatWeapon *pActiveWeapon, IPhysicsObject *pHeldObject )
{
	float mass = 0.0f;
	CWeaponPhysCannon *pCannon = dynamic_cast<CWeaponPhysCannon *>(pActiveWeapon);
	if ( pCannon )
	{
		CGrabController &grab = pCannon->GetGrabController();
		mass = grab.GetSavedMass( pHeldObject );
	}

	return mass;
}

CBaseEntity *PhysCannonGetHeldEntity( CBaseCombatWeapon *pActiveWeapon )
{
	CWeaponPhysCannon *pCannon = dynamic_cast<CWeaponPhysCannon *>(pActiveWeapon);
	if ( pCannon )
	{
		CGrabController &grab = pCannon->GetGrabController();
		return grab.GetAttached();
	}

	return NULL;
}

CBaseEntity *GetPlayerHeldEntity( CBasePlayer *pPlayer )
{
	CBaseEntity *pObject = NULL;
	CPlayerPickupController *pPlayerPickupController = (CPlayerPickupController *)(pPlayer->GetUseEntity());

	if ( pPlayerPickupController )
	{
		pObject = pPlayerPickupController->GetGrabController().GetAttached();
	}

	return pObject;
}

bool PhysCannonAccountableForObject( CBaseCombatWeapon *pPhysCannon, CBaseEntity *pObject )
{
	return false;
}

float PlayerPickupGetHeldObjectMass( CBaseEntity *pPickupControllerEntity, IPhysicsObject *pHeldObject )
{
	float mass = 0.0f;
	CPlayerPickupController *pController = dynamic_cast<CPlayerPickupController *>(pPickupControllerEntity);
	if ( pController )
	{
		CGrabController &grab = pController->GetGrabController();
		mass = grab.GetSavedMass( pHeldObject );
	}
	return mass;
}

#ifdef CLIENT_DLL

extern void FX_GaussExplosion( const Vector &pos, const Vector &dir, int type );

void CallbackPhyscannonImpact( const CEffectData &data )
{
	C_BaseEntity *pEnt = data.GetEntity();
	if ( pEnt == NULL )
		return;

	Vector	vecAttachment;
	QAngle	vecAngles;

	C_BaseCombatWeapon *pWeapon = dynamic_cast<C_BaseCombatWeapon *>(pEnt);

	if ( pWeapon == NULL )
		return;

	pWeapon->GetAttachment( 1, vecAttachment, vecAngles );

	Vector	dir = ( data.m_vOrigin - vecAttachment );
	VectorNormalize( dir );

	// Do special first-person fix-up
	if ( pWeapon->GetOwner() == CBasePlayer::GetLocalPlayer() )
	{
		// Translate the attachment entity to the viewmodel
		C_BasePlayer *pPlayer = dynamic_cast<C_BasePlayer *>(pWeapon->GetOwner());

		if ( pPlayer )
		{
			pEnt = pPlayer->GetViewModel();
		}

		// Format attachment for first-person view!
		::FormatViewModelAttachment( vecAttachment, true );

		// Explosions at the impact point
		FX_GaussExplosion( data.m_vOrigin, -dir, 0 );

		// Draw a beam
		BeamInfo_t beamInfo;

		beamInfo.m_pStartEnt = pEnt;
		beamInfo.m_nStartAttachment = 1;
		beamInfo.m_pEndEnt = NULL;
		beamInfo.m_nEndAttachment = -1;
		beamInfo.m_vecStart = vec3_origin;
		beamInfo.m_vecEnd = data.m_vOrigin;
		beamInfo.m_pszModelName = PHYSCANNON_BEAM_SPRITE;
		beamInfo.m_flHaloScale = 0.0f;
		beamInfo.m_flLife = 0.1f;
		beamInfo.m_flWidth = 12.0f;
		beamInfo.m_flEndWidth = 4.0f;
		beamInfo.m_flFadeLength = 0.0f;
		beamInfo.m_flAmplitude = 0;
		beamInfo.m_flBrightness = 255.0;
		beamInfo.m_flSpeed = 0.0f;
		beamInfo.m_nStartFrame = 0.0;
		beamInfo.m_flFrameRate = 30.0;
		beamInfo.m_flRed = 255.0;
		beamInfo.m_flGreen = 255.0;
		beamInfo.m_flBlue = 255.0;
		beamInfo.m_nSegments = 16;
		beamInfo.m_bRenderable = true;
		beamInfo.m_nFlags = FBEAM_ONLYNOISEONCE;

		beams->CreateBeamEntPoint( beamInfo );
	}
	else
	{
		// Explosion at the starting point
		FX_GaussExplosion( vecAttachment, dir, 0 );
	}
}

DECLARE_CLIENT_EFFECT( "PhyscannonImpact", CallbackPhyscannonImpact );

#endif
