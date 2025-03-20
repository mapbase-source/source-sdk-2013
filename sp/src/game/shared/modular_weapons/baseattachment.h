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

class CBaseWeaponAttachment : public CBaseAnimating
{
public:
    DECLARE_CLASS(CBaseWeaponAttachment, CBaseAnimating)
#ifndef CLIENT_DLL
    DECLARE_DATADESC();
#endif // !CLIENT_DLL
    DECLARE_NETWORKCLASS();
    DECLARE_PREDICTABLE();

    CBaseWeaponAttachment();
    ~CBaseWeaponAttachment();

    virtual const char* GetClassName() { return nullptr; }; // Each derived class must implement this
    virtual void Save(CSave& save);
    virtual void Restore(CRestore& restore);
    virtual void Spawn();
    virtual void Precache(void);
#ifdef CLIENT_DLL
    virtual RenderGroup_t GetRenderGroup() { return RENDER_GROUP_VIEW_MODEL_TRANSLUCENT; };
    virtual void OnDataChanged(DataUpdateType_t updateType);
    virtual bool OnInternalDrawModel(ClientModelRenderInfo_t* pInfo);
#endif // CLIENT_DLL

#ifdef GAME_DLL
    virtual int	ShouldTransmit(const CCheckTransmitInfo* pInfo);
    virtual int	UpdateTransmitState(void);
    virtual void SetLightingOrigin(CBaseEntity* pLightingOrigin);
#endif // GAME_DLL

    virtual void UpdateAttachmentVisibility(void);

    inline AttachmentType_t GetAttachmentType() const { return m_AttachmentType.Get(); };
    inline void SetAttachmentType(AttachmentType_t type) { m_AttachmentType.GetForModify() = type; };

    inline float GetDamageModifier() const { return m_flDamageModifier.Get(); }
    inline void SetDamageModifier(float damage) { m_flDamageModifier.GetForModify() = damage; }

    inline float GetFireRateModifier() const { return m_flFireRateModifier.Get(); }
    inline void SetFireRateModifier(float fireRate) { m_flFireRateModifier.GetForModify() = fireRate; }

    inline float GetSpreadModifier() const { return m_flSpreadModifier.Get(); }
    inline void SetSpreadModifier(float spread) { m_flSpreadModifier.GetForModify() = spread; }

    bool IsCompatibleWithWeapon(const char* WeaponClassName);
    bool IsCompatibleWithWeapon(CBaseModularWeapon* pWeapon);

    inline const char* GetFireSound() { return m_szFireSound.Get(); };
    inline void SetFireSound(const char* szFireSound) { V_strncpy(m_szFireSound.GetForModify(), szFireSound, V_strlen(szFireSound)+1); };


    // Method to add compatible weapons
    void AddCompatibleWeapon(CBaseModularWeapon* pWeapon);
    void AddCompatibleWeapon(const char* szWeaponClassName);

    void AddCompatibleWeapons(std::initializer_list<const char*> weapons) {
        for (const auto& weapon : weapons) {
            AddWeapon(weapon);
        }
    }

#ifdef GAME_DLL
    void UpdateCompatibleWeaponList();
#endif // GAME_DLL

    CUtlVector<const char*>& GetCompatibleWeaponsVec() {
        return m_CompatibleWeapons;
    };

    virtual const char* GetModel() { return m_szAttacmentModel.Get(); };

    void SetModelPath(const char* szMDLPath)
    {
        V_strcpy(m_szAttacmentModel.GetForModify(), szMDLPath);
    }


    void GetCompatibleWeapons(CUtlVector<const char*>& vec);

private:
    CNetworkVar(AttachmentType_t, m_AttachmentType);
    CNetworkVar(float, m_flDamageModifier);
    CNetworkVar(float, m_flFireRateModifier);
    CNetworkVar(float, m_flSpreadModifier);
    CNetworkString(m_szFireSound, MAX_PATH);

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

    CNetworkString(m_CompatibleWeaponList, 512);
    CNetworkString(m_szAttacmentModel, MAX_PATH);
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