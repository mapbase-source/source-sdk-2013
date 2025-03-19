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
//SendPropArray3(SENDINFO_ARRAY3(m_hAttachmentEnts), SendPropEHandle(SENDINFO_ARRAY(m_hAttachmentEnts))),
SendPropEHandle(SENDINFO(LastAttachment))
#else
//RecvPropArray3(RECVINFO_ARRAY(m_hAttachmentEnts), RecvPropEHandle(RECVINFO(m_hAttachmentEnts[0]))),
RecvPropEHandle(RECVINFO(LastAttachment)),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA(CBaseModularWeapon)
DEFINE_PRED_FIELD(m_flTimeWeaponIdle, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK),
END_PREDICTION_DATA()
#endif

#ifdef GAME_DLL
BEGIN_DATADESC(CBaseModularWeapon)
DEFINE_UTLMAP(m_Attachments,FIELD_INTEGER, FIELD_CLASSPTR),
END_DATADESC()
#endif

CBaseModularWeapon::CBaseModularWeapon()
{
	m_Attachments.SetLessFunc(DefLessFunc(AttachmentType_t));
}

CBaseModularWeapon::~CBaseModularWeapon()
{
}

#ifdef CLIENT_DLL
void CBaseModularWeapon::ClientThink()
{
	//CBasePlayer* pPlayer = CBasePlayer::GetLocalPlayer();
	//C_BaseViewModel* pVM = pPlayer->GetViewModel();
	//if (pVM)
	//{
	//	for (int i = 0; i < (int)(m_hAttachmentEnts.Count()); i++)
	//	{
	//		CBaseWeaponAttachment* attachment = m_hAttachmentEnts[i].Get();
	//		if (attachment) {
	//			PrecacheModel(attachment->GetModel());
	//			attachment->AddEffects(EF_BONEMERGE | EF_BONEMERGE_FASTCULL | EF_PARENT_ANIMATES);
	//			attachment->InitializeAsClientEntity(attachment->GetModel(), RENDER_GROUP_VIEW_MODEL_TRANSLUCENT);
	//			SetParent(pVM);
	//			SetLocalOrigin(vec3_origin);
	//			AddSolidFlags(FSOLID_NOT_SOLID);
	//		}
	//	}
	//}
}

void CBaseModularWeapon::OnDataChanged(DataUpdateType_t updateType)
{
	BaseClass::OnDataChanged(updateType);

	if (updateType == DATA_UPDATE_DATATABLE_CHANGED)
	{
		if (LastAttachment.Get())
		{
			static CBaseHandle oldindex;
			if (LastAttachment.m_Value != oldindex)
			{
				oldindex = LastAttachment.m_Value;
				LastAttachment->GetCompatibleWeapons(LastAttachment->GetCompatibleWeaponsVec());
				EquipAttachment(LastAttachment);
			}

			//for (int i = m_Attachments.FirstInorder(); i != m_Attachments.InvalidIndex(); i = m_Attachments.NextInorder(i))
			//{
			//	CBaseWeaponAttachment* pAttachment = m_Attachments.Element(i);
			//	if (pAttachment)
			//	{
			//
			//		static bool weapon_visible = false;
			//		if (IsWeaponVisible() && !weapon_visible)
			//		{
			//			pAttachment->FollowEntity(this);
			//			pAttachment->SetParent(this);
			//			weapon_visible = true;
			//		}
			//		else if (!IsWeaponVisible() && weapon_visible)
			//		{
			//			weapon_visible = false;
			//		}
			//	}
			//}
		}

	}
}
#endif // CLIENT_DLL

// TODO: use this somewhere
////CBasePlayer* pOwner = ToBasePlayer(GetOwner());
//BaseClass::SetWeaponVisible(visible);
//for (int i = 0; i < (int)(m_Attachments.Count()); i++)
//{
//	if (m_Attachments.IsValidIndex(i)) {
//		EquipAttachment(m_Attachments[i]);
//	}
//}

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
#ifdef GAME_DLL
				LastAttachment.GetForModify() = pAttachment;
#endif // GAME_DLL

				unsigned short index = m_Attachments.Insert(ATTACHMENT_SILENCER, pAttachment); 
				if (m_Attachments[index])
				{
					CBasePlayer* pPlayer = ToBasePlayer(GetOwnerEntity());
					if (pPlayer)
					{
						CBaseViewModel* pVM = pPlayer->GetViewModel();

						if (pVM)
						{

							pAttachment->SetParent(pVM);
							pAttachment->FollowEntity(pVM);
							pAttachment->AddSolidFlags(FSOLID_NOT_SOLID);
#ifdef GAME_DLL
							pAttachment->SetLightingOrigin(pVM);
#endif // CLIENT_DLL
						}
					}
				}
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

void CBaseModularWeapon::SetWeaponVisible(bool visible)
{
	BaseClass::SetWeaponVisible(visible);
	for (int i = m_Attachments.FirstInorder(); i != m_Attachments.InvalidIndex(); i = m_Attachments.NextInorder(i))
	{
		CBaseWeaponAttachment* pAttachment = m_Attachments.Element(i);
		CBasePlayer* pPlayer = ToBasePlayer(GetOwnerEntity());
		if (pAttachment && pPlayer)
		{
			if (visible)
			{
				pAttachment->RemoveEffects(EF_NODRAW);
				pAttachment->SetParent(pPlayer->GetViewModel());
				pAttachment->FollowEntity(pPlayer->GetViewModel());
#ifdef GAME_DLL
				pAttachment->SetLightingOrigin(pPlayer->GetViewModel());
#endif // GAME_DLL

			}
			else
			{
				pAttachment->AddEffects(EF_NODRAW);
			}
				
		}
	}
}

bool CBaseModularWeapon::Holster(CBaseCombatWeapon* pSwitchingTo)
{
	SetWeaponVisible(false);
	return BaseClass::Holster(pSwitchingTo);
}

//Damage on Weapons is calculated using a multiplier from the attachment if you want to do more damage the attachment should do for example 1.1x the normal damage or if its less something like 0.9x damage
float CBaseModularWeapon::GetDamage()
{
	// Start with the base damage from ammo settings
	float totalDamage = GetAmmoDef()->GetAmmoOfIndex(this->GetPrimaryAmmoType())->pPlrDmgCVar->GetFloat();

	// Server-side logic remains the same
	for (int i = m_Attachments.FirstInorder(); i != m_Attachments.InvalidIndex(); i = m_Attachments.NextInorder(i))
	{
		CBaseWeaponAttachment* pAttachment = m_Attachments.Element(i);
		if (pAttachment)
		{
 			totalDamage *= pAttachment->GetDamageModifier();
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
	float flDmg = GetDamage();
	info.SetPlayerDamage(flDmg);
	info.SetDamage(flDmg);
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