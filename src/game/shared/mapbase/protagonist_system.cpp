//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose:		System to easily switch between player characters.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "protagonist_system.h"
#include "weapon_parse.h"
#include "filesystem.h"
#include "hl2_player_shared.h"
#ifdef HL2MP
#include "hl2mp_gamerules.h"
#endif

CProtagonistSystem g_ProtagonistSystem;

//=============================================================================
//=============================================================================
bool CProtagonistSystem::Init()
{
	return true;
}

void CProtagonistSystem::Shutdown()
{
	PurgeProtagonists();
}

void CProtagonistSystem::LevelInitPreEntity()
{
	LoadProtagonistManifest( "scripts/protagonists/protagonists_manifest.txt" );
}

void CProtagonistSystem::LevelShutdownPostEntity()
{
	PurgeProtagonists();
}

//----------------------------------------------------------------------------

void CProtagonistSystem::LoadProtagonistManifest( const char *pszFile )
{
	KeyValues *pManifest = new KeyValues( "ProtagonistsManifest" );
	if (pManifest->LoadFromFile( filesystem, pszFile ))
	{
		FOR_EACH_SUBKEY( pManifest, pSubKey )
		{
			LoadProtagonistFile( pSubKey->GetString() );
		}
	}
	pManifest->deleteThis();
}

void CProtagonistSystem::LoadProtagonistFile( const char *pszFile )
{
	KeyValues *pManifest = new KeyValues( "Protagonists" );
	if (pManifest->LoadFromFile( filesystem, pszFile ))
	{
		FOR_EACH_SUBKEY( pManifest, pProtagKey )
		{
			if (!pProtagKey->GetName()[0])
				continue;

			ProtagonistData_t *pProtag = FindProtagonist( pProtagKey->GetName() );
			if (!pProtag)
			{
				pProtag = &m_Protagonists[m_Protagonists.AddToTail()];
				V_strncpy( pProtag->szName, pProtagKey->GetName(), sizeof( pProtag->szName ) );
			}

			FOR_EACH_SUBKEY( pProtagKey, pSubKey )
			{
				const char *pszSubKeyName = pSubKey->GetName();

				//----------------------------------------------------------------------------
				// Metadata
				//----------------------------------------------------------------------------
				if (FStrEq( pszSubKeyName, "inherits_from" ))
				{
					char szParents[128];
					V_strncpy( szParents, pSubKey->GetString(), sizeof( szParents ) );

					char *pszToken = strtok( szParents, ";" );
					for (; pszToken != NULL; pszToken = strtok( NULL, ";" ))
					{
						if (!pszToken || !*pszToken)
							continue;

						int nParent = FindProtagonistIndex( pszToken );
						if (nParent >= 0)
						{
							pProtag->vecParents.FindAndRemove( nParent ); // If it already exists, it will be moved to the front

							pProtag->vecParents.AddToTail( nParent );
						}
					}
				}

#ifdef CLIENT_DLL
#else
				//----------------------------------------------------------------------------
				// Playermodel
				//----------------------------------------------------------------------------
				else if (V_strnicmp( pszSubKeyName, "playermodel", 11 ) == 0)
				{
					pszSubKeyName += 11;
					if (!pszSubKeyName[0])
					{
						// Model
						pProtag->pszPlayerModel = AllocateString( pSubKey->GetString() );
					}
					else if (FStrEq( pszSubKeyName, "_skin" ))
					{
						// Skin
						pProtag->nPlayerSkin = pSubKey->GetInt();
					}
					else if (FStrEq( pszSubKeyName, "_body" ))
					{
						// Bodygroup
						pProtag->nPlayerBody = pSubKey->GetInt();
					}
				}

				//----------------------------------------------------------------------------
				// Hands
				//----------------------------------------------------------------------------
				else if (V_strnicmp( pszSubKeyName, "hands", 5 ) == 0)
				{
					pszSubKeyName += 5;
					if (!pszSubKeyName[0])
					{
						// Model
						pProtag->pszHandModels[HANDRIG_DEFAULT] = AllocateString( pSubKey->GetString() );
					}
					else if (FStrEq( pszSubKeyName, "_skin" ))
					{
						// Skin
						pProtag->nHandSkin = pSubKey->GetInt();
					}
					else if (FStrEq( pszSubKeyName, "_body" ))
					{
						// Bodygroup
						pProtag->nHandBody = pSubKey->GetInt();
					}
					else
					{
						// Other Rigs
						pszSubKeyName += 1;
						for (int i = 0; i < NUM_HAND_RIG_TYPES; i++)
						{
							extern const char *pHandRigs[NUM_HAND_RIG_TYPES];

							if (V_stricmp( pszSubKeyName, pHandRigs[i] ) == 0)
							{
								pProtag->pszHandModels[i] = AllocateString( pSubKey->GetString() );
								break;
							}
						}
					}
				}

				//----------------------------------------------------------------------------
				// Responses
				//----------------------------------------------------------------------------
				else if (FStrEq( pszSubKeyName, "response_contexts" ))
				{
					pProtag->pszResponseContexts = AllocateString( pSubKey->GetString() );
				}

				//----------------------------------------------------------------------------
				// Multiplayer
				//----------------------------------------------------------------------------
				else if (FStrEq( pszSubKeyName, "team" ))
				{
#ifdef HL2MP
					if (FStrEq( pSubKey->GetString(), "combine" ))
					{
						pProtag->nTeam = TEAM_COMBINE;
					}
					else if (FStrEq( pSubKey->GetString(), "rebels" ))
					{
						pProtag->nTeam = TEAM_REBELS;
					}
					else
#endif
					{
						// Try to get a direct integer
						pProtag->nTeam = atoi( pSubKey->GetString() );
					}
				}
#endif
				//----------------------------------------------------------------------------
				// Weapon Data
				//----------------------------------------------------------------------------
				else if (V_strnicmp( pszSubKeyName, "wpn_viewmodels", 14 ) == 0)
				{
					// "wpn_viewmodels_c" = models which support arms
					bool bHands = FStrEq( pszSubKeyName + 14, "_c" );

					FOR_EACH_SUBKEY( pSubKey, pWeaponKey )
					{
						int i = pProtag->dictWpnData.Find( pWeaponKey->GetName() );
						if (i == pProtag->dictWpnData.InvalidIndex())
							i = pProtag->dictWpnData.Insert( pWeaponKey->GetName() );

						pProtag->dictWpnData[i].pszVM = AllocateString( pWeaponKey->GetString() );
						pProtag->dictWpnData[i].bUsesHands = bHands;
					}
				}
				else if (FStrEq( pszSubKeyName, "wpn_data" )) // More expanded/explicit
				{
					FOR_EACH_SUBKEY( pSubKey, pWeaponKey )
					{
						int i = pProtag->dictWpnData.Find( pWeaponKey->GetName() );
						if (i == pProtag->dictWpnData.InvalidIndex())
							i = pProtag->dictWpnData.Insert( pWeaponKey->GetName() );

						const char *pszVM = pWeaponKey->GetString( "viewmodel", NULL );
						if (pszVM)
							pProtag->dictWpnData[i].pszVM = AllocateString( pszVM );

						const char *pszHandRig = pWeaponKey->GetString( "hand_rig", NULL );
						if (pszHandRig)
						{
							// If there's a specific rig, then it must use hands
							pProtag->dictWpnData[i].bUsesHands = true;

							for (int j = 0; j < NUM_HAND_RIG_TYPES; j++)
							{
								extern const char *pHandRigs[NUM_HAND_RIG_TYPES];

								if (V_stricmp( pszHandRig, pHandRigs[j] ) == 0)
								{
									pProtag->dictWpnData[i].nHandRig = j;
									break;
								}
							}
						}
						else
						{
							KeyValues *pUsesHands = pWeaponKey->FindKey( "uses_hands" );
							if (pUsesHands)
								pProtag->dictWpnData[i].bUsesHands = pUsesHands->GetBool();
						}

						KeyValues *pVMFOV = pWeaponKey->FindKey( "viewmodel_fov" );
						if (pVMFOV)
							pProtag->dictWpnData[i].flVMFOV = pVMFOV->GetFloat();
					}
				}
			}
		}
	}
	pManifest->deleteThis();
}

//----------------------------------------------------------------------------

CProtagonistSystem::ProtagonistData_t *CProtagonistSystem::GetPlayerProtagonist( const CBasePlayer *pPlayer )
{
#ifdef CLIENT_DLL
	const C_BaseHLPlayer *pHL2Player = static_cast<const C_BaseHLPlayer *>(pPlayer);
#else
	const CHL2_Player *pHL2Player = static_cast<const CHL2_Player *>(pPlayer);
#endif
	if (!pHL2Player)
		return NULL;

	int i = pHL2Player->GetProtagonistIndex();
	if (i >= 0 && i < m_Protagonists.Count())
	{
		return &g_ProtagonistSystem.m_Protagonists[i];
	}

	return NULL;
}

CProtagonistSystem::ProtagonistData_t *CProtagonistSystem::FindProtagonist( const char *pszName )
{
	FOR_EACH_VEC( m_Protagonists, i )
	{
		if (FStrEq( pszName, m_Protagonists[i].szName ))
		{
			return &m_Protagonists[i];
		}
	}

	return NULL;
}

CProtagonistSystem::ProtagonistData_t *CProtagonistSystem::FindProtagonist( int nIndex )
{
	if (nIndex < 0 || nIndex >= m_Protagonists.Count())
		return NULL;

	return &m_Protagonists[nIndex];
}

int CProtagonistSystem::FindProtagonistIndex( const char *pszName )
{
	FOR_EACH_VEC( m_Protagonists, i )
	{
		if (FStrEq( pszName, m_Protagonists[i].szName ))
		{
			return i;
		}
	}

	return -1;
}

const char *CProtagonistSystem::FindProtagonistByModel( const char *pszModelName )
{
#ifndef CLIENT_DLL
	FOR_EACH_VEC( m_Protagonists, i )
	{
		if (m_Protagonists[i].pszPlayerModel && FStrEq( pszModelName, m_Protagonists[i].pszPlayerModel ))
		{
			return m_Protagonists[i].szName;
		}
	}
#endif

	return NULL;
}

void CProtagonistSystem::PrecacheProtagonist( CBaseEntity *pSource, int nIdx )
{
#ifndef CLIENT_DLL
	if (nIdx < 0)
		return;

	ProtagonistData_t &pProtag = m_Protagonists[nIdx];

	// Don't if it's already precached
	if (pProtag.bPrecached)
		return;

	CBaseEntity::SetAllowPrecache( true );

	// Playermodel
	if (pProtag.pszPlayerModel)
	{
		pSource->PrecacheModel( pProtag.pszPlayerModel );
	}

	// Hands
	for (int i = 0; i < NUM_HAND_RIG_TYPES; i++)
	{
		if (pProtag.pszHandModels[i])
		{
			pSource->PrecacheModel( pProtag.pszHandModels[i] );
		}
	}

	// Weapon Data
	FOR_EACH_DICT_FAST( pProtag.dictWpnData, i )
	{
		if (pProtag.dictWpnData[i].pszVM)
		{
			pSource->PrecacheModel( pProtag.dictWpnData[i].pszVM );
		}
	}

	CBaseEntity::SetAllowPrecache( false );

	// Precache parents
	FOR_EACH_VEC( pProtag.vecParents, i )
	{
		PrecacheProtagonist( pSource, pProtag.vecParents[i] );
	}

	pProtag.bPrecached = true;
#endif
}

//----------------------------------------------------------------------------

#define GetProtagParamInner( name, ... )	DoGetProtagonist_##name( *pProtag, ##__VA_ARGS__ )

#define GetProtagParam( name, type, invoke, ... ) \
type CProtagonistSystem::GetProtagonist_##name( const CBasePlayer *pPlayer, ##__VA_ARGS__ ) \
{ \
	ProtagonistData_t *pProtag = GetPlayerProtagonist( pPlayer ); \
	if (!pProtag) \
		return NULL; \
	return invoke; \
} \
type CProtagonistSystem::GetProtagonist_##name( const int nProtagonistIndex, ##__VA_ARGS__ ) \
{ \
	ProtagonistData_t *pProtag = FindProtagonist( nProtagonistIndex ); \
	if (!pProtag) \
		return NULL; \
	return invoke; \
} \

#define GetProtagParamBody( name, type, invoke, body, ... ) \
type CProtagonistSystem::GetProtagonist_##name( const CBasePlayer *pPlayer, ##__VA_ARGS__ ) \
{ \
	ProtagonistData_t *pProtag = GetPlayerProtagonist( pPlayer ); \
	if (!pProtag) \
		return NULL; \
	type returnVal = invoke; \
	body \
	return returnVal; \
} \
type CProtagonistSystem::GetProtagonist_##name( const int nProtagonistIndex, ##__VA_ARGS__ ) \
{ \
	ProtagonistData_t *pProtag = FindProtagonist( nProtagonistIndex ); \
	if (!pProtag) \
		return NULL; \
	type returnVal = invoke; \
	body \
	return returnVal; \
} \

#ifdef CLIENT_DLL
#else
GetProtagParam( PlayerModel,			const char*,	GetProtagParamInner( PlayerModel ) )
GetProtagParam( PlayerModelSkin,		int,			GetProtagParamInner( PlayerModelSkin ) )
GetProtagParam( PlayerModelBody,		int,			GetProtagParamInner( PlayerModelBody ) )
GetProtagParam( HandModel,			const char*,	GetProtagParamInner( HandModel, pWeapon ), const CBaseCombatWeapon *pWeapon )
GetProtagParam( HandModelSkin,		int,			GetProtagParamInner( HandModelSkin, pWeapon ), const CBaseCombatWeapon *pWeapon )
GetProtagParam( HandModelBody,		int,			GetProtagParamInner( HandModelBody, pWeapon ), const CBaseCombatWeapon *pWeapon )
GetProtagParamBody( ResponseContexts,	bool,			GetProtagParamInner( ResponseContexts, pszContexts, nContextsSize ), {
		if (pszContexts[0] != '\0')
		{
			// Replace trailing comma
			int nLast = V_strlen( pszContexts )-1;
			if (pszContexts[nLast] == ',')
			{
				pszContexts[nLast] = '\0';
			}
		}
	}, char *pszContexts, int nContextsSize )
GetProtagParam( Team,				int,			GetProtagParamInner( Team ) )
#endif

GetProtagParam( ViewModel,			const char*,	GetProtagParamInner( ViewModel, pWeapon ), const CBaseCombatWeapon *pWeapon )
GetProtagParam( ViewModelFOV,		float*,			GetProtagParamInner( ViewModelFOV, pWeapon ), const CBaseCombatWeapon *pWeapon )
GetProtagParam( UsesHands,			bool*,			GetProtagParamInner( UsesHands, pWeapon ), const CBaseCombatWeapon *pWeapon )
GetProtagParam( HandRig,			int*,			GetProtagParamInner( HandRig, pWeapon ), const CBaseCombatWeapon *pWeapon )

//----------------------------------------------------------------------------

#define GetProtagonistRecurse( funcName, ... ) \
	FOR_EACH_VEC( pProtag.vecParents, i ) \
	{ \
		ProtagonistData_t *pParent = FindProtagonist(pProtag.vecParents[i]); \
		if (!pParent) \
			continue; \
		auto returnVar = funcName( *pParent, ##__VA_ARGS__ ); \
		if (returnVar) \
			return returnVar; \
	} \

#define GetProtagonistRecurseNoReturn( funcName, ... ) \
	FOR_EACH_VEC( pProtag.vecParents, i ) \
	{ \
		ProtagonistData_t *pParent = FindProtagonist(pProtag.vecParents[i]); \
		if (!pParent) \
			continue; \
		funcName( *pParent, ##__VA_ARGS__ ); \
	} \

#ifdef CLIENT_DLL
#else
const char *CProtagonistSystem::DoGetProtagonist_PlayerModel( ProtagonistData_t &pProtag )
{
	if (pProtag.pszPlayerModel)
		return pProtag.pszPlayerModel;

	// Recursively search parent protagonists
	GetProtagonistRecurse( DoGetProtagonist_PlayerModel )

	return NULL;
}

int CProtagonistSystem::DoGetProtagonist_PlayerModelSkin( ProtagonistData_t &pProtag )
{
	if (pProtag.nPlayerSkin >= 0)
		return pProtag.nPlayerSkin;

	// Recursively search parent protagonists
	GetProtagonistRecurse( DoGetProtagonist_PlayerModelSkin )

	return 0;
}

int CProtagonistSystem::DoGetProtagonist_PlayerModelBody( ProtagonistData_t &pProtag )
{
	if (pProtag.nPlayerBody >= 0)
		return pProtag.nPlayerBody;

	// Recursively search parent protagonists
	GetProtagonistRecurse( DoGetProtagonist_PlayerModelBody )

	return 0;
}

const char *CProtagonistSystem::DoGetProtagonist_HandModel( ProtagonistData_t &pProtag, const CBaseCombatWeapon *pWeapon )
{
	int nRigType = pWeapon ? pWeapon->GetHandRig() : HANDRIG_DEFAULT;
	if (pProtag.pszHandModels[nRigType])
		return pProtag.pszHandModels[nRigType];

	// Recursively search parent protagonists
	GetProtagonistRecurse( DoGetProtagonist_HandModel, pWeapon )

	return NULL;
}

int CProtagonistSystem::DoGetProtagonist_HandModelSkin( ProtagonistData_t &pProtag, const CBaseCombatWeapon *pWeapon )
{
	if (pProtag.nHandSkin >= 0)
		return pProtag.nHandSkin;

	// Recursively search parent protagonists
	GetProtagonistRecurse( DoGetProtagonist_HandModelSkin, pWeapon )

	return NULL;
}

int CProtagonistSystem::DoGetProtagonist_HandModelBody( ProtagonistData_t &pProtag, const CBaseCombatWeapon *pWeapon )
{
	if (pProtag.nHandBody >= 0)
		return pProtag.nHandBody;

	// Recursively search parent protagonists
	GetProtagonistRecurse( DoGetProtagonist_HandModelBody, pWeapon )

	return NULL;
}

bool CProtagonistSystem::DoGetProtagonist_ResponseContexts( ProtagonistData_t &pProtag, char *pszContexts, int nContextsSize )
{
	if (pProtag.pszResponseContexts)
	{
		V_strncat( pszContexts, pProtag.pszResponseContexts, nContextsSize );
		V_strncat( pszContexts, ",", nContextsSize );
	}

	// Recursively search parent protagonists
	GetProtagonistRecurseNoReturn( DoGetProtagonist_ResponseContexts, pszContexts, nContextsSize )

	return pszContexts[0] != '\0';
}

int CProtagonistSystem::DoGetProtagonist_Team( ProtagonistData_t &pProtag )
{
	if (pProtag.nTeam >= -1)
		return pProtag.nTeam;

	// Recursively search parent protagonists
	GetProtagonistRecurse( DoGetProtagonist_Team )

	return TEAM_ANY;
}
#endif

const char *CProtagonistSystem::DoGetProtagonist_ViewModel( ProtagonistData_t &pProtag, const CBaseCombatWeapon *pWeapon )
{
	FOR_EACH_DICT_FAST( pProtag.dictWpnData, i )
	{
		// HACKHACK: GetClassname is not const
		if (!FStrEq( pProtag.dictWpnData.GetElementName( i ), const_cast<CBaseCombatWeapon*>(pWeapon)->GetClassname() ))
			continue;

		if (pProtag.dictWpnData[i].pszVM)
			return pProtag.dictWpnData[i].pszVM;
		break;
	}

	// Recursively search parent protagonists
	GetProtagonistRecurse( DoGetProtagonist_ViewModel, pWeapon )

	return NULL;
}

float *CProtagonistSystem::DoGetProtagonist_ViewModelFOV( ProtagonistData_t &pProtag, const CBaseCombatWeapon *pWeapon )
{
	FOR_EACH_DICT_FAST( pProtag.dictWpnData, i )
	{
		// HACKHACK: GetClassname is not const
		if (!FStrEq( pProtag.dictWpnData.GetElementName( i ), const_cast<CBaseCombatWeapon*>(pWeapon)->GetClassname() ))
			continue;

		return &pProtag.dictWpnData[i].flVMFOV;
	}

	// Recursively search parent protagonists
	GetProtagonistRecurse( DoGetProtagonist_ViewModelFOV, pWeapon )

	return NULL;
}

bool *CProtagonistSystem::DoGetProtagonist_UsesHands( ProtagonistData_t &pProtag, const CBaseCombatWeapon *pWeapon )
{
	FOR_EACH_DICT_FAST( pProtag.dictWpnData, i )
	{
		// HACKHACK: GetClassname is not const
		if (!FStrEq( pProtag.dictWpnData.GetElementName( i ), const_cast<CBaseCombatWeapon*>(pWeapon)->GetClassname() ))
			continue;

		return &pProtag.dictWpnData[i].bUsesHands;
	}

	// Recursively search parent protagonists
	GetProtagonistRecurse( DoGetProtagonist_UsesHands, pWeapon )

	return NULL;
}

int *CProtagonistSystem::DoGetProtagonist_HandRig( ProtagonistData_t &pProtag, const CBaseCombatWeapon *pWeapon )
{
	FOR_EACH_DICT_FAST( pProtag.dictWpnData, i )
	{
		// HACKHACK: GetClassname is not const
		if (!FStrEq( pProtag.dictWpnData.GetElementName( i ), const_cast<CBaseCombatWeapon*>(pWeapon)->GetClassname() ))
			continue;

		return &pProtag.dictWpnData[i].nHandRig;
	}

	// Recursively search parent protagonists
	GetProtagonistRecurse( DoGetProtagonist_HandRig, pWeapon )

	return NULL;
}

//----------------------------------------------------------------------------

const char *CProtagonistSystem::FindString( const char *string )
{
	unsigned short i = m_Strings.Find( string );
	return i == m_Strings.InvalidIndex() ? NULL : m_Strings[i];
}

const char *CProtagonistSystem::AllocateString( const char *string )
{
	int i = m_Strings.Find( string );
	if (i != m_Strings.InvalidIndex())
	{
		return m_Strings[i];
	}

	int len = Q_strlen( string );
	char *out = new char[len + 1];
	Q_memcpy( out, string, len );
	out[len] = 0;

	return m_Strings[m_Strings.Insert( out )];
}

//----------------------------------------------------------------------------

void CProtagonistSystem::PurgeProtagonists()
{
	m_Protagonists.RemoveAll();

	for (unsigned int i = 0; i < m_Strings.Count(); i++)
	{
		delete m_Strings[i];
	}
	m_Strings.Purge();
}

void CProtagonistSystem::PrintProtagonistData()
{
	Msg( "PROTAGONISTS\n\n" );

	FOR_EACH_VEC( m_Protagonists, i )
	{
		ProtagonistData_t &pProtag = m_Protagonists[i];

		Msg( "\t\"%s\"\n", pProtag.szName );

		Msg( "\t\tParents: %i\n", pProtag.vecParents.Count() );
		if (pProtag.vecParents.Count() > 0)
		{
			FOR_EACH_VEC( pProtag.vecParents, j )
			{
				ProtagonistData_t *pParent = FindProtagonist( pProtag.vecParents[j] );
				if (!pParent)
					continue;

				Msg( "\t\t\t\"%s\"\n", pParent->szName );
			}
		}

#ifdef CLIENT_DLL
#else
		if (pProtag.pszPlayerModel)
			Msg( "\t\tPlayer model: \"%s\" (%i, %i)\n", pProtag.pszPlayerModel, pProtag.nPlayerSkin, pProtag.nPlayerBody );

#ifdef HL2MP
		if ( pProtag.nTeam != TEAM_ANY )
			Msg( "\t\tTeam: %i\n", pProtag.nTeam );
#endif

		for (int j = 0; j < NUM_HAND_RIG_TYPES; j++)
		{
			extern const char *pHandRigs[NUM_HAND_RIG_TYPES];
			if (pProtag.pszHandModels[j])
				Msg( "\t\tHand %s model: \"%s\" (%i, %i)\n", pHandRigs[j], pProtag.pszHandModels[j], pProtag.nPlayerSkin, pProtag.nPlayerBody );
		}

		// Weapon Data
		Msg( "\t\tWeapon Data: %i\n", pProtag.dictWpnData.Count() );
		FOR_EACH_DICT_FAST( pProtag.dictWpnData, j )
		{
			Msg( "\t\t\t%s\n", pProtag.dictWpnData.GetElementName(j) );
			if (pProtag.dictWpnData[j].pszVM)
				Msg( "\t\t\t\tViewmodel: \"%s\"\n", pProtag.dictWpnData[j].pszVM );

			Msg( "\t\t\t\tUses hands: %d, hand rig: %i\n", pProtag.dictWpnData[j].bUsesHands, pProtag.dictWpnData[j].nHandRig );
		}
#endif
	}
}

//----------------------------------------------------------------------------

CON_COMMAND_SHARED( protagonist_reload, "Reloads protagonist data" )
{
#ifdef CLIENT_DLL
	if (C_BasePlayer::GetLocalPlayer() == NULL)
	{
		Msg( "Must be in a level\n" );
		return;
	}
#endif

	g_ProtagonistSystem.PurgeProtagonists();
	g_ProtagonistSystem.LoadProtagonistManifest( "scripts/protagonists/protagonists_manifest.txt" );
}

CON_COMMAND_SHARED( protagonist_dump, "Dumps protagonist data" )
{
#ifdef CLIENT_DLL
	if (C_BasePlayer::GetLocalPlayer() == NULL)
	{
		Msg( "Must be in a level\n" );
		return;
	}
#endif

	g_ProtagonistSystem.PrintProtagonistData();
}
