//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Projectile shot from the AR2 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef	WEAPONAR2_H
#define	WEAPONAR2_H

#include "basegrenade_shared.h"
#include "weapon_hl2mpbase_machinegun.h"

#ifdef CLIENT_DLL
#define CWeaponAR2 C_WeaponAR2
#endif

class CWeaponAR2 : public CHL2MPMachineGun
{
public:
	DECLARE_CLASS( CWeaponAR2, CHL2MPMachineGun );

	CWeaponAR2();

	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

#ifndef CLIENT_DLL
	DECLARE_DATADESC();
#endif
	DECLARE_ACTTABLE();

	void	ItemPostFrame( void );
	void	Precache( void );
	
	void	SecondaryAttack( void );
	void	DelayedAttack( void );

	const char *GetTracerType( void ) { return "AR2Tracer"; }

	void	AddViewKick( void );

#ifndef CLIENT_DLL
	void	FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles );
	void	FireNPCSecondaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles );
	void	Operator_ForceNPCFire( CBaseCombatCharacter  *pOperator, bool bSecondary );
	void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
#endif

	int		GetMinBurst( void ) { return 2; }
	int		GetMaxBurst( void ) { return 5; }
	float	GetFireRate( void ) { return 0.1f; }

	bool	CanHolster( void );
	bool	Reload( void );
	
#ifndef CLIENT_DLL
	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
#endif

	Activity	GetPrimaryAttackActivity( void );
	
	void	DoImpactEffect( trace_t &tr, int nDamageType );
	
	virtual bool Deploy( void );


	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone;
		
		cone = VECTOR_CONE_3DEGREES;

		return cone;
	}
	
	const WeaponProficiencyInfo_t *GetProficiencyValues();

private:
	CWeaponAR2( const CWeaponAR2 & );

protected:

	CNetworkVar( float,		m_flDelayedFire );
	CNetworkVar( bool,		m_bShotDelayed );
	int						m_nVentPose;
};


#endif	//WEAPONAR2_H
