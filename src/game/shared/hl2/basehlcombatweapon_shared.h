//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "basecombatweapon_shared.h"
#include "hl2_weapon_parse.h"

#ifndef BASEHLCOMBATWEAPON_SHARED_H
#define BASEHLCOMBATWEAPON_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#if defined( CLIENT_DLL )
#define CBaseHLCombatWeapon C_BaseHLCombatWeapon
#endif

#if defined(HL2_DLL) || defined(HL2MP) // HL2MP is effectively used here as "present on client in MP"
#define USE_BACKUP_WEAPON_ACTIVITIES 1
#endif

class CBaseHLCombatWeapon : public CBaseCombatWeapon
{
#if !defined( CLIENT_DLL )
#ifndef _XBOX
	DECLARE_DATADESC();
#else
protected:
	DECLARE_DATADESC();
private:
#endif
#endif

	DECLARE_CLASS( CBaseHLCombatWeapon, CBaseCombatWeapon );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
#ifdef MAPBASE_VSCRIPT
	DECLARE_ENT_SCRIPTDESC();
#endif

	virtual bool	WeaponShouldBeLowered( void );

			bool	CanLower();
	virtual bool	Ready( void );
	virtual bool	Lower( void );
	virtual bool	Deploy( void );
	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual void	WeaponIdle( void );

	virtual void	AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles );
	virtual	float	CalcViewmodelBob( void );

	virtual Vector	GetBulletSpread( WeaponProficiency_t proficiency );
	virtual float	GetSpreadBias( WeaponProficiency_t proficiency );

	virtual const	WeaponProficiencyInfo_t *GetProficiencyValues();
	static const	WeaponProficiencyInfo_t *GetDefaultProficiencyValues();

	virtual void	ItemHolsterFrame( void );

#ifdef MAPBASE
	virtual WeaponClass_t	WeaponClassify();
	static WeaponClass_t	WeaponClassFromString( const char *str );

	virtual bool		SupportsBackupActivity( Activity activity );
	static acttable_t	*GetDefaultBackupActivityList( acttable_t *pTable, int &actCount );

	virtual const char *GetViewModel( int viewmodelindex = 0 ) const;
	virtual float	GetViewmodelFOVOverride() const;
	float			GetBobScale() const;
	float			GetSwayScale() const;
	float			GetSwaySpeedScale() const;
	virtual bool	CanBePickedUpByNPCs( void );
	virtual bool	UsesHands( void ) const;
#endif

#ifdef MAPBASE_VSCRIPT
	int				ScriptWeaponClassify() { return WeaponClassify(); }
#endif

	// Get HL2 weapon specific weapon data.
	CHL2WeaponInfo const	&GetHL2WpnData() const;

	int				m_iPrimaryAttacks;		// # of primary attacks performed with this weapon
	int				m_iSecondaryAttacks;	// # of secondary attacks performed with this weapon

protected:

	bool			m_bLowered;			// Whether the viewmodel is raised or lowered
	float			m_flRaiseTime;		// If lowered, the time we should raise the viewmodel
	float			m_flHolsterTime;	// When the weapon was holstered
};

#endif // BASEHLCOMBATWEAPON_SHARED_H
