#include "cbase.h"
#include "attachment_sight.h"
#include "basemodularweapon.h"

PRECACHE_REGISTER(attachment_sight);

CSightAttachment::CSightAttachment()
{
	AddCompatibleWeapons({ "weapon_glock18c" });
	SetAttachmentType(ATTACHMENT_SCOPE);
	SetModelPath("models/weapons/attachments/attachment_sight.mdl");
}

CSightAttachment::~CSightAttachment()
{
}

void CSightAttachment::Spawn(void)
{
	BaseClass::Spawn();
}

void CSightAttachment::Save(CSave& save)
{
	BaseClass::Save(save);
}

void CSightAttachment::Restore(CRestore& restore)
{
	BaseClass::Restore(restore);
}

LINK_ENTITY_TO_CLASS(attachment_sight, CSightAttachment);

#ifndef CLIENT_DLL
// Should always be server-sided, also i'd probably move this somewhere else in the future -Wire
CON_COMMAND_F(fp_give_sight, "gives the weapon attachment sight to the player", FCVAR_CHEAT)
{
	CSightAttachment* pSight = CREATE_ENTITY(CSightAttachment, "attachment_sight");

	CBasePlayer* pPlayer = UTIL_GetLocalPlayer();

	if (pPlayer && pSight)
	{
		DispatchSpawn(pSight);
		CBaseModularWeapon* pWeapon = (CBaseModularWeapon*)(pPlayer->Weapon_OwnsThisType("weapon_glock18c"));

		if (pWeapon)
		{
			pWeapon->EquipAttachment(pSight);
		}
		else
		{
			pSight->Remove();
		}

	}
}
#endif // CLIENT_DLL