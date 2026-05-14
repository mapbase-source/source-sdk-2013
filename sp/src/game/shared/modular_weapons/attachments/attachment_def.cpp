#include "cbase.h"
#include "attachment_def.h"
#include "filesystem.h"
#include "KeyValues.h"

#include "tier0/memdbgon.h"

bool AttachmentDef_t::IsCompatibleWith( const char *pszWeaponClass ) const
{
    if ( !pszWeaponClass || !*pszWeaponClass )
        return false;

    for ( int i = 0; i < compatibleWeapons.Count(); i++ )
    {
        const char *pszEntry = compatibleWeapons[i].Get();
        if ( pszEntry[0] == '*' && pszEntry[1] == '\0' )
            return true;
        if ( FStrEq( pszEntry, pszWeaponClass ) )
            return true;
    }
    return false;
}

CAttachmentDefRegistry &CAttachmentDefRegistry::Instance()
{
    static CAttachmentDefRegistry s_Instance;
    return s_Instance;
}

const AttachmentDef_t *CAttachmentDefRegistry::FindByName( const char *pszName ) const
{
    if ( !pszName || !*pszName )
        return NULL;

    unsigned short idx = m_Defs.Find( pszName );
    if ( idx == m_Defs.InvalidIndex() )
        return NULL;

    return m_Defs[idx];
}

const AttachmentDef_t *CAttachmentDefRegistry::FindByIndex( unsigned short index ) const
{
    if ( index == INVALID_ATTACHMENT_DEF_INDEX )
        return NULL;
    if ( !m_Defs.IsValidIndex( index ) )
        return NULL;
    return m_Defs[index];
}

unsigned short CAttachmentDefRegistry::FindIndexByName( const char *pszName ) const
{
    if ( !pszName || !*pszName )
        return INVALID_ATTACHMENT_DEF_INDEX;

    unsigned short idx = m_Defs.Find( pszName );
    if ( idx == m_Defs.InvalidIndex() )
        return INVALID_ATTACHMENT_DEF_INDEX;
    return idx;
}

void CAttachmentDefRegistry::PrecacheAll()
{
    // Lazy-load defs on first call. Scripts don't change between levels,
    // so we only do this once per process.
    static bool s_bLoaded = false;
    if (!s_bLoaded)
    {
        LoadAll();
        s_bLoaded = true;
    }

    for (unsigned short i = m_Defs.First(); i != m_Defs.InvalidIndex(); i = m_Defs.Next(i))
    {
        AttachmentDef_t* pDef = m_Defs[i];
        if (!pDef) continue;

        if (pDef->szModel[0])
        {
            int idx = CBaseEntity::PrecacheModel(pDef->szModel);
            if (idx == -1)
                Warning("AttachmentDefRegistry: failed to precache model '%s' for '%s'\n",
                    pDef->szModel, pDef->szName);
        }

        if (pDef->szFireSound[0])
        {
            CBaseEntity::PrecacheScriptSound(pDef->szFireSound);
        }
    }
}

AttachmentType_t CAttachmentDefRegistry::ParseTypeString( const char *pszType ) const
{
    if ( !pszType ) return ATTACHMENT_NONE;
    if ( FStrEq( pszType, "silencer" ) )   return ATTACHMENT_SILENCER;
    if ( FStrEq( pszType, "scope" ) )      return ATTACHMENT_SCOPE;
    if ( FStrEq( pszType, "sight" ) )      return ATTACHMENT_SCOPE; // alias
    if ( FStrEq( pszType, "mag" ) )        return ATTACHMENT_MAG;
    if ( FStrEq( pszType, "grip" ) )       return ATTACHMENT_GRIP;
    if ( FStrEq( pszType, "laser" ) )      return ATTACHMENT_LASER_SIGHT;
    if ( FStrEq( pszType, "flashlight" ) ) return ATTACHMENT_FLASHLIGHT;
    if ( FStrEq( pszType, "barrel" ) )     return ATTACHMENT_BARREL;
    return ATTACHMENT_NONE;
}

void CAttachmentDefRegistry::LoadAll()
{
    m_Defs.PurgeAndDeleteElements();

    FileFindHandle_t findHandle;
    const char *pszFile = filesystem->FindFirstEx( "scripts/attachments/*.txt", "GAME", &findHandle );
    while ( pszFile )
    {
        char szPath[MAX_PATH];
        V_snprintf( szPath, sizeof(szPath), "scripts/attachments/%s", pszFile );
        ParseFile( szPath );
        pszFile = filesystem->FindNext( findHandle );
    }
    filesystem->FindClose( findHandle );

    DevMsg( "AttachmentDefRegistry: loaded %d attachment definition(s)\n", m_Defs.Count() );
}

bool CAttachmentDefRegistry::ParseFile(const char* pszPath)
{
    KeyValues* pKV = new KeyValues("attachment");
    if (!pKV->LoadFromFile(filesystem, pszPath, "GAME"))
    {
        Warning("AttachmentDefRegistry: failed to parse %s\n", pszPath);
        pKV->deleteThis();
        return false;
    }

    // Each top-level key in the file is one attachment definition.
    for (KeyValues* pBlock = pKV; pBlock; pBlock = pBlock->GetNextKey())
    {
        const char* pszName = pBlock->GetName();
        if (!pszName || !*pszName)
            continue;

        if (m_Defs.Find(pszName) != m_Defs.InvalidIndex())
        {
            Warning("AttachmentDefRegistry: duplicate '%s' in %s\n", pszName, pszPath);
            continue;
        }

        AttachmentDef_t* pDef = new AttachmentDef_t;
        V_strncpy(pDef->szName, pszName, sizeof(pDef->szName));

        const char* pszType = pBlock->GetString("type", "");
        pDef->type = ParseTypeString(pszType);
        if (pDef->type == ATTACHMENT_NONE)
        {
            Warning("AttachmentDefRegistry: unknown type '%s' for '%s'\n", pszType, pszName);
            delete pDef;
            continue;
        }

        V_strncpy(pDef->szModel, pBlock->GetString("model", ""), sizeof(pDef->szModel));
        V_strncpy(pDef->szFireSound, pBlock->GetString("fire_sound", ""), sizeof(pDef->szFireSound));

        pDef->flDamageMod = pBlock->GetFloat("damage_modifier", 1.0f);
        pDef->flFireRateMod = pBlock->GetFloat("firerate_modifier", 1.0f);
        pDef->flSpreadMod = pBlock->GetFloat("spread_modifier", 1.0f);

        if (KeyValues* pOff = pBlock->FindKey("vm_offset"))
        {
            pDef->vecVMOffset.x = pOff->GetFloat("forward", 0);
            pDef->vecVMOffset.y = pOff->GetFloat("right", 0);
            pDef->vecVMOffset.z = pOff->GetFloat("up", 0);
        }

        if (KeyValues* pCompat = pBlock->FindKey("compatible_weapons"))
        {
            for (KeyValues* pSub = pCompat->GetFirstSubKey(); pSub; pSub = pSub->GetNextKey())
            {
                const char* pszWep = pSub->GetString();
                if (pszWep && *pszWep)
                    pDef->compatibleWeapons.AddToTail(CUtlString(pszWep));
            }
        }

        m_Defs.Insert(pszName, pDef);
    }

    pKV->deleteThis();
    return true;
}

#ifndef CLIENT_DLL
CON_COMMAND_F( fp_reload_attachments, "Reload attachment defs from scripts/attachments/", FCVAR_CHEAT )
{
    CAttachmentDefRegistry::Instance().LoadAll();
}
#endif