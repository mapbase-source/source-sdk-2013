#ifndef ATTACHMENT_INVENTORY_H
#define ATTACHMENT_INVENTORY_H
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "utlvector.h"
#include "attachment_def.h"

#ifndef CLIENT_DLL

class CBaseModularWeapon;

typedef int AttachmentInstanceID_t;
#define INVALID_ATTACHMENT_INSTANCE_ID 0

enum AttachmentLocation_t
{
    ATTACH_LOC_INVENTORY = 0,
    ATTACH_LOC_EQUIPPED,
};

//-----------------------------------------------------------------------------
// One owned attachment. Lives by value inside the player's inventory vector.
// Stable references are by ID, never by pointer (vector may reallocate).
//-----------------------------------------------------------------------------
struct AttachmentInstance_t
{
    AttachmentInstance_t()
        : id( INVALID_ATTACHMENT_INSTANCE_ID )
        , defIndex( INVALID_ATTACHMENT_DEF_INDEX )
        , location( ATTACH_LOC_INVENTORY )
    {}

    AttachmentInstanceID_t id;
    unsigned short         defIndex;
    int                    location;        // AttachmentLocation_t, stored as int for save
    EHANDLE                hEquippedWeapon;

    DECLARE_SIMPLE_DATADESC();
};

//-----------------------------------------------------------------------------
// Per-player attachment inventory. Server-side only.
// Embedded by value on the player entity, saved with the player.
//-----------------------------------------------------------------------------
class CAttachmentInventory
{
public:
    CAttachmentInventory();

    AttachmentInstanceID_t AddByDefName( const char *pszDefName );
    AttachmentInstanceID_t AddByDefIndex( unsigned short defIndex );

    bool Remove( AttachmentInstanceID_t id );

    bool Equip( AttachmentInstanceID_t id, CBaseModularWeapon *pWeapon );
    bool Unequip( AttachmentInstanceID_t id );

    // Called when a weapon is destroyed; flips its equipped attachments
    // back to inventory state.
    void OnWeaponDestroyed( CBaseModularWeapon *pWeapon );

    AttachmentInstance_t       *FindInstance( AttachmentInstanceID_t id );
    const AttachmentInstance_t *FindInstance( AttachmentInstanceID_t id ) const;

    int                          Count() const { return m_Instances.Count(); }
    const AttachmentInstance_t  &Get( int i )  const { return m_Instances[i]; }

    DECLARE_SIMPLE_DATADESC();

private:
    AttachmentInstanceID_t AllocateID() { return ++m_NextID; }

    CUtlVector<AttachmentInstance_t> m_Instances;
    AttachmentInstanceID_t           m_NextID;
};

#endif // !CLIENT_DLL

#endif // ATTACHMENT_INVENTORY_H