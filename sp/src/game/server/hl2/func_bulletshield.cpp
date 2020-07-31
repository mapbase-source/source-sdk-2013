//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "cbase.h"
#include "entityoutput.h"
#include "ndebugoverlay.h"
#include "func_bulletshield.h"
#include "collisionutils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( func_bulletshield, CFuncBulletShield );

BEGIN_DATADESC( CFuncBulletShield )
END_DATADESC()

void CFuncBulletShield::Spawn( void )
{
	BaseClass::Spawn();

	AddSolidFlags( FSOLID_CUSTOMRAYTEST );
	AddSolidFlags( FSOLID_CUSTOMBOXTEST );

	VPhysicsDestroyObject();
}

bool CFuncBulletShield::TestCollision( const Ray_t &ray, unsigned int mask, trace_t& trace )
{
	// ignore unless a shot
	if ( ( mask & MASK_SHOT ) == MASK_SHOT )
	{
		// use obb collision
		ICollideable *pCol = GetCollideable();
		Assert( pCol );

		return IntersectRayWithOBB( ray,pCol->GetCollisionOrigin(),pCol->GetCollisionAngles(),
			pCol->OBBMins(),pCol->OBBMaxs(),1.0f,&trace );
	}
	else
		return false;
}
