#include "cbase.h"
#include "attachment_inventory.h"

#ifndef CLIENT_DLL

#include "basemodularweapon.h"
#include "saverestore.h"
#include "saverestore_utlvector.h"
#include "hl2_player.h"

#include "tier0/memdbgon.h"

BEGIN_SIMPLE_DATADESC( AttachmentInstance_t )
    DEFINE_FIELD( id,              FIELD_INTEGER ),
    DEFINE_FIELD( defIndex,        FIELD_SHORT ),
    DEFINE_FIELD( location,        FIELD_INTEGER ),
    DEFINE_FIELD( hEquippedWeapon, FIELD_EHANDLE ),
END_DATADESC()

BEGIN_SIMPLE_DATADESC( CAttachmentInventory )
    DEFINE_UTLVECTOR( m_Instances, FIELD_EMBEDDED ),
    DEFINE_FIELD( m_NextID, FIELD_INTEGER ),
END_DATADESC()

CAttachmentInventory::CAttachmentInventory()
    : m_NextID( INVALID_ATTACHMENT_INSTANCE_ID )
{
}

AttachmentInstance_t *CAttachmentInventory::FindInstance( AttachmentInstanceID_t id )
{
    if ( id == INVALID_ATTACHMENT_INSTANCE_ID )
        return NULL;
    for ( int i = 0; i < m_Instances.Count(); i++ )
    {
        if ( m_Instances[i].id == id )
            return &m_Instances[i];
    }
    return NULL;
}

const AttachmentInstance_t *CAttachmentInventory::FindInstance( AttachmentInstanceID_t id ) const
{
    if ( id == INVALID_ATTACHMENT_INSTANCE_ID )
        return NULL;
    for ( int i = 0; i < m_Instances.Count(); i++ )
    {
        if ( m_Instances[i].id == id )
            return &m_Instances[i];
    }
    return NULL;
}

AttachmentInstanceID_t CAttachmentInventory::AddByDefName( const char *pszDefName )
{
    unsigned short idx = CAttachmentDefRegistry::Instance().FindIndexByName( pszDefName );
    if ( idx == INVALID_ATTACHMENT_DEF_INDEX )
    {
        Warning( "CAttachmentInventory: unknown def '%s'\n", pszDefName );
        return INVALID_ATTACHMENT_INSTANCE_ID;
    }
    return AddByDefIndex( idx );
}

AttachmentInstanceID_t CAttachmentInventory::AddByDefIndex( unsigned short defIndex )
{
    if ( !GetAttachmentDef( defIndex ) )
        return INVALID_ATTACHMENT_INSTANCE_ID;

    AttachmentInstance_t inst;
    inst.id              = AllocateID();
    inst.defIndex        = defIndex;
    inst.location        = ATTACH_LOC_INVENTORY;
    inst.hEquippedWeapon = NULL;

    m_Instances.AddToTail( inst );
    return inst.id;
}

bool CAttachmentInventory::Remove( AttachmentInstanceID_t id )
{
    for ( int i = 0; i < m_Instances.Count(); i++ )
    {
        if ( m_Instances[i].id != id )
            continue;

        if ( m_Instances[i].location == ATTACH_LOC_EQUIPPED )
            Unequip( id );

        m_Instances.Remove( i );
        return true;
    }
    return false;
}

bool CAttachmentInventory::Equip( AttachmentInstanceID_t id, CBaseModularWeapon *pWeapon )
{
    if ( !pWeapon )
        return false;

    AttachmentInstance_t *pInst = FindInstance( id );
    if ( !pInst )
        return false;

    if ( pInst->location == ATTACH_LOC_EQUIPPED )
    {
        DevMsg( "CAttachmentInventory::Equip: instance %d already equipped — unequip first\n", id );
        return false;
    }

    const AttachmentDef_t *pDef = GetAttachmentDef( pInst->defIndex );
    if ( !pDef )
        return false;

    if ( !pDef->IsCompatibleWith( pWeapon->GetClassname() ) )
    {
        DevMsg( "CAttachmentInventory::Equip: '%s' not compatible with %s\n",
                pDef->szName, pWeapon->GetClassname() );
        return false;
    }

    if ( pWeapon->HasAttachmentInSlot( pDef->type ) )
    {
        DevMsg( "CAttachmentInventory::Equip: slot %d already occupied on %s\n",
                pDef->type, pWeapon->GetClassname() );
        return false;
    }

    pWeapon->SetAttachmentInSlot( pDef->type, id, pInst->defIndex );
    pInst->location        = ATTACH_LOC_EQUIPPED;
    pInst->hEquippedWeapon = pWeapon;
    return true;
}

bool CAttachmentInventory::Unequip( AttachmentInstanceID_t id )
{
    AttachmentInstance_t *pInst = FindInstance( id );
    if ( !pInst )
        return false;

    if ( pInst->location != ATTACH_LOC_EQUIPPED )
        return false;

    CBaseModularWeapon *pWeapon = static_cast<CBaseModularWeapon*>( pInst->hEquippedWeapon.Get() );
    if ( pWeapon )
    {
        const AttachmentDef_t *pDef = GetAttachmentDef( pInst->defIndex );
        if ( pDef )
            pWeapon->ClearAttachmentInSlot( pDef->type );
    }

    pInst->location        = ATTACH_LOC_INVENTORY;
    pInst->hEquippedWeapon = NULL;
    return true;
}

void CAttachmentInventory::OnWeaponDestroyed( CBaseModularWeapon *pWeapon )
{
    if ( !pWeapon )
        return;

    for ( int i = 0; i < m_Instances.Count(); i++ )
    {
        if ( m_Instances[i].location == ATTACH_LOC_EQUIPPED &&
             m_Instances[i].hEquippedWeapon.Get() == pWeapon )
        {
            m_Instances[i].location        = ATTACH_LOC_INVENTORY;
            m_Instances[i].hEquippedWeapon = NULL;
        }
    }
}

// ---------------------------------------------------------------------------
// Console commands
// ---------------------------------------------------------------------------

static int fp_give_attachment_completion(const char* partial,
    char commands[COMMAND_COMPLETION_MAXITEMS][COMMAND_COMPLETION_ITEM_LENGTH])
{
    static const char* pszCmd = "fp_give_attachment ";
    const size_t cmdLen = V_strlen(pszCmd);

    if (V_strstr(partial, pszCmd) != partial)
        return 0;

    const char* pszArgs = partial + cmdLen;
    const char* pszSpace = V_strstr(pszArgs, " ");

    int count = 0;
    CAttachmentDefRegistry& registry = CAttachmentDefRegistry::Instance();

    if (!pszSpace)
    {
        // Arg 1: attachment def names.
        const size_t argLen = V_strlen(pszArgs);

        for (unsigned short i = 0; i < registry.Count(); i++)
        {
            if (!registry.IsValidIndex(i)) continue;
            const AttachmentDef_t& def = registry.Get(i);

            if (argLen > 0 && V_strnicmp(def.szName, pszArgs, argLen) != 0)
                continue;

            V_snprintf(commands[count], COMMAND_COMPLETION_ITEM_LENGTH,
                "%s%s", pszCmd, def.szName);
            if (++count >= COMMAND_COMPLETION_MAXITEMS)
                break;
        }
    }
    else
    {
        // Arg 2: weapons the player owns that are compatible with arg 1.
        char szDefName[64];
        const size_t arg1Len = pszSpace - pszArgs;
        if (arg1Len == 0 || arg1Len >= sizeof(szDefName))
            return 0;
        V_strncpy(szDefName, pszArgs, arg1Len + 1);
        szDefName[arg1Len] = '\0';

        const AttachmentDef_t* pDef = registry.FindByName(szDefName);
        if (!pDef)
            return 0;

        const char* pszArg2 = pszSpace + 1;
        const size_t arg2Len = V_strlen(pszArg2);

        CBasePlayer* pPlayer = UTIL_GetCommandClient();
        if (!pPlayer)
            return 0;

        for (int i = 0; i < pPlayer->WeaponCount(); i++)
        {
            CBaseCombatWeapon* pWep = pPlayer->GetWeapon(i);
            if (!pWep) continue;

            const char* pszWep = pWep->GetClassname();

            if (!pDef->IsCompatibleWith(pszWep))
                continue;

            if (arg2Len > 0 && V_strnicmp(pszWep, pszArg2, arg2Len) != 0)
                continue;

            V_snprintf(commands[count], COMMAND_COMPLETION_ITEM_LENGTH,
                "%s%s %s", pszCmd, szDefName, pszWep);
            if (++count >= COMMAND_COMPLETION_MAXITEMS)
                break;
        }
    }

    return count;
}

CON_COMMAND_F_COMPLETION( fp_give_attachment,
    "fp_give_attachment <def_name> [weapon_class] — add attachment to inventory; optionally equip",
    FCVAR_CHEAT, fp_give_attachment_completion )
{
    if ( args.ArgC() < 2 )
    {
        Msg( "Usage: fp_give_attachment <def_name> [weapon_class]\n" );
        return;
    }

    CHL2_Player*pPlayer = ToHL2Player(UTIL_GetCommandClient());
    if ( !pPlayer ) return;

    CAttachmentInventory *pInv = pPlayer->GetAttachmentInventory();
    if ( !pInv ) return;

    AttachmentInstanceID_t id = pInv->AddByDefName( args.Arg( 1 ) );
    if ( id == INVALID_ATTACHMENT_INSTANCE_ID )
    {
        Msg( "Unknown attachment '%s'\n", args.Arg( 1 ) );
        return;
    }

    Msg( "Added attachment '%s' (id %d) to inventory\n", args.Arg( 1 ), id );

    if ( args.ArgC() >= 3 )
    {
        CBaseModularWeapon *pWep = ToModularWeapon( pPlayer->Weapon_OwnsThisType( args.Arg( 2 ) ) );
        if ( pWep )
        {
            if ( pInv->Equip( id, pWep ) )
                Msg( "Equipped onto %s\n", args.Arg( 2 ) );
            else
                Msg( "Equip failed\n" );
        }
        else
        {
            Msg( "Player doesn't own %s\n", args.Arg( 2 ) );
        }
    }
}

static int fp_unequip_attachment_completion(const char* partial,
    char commands[COMMAND_COMPLETION_MAXITEMS][COMMAND_COMPLETION_ITEM_LENGTH])
{
    static const char* pszCmd = "fp_unequip_attachment ";
    const size_t cmdLen = V_strlen(pszCmd);
    if (V_strstr(partial, pszCmd) != partial)
        return 0;

    const char* pszArg = partial + cmdLen;
    const size_t argLen = V_strlen(pszArg);

    CBasePlayer* pPlayer = UTIL_GetCommandClient();
    if (!pPlayer) return 0;

    CHL2_Player* pHL2 = dynamic_cast<CHL2_Player*>(pPlayer);
    if (!pHL2) return 0;

    CAttachmentInventory* pInv = pHL2->GetAttachmentInventory();
    if (!pInv) return 0;

    int count = 0;
    for (int i = 0; i < pInv->Count(); i++)
    {
        const AttachmentInstance_t& inst = pInv->Get(i);
        if (inst.location != ATTACH_LOC_EQUIPPED)
            continue;

        char szID[16];
        V_snprintf(szID, sizeof(szID), "%d", inst.id);

        if (argLen > 0 && V_strncmp(szID, pszArg, argLen) != 0)
            continue;

        V_snprintf(commands[count], COMMAND_COMPLETION_ITEM_LENGTH,
            "%s%d", pszCmd, inst.id);
        if (++count >= COMMAND_COMPLETION_MAXITEMS)
            break;
    }
    return count;
}

CON_COMMAND_F_COMPLETION( fp_unequip_attachment, "fp_unequip_attachment <instance_id>", FCVAR_CHEAT, fp_unequip_attachment_completion )
{
    if ( args.ArgC() < 2 ) return;
    CHL2_Player *pPlayer = ToHL2Player(UTIL_GetCommandClient());
    if ( !pPlayer ) return;

    CAttachmentInventory *pInv = pPlayer->GetAttachmentInventory();
    if ( !pInv ) return;

    pInv->Unequip( atoi( args.Arg( 1 ) ) );
}

CON_COMMAND_F( fp_list_attachments, "List the player's attachment inventory", FCVAR_CHEAT )
{
    CHL2_Player *pPlayer = ToHL2Player(UTIL_GetCommandClient());
    if ( !pPlayer ) return;

    CAttachmentInventory *pInv = pPlayer->GetAttachmentInventory();
    if ( !pInv ) return;

    Msg( "Player has %d attachment(s):\n", pInv->Count() );
    for ( int i = 0; i < pInv->Count(); i++ )
    {
        const AttachmentInstance_t &inst = pInv->Get( i );
        const AttachmentDef_t *pDef = GetAttachmentDef( inst.defIndex );
        const char *pszLoc = ( inst.location == ATTACH_LOC_EQUIPPED ) ? "equipped" : "inventory";
        const char *pszOn = "";
        if ( inst.location == ATTACH_LOC_EQUIPPED && inst.hEquippedWeapon.Get() )
            pszOn = inst.hEquippedWeapon->GetClassname();
        Msg( "  [%d] %s — %s %s\n", inst.id, pDef ? pDef->szName : "<bad def>", pszLoc, pszOn );
    }
}

#endif // !CLIENT_DLL