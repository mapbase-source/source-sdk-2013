//------------------------------------------------------------------------------------------//
//	Base Modular Weapon
// 
//	This is the base for making modular weapons which can change attachemnts on them
//	
//  Author: Nbc66
//	Last Modified: 2025-2-24
//------------------------------------------------------------------------------------------//

#include "cbase.h"

#include "basemodularweapon.h"
#include "saverestore_utlmap.h"
#include "ammodef.h"

#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED(BaseModularWeapon, DT_BaseModularWeapon)

BEGIN_NETWORK_TABLE(CBaseModularWeapon, DT_BaseModularWeapon)
#ifdef GAME_DLL
SendPropExclude("DT_AnimTimeMustBeFirst", "m_flAnimTime"),
SendPropExclude("DT_BaseAnimating", "m_nSequence"),
SendPropArray3(SENDINFO_ARRAY3(m_hAttachmentEnts), SendPropEHandle(SENDINFO_ARRAY(m_hAttachmentEnts))),
SendPropInt(SENDINFO(m_bActiveMerge), 1, SPROP_UNSIGNED),
#else
RecvPropArray3(RECVINFO_ARRAY(m_hAttachmentEnts), RecvPropEHandle(RECVINFO(m_hAttachmentEnts[0]))),
RecvPropInt(RECVINFO(m_bActiveMerge)),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA(CBaseModularWeapon)
DEFINE_PRED_FIELD(m_flTimeWeaponIdle, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK),
END_PREDICTION_DATA()
#endif

#ifdef GAME_DLL
BEGIN_DATADESC(CBaseModularWeapon)
END_DATADESC()
#endif

CBaseModularWeapon::CBaseModularWeapon()
{
	m_bActiveMerge = false;
	m_Attachments.SetLessFunc(DefLessFunc(AttachmentType_t));
}

CBaseModularWeapon::~CBaseModularWeapon()
{
}

#ifdef CLIENT_DLL
void CBaseModularWeapon::ClientThink()
{
	CBasePlayer* pPlayer = CBasePlayer::GetLocalPlayer();
	C_BaseViewModel* pVM = pPlayer->GetViewModel();
	if (pVM)
	{
		for (int i = 0; i < (int)(m_hAttachmentEnts.Count()); i++)
		{
			CBaseWeaponAttachment* attachment = m_hAttachmentEnts[i].Get();
			if (attachment) {
				PrecacheModel(attachment->GetModel());
				attachment->AddEffects(EF_BONEMERGE | EF_BONEMERGE_FASTCULL | EF_PARENT_ANIMATES);
				attachment->InitializeAsClientEntity(attachment->GetModel(), RENDER_GROUP_VIEW_MODEL_TRANSLUCENT);
				SetParent(pVM);
				SetLocalOrigin(vec3_origin);
				AddSolidFlags(FSOLID_NOT_SOLID);
			}
		}
	}
}

void CBaseModularWeapon::OnDataChanged(DataUpdateType_t updateType)
{
	BaseClass::OnDataChanged(updateType);
	if (updateType == DATA_UPDATE_CREATED)
	{
		SetNextClientThink(CLIENT_THINK_ALWAYS);
	}
}
#endif // CLIENT_DLL

void CBaseModularWeapon::ItemPreFrame(void)
{
	BaseClass::ItemPreFrame();
}

void CBaseModularWeapon::EquipAttachment(CBaseWeaponAttachment* pAttachment)
{
	if (pAttachment)
	{
		switch (pAttachment->GetAttachmentType())
		{
		case ATTACHMENT_SILENCER:
		{
			if (pAttachment->IsCompatibleWithWeapon(this))
			{
				
				m_Attachments.Insert(ATTACHMENT_SILENCER, pAttachment);
				m_hAttachmentEnts.Set(ATTACHMENT_SILENCER, pAttachment);
			}
			break;
		}

		default:
			break;
		}
	}

	return;
}

void CBaseModularWeapon::RemoveAttachment(AttachmentType_t type)
{
	return; //TODO: Implement
}

//Damage on Weapons is calculated using a multiplier from the attachment if you want to do more damage the attachment should do for example 1.1x the normal damage or if its less something like 0.9x damage
float CBaseModularWeapon::GetDamage()
{
	// Start with the base damage from ammo settings
	float totalDamage = GetAmmoDef()->GetAmmoOfIndex(this->GetPrimaryAmmoType())->pPlrDmgCVar->GetFloat();

	for (int iter = m_Attachments.FirstInorder(); iter != m_Attachments.InvalidIndex(); iter = m_Attachments.NextInorder(iter))
	{
		// Access the attachment type (key) and attachment (value)
		AttachmentType_t attachmentType = m_Attachments.Key(iter);
		CBaseWeaponAttachment* pAttachment = m_Attachments[attachmentType];  // Access the attachment using the key

		// If attachment is valid, apply its damage modifier
		if (pAttachment)
		{
			totalDamage *= pAttachment->GetDamageModifier();  // Apply the damage modifier for this attachment
		}
	}

	// Return the total calculated damage after applying all active attachments
	return totalDamage;
}

//only change here is that now we set the FireBullets damage from the weapon
void CBaseModularWeapon::PrimaryAttack(void)
{
	// If my clip is empty (and I use clips) start reload
	if (UsesClipsForAmmo1() && !m_iClip1)
	{
		Reload();
		return;
	}

	// Only the player fires this way so we can cast
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());

	if (!pPlayer)
	{
		return;
	}

	pPlayer->DoMuzzleFlash();

	SendWeaponAnim(GetPrimaryAttackActivity());

	// player "shoot" animation
	pPlayer->SetAnimation(PLAYER_ATTACK1);

	FireBulletsInfo_t info;
	info.m_vecSrc = pPlayer->Weapon_ShootPosition();

	info.m_vecDirShooting = pPlayer->GetAutoaimVector(AUTOAIM_SCALE_DEFAULT);

	// To make the firing framerate independent, we may have to fire more than one bullet here on low-framerate systems, 
	// especially if the weapon we're firing has a really fast rate of fire.
	info.m_iShots = 0;
	float fireRate = GetFireRate();

	while (m_flNextPrimaryAttack <= gpGlobals->curtime)
	{
		// MUST call sound before removing a round from the clip of a CMachineGun
		WeaponSound(SINGLE, m_flNextPrimaryAttack);
		m_flNextPrimaryAttack = m_flNextPrimaryAttack + fireRate;
		info.m_iShots++;
		if (!fireRate)
			break;
	}

	// Make sure we don't fire more than the amount in the clip
	if (UsesClipsForAmmo1())
	{
		info.m_iShots = MIN(info.m_iShots, m_iClip1);
		m_iClip1 -= info.m_iShots;
	}
	else
	{
		info.m_iShots = MIN(info.m_iShots, pPlayer->GetAmmoCount(m_iPrimaryAmmoType));
		pPlayer->RemoveAmmo(info.m_iShots, m_iPrimaryAmmoType);
	}

	info.m_flDistance = MAX_TRACE_LENGTH;
	info.m_iAmmoType = m_iPrimaryAmmoType;
	info.m_iTracerFreq = 2;

#if !defined( CLIENT_DLL )
	// Fire the bullets
	info.m_vecSpread = pPlayer->GetAttackSpread(this);
	info.SetPlayerDamage(GetDamage());
#else
	//!!!HACKHACK - what does the client want this function for? 
	info.m_vecSpread = GetActiveWeapon()->GetBulletSpread();
#endif // CLIENT_DLL

	pPlayer->FireBullets(info);

	if (!m_iClip1 && pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
	}

	//Add our view kick in
	AddViewKick();
}