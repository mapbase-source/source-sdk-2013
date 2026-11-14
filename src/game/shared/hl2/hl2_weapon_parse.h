//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose:		HL2's custom weapon data
//
//				(Added by Mapbase for generalized use; most code will act as though
//				this is a default feature)
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef HL2_WEAPON_PARSE_H
#define HL2_WEAPON_PARSE_H
#ifdef _WIN32
#pragma once
#endif


#include "weapon_parse.h"
#include "networkvar.h"


#ifdef MAPBASE
// ------------------------------------
// Explicit weapon usage permissions
// ------------------------------------
enum WeaponUsageRestricions_e
{
	WPNRESTRICT_NONE = 0,
	WPNRESTRICT_PLAYER_ONLY,
	WPNRESTRICT_NPCS_ONLY,

	NUM_WEAPON_RESTRICTION_TYPES
};

// ------------------------------------
// HL2 weapon classes
// ------------------------------------
enum WeaponClass_t
{
	WEPCLASS_INVALID = 0,

	WEPCLASS_HANDGUN,
	WEPCLASS_RIFLE,
	WEPCLASS_SHOTGUN,
	WEPCLASS_HEAVY,

	WEPCLASS_MELEE,
};
#endif // MAPBASE

//--------------------------------------------------------------------------------------------------------
class CHL2WeaponInfo : public FileWeaponInfo_t
{
public:
	DECLARE_CLASS_GAMEROOT( CHL2WeaponInfo, FileWeaponInfo_t );
	
	CHL2WeaponInfo();
	
	virtual void Parse( ::KeyValues *pKeyValuesData, const char *szWeaponName );

#ifdef MAPBASE
	float		m_flBobScale;
	float		m_flSwayScale;
	float		m_flSwaySpeedScale;

	bool		m_bUsesHands;
	//int		m_nHandRig;		// If you need to use multiple viewmodel rigs, consider recompiling using $definebone fixup params

	byte		m_nWeaponRestriction;
#endif
};


#endif // SDK_WEAPON_PARSE_H
