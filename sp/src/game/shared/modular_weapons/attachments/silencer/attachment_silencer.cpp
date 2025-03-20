#include "cbase.h"
#include "attachment_silencer.h"
#include "basemodularweapon.h"

PRECACHE_REGISTER(silencerattachment);

CSilencerAttachment::CSilencerAttachment()
{
	AddCompatibleWeapons({ "weapon_glock18c" });
	SetAttachmentType(ATTACHMENT_SILENCER);
	SetDamageModifier(0.5f);
	SetFireRateModifier(1.0f);
	SetSpreadModifier(1.0f);
	SetModelPath("models/weapons/attachments/attachment_silencer.mdl");
	SetFireSound("Weapon_Glock18c.Silenced");
}

CSilencerAttachment::~CSilencerAttachment()
{
}

void CSilencerAttachment::Spawn(void)
{
	BaseClass::Spawn();
}

void CSilencerAttachment::Save(CSave& save)
{
	BaseClass::Save(save);
}

void CSilencerAttachment::Restore(CRestore& restore)
{
	BaseClass::Restore(restore);
}

LINK_ENTITY_TO_CLASS(silencerattachment, CSilencerAttachment);

#ifndef CLIENT_DLL
// Should always be server-sided, also i'd probably move this somewhere else in the future -Wire
CON_COMMAND_F(fp_give_silencer, "gives the silencer to the player", FCVAR_CHEAT)
{
	CSilencerAttachment* pSilencer = CREATE_ENTITY(CSilencerAttachment, "silencerattachment");

	CBasePlayer* pPlayer = UTIL_GetLocalPlayer();

	if (pPlayer && pSilencer)
	{
		DispatchSpawn(pSilencer);
		CBaseModularWeapon *pWeapon = (CBaseModularWeapon*)(pPlayer->Weapon_OwnsThisType("weapon_glock18c"));

		if (pWeapon)
		{
			pWeapon->EquipAttachment(pSilencer);
		}
		else
		{
			pSilencer->Remove();
		}

	}
}
#endif // CLIENT_DLL