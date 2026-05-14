// silencer_attachment.h
#ifndef SIGHT_ATTACHMENT_H
#define SIGHT_ATTACHMENT_H
#include "cbase.h"
#include "baseattachment.h"
//#include "basemodularweapon.h"

#ifdef CLIENT_DLL
#define CSightAttachment C_SightAttachment
#endif // CLIENT_DLL


class CSightAttachment : public CBaseWeaponAttachment
{
public:
    DECLARE_CLASS(CSightAttachment, CBaseWeaponAttachment)

    //virtual const char* GetModel() { return "models/weapons/attachments/attachment_silencer.mdl"; };

    CSightAttachment();
    ~CSightAttachment();

    virtual void Spawn(void);

    // Required for factory system
    virtual const char* GetClassName() const { return "CSilencerAttachment"; }

    // Override Save/Restore to handle class-specific data
    virtual void Save(CSave& save);
    virtual void Restore(CRestore& restore);
};

#endif