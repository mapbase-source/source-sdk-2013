#ifndef BASE_WEAPON_ATTACHMENT_H
#define BASE_WEAPON_ATTACHMENT_H
#include "cbase.h"
#include "saverestore.h"

#ifdef CLIENT_DLL
#define CBaseModularWeapon C_BaseModularWeapon
#define CBaseWeaponAttachment C_BaseWeaponAttachment
#define CAttachmentFactory C_BaseAttachmentFactory
#endif // CLIENT_DLL

class CBaseModularWeapon; // Forward declaration
class CAttachmentFactory; // Forward declaration

enum AttachmentType_t
{
    ATTACHMENT_NONE = 0,     
    ATTACHMENT_SILENCER,     // Reduces noise, changes firing sound
    ATTACHMENT_SCOPE,        // Changes ADS behavior, modifies zoom
    ATTACHMENT_MAG,          // Increases ammo capacity
    ATTACHMENT_GRIP,         // Reduces recoil
    ATTACHMENT_LASER_SIGHT,  // Adds laser dot for hip-fire accuracy
    ATTACHMENT_FLASHLIGHT,   // Enables flashlight
    ATTACHMENT_BARREL,       // Modifies bullet spread

    ATTACHMENT_LAST,        // PUT NEW ATTACHMENTS ABOVE THIS ONE

    //Used for getting how many attachments a weapon can have
    ATTACHMENT_COUNT = ATTACHMENT_LAST - 1 
};

class CBaseWeaponAttachment : public CBaseEntity
{
public:
    DECLARE_CLASS(CBaseWeaponAttachment, CBaseEntity)
#ifndef CLIENT_DLL
    DECLARE_DATADESC();
#endif // !CLIENT_DLL
    DECLARE_NETWORKCLASS();
    DECLARE_PREDICTABLE();

    virtual const char* GetClassName() { return nullptr; }; // Each derived class must implement this
    virtual void Save(CSave& save);
    virtual void Restore(CRestore& restore);

    inline AttachmentType_t GetAttachmentType() const { return m_AttachmentType; };
    inline void SetAttachmentType(AttachmentType_t type) { m_AttachmentType = type; };

    inline float GetDamageModifier() const { return m_flDamageModifier; }
    inline void SetDamageModifier(float damage) { m_flDamageModifier = damage; }

    inline float GetFireRateModifier() const { return m_flFireRateModifier; }
    inline void SetFireRateModifier(float fireRate) { m_flFireRateModifier = fireRate; }

    inline float GetSpreadModifier() const { return m_flSpreadModifier; }
    inline void SetSpreadModifier(float spread) { m_flSpreadModifier = spread; }

    bool IsCompatibleWithWeapon(const char* WeaponClassName);
    bool IsCompatibleWithWeapon(CBaseModularWeapon* pWeapon);

    virtual const char* GetModel() { return ""; };

    // Method to add compatible weapons
    void AddCompatibleWeapon(CBaseModularWeapon* pWeapon);
    void AddCompatibleWeapon(const char* szWeaponClassName);

    template<typename ...Args>
    void AddCompatibleWeapons(Args ...args);

private:
    AttachmentType_t m_AttachmentType;
    float m_flDamageModifier;
    float m_flFireRateModifier;
    float m_flSpreadModifier;

    bool IsDuplicate(CUtlVector<const char*>& vec, const char* str)
    {
        for (int i = 0; i < vec.Count(); i++)
        {
            if (!V_strcmp(vec[i], str))
            {
                return true;
            }
        }
        return false;
    }

    void AddWeapon(const char* szWeaponClassName);

    CUtlVector<const char*> m_CompatibleWeapons;    // List of compatible weapons
};

// Factory system for attachment registration
class CAttachmentFactory
{
public:
    typedef CBaseWeaponAttachment* (*CreateAttachmentFn)();

    static void RegisterAttachmentClass(AttachmentType_t type, const char* className, CreateAttachmentFn createFn);
    static CBaseWeaponAttachment* CreateAttachment(AttachmentType_t type, const char* className);

private:
    struct AttachmentCreator
    {
        char m_ClassName[64];
        CreateAttachmentFn m_CreateFn;

        AttachmentCreator(const char* name, CreateAttachmentFn fn);
    };

    static CUtlMap<AttachmentType_t, CUtlVector<AttachmentCreator>*> m_Creators;
};

// Macro to register an attachment class
#define REGISTER_ATTACHMENT_CLASS(className, attachmentType) \
    static CBaseWeaponAttachment* Create_##className() { return new className(); } \
    static struct className##_Register { \
        className##_Register() { \
            CAttachmentFactory::RegisterAttachmentClass(attachmentType, #className, Create_##className); \
        } \
    } g_##className##_Register;

// Helper functions for saving/restoring vector of attachments
void SaveWeaponAttachments(CSave& save, CUtlVector<CBaseWeaponAttachment*>& attachments);
void RestoreWeaponAttachments(CRestore& restore, CUtlVector<CBaseWeaponAttachment*>& attachments);
#endif // !BASE_WEAPON_ATTACHMENT_H