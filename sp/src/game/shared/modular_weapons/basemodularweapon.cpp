//------------------------------------------------------------------------------------------//
//	Base Modular Weapon
// 
//	This is the base for making modular weapons which can change attachemnts on them
//	
//  Author: Nbc66
//	Last Modified: 2025-2-24
//------------------------------------------------------------------------------------------//

#include "cbase.h"

#include "basemodularweapon.h"
#include "saverestore_utlmap.h"

#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED(BaseModularWeapon, DT_BaseModularWeapon)

BEGIN_NETWORK_TABLE(CBaseModularWeapon, DT_BaseModularWeapon)
#ifdef GAME_DLL
SendPropExclude("DT_AnimTimeMustBeFirst", "m_flAnimTime"),
SendPropExclude("DT_BaseAnimating", "m_nSequence"),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA(CBaseModularWeapon)
DEFINE_PRED_FIELD(m_flTimeWeaponIdle, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK),
END_PREDICTION_DATA()
#endif

//LINK_ENTITY_TO_CLASS(weapon_base, CBaseModularWeapon);

#ifdef GAME_DLL
BEGIN_DATADESC(CBaseModularWeapon)
END_DATADESC()
#endif

CBaseModularWeapon::CBaseModularWeapon()
{
}

CBaseModularWeapon::~CBaseModularWeapon()
{
}

void CBaseModularWeapon::EquipAttachment(CBaseWeaponAttachment* pAttachment)
{
	return; //TODO: Implement
}

void CBaseModularWeapon::RemoveAttachment(AttachmentType_t type)
{
	return; //TODO: Implement
}

float CBaseModularWeapon::GetFireRate()
{
	return BaseClass::GetFireRate();
}

float CBaseModularWeapon::GetDamage()
{
	return 0.0f;
}

float CBaseModularWeapon::GetSpread()
{
	return 0.0f;
}
