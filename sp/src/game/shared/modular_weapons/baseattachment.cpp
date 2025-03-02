#include "cbase.h"
#include "baseattachment.h"
#include "basemodularweapon.h"  // Include the full definition here

#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED(BaseWeaponAttachment, DT_BaseWeaponAttachment)

BEGIN_NETWORK_TABLE(CBaseWeaponAttachment, DT_BaseWeaponAttachment)
#ifdef GAME_DLL
#endif
#ifdef CLIENT_DLL
#endif // CLIENT_DLL
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA(CBaseWeaponAttachment)
//DEFINE_PRED_FIELD(m_flTimeWeaponIdle, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK),
END_PREDICTION_DATA()
#endif

//LINK_ENTITY_TO_CLASS(weapon_base, CBaseModularWeapon);

#ifdef GAME_DLL
BEGIN_DATADESC(CBaseWeaponAttachment)
END_DATADESC()
#endif

bool CBaseWeaponAttachment::IsCompatibleWithWeapon(CBaseModularWeapon* pWeapon)
{
    for (int i = 0; i < m_CompatibleWeapons.Count(); i++)
    {
        if (!V_strcmp(m_CompatibleWeapons.Element(i), pWeapon->GetClassname()))
        {
            return true;
        }
    }

    return false;
}

bool CBaseWeaponAttachment::IsCompatibleWithWeapon(const char* WeaponClassName)
{
    for (int i = 0; i < m_CompatibleWeapons.Count(); i++)
    {
        if (!V_strcmp(m_CompatibleWeapons.Element(i), WeaponClassName))
        {
            return true;
        }
    }

    return false;
}

void CBaseWeaponAttachment::AddCompatibleWeapon(CBaseModularWeapon* pWeapon)
{
    //no need to add duplicates
    if (!IsDuplicate(m_CompatibleWeapons, pWeapon->GetClassname()))
    {
        AddWeapon(pWeapon->GetClassname());
    }
}

void CBaseWeaponAttachment::AddCompatibleWeapon(const char* szWeaponClassName)
{
    //no need to add duplicates
    if (!IsDuplicate(m_CompatibleWeapons, szWeaponClassName))
    {
        AddWeapon(szWeaponClassName);
    }
}

template <typename... Args>
void CBaseWeaponAttachment::AddCompatibleWeapons(Args... args)
{
    static_assert((std::is_same_v<Args, const char*> && ...), "Only const char* strings are allowed!");

    (AddWeapon(args), ...);  // Fold expression to call AddWeapon on each argument
}

void CBaseWeaponAttachment::AddWeapon(const char* szWeaponClassName)
{
    // No need to add duplicates
    if (!IsDuplicate(m_CompatibleWeapons, szWeaponClassName))
    {
        m_CompatibleWeapons.AddToTail(szWeaponClassName);
    }
}
