#ifndef BASE_WEAPON_ATTACHMENT_H
#define BASE_WEAPON_ATTACHMENT_H
#include "cbase.h"

#ifdef CLIENT_DLL
#define CBaseModularWeapon C_BaseModularWeapon
#define CBaseWeaponAttachment C_BaseWeaponAttachment
#endif // CLIENT_DLL

class CBaseModularWeapon; // Forward declaration

enum AttachmentType_t
{
    ATTACHMENT_NONE = 0,     // No attachment
    ATTACHMENT_SILENCER,     // Reduces noise, changes firing sound
    ATTACHMENT_SCOPE,        // Changes ADS behavior, modifies zoom
    ATTACHMENT_MAG, // Increases ammo capacity
    ATTACHMENT_GRIP,         // Reduces recoil
    ATTACHMENT_LASER_SIGHT,  // Adds laser dot for hip-fire accuracy
    ATTACHMENT_FLASHLIGHT,   // Enables flashlight
    ATTACHMENT_BARREL,       // Modifies bullet spread
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

    AttachmentType_t GetAttachmentType();

    virtual float GetDamageModifier();
    virtual float GetFireRateModifier();
    virtual float GetSpreadModifier();

    bool IsCompatibleWithWeapon(const char* WeaponClassName);

    bool IsCompatibleWithWeapon(CBaseModularWeapon* pWeapon);

    // Method to add compatible weapons
    void AddCompatibleWeapon(CBaseModularWeapon* pWeapon);


private:
    AttachmentType_t m_AttachmentType;
    float m_flDamageModifier;
    float m_flFireRateModifier;
    float m_flSpreadModifier;

    CUtlVector<const char*> m_CompatibleWeapons;  // List of compatible weapons

};
#endif // !BASE_WEAPON_ATTACHMENT_H