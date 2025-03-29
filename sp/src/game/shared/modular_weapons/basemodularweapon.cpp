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
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "prediction.h"
#endif // CLIENT_DLL


//forward declarations of callbacks used by viewmodel_adjust_enable and viewmodel_adjust_fov
void vm_adjust_enable_callback(IConVar* pConVar, char const* pOldString, float flOldValue);
void vm_adjust_fov_callback(IConVar* pConVar, const char* pOldString, float flOldValue);

ConVar viewmodel_adjust_forward("viewmodel_adjust_forward", "0", FCVAR_REPLICATED);
ConVar viewmodel_adjust_right("viewmodel_adjust_right", "0", FCVAR_REPLICATED);
ConVar viewmodel_adjust_up("viewmodel_adjust_up", "0", FCVAR_REPLICATED);
ConVar viewmodel_adjust_pitch("viewmodel_adjust_pitch", "0", FCVAR_REPLICATED);
ConVar viewmodel_adjust_yaw("viewmodel_adjust_yaw", "0", FCVAR_REPLICATED);
ConVar viewmodel_adjust_roll("viewmodel_adjust_roll", "0", FCVAR_REPLICATED);
ConVar viewmodel_adjust_fov("viewmodel_adjust_fov", "0", FCVAR_REPLICATED, "Note: this feature is not available during any kind of zoom", vm_adjust_fov_callback);
ConVar viewmodel_adjust_enabled("viewmodel_adjust_enabled", "0", FCVAR_REPLICATED | FCVAR_CHEAT, "enabled viewmodel adjusting", vm_adjust_enable_callback);

#ifdef CLIENT_DLL
void CC_ToggleIronSights(void)
{
	CBasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (pPlayer == NULL)
		return;

	CBaseModularWeapon* pWeapon = ToModularWeapon(pPlayer->GetActiveWeapon());
	if (pWeapon == NULL)
		return;

	pWeapon->ToggleIronsights();

	engine->ServerCmd("toggle_ironsight"); //forward to server
}

static ConCommand toggle_ironsight("toggle_ironsight", CC_ToggleIronSights);
#endif


#ifdef CLIENT_DLL
void RecvProxy_ToggleSights(const CRecvProxyData* pData, void* pStruct, void* pOut)
{
	CBaseModularWeapon* pWeapon = ToModularWeapon((CBaseEntity*)pStruct);
	if (pWeapon)
	{
		if (pData->m_Value.m_Int)
			pWeapon->EnableIronsights();
		else
			pWeapon->DisableIronsights();
	}
}
#endif

IMPLEMENT_NETWORKCLASS_ALIASED(BaseModularWeapon, DT_BaseModularWeapon)

BEGIN_NETWORK_TABLE(CBaseModularWeapon, DT_BaseModularWeapon)
#ifdef GAME_DLL
SendPropExclude("DT_AnimTimeMustBeFirst", "m_flAnimTime"),
SendPropExclude("DT_BaseAnimating", "m_nSequence"),
//SendPropArray3(SENDINFO_ARRAY3(m_hAttachmentEnts), SendPropEHandle(SENDINFO_ARRAY(m_hAttachmentEnts))),
SendPropEHandle(SENDINFO(LastAttachment)),
SendPropBool(SENDINFO(m_bIsIronsighted)),
SendPropFloat(SENDINFO(m_flIronsightedTime)),
#else
//RecvPropArray3(RECVINFO_ARRAY(m_hAttachmentEnts), RecvPropEHandle(RECVINFO(m_hAttachmentEnts[0]))),
RecvPropEHandle(RECVINFO(LastAttachment)),
RecvPropInt(RECVINFO(m_bIsIronsighted), 0, RecvProxy_ToggleSights), //note: RecvPropBool is actually RecvPropInt (see its implementation), but we need a proxy
RecvPropFloat(RECVINFO(m_flIronsightedTime)),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA(CBaseModularWeapon)
DEFINE_PRED_FIELD(m_flTimeWeaponIdle, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK),
DEFINE_PRED_FIELD(m_bIsIronsighted, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE),
DEFINE_PRED_FIELD(m_flIronsightedTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE),
END_PREDICTION_DATA()
#endif

#ifdef GAME_DLL
BEGIN_DATADESC(CBaseModularWeapon)
DEFINE_UTLMAP(m_Attachments,FIELD_INTEGER, FIELD_CLASSPTR),
DEFINE_FIELD(m_bIsIronsighted, FIELD_BOOLEAN),
DEFINE_FIELD(m_flIronsightedTime, FIELD_FLOAT),
END_DATADESC()
#endif

CBaseModularWeapon::CBaseModularWeapon()
{
	m_Attachments.SetLessFunc(DefLessFunc(AttachmentType_t));
	m_bIsIronsighted.GetForModify() = false;
	m_flIronsightedTime.GetForModify() = 0.0f;
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

	CBasePlayer* pOwner = ToBasePlayer(GetOwner());

	if (pOwner == NULL)
		return;

	//Handle FireMode toggling
	//using secondary attack timer for this will probably change in the future
	//-Nbc66
	if (m_flNextSecondaryAttack < gpGlobals->curtime && (pOwner->m_nButtons & IN_FIREMODE))
	{

		//CBasePlayer* pPlayer = ToBasePlayer(GetOwner());

		ToggleFireMode();

		switch (m_nFireMode)
		{
		case FM_SINGLE:
			//(pPlayer, HUD_PRINTCENTER, FIREMODE_STRING, "Single");
			break;
		case FM_BURST:
			//ClientPrint(pPlayer, HUD_PRINTCENTER, FIREMODE_STRING, "Burst");
			break;
		case FM_FULLAUTO:
			//ClientPrint(pPlayer, HUD_PRINTCENTER, FIREMODE_STRING, "Full Auto");
			break;
		default:
			break;
		}

		m_flNextSecondaryAttack = gpGlobals->curtime + 1.0f;
	}
}

void CBaseModularWeapon::ItemPostFrame(void)
{
	BaseClass::ItemPostFrame();

	if (m_bInReload)
		return;

	// Burst firing timing control
	HandleBurstFire();
}

//Used to handle how burst fire works on a weapon
void CBaseModularWeapon::HandleBurstFire(void)
{
	CBasePlayer* pOwner = ToBasePlayer(GetOwner());

	if (pOwner == NULL)
		return;

	if (m_nFireMode == FM_BURST && burstFire > 0)
	{
		if (gpGlobals->curtime > m_flNextPrimaryAttack && (pOwner->m_nButtons & IN_ATTACK) == false) // Check for fire rate timing
		{
			if (burstFire < 3 && m_iClip1 > 0) // Ensure burst has not reached max shots
			{
				PrimaryAttack(); // Fire the next burst shot
			}
			else
			{
				burstFire = 0; // Reset burst counter after the burst is complete
			}
		}
	}
}

void CBaseModularWeapon::EquipAttachment(CBaseWeaponAttachment* pAttachment)
{
	if (pAttachment)
	{
		switch (pAttachment->GetAttachmentType())
		{
		case ATTACHMENT_SILENCER:
		case ATTACHMENT_SCOPE:
		{
			if (pAttachment->IsCompatibleWithWeapon(this))
			{
#ifdef GAME_DLL
				LastAttachment.GetForModify() = pAttachment;
#endif // GAME_DLL

				unsigned short index = m_Attachments.Insert(pAttachment->GetAttachmentType(), pAttachment);
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
	DisableIronsights();
	return BaseClass::Holster(pSwitchingTo);
}

bool CBaseModularWeapon::DefaultReload(int iClipSize1, int iClipSize2, int iActivity)
{
	//For Now We will dissable ADS when reloading will probably come back here when we add pose parrameter ads
	DisableIronsights();
	return BaseClass::DefaultReload(iClipSize1, iClipSize2, iActivity);
}

Vector CBaseModularWeapon::GetIronsightPositionOffset(void) const
{
	if (viewmodel_adjust_enabled.GetBool())
		return Vector(viewmodel_adjust_forward.GetFloat(), viewmodel_adjust_right.GetFloat(), viewmodel_adjust_up.GetFloat());
	
	//Check if we have a scope/sight attached to our weapon and use the attachments offset
	//-Nbc66
	unsigned int index = m_Attachments.Find(ATTACHMENT_SCOPE);
	if (index != m_Attachments.InvalidIndex())
	{
		CBaseWeaponAttachment* pAttachment = m_Attachments.Element(index);;
		if (pAttachment)
		{
			Attachment_Data data = pAttachment->GetAttachmentData();
			return Vector(data.VM_offset_forward, data.VM_offset_right, data.VM_offset_up);
		}
	}

	return GetWpnData().vecIronsightPosOffset;
}

QAngle CBaseModularWeapon::GetIronsightAngleOffset(void) const
{
	if (viewmodel_adjust_enabled.GetBool())
		return QAngle(viewmodel_adjust_pitch.GetFloat(), viewmodel_adjust_yaw.GetFloat(), viewmodel_adjust_roll.GetFloat());
	return GetWpnData().angIronsightAngOffset;
}

float CBaseModularWeapon::GetIronsightFOVOffset(void) const
{
	if (viewmodel_adjust_enabled.GetBool())
		return viewmodel_adjust_fov.GetFloat();
	return GetWpnData().flIronsightFOVOffset;
}

void vm_adjust_enable_callback(IConVar* pConVar, char const* pOldString, float flOldValue)
{
	ConVarRef sv_cheats("sv_cheats");
	if (!sv_cheats.IsValid() || sv_cheats.GetBool())
		return;

	ConVarRef var(pConVar);

	if (var.GetBool())
		var.SetValue("0");
}

void vm_adjust_fov_callback(IConVar* pConVar, char const* pOldString, float flOldValue)
{
	if (!viewmodel_adjust_enabled.GetBool())
		return;

	ConVarRef var(pConVar);

	CBasePlayer* pPlayer =
#ifdef GAME_DLL
		UTIL_GetCommandClient();
#else
		C_BasePlayer::GetLocalPlayer();
#endif
	if (!pPlayer)
		return;

	if (!pPlayer->SetFOV(pPlayer, pPlayer->GetDefaultFOV() + var.GetFloat(), 0.1f))
	{
		Warning("Could not set FOV\n");
		var.SetValue("0");
	}
}

bool CBaseModularWeapon::IsIronsighted(void)
{
	return (m_bIsIronsighted || viewmodel_adjust_enabled.GetBool());
}

void CBaseModularWeapon::ToggleIronsights(void)
{
	if (m_bIsIronsighted)
		DisableIronsights();
	else
		EnableIronsights();
}

void CBaseModularWeapon::EnableIronsights(void)
{
#ifdef CLIENT_DLL
	if (!prediction->IsFirstTimePredicted())
		return;
#endif
	if (!HasIronsights() || m_bIsIronsighted)
		return;

	CBasePlayer* pOwner = ToBasePlayer(GetOwner());

	if (!pOwner)
		return;

	if (pOwner->SetFOV(this, pOwner->GetDefaultFOV() + GetIronsightFOVOffset(), 1.0f)) //modify the last value to adjust how fast the fov is applied
	{
		m_bIsIronsighted = true;
		SetIronsightTime();
	}
}

void CBaseModularWeapon::DisableIronsights(void)
{
#ifdef CLIENT_DLL
	if (!prediction->IsFirstTimePredicted())
		return;
#endif
	if (!HasIronsights() || !m_bIsIronsighted)
		return;

	CBasePlayer* pOwner = ToBasePlayer(GetOwner());

	if (!pOwner)
		return;

	if (pOwner->SetFOV(this, 0, 0.4f)) //modify the last value to adjust how fast the fov is applied
	{
		m_bIsIronsighted = false;
		SetIronsightTime();
	}
}

void CBaseModularWeapon::SetIronsightTime(void)
{
	m_flIronsightedTime = gpGlobals->curtime;
}

//Override this so we can have custom weapon sounds based on the equiped attachment
char const* CBaseModularWeapon::GetShootSound(int iIndex) const
{
	if (iIndex == SINGLE)
	{
		int index = m_Attachments.Find(ATTACHMENT_SILENCER);
		if (index != m_Attachments.InvalidIndex())
		{
			CBaseWeaponAttachment* pAttachment = m_Attachments.Element(index);
			if (pAttachment)
			{
				return pAttachment->GetFireSound();
			}
		}
	}

	return BaseClass::GetShootSound(iIndex);
}



//Damage on Weapons is calculated using a multiplier from the attachment
//if you want to do more damage the attachment should do for example 1.1x the normal damage or if its less something like 0.9x damage
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

void CBaseModularWeapon::ToggleFireMode(void)
{
	if (m_nFireMode < FM_MAX_FIREMODE - 1)
	{
		m_nFireMode++;
	}
	else
	{
		m_nFireMode = 0;
	}
	EmitSound("Weapon.FireModeSwitch");
	burstFire = 0;
}
