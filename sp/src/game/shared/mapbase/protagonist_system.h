//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose:		System to easily switch between player characters.
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef PROTAGONIST_SYSTEM_H
#define PROTAGONIST_SYSTEM_H
#ifdef _WIN32
#pragma once
#endif

#ifdef CLIENT_DLL
#include "c_baseplayer.h"
#else
#include "player.h"
#endif

#define MAX_PROTAGONIST_NAME	32

//=============================================================================
//=============================================================================
class CProtagonistSystem : public CAutoGameSystem
{
public:
	CProtagonistSystem() : m_Strings( 256, 0, &StringLessThan ) { }
	~CProtagonistSystem() { PurgeProtagonists(); }

private:
	
	struct ProtagonistData_t
	{
		ProtagonistData_t()
		{
#ifndef CLIENT_DLL
			for (int i = 0; i < NUM_HAND_RIG_TYPES; i++)
				pszHandModels[i] = NULL;
#endif
		}

		char szName[MAX_PROTAGONIST_NAME];

		CUtlVector<int>	vecParents;

#ifdef CLIENT_DLL
#else
		// Playermodel
		const char *pszPlayerModel = NULL;
		int nPlayerSkin = -1;
		int nPlayerBody = -1;

		// Hands
		const char *pszHandModels[NUM_HAND_RIG_TYPES];
		int nHandSkin = -1;
		int nHandBody = -1;

		// Responses
		const char *pszResponseContexts = NULL;

		// Multiplayer
		int nTeam = TEAM_ANY;

		// Precached (used by system, not actual data)
		bool bPrecached = false;
#endif

		// Weapon Data
		struct WeaponDataOverride_t
		{
			const char *pszVM = NULL;
			bool bUsesHands = false;
			int nHandRig = 0;
			float flVMFOV = 0.0f;
		};

		CUtlDict<WeaponDataOverride_t> dictWpnData;
	};

public:

	bool Init();
	void Shutdown();
	void LevelInitPreEntity();
	void LevelShutdownPostEntity();

	//----------------------------------------------------------------------------

	void LoadProtagonistManifest( const char *pszFile );
	void LoadProtagonistFile( const char *pszFile );

	//----------------------------------------------------------------------------

	int FindProtagonistIndex( const char *pszName );
	const char *FindProtagonistByModel( const char *pszModelName );

	void PrecacheProtagonist( CBaseEntity *pSource, int nIdx );

	//----------------------------------------------------------------------------

	#define DeclareProtagonistFunc(type, name, ...) \
		type GetProtagonist_##name( const CBasePlayer *pPlayer, ##__VA_ARGS__ ); \
		type GetProtagonist_##name( const int nProtagonistIndex, ##__VA_ARGS__ ); \
	private: \
		type DoGetProtagonist_##name( ProtagonistData_t &pProtag, ##__VA_ARGS__ ); \
	public: \

#ifdef CLIENT_DLL
#else
	DeclareProtagonistFunc( const char*,	PlayerModel )
	DeclareProtagonistFunc( int,			PlayerModelSkin )
	DeclareProtagonistFunc( int,			PlayerModelBody )
	DeclareProtagonistFunc( const char*,	HandModel, const CBaseCombatWeapon *pWeapon )
	DeclareProtagonistFunc( int,			HandModelSkin, const CBaseCombatWeapon *pWeapon )
	DeclareProtagonistFunc( int,			HandModelBody, const CBaseCombatWeapon *pWeapon )
	DeclareProtagonistFunc( bool,			ResponseContexts, char *pszContexts, int nContextsSize )
	DeclareProtagonistFunc( int,			Team )
#endif
	
	DeclareProtagonistFunc( const char*,	ViewModel, const CBaseCombatWeapon *pWeapon )
	DeclareProtagonistFunc( float*,			ViewModelFOV, const CBaseCombatWeapon *pWeapon )
	DeclareProtagonistFunc( bool*,			UsesHands, const CBaseCombatWeapon *pWeapon )
	DeclareProtagonistFunc( int*,			HandRig, const CBaseCombatWeapon *pWeapon )
	
	//----------------------------------------------------------------------------

	void PurgeProtagonists();
	void PrintProtagonistData();

	//----------------------------------------------------------------------------

private:

	ProtagonistData_t *GetPlayerProtagonist( const CBasePlayer *pPlayer );
	ProtagonistData_t *FindProtagonist( const char *pszName );
	ProtagonistData_t *FindProtagonist( int nIndex );

	//----------------------------------------------------------------------------

	const char *FindString( const char *string );
	const char *AllocateString( const char *string );
	
	//----------------------------------------------------------------------------

private:
	CUtlVector<ProtagonistData_t>	m_Protagonists;

	// Dedicated strings, copied from game string pool
	CUtlRBTree<const char *> m_Strings;

};

extern CProtagonistSystem g_ProtagonistSystem;

#endif // PROTAGONIST_SYSTEM_H
