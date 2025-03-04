#include "cbase.h"
#include "attachment_silencer.h"
#include "basemodularweapon.h"

CSilencerAttachment::CSilencerAttachment()
{
	AddCompatibleWeapon("weapon_glock18c");
	SetAttachmentType(ATTACHMENT_SILENCER);
	SetDamageModifier(0.5f);
}

CSilencerAttachment::~CSilencerAttachment()
{
}

void CSilencerAttachment::Spawn(void)
{
	BaseClass::Spawn();
}

void CSilencerAttachment::Precache(void)
{
	BaseClass::Precache();
}

void CSilencerAttachment::Save(CSave& save)
{
	BaseClass::Save(save);
}

void CSilencerAttachment::Restore(CRestore& restore)
{
	BaseClass::Restore(restore);
}
#ifdef CLIENT_DLL
CON_COMMAND_F(fp_give_silencer, "gives the silencer to the player", FCVAR_CHEAT)
{
	CSilencerAttachment* pSilencer = new CSilencerAttachment;

	CBasePlayer* pPlayer = CBasePlayer::GetLocalPlayer();

	if (pPlayer && pSilencer)
	{
		CBaseModularWeapon *pWeapon = (CBaseModularWeapon*)(pPlayer->Weapon_OwnsThisType("weapon_glock18c"));

		if (pWeapon)
		{
			pWeapon->EquipAttachment(pSilencer);
		}
		else
		{
			delete pSilencer;
		}

	}
}
#endif // CLIENT_DLL