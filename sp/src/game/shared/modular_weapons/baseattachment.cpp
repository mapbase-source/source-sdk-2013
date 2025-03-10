#include "cbase.h"
#include "baseattachment.h"
#include "basemodularweapon.h"  // Include the full definition here

#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED(BaseWeaponAttachment, DT_BaseWeaponAttachment)

BEGIN_NETWORK_TABLE(CBaseWeaponAttachment, DT_BaseWeaponAttachment)
#ifdef GAME_DLL
SendPropInt(SENDINFO(m_AttachmentType)),
SendPropFloat(SENDINFO(m_flDamageModifier)),
SendPropFloat(SENDINFO(m_flFireRateModifier)),
SendPropFloat(SENDINFO(m_flSpreadModifier)),
SendPropString(SENDINFO(m_CompatibleWeaponList)),
SendPropString(SENDINFO(m_szAttacmentModel)),
#endif
#ifdef CLIENT_DLL
RecvPropInt(RECVINFO(m_AttachmentType)),
RecvPropFloat(RECVINFO(m_flDamageModifier)),
RecvPropFloat(RECVINFO(m_flFireRateModifier)),
RecvPropFloat(RECVINFO(m_flSpreadModifier)),
RecvPropString(RECVINFO(m_CompatibleWeaponList)),
RecvPropString(RECVINFO(m_szAttacmentModel)),
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
        if (!V_strcmp(m_CompatibleWeapons[i], pWeapon->GetClassname()))
        {
            return true;
        }
    }

    return false;
}

CBaseWeaponAttachment::CBaseWeaponAttachment()
{
    m_AttachmentType = ATTACHMENT_NONE;
    m_flDamageModifier.GetForModify() = 1.0f;
    m_flFireRateModifier.GetForModify() = 1.0f;
    m_flSpreadModifier.GetForModify() = 1.0f;
}

CBaseWeaponAttachment::~CBaseWeaponAttachment()
{
#ifdef CLIENT_DLL
    for (int i = 0; i < m_CompatibleWeapons.Count(); i++)
    {
        if (m_CompatibleWeapons[i])
        {
            delete[] m_CompatibleWeapons[i];
        }
    }
#endif // CLIENT_DLL
    m_CompatibleWeapons.Purge();
}

void CBaseWeaponAttachment::Save(CSave& save)
{
    // Save base properties
    save.WriteInt((int*)&m_AttachmentType);
    save.WriteFloat(&m_flDamageModifier.Get());
    save.WriteFloat(&m_flFireRateModifier.Get());
    save.WriteFloat(&m_flSpreadModifier.Get());

    // Save compatible weapons list
    int weaponCount = m_CompatibleWeapons.Count();
    save.WriteInt(&weaponCount);

    for (int i = 0; i < weaponCount; i++)
    {
        save.WriteString(m_CompatibleWeapons[i]);
    }
}

void CBaseWeaponAttachment::Restore(CRestore& restore)
{
    // Restore base properties
    restore.ReadInt((int*)&m_AttachmentType);
    restore.ReadFloat(&m_flDamageModifier.GetForModify());
    restore.ReadFloat(&m_flFireRateModifier.GetForModify());
    restore.ReadFloat(&m_flSpreadModifier.GetForModify());

    // Restore compatible weapons list
    int weaponCount;
    restore.ReadInt(&weaponCount);

    for (int i = 0; i < m_CompatibleWeapons.Count(); i++)
    {
        delete[] m_CompatibleWeapons[i];
    }
    m_CompatibleWeapons.Purge();
    char buffer[128];

    for (int i = 0; i < weaponCount; i++)
    {
        restore.ReadString(buffer, sizeof(buffer), 0);
        m_CompatibleWeapons.AddToTail(strdup(buffer)); // Note: This allocates memory
    }
}

void CBaseWeaponAttachment::Spawn()
{
    Precache();
#ifndef CLIENT_DLL
    SetTransmitState(FL_EDICT_ALWAYS);
#endif // CLIENT_DLL
    BaseClass::Spawn();
}

void CBaseWeaponAttachment::Precache(void)
{
    BaseClass::Precache();
    PrecacheModel(GetModel());
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
#ifdef GAME_DLL
void CBaseWeaponAttachment::UpdateCompatibleWeaponList()
{
    char buffer[512] = { 0 };  // Initialize with zeros

    for (int i = 0; i < m_CompatibleWeapons.Count(); i++)
    {
        V_strcat(buffer, m_CompatibleWeapons[i], sizeof(buffer));  // Append weapon name

        if (i < m_CompatibleWeapons.Count() - 1)
        {
            V_strcat(buffer, ",", sizeof(buffer));  // Separate with commas
        }
    }

    // Ensure the string is null-terminated
    // Find the length of the string in the buffer
    size_t strLength = V_strlen(buffer);

    // Ensure m_CompatibleWeaponList has enough space for the copied string
    V_strncpy(m_CompatibleWeaponList.GetForModify(), buffer, strLength + 1);  // +1 to copy the null terminator
}
#endif // GAME_DLL

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
#ifdef GAME_DLL
        UpdateCompatibleWeaponList(); // Update the networked string
#endif // GAME_DLL
    }
}

void CBaseWeaponAttachment::GetCompatibleWeapons(CUtlVector<const char*>& vec)
{
    char* buffer = new char[512];  // Ensure buffer is zero-initialized
    const char* weaponlist = m_CompatibleWeaponList.Get();
    V_strncpy(buffer, weaponlist, 512);  // Copy weapon list to buffer

    char* token = strtok(buffer, ",");  // Tokenize based on commas
    while (token)
    {
        vec.AddToTail(token);  // Add token to vector
        token = strtok(nullptr, ",");  // Continue tokenizing
    }
}

// Implementation of CAttachmentFactory static members
CUtlMap<AttachmentType_t, CUtlVector<CAttachmentFactory::AttachmentCreator>*> CAttachmentFactory::m_Creators(DefLessFunc(AttachmentType_t));

CAttachmentFactory::AttachmentCreator::AttachmentCreator(const char* name, CreateAttachmentFn fn)
{
    V_strncpy(m_ClassName, name, sizeof(m_ClassName));
    m_CreateFn = fn;
}

void CAttachmentFactory::RegisterAttachmentClass(AttachmentType_t type, const char* className, CreateAttachmentFn createFn)
{
    // Find or create list for this type
    int idx = m_Creators.Find(type);
    if (idx == m_Creators.InvalidIndex())
    {
        idx = m_Creators.Insert(type, new CUtlVector<AttachmentCreator>());
    }

    m_Creators[idx]->AddToTail(AttachmentCreator(className, createFn));
}

CBaseWeaponAttachment* CAttachmentFactory::CreateAttachment(AttachmentType_t type, const char* className)
{
    int idx = m_Creators.Find(type);
    if (idx == m_Creators.InvalidIndex())
        return NULL;

    CUtlVector<AttachmentCreator>* creators = m_Creators[idx];  // Use pointer here

    // If no specific class name provided, use the first registered creator for this type
    if (!className || !className[0])
        return (*creators)[0].m_CreateFn();  // Dereference the pointer

    // Find the specific class by name
    for (int i = 0; i < creators->Count(); i++)  // Use -> instead of .
    {
        if (V_strcmp((*creators)[i].m_ClassName, className) == 0)  // Dereference the pointer
            return (*creators)[i].m_CreateFn();  // Dereference the pointer
    }

    return NULL;
}

// Helper functions implementation
void SaveWeaponAttachments(CSave& save, CUtlVector<CBaseWeaponAttachment*>& attachments)
{
    int count = attachments.Count();
    save.WriteInt(&count);

    for (int i = 0; i < count; i++)
    {
        CBaseWeaponAttachment* pAttachment = attachments[i];

        // Save the attachment type
        int type = (int)(pAttachment->GetAttachmentType());
        save.WriteInt(&type);

        // Save the class name
        const char* className = pAttachment->GetClassName();
        save.WriteString(className);

        // Save instance data
        pAttachment->Save(save);
    }
}

void RestoreWeaponAttachments(CRestore& restore, CUtlVector<CBaseWeaponAttachment*>& attachments)
{
    // Clear existing vector
    attachments.Purge();

    int count;
    restore.ReadInt(&count);

    for (int i = 0; i < count; i++)
    {
        // Read attachment type
        int typeInt;
        restore.ReadInt(&typeInt);
        AttachmentType_t type = static_cast<AttachmentType_t>(typeInt);

        // Read class name
        char className[64];
        restore.ReadString(className, sizeof(className), 0);

        // Create attachment using factory
        CBaseWeaponAttachment* pNewAttachment = CAttachmentFactory::CreateAttachment(type, className);
        if (pNewAttachment)
        {
            pNewAttachment->Restore(restore);
            attachments.AddToTail(pNewAttachment);
        }
        else
        {
            Warning("Failed to create attachment of type %d, class %s\n", type, className);
        }
    }
}