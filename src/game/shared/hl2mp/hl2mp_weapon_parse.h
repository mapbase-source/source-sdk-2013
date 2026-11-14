//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef HL2MP_WEAPON_PARSE_H
#define HL2MP_WEAPON_PARSE_H
#ifdef _WIN32
#pragma once
#endif


#include "hl2_weapon_parse.h"
#include "networkvar.h"


//--------------------------------------------------------------------------------------------------------
class CHL2MPSWeaponInfo : public CHL2WeaponInfo
{
public:
	DECLARE_CLASS_GAMEROOT( CHL2MPSWeaponInfo, CHL2WeaponInfo );
	
	CHL2MPSWeaponInfo();
	
	virtual void Parse( ::KeyValues *pKeyValuesData, const char *szWeaponName );


public:

	int m_iPlayerDamage;
};


#endif // HL2MP_WEAPON_PARSE_H
