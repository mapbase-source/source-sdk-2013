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

#include "cbase.h"
#include <KeyValues.h>
#include "hl2_weapon_parse.h"


#ifndef HL2MP
FileWeaponInfo_t* CreateWeaponInfo()
{
	return new CHL2WeaponInfo;
}
#endif


CHL2WeaponInfo::CHL2WeaponInfo()
{
#ifdef MAPBASE
	m_flBobScale = 1.0f;
	m_flSwayScale = 1.0f;
	m_flSwaySpeedScale = 1.0f;
	m_bUsesHands = false;
	m_nWeaponRestriction = WPNRESTRICT_NONE;
#endif
}

#ifdef MAPBASE
const char* pWeaponRestrictions[NUM_WEAPON_RESTRICTION_TYPES] = {
	"none",
	"player_only",
	"npc_only",
};
#endif // MAPBASE


void CHL2WeaponInfo::Parse( KeyValues *pKeyValuesData, const char *szWeaponName )
{
	BaseClass::Parse( pKeyValuesData, szWeaponName );

#ifdef MAPBASE
	m_flBobScale = pKeyValuesData->GetFloat( "bob_scale", 1.0f );
	m_flSwayScale = pKeyValuesData->GetFloat( "sway_scale", 1.0f );
	m_flSwaySpeedScale = pKeyValuesData->GetFloat( "sway_speed_scale", 1.0f );

	m_bUsesHands = ( pKeyValuesData->GetInt( "uses_hands", 0 ) != 0 ) ? true : false;

	const char* pszRestrictString = pKeyValuesData->GetString("usage_restriction", nullptr);
	if (pszRestrictString)
	{
		for (int i = 0; i < NUM_WEAPON_RESTRICTION_TYPES; i++)
		{
			if (V_stricmp(pszRestrictString, pWeaponRestrictions[i]) == 0)
			{
				m_nWeaponRestriction = i;
				break;
			}
		}
	}
#endif
}


