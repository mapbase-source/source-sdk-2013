#ifndef ATTACHMENT_DEF_H
#define ATTACHMENT_DEF_H
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "utlvector.h"
#include "utldict.h"
#include "utlstring.h"

//-----------------------------------------------------------------------------
// Attachment slot types. ATTACHMENT_NONE is reserved as "empty slot".
// Add new types above ATTACHMENT_LAST.
//-----------------------------------------------------------------------------
enum AttachmentType_t
{
    ATTACHMENT_NONE = 0,
    ATTACHMENT_SILENCER,
    ATTACHMENT_SCOPE,
    ATTACHMENT_MAG,
    ATTACHMENT_GRIP,
    ATTACHMENT_LASER_SIGHT,
    ATTACHMENT_FLASHLIGHT,
    ATTACHMENT_BARREL,

    ATTACHMENT_LAST,
    ATTACHMENT_COUNT = ATTACHMENT_LAST  // includes slot 0 (unused); size of arrays
};

// Sentinel for "no def loaded / empty slot".
#define INVALID_ATTACHMENT_DEF_INDEX ((unsigned short)0xFFFF)

//-----------------------------------------------------------------------------
// Static definition of an attachment, loaded from script files at startup.
// Definitions are immutable shared data — instances reference these by index.
//-----------------------------------------------------------------------------
struct AttachmentDef_t
{
    AttachmentDef_t()
        : type( ATTACHMENT_NONE )
        , flDamageMod( 1.0f )
        , flFireRateMod( 1.0f )
        , flSpreadMod( 1.0f )
        , vecVMOffset( 0, 0, 0 )
    {
        szName[0] = '\0';
        szModel[0] = '\0';
        szFireSound[0] = '\0';
    }

    char              szName[64];
    AttachmentType_t  type;
    char              szModel[MAX_PATH];
    char              szFireSound[64];

    float             flDamageMod;
    float             flFireRateMod;
    float             flSpreadMod;

    Vector            vecVMOffset;  // viewmodel offset for ADS (mainly for scopes)

    CUtlVector<CUtlString> compatibleWeapons;

    bool IsCompatibleWith( const char *pszWeaponClass ) const;
};

//-----------------------------------------------------------------------------
// Singleton registry. Loaded once per DLL from the same script files,
// giving server and client matching indices.
//-----------------------------------------------------------------------------
class CAttachmentDefRegistry
{
public:
    static CAttachmentDefRegistry &Instance();

    void LoadAll();

    const AttachmentDef_t *FindByName( const char *pszName ) const;
    const AttachmentDef_t *FindByIndex( unsigned short index ) const;
    unsigned short        FindIndexByName( const char *pszName ) const;

    int Count() const { return m_Defs.Count(); }

    // Iteration helpers for debugging / UI.
    const AttachmentDef_t &Get( unsigned short index ) const { return *m_Defs[index]; }
    bool IsValidIndex( unsigned short index ) const { return m_Defs.IsValidIndex( index ); }

    void PrecacheAll();

private:
    CAttachmentDefRegistry() {}
    ~CAttachmentDefRegistry() 
    {
        m_Defs.PurgeAndDeleteElements();
    }

    bool             ParseFile( const char *pszPath );
    AttachmentType_t ParseTypeString( const char *pszType ) const;

    CUtlDict< AttachmentDef_t*, unsigned short > m_Defs;  // pointer now
};

inline const AttachmentDef_t *GetAttachmentDef( unsigned short index )
{
    return CAttachmentDefRegistry::Instance().FindByIndex( index );
}

#endif // ATTACHMENT_DEF_H