//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "basehlcombatweapon.h"
#include "npcevent.h"
#include "basecombatcharacter.h"
#include "ai_basenpc.h"
#include "player.h"
#include "game.h"
#include "in_buttons.h"
#include "grenade_ar2.h"
#include "ai_memory.h"
#include "soundent.h"
#include "rumble_shared.h"
#include "gamestats.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// extern ConVar    sk_plr_dmg_smg1_grenade;

class CWeaponLongStrike : public CHLSelectFireMachineGun
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS(CWeaponLongStrike, CHLSelectFireMachineGun);

	CWeaponLongStrike();

	DECLARE_SERVERCLASS();

	//	void	Precache( void );
	void	AddViewKick(void);
	//	void	SecondaryAttack( void );

	int		GetMinBurst() { return 2; }
	int		GetMaxBurst() { return 5; }

	virtual void Equip(CBaseCombatCharacter *pOwner);
	bool	Reload(void);

	float	GetFireRate(void) { return 1.00000f; }	// idk
	int		CapabilitiesGet(void) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	//	int		WeaponRangeAttack2Condition( float flDot, float flDist );
	Activity	GetPrimaryAttackActivity(void);

	virtual const Vector& GetBulletSpread(void)
	{
		static const Vector cone = VECTOR_CONE_1DEGREES;
		return cone;
	}

	const WeaponProficiencyInfo_t *GetProficiencyValues();

	void FireNPCPrimaryAttack(CBaseCombatCharacter *pOperator, Vector &vecShootOrigin, Vector &vecShootDir);
	void Operator_ForceNPCFire(CBaseCombatCharacter  *pOperator, bool bSecondary);
	void Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator);

	DECLARE_ACTTABLE();

protected:

	Vector	m_vecTossVelocity;
	float	m_flNextGrenadeCheck;
};

IMPLEMENT_SERVERCLASS_ST(CWeaponLongStrike, DT_WeaponLongStrike)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(weapon_longstrike, CWeaponLongStrike);
PRECACHE_WEAPON_REGISTER(weapon_longstrike);

BEGIN_DATADESC(CWeaponLongStrike)

DEFINE_FIELD(m_vecTossVelocity, FIELD_VECTOR),
DEFINE_FIELD(m_flNextGrenadeCheck, FIELD_TIME),

END_DATADESC()

acttable_t	CWeaponLongStrike::m_acttable[] =
{
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_SMG1,			true },
	{ ACT_RELOAD,					ACT_RELOAD_SMG1,				true },
	{ ACT_IDLE,						ACT_IDLE_SMG1,					true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_SMG1,			true },

	{ ACT_WALK,						ACT_WALK_RIFLE,					true },
	{ ACT_WALK_AIM,					ACT_WALK_AIM_RIFLE,				true  },

	// Readiness activities (not aiming)
		{ ACT_IDLE_RELAXED,				ACT_IDLE_SMG1_RELAXED,			false },//never aims
		{ ACT_IDLE_STIMULATED,			ACT_IDLE_SMG1_STIMULATED,		false },
		{ ACT_IDLE_AGITATED,			ACT_IDLE_ANGRY_SMG1,			false },//always aims

		{ ACT_WALK_RELAXED,				ACT_WALK_RIFLE_RELAXED,			false },//never aims
		{ ACT_WALK_STIMULATED,			ACT_WALK_RIFLE_STIMULATED,		false },
		{ ACT_WALK_AGITATED,			ACT_WALK_AIM_RIFLE,				false },//always aims

		{ ACT_RUN_RELAXED,				ACT_RUN_RIFLE_RELAXED,			false },//never aims
		{ ACT_RUN_STIMULATED,			ACT_RUN_RIFLE_STIMULATED,		false },
		{ ACT_RUN_AGITATED,				ACT_RUN_AIM_RIFLE,				false },//always aims

	// Readiness activities (aiming)
		{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_SMG1_RELAXED,			false },//never aims	
		{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_RIFLE_STIMULATED,	false },
		{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_SMG1,			false },//always aims

		{ ACT_WALK_AIM_RELAXED,			ACT_WALK_RIFLE_RELAXED,			false },//never aims
		{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_RIFLE_STIMULATED,	false },
		{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_RIFLE,				false },//always aims

		{ ACT_RUN_AIM_RELAXED,			ACT_RUN_RIFLE_RELAXED,			false },//never aims
		{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_RIFLE_STIMULATED,	false },
		{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_RIFLE,				false },//always aims
	//End readiness activities

		{ ACT_WALK_AIM,					ACT_WALK_AIM_RIFLE,				true },
		{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RIFLE,			true },
		{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_RIFLE,		true },
		{ ACT_RUN,						ACT_RUN_RIFLE,					true },
		{ ACT_RUN_AIM,					ACT_RUN_AIM_RIFLE,				true },
		{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RIFLE,			true },
		{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_RIFLE,		true },
		{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_SMG1,	true },
		{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_SMG1_LOW,		true },
		{ ACT_COVER_LOW,				ACT_COVER_SMG1_LOW,				false },
		{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_SMG1_LOW,			false },
		{ ACT_RELOAD_LOW,				ACT_RELOAD_SMG1_LOW,			false },
		{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_SMG1,		true },
};

IMPLEMENT_ACTTABLE(CWeaponLongStrike);

//=========================================================
CWeaponLongStrike::CWeaponLongStrike()
{
	m_fMinRange1 = 0;// No minimum range. 
	m_fMaxRange1 = 1400;

	m_bAltFiresUnderwater = false;
}

//-----------------------------------------------------------------------------
// Purpose: Give this weapon longer range when wielded by an ally NPC.
//-----------------------------------------------------------------------------
void CWeaponLongStrike::Equip(CBaseCombatCharacter *pOwner)
{
	if (pOwner->Classify() == CLASS_PLAYER_ALLY)
	{
		m_fMaxRange1 = 3000;
	}
	else
	{
		m_fMaxRange1 = 1400;
	}

	BaseClass::Equip(pOwner);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponLongStrike::FireNPCPrimaryAttack(CBaseCombatCharacter *pOperator, Vector &vecShootOrigin, Vector &vecShootDir)
{
	// FIXME: use the returned number of bullets to account for >10hz firerate
	WeaponSoundRealtime(SINGLE_NPC);

	CSoundEnt::InsertSound(SOUND_COMBAT | SOUND_CONTEXT_GUNFIRE, pOperator->GetAbsOrigin(), SOUNDENT_VOLUME_MACHINEGUN, 0.2, pOperator, SOUNDENT_CHANNEL_WEAPON, pOperator->GetEnemy());
	pOperator->FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_PRECALCULATED,
		MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 2, entindex(), 0);

	pOperator->DoMuzzleFlash();
	m_iClip1 = m_iClip1 - 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponLongStrike::Operator_ForceNPCFire(CBaseCombatCharacter *pOperator, bool bSecondary)
{
	// Ensure we have enough rounds in the clip
	m_iClip1++;

	Vector vecShootOrigin, vecShootDir;
	QAngle	angShootDir;
	GetAttachment(LookupAttachment("muzzle"), vecShootOrigin, angShootDir);
	AngleVectors(angShootDir, &vecShootDir);
	FireNPCPrimaryAttack(pOperator, vecShootOrigin, vecShootDir);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponLongStrike::Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator)
{
	switch (pEvent->event)
	{
	case EVENT_WEAPON_SMG1:
	{
		Vector vecShootOrigin, vecShootDir;
		QAngle angDiscard;

		// Support old style attachment point firing
		if ((pEvent->options == NULL) || (pEvent->options[0] == '\0') || (!pOperator->GetAttachment(pEvent->options, vecShootOrigin, angDiscard)))
		{
			vecShootOrigin = pOperator->Weapon_ShootPosition();
		}

		CAI_BaseNPC *npc = pOperator->MyNPCPointer();
		ASSERT(npc != NULL);
		vecShootDir = npc->GetActualShootTrajectory(vecShootOrigin);

		FireNPCPrimaryAttack(pOperator, vecShootOrigin, vecShootDir);
	}
	break;

	default:
		BaseClass::Operator_HandleAnimEvent(pEvent, pOperator);
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CWeaponLongStrike::GetPrimaryAttackActivity(void)
{
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
	return ACT_VM_PRIMARYATTACK;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CWeaponLongStrike::Reload(void)
{
	bool fRet;
	//	float fCacheTime = m_flNextSecondaryAttack;

	fRet = DefaultReload(GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD);
	if (fRet)
	{
		// Undo whatever the reload process has done to our secondary
		// attack timer. We allow you to interrupt reloading to fire
		// a grenade.
//		m_flNextSecondaryAttack = GetOwner()->m_flNextAttack = fCacheTime;

		WeaponSound(RELOAD);
	}

	return fRet;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponLongStrike::AddViewKick(void)
{
#define	EASY_DAMPEN			0.5f
#define	MAX_VERTICAL_KICK	1.0f	//Degrees
#define	SLIDE_LIMIT			2.0f	//Seconds

	//Get the view kick
	CBasePlayer *pPlayer = ToBasePlayer(GetOwner());

	if (pPlayer == NULL)
		return;

	DoMachineGunKick(pPlayer, EASY_DAMPEN, MAX_VERTICAL_KICK, m_fFireDuration, SLIDE_LIMIT);
}

#define	COMBINE_MIN_GRENADE_CLEAR_DIST 256

const WeaponProficiencyInfo_t *CWeaponLongStrike::GetProficiencyValues()
{
	return GetDefaultProficiencyValues();
}
