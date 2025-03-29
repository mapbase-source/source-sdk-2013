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
    virtual void SetWeaponVisible(bool visible);
    virtual bool Holster(CBaseCombatWeapon* pSwitchingTo);
    virtual bool DefaultReload(int iClipSize1, int iClipSize2, int iActivity);

    virtual Vector	GetIronsightPositionOffset(void) const;
    virtual QAngle	GetIronsightAngleOffset(void) const;
    virtual float	GetIronsightFOVOffset(void) const;
    virtual bool    HasIronsights(void) { return true; } //default yes; override and return false for weapons with no ironsights (like weapon_crowbar)
    bool		    IsIronsighted(void);
    void		    ToggleIronsights(void);
    void		    EnableIronsights(void);
    void		    DisableIronsights(void);
    void		    SetIronsightTime(void);

    virtual void	AddViewmodelBob(CBaseViewModel* viewmodel, Vector& origin, QAngle& angles);
    virtual	float	CalcViewmodelBob(void);

    virtual bool	IsBaseModularWeapon(void) const { return true; }

    virtual char const* GetShootSound(int iIndex) const;

    virtual float GetDamage();

    virtual void PrimaryAttack(void);
    virtual void ToggleFireMode(void);
    virtual void ItemPreFrame(void);
    virtual void ItemPostFrame(void);
    virtual void HandleBurstFire(void);

    CNetworkVar(bool, m_bIsIronsighted);
    CNetworkVar(float, m_flIronsightedTime);


    //Some weapons can have the burst fire mode
    //this is used to track how many bullets we fired durring the burst mode
    //this is usualy a max of 3 but it can be anything you want
    int burstFire = 0;

private:
    float m_flBaseDamage = 0.0f;
    CUtlMap<AttachmentType_t, CBaseWeaponAttachment*> m_Attachments;

    CNetworkHandle(CBaseWeaponAttachment, LastAttachment);
};

inline CBaseModularWeapon* ToModularWeapon(CBaseEntity* pEntity)
{
    if (!pEntity || !pEntity->IsBaseModularWeapon())
        return NULL;
    return static_cast<CBaseModularWeapon*>(pEntity);
}

#endif // !MODULAR_WEAPON_BASE_H
