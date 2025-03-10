// silencer_attachment.h
#ifndef SILENCER_ATTACHMENT_H
#define SILENCER_ATTACHMENT_H
#include "cbase.h"
#include "baseattachment.h"
//#include "basemodularweapon.h"

#ifdef CLIENT_DLL
#define CSilencerAttachment C_SilencerAttachment
#endif // CLIENT_DLL


class CSilencerAttachment : public CBaseWeaponAttachment
{
public:
    DECLARE_CLASS(CSilencerAttachment, CBaseWeaponAttachment)

    virtual const char* GetModel() { return "models/weapons/attachments/attachment_silencer.mdl"; };

    CSilencerAttachment();
    ~CSilencerAttachment();

    virtual void Spawn(void);

    // Required for factory system
    virtual const char* GetClassName() const { return "CSilencerAttachment"; }

    // Override Save/Restore to handle class-specific data
    virtual void Save(CSave& save);
    virtual void Restore(CRestore& restore);
};

#endif