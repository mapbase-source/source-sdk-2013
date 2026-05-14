#ifndef MODULAR_WEAPON_BASE_H
#define MODULAR_WEAPON_BASE_H
#ifdef _WIN32
#pragma once
#endif

#include "basehlcombatweapon_shared.h"
#include "attachments/attachment_def.h"
#include "attachments/attachment_renderable.h"
#ifndef CLIENT_DLL
#include "attachments/attachment_inventory.h"
#endif

#if defined( CLIENT_DLL )
#define CBaseModularWeapon C_BaseModularWeapon
#endif

class CBaseModularWeapon : public CBaseHLCombatWeapon
{
public:
    DECLARE_CLASS(CBaseModularWeapon, CBaseHLCombatWeapon)
    DECLARE_NETWORKCLASS();
    DECLARE_PREDICTABLE();

    CBaseModularWeapon();
    virtual ~CBaseModularWeapon();

    virtual void Precache(void);

#ifndef CLIENT_DLL
    DECLARE_DATADESC();
    virtual void UpdateOnRemove(void);
#else
    virtual void OnDataChanged(DataUpdateType_t updateType);
    void         UpdateClientAttachments(void);
#endif

    // Attachment slot interface (used by CAttachmentInventory).
    bool                  HasAttachmentInSlot(AttachmentType_t type) const;
    unsigned short        GetAttachmentDefIndex(AttachmentType_t type) const;
    const AttachmentDef_t* GetAttachmentDefForSlot(AttachmentType_t type) const;

#ifndef CLIENT_DLL
    void                   SetAttachmentInSlot(AttachmentType_t type, AttachmentInstanceID_t instanceID, unsigned short defIndex);
    void                   ClearAttachmentInSlot(AttachmentType_t type);
    AttachmentInstanceID_t GetAttachmentInstanceID(AttachmentType_t type) const;
#endif

    // Existing weapon API (preserved from original).
    virtual void  SetWeaponVisible(bool visible);
    virtual bool  Holster(CBaseCombatWeapon* pSwitchingTo);
    virtual bool  DefaultReload(int iClipSize1, int iClipSize2, int iActivity);

    virtual Vector  GetIronsightPositionOffset(void) const;
    virtual QAngle  GetIronsightAngleOffset(void) const;
    virtual float   GetIronsightFOVOffset(void) const;
    virtual bool    HasIronsights(void) { return true; }
    bool            IsIronsighted(void);
    void            ToggleIronsights(void);
    void            EnableIronsights(void);
    void            DisableIronsights(void);
    void            SetIronsightTime(void);

    virtual void  AddViewmodelBob(CBaseViewModel* viewmodel, Vector& origin, QAngle& angles);
    virtual float CalcViewmodelBob(void);

    virtual bool  IsBaseModularWeapon(void) const { return true; }

    virtual char const* GetShootSound(int iIndex) const;
    virtual float       GetDamage(void);

    virtual void PrimaryAttack(void);
    virtual void ToggleFireMode(void);
    virtual void ItemPreFrame(void);
    virtual void ItemPostFrame(void);
    virtual void HandleBurstFire(void);

    CNetworkVar(bool, m_bIsIronsighted);
    CNetworkVar(float, m_flIronsightedTime);

    int burstFire = 0;

private:
    // Source of truth for which attachments are equipped, indexed by AttachmentType_t.
    // Networked so clients can render attachment props. Slot value of
    // INVALID_ATTACHMENT_DEF_INDEX means empty.
    CNetworkArray(unsigned short, m_AttachmentDefIndices, ATTACHMENT_COUNT);

#ifndef CLIENT_DLL
    // Server-only: pairs with m_AttachmentDefIndices to track which instance
    // (in the player's inventory) is in each slot. Not networked � clients
    // only need the def to render; IDs are bookkeeping for unequip flow.
    AttachmentInstanceID_t m_AttachmentInstanceIDs[ATTACHMENT_COUNT];
#else
    CHandle< C_AttachmentRenderable > m_hClientAttachments[ATTACHMENT_COUNT];
    unsigned short             m_LastAttachmentDefIndices[ATTACHMENT_COUNT];

    void          ReleaseClientAttachment(int slot);
    bool          CreateClientAttachment(int slot, const AttachmentDef_t* pDef, C_BaseEntity* pParent);
    C_BaseEntity* GetAttachmentRenderParent(void);
#endif
};

inline CBaseModularWeapon* ToModularWeapon(CBaseEntity* pEntity)
{
    if (!pEntity || !pEntity->IsBaseModularWeapon())
        return NULL;
    return static_cast<CBaseModularWeapon*>(pEntity);
}

#endif // MODULAR_WEAPON_BASE_H
