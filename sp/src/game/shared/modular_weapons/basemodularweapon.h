#ifndef MODULAR_WEAPON_BASE_H
#define MODULAR_WEAPON_BASE_H
#ifdef _WIN32
#pragma once
#endif
#include "basehlcombatweapon_shared.h"
#include "baseattachment.h"

#if defined( CLIENT_DLL )
#define CBaseModularWeapon C_BaseModularWeapon
#endif // CLIENT_DLL

class CBaseModularWeapon : public CBaseHLCombatWeapon
{
public:
    DECLARE_CLASS(CBaseModularWeapon, CBaseHLCombatWeapon)
    DECLARE_NETWORKCLASS();
    DECLARE_PREDICTABLE();

    CBaseModularWeapon();

#ifndef CLIENT_DLL
    DECLARE_DATADESC();
#else
    virtual void ClientThink();
    virtual void OnDataChanged(DataUpdateType_t updateType);
#endif // !CLIENT_DLL
	
    virtual ~CBaseModularWeapon();
    virtual void EquipAttachment(CBaseWeaponAttachment* pAttachment);
    virtual void RemoveAttachment(AttachmentType_t type);

    virtual float GetDamage();

    virtual void PrimaryAttack(void);
    virtual void ItemPreFrame(void);

private:
    float m_flBaseDamage = 0.0f;
    CUtlMap<AttachmentType_t, CBaseWeaponAttachment*> m_Attachments;

    CNetworkHandle(CBaseWeaponAttachment, LastAttachment);
};

inline CBaseModularWeapon* ToModularWeapon(CBaseEntity* pEntity)
{
    if (!pEntity || !pEntity->IsBaseCombatWeapon())
        return NULL;
    return static_cast<CBaseModularWeapon*>(pEntity);
}

#endif // !MODULAR_WEAPON_BASE_H
