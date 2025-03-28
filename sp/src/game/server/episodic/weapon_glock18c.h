#ifndef WEAPON_GLOCK18C_H
#define WEAPON_GLOCK18C_H
#include "cbase.h"
#include "weapon_pistol.h"

#include "basemodularweapon.h"

class CWeaponGlock18C : public CBaseModularWeapon
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS(CWeaponGlock18C, CBaseModularWeapon)

	CWeaponGlock18C(void);

	DECLARE_SERVERCLASS();

	virtual void	Precache(void);
	virtual void	ItemPostFrame(void);
	virtual void	ItemPreFrame(void);
	virtual void	ItemBusyFrame(void);
	virtual void	PrimaryAttack(void);
	virtual void	SecondaryAttack(void);
	virtual void	ToggleFireMode(void);
	virtual void	AddViewKick(void);
	virtual void	DryFire(void);
	virtual void	Operator_HandleAnimEvent(animevent_t* pEvent, CBaseCombatCharacter* pOperator);
#ifdef MAPBASE
	virtual void	FireNPCPrimaryAttack(CBaseCombatCharacter* pOperator, Vector& vecShootOrigin, Vector& vecShootDir);
	virtual void	Operator_ForceNPCFire(CBaseCombatCharacter* pOperator, bool bSecondary);
#endif

	virtual void	UpdatePenaltyTime(void);

	virtual int		CapabilitiesGet(void) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	Activity	GetPrimaryAttackActivity(void);

	virtual bool Reload(void);

	virtual const Vector& GetBulletSpread(void)
	{
		// Handle NPCs first
		static Vector npcCone = VECTOR_CONE_5DEGREES;
		if (GetOwner() && GetOwner()->IsNPC())
			return npcCone;

		static Vector cone;

		if (pistol_use_new_accuracy.GetBool())
		{
			float ramp = RemapValClamped(m_flAccuracyPenalty,
				0.0f,
				PISTOL_ACCURACY_MAXIMUM_PENALTY_TIME,
				0.0f,
				1.0f);

			// We lerp from very accurate to inaccurate over time
			VectorLerp(VECTOR_CONE_1DEGREES, VECTOR_CONE_6DEGREES, ramp, cone);
		}
		else
		{
			// Old value
			cone = VECTOR_CONE_4DEGREES;
		}

		return cone;
	}

	virtual int	GetMinBurst()
	{
		return 1;
	}

	virtual int	GetMaxBurst()
	{
		return 3;
	}

	virtual float GetFireRate(void)
	{
		switch (m_nFireMode.Get())
		{
		case FM_FULLAUTO:
			return 0.05f;
			break;
		case FM_BURST:
			return 0.05f;
			break;
		case FM_SINGLE:
			return 0.5f;
			break;
		default:
			return 0.5f;;
			break;
		}

		return 0.5f;
	}

	virtual float IsFullAuto() { return m_bFullAutoMode; }

#ifdef MAPBASE
	// Pistols are their own backup activities
	virtual acttable_t* GetBackupActivityList() { return NULL; }
	virtual int				GetBackupActivityListCount() { return 0; }
#endif

	DECLARE_ACTTABLE();

private:
	float	m_flSoonestPrimaryAttack;
	float	m_flLastAttackTime;
	float	m_flAccuracyPenalty;
	int		m_nNumShotsFired;
	bool	m_bFullAutoMode;
};
#endif //WEAPON_GLOCK18C_H