// silencer_attachment.h
#ifndef SILENCER_ATTACHMENT_H
#define SILENCER_ATTACHMENT_H
#include "cbase.h"
#include "baseattachment.h"

class CSilencerAttachment : public CBaseWeaponAttachment
{
public:
    DECLARE_CLASS(CSilencerAttachment, CBaseWeaponAttachment)

    CSilencerAttachment();
    virtual ~CSilencerAttachment();

    // Required for factory system
    virtual const char* GetClassName() const { return "CSilencerAttachment"; }

    // Override Save/Restore to handle class-specific data
    virtual void Save(CSave& save);
    virtual void Restore(CRestore& restore);

    // Class-specific methods
    virtual float GetNoiseReduction() { return m_flNoiseReduction; }

private:
    float m_flNoiseReduction;
};

#endif