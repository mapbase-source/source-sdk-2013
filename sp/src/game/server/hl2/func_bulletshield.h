//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef FUNC_BULLETSHIELD_H
#define FUNC_BULLETSHIELD_H
#ifdef _WIN32
#pragma once
#endif

#include "modelentities.h"

//-----------------------------------------------------------------------------
// Purpose: shield that stops bullets, but not other objects
// enabled state:	brush is visible
// disabled staute:	brush not visible
//-----------------------------------------------------------------------------
class CFuncBulletShield : public CFuncBrush
{
public:
	DECLARE_CLASS( CFuncBulletShield, CFuncBrush );
	DECLARE_DATADESC();

	virtual void Spawn( void );
	
	bool TestCollision( const Ray_t &ray, unsigned int mask, trace_t& trace );
};

#endif // MODELENTITIES_H
