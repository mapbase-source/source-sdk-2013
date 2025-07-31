//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose:		Multiplayer save/restore for level transitions.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"

#include "mapbase_mp_saverestore.h"

#ifndef CLIENT_DLL
#include "saverestoretypes.h"
#include "filesystem.h"
#include "gameinterface.h"
#include "in_buttons.h"
#ifdef HL2MP
#include "hl2mp_player.h"
#endif
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifndef CLIENT_DLL
ConVar mp_save_transition( "mp_save_transition", "1" );
ConVar mp_save_transition_in_sp( "mp_save_transition_in_sp", "0", FCVAR_NONE, "Allow MP level transitions in singleplayer" );
ConVar mp_save_transition_in_dm( "mp_save_transition_in_dm", "1", FCVAR_NONE, "Allow MP level transitions in deathmatch mode" );
ConVar mp_save_transition_wait_time( "mp_save_transition_wait_time", "31", FCVAR_NONE, "How long to wait for players to reach a level transition" );
ConVar mp_save_transition_wait_time_warn( "mp_save_transition_wait_time_warn", "10", FCVAR_NONE, "What time the transition wait timer should turn red" );
ConVar mp_save_debug_transition( "mp_save_debug_transition", "0" );

// These are cvars so that they can be set and stored without the server running.
// Use mp_load and mp_load_transition instead of setting these directly.
ConVar mp_save_dat_load( "mp_save_dat_load", "", FCVAR_HIDDEN );
ConVar mp_save_dat_map_from( "mp_save_dat_map_from", "", FCVAR_HIDDEN );
ConVar mp_save_dat_landmark( "mp_save_dat_landmark", "", FCVAR_HIDDEN );
#endif

inline void ResolveMPSavePath( char *szPath, const char *pszSaveName, size_t nPathSize, bool bTransitioning )
{
	szPath[0] = '\0';

	if (!V_strchr( szPath, '/' ) && !V_strchr( szPath, '\\' ))
		V_strncat( szPath, "save/", nPathSize );

	V_strncat( szPath, pszSaveName, nPathSize );

	if (bTransitioning)
	{
		V_SetExtension( szPath, ".mptrs", nPathSize );
	}
	else
	{
		V_SetExtension( szPath, ".mpsav", nPathSize );
	}
}

#ifndef CLIENT_DLL

extern CServerGameDLL g_ServerGameDLL;

extern void SaveEntityOnTable( CBaseEntity *pEntity, CSaveRestoreData *pSaveData, int &iSlot );

extern const char *GetEscapePointForLandmark( const char *pszLandmark, CBaseTrigger **ppChangeLevel );

CMPSaveRestore g_MPSaveRestore;

CMPSaveRestore::CMPSaveRestore()
{
}

CMPSaveRestore::~CMPSaveRestore()
{
}

bool CMPSaveRestore::EnabledTransitions() const
{
	if (gpGlobals->maxClients == 1)
		return mp_save_transition_in_sp.GetBool();

	return mp_save_transition.GetBool();
}

bool CMPSaveRestore::EnabledTransitionsInDeathmatch() const
{
	return mp_save_transition_in_dm.GetBool();
}

void CMPSaveRestore::StartTransition( const char *pszLandmark )
{
	m_bSaving = true;
	mp_save_dat_map_from.SetValue( STRING( gpGlobals->mapname ) );
	mp_save_dat_landmark.SetValue( pszLandmark );
}

void CMPSaveRestore::EndTransition()
{
	m_bSaving = false;
	mp_save_dat_map_from.SetValue( "" );
	mp_save_dat_landmark.SetValue( "" );
}

bool CMPSaveRestore::IsTransitioning() const
{
	return mp_save_dat_map_from.GetString()[0] != '\0';
}

void CMPSaveRestore::SetSaving( bool bSaving )
{
	m_bSaving = bSaving;
}

bool CMPSaveRestore::IsSaving() const
{
	return m_bSaving;
}

void CMPSaveRestore::StartLoadingSave( const char *pszSave )
{
	char szPath[MAX_PATH];
	ResolveSavePath( szPath, pszSave, sizeof( szPath ) );

	KeyValues *pSaveMetadata = new KeyValues( "MPSaveData" );
	if ( pSaveMetadata && pSaveMetadata->LoadFromFile( g_pFullFileSystem, UTIL_VarArgs( "%s.txt", szPath ), "MOD" ) )
	{
		const char *pszMap = pSaveMetadata->GetString( "map_from", NULL );
		if ( pszMap )
		{
			mp_save_dat_load.SetValue( szPath );

			// gameinterface.cpp will load the save automatically
			engine->ChangeLevel( pszMap, NULL );

			pSaveMetadata->deleteThis();
			pSaveMetadata = NULL;
			return;
		}
	}
	
	// Failure case
	pSaveMetadata->deleteThis();
	pSaveMetadata = NULL;
	Warning( "Failed to start loading save %s\n", pszSave );
	StopLoadingSave();
}

void CMPSaveRestore::StopLoadingSave()
{
	mp_save_dat_load.SetValue( "" );
}

bool CMPSaveRestore::IsLoadingSave() const
{
	return mp_save_dat_load.GetString()[0] != '\0';
}

//-----------------------------------------------------------------------------
//
// Player Save/Restore
//
// This is used to delay the restoring of players and their dependent ents (e.g. weapons)
// until they're fully connected.
//
//-----------------------------------------------------------------------------

static short MPPLAYER_SAVE_RESTORE_VERSION = 1;

// Standard save/restore memory isn't freed until all allocated memory is freed,
// so the player restore data uses a separate memory allocator
CUtlMemory<byte>	g_MPPlayerRestoreMemory;

//-----------------------------------------------------------------------------

class CMPPlayerSaveRestoreBlockHandler : public CDefSaveRestoreBlockHandler
{
public:
	~CMPPlayerSaveRestoreBlockHandler()
	{
		if ( m_pSaveData )
		{
			g_MPSaveRestore.CleanupRestorePostPlayers( m_pSaveData );
			m_pSaveData = NULL;
		}
	}

	const char *GetBlockName()
	{
		return "MPPlayer";
	}

	//---------------------------------

	void Save( ISave *pSave )
	{
		pSave->StartBlock( "MPPlayers" );
		
		// Write a dummy value for us to overwrite later with the number of players and their sizes
		int nStartPos = pSave->GetWritePos();
		short dummy = 0;
		pSave->WriteShort( &dummy );
		pSave->WriteInt( &nStartPos );

		short nNumActualClients = 0;
		int nSizeActualClients = sizeof(short) + sizeof(int); // Start with the two values above

		CGameSaveRestoreInfo *pSaveData = pSave->GetGameSaveRestoreInfo();
		for ( int i = 0; i < gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( !pPlayer )
			{
				// Just skip without writing anything, nNumActualClients will ensure this isn't used
				//uint64 dummy = 0ULL;
				//pSave->WriteInt64( &dummy );
			}
			else
			{
				// Get the current save pos for us to overwrite later
				int nPrePlayerPos = pSave->GetWritePos();
				pSave->WriteInt( &nPrePlayerPos );

				uint64 steamID = pPlayer->GetSteamIDAsUInt64();
				if ( pPlayer->IsFakeClient() )
				{
#ifdef HL2MP
					// If this bot is a proxy for someone else, use their ID instead
					/*CHL2MP_Player *pBot = ToHL2MPPlayer( pPlayer );
					if ( pBot && pBot->GetBotTakeOverAvatar() )
					{
						steamID = pBot->GetBotTakeOverAvatar()->GetSteamIDAsUInt64();
					}
					else*/
#endif
					steamID = (uint64)pPlayer->entindex();
				}

				pSave->WriteInt64( &steamID );

				//-------------------------------------

				pSaveData->SetCurrentEntityContext( pPlayer );
				pPlayer->Save( *pSave );
				pSaveData->SetCurrentEntityContext( NULL );

				//-------------------------------------

				// Now save its dependencies
				WritePlayerDependencyData( pSaveData, pSave, pPlayer );

				// Now go back and note how big the player data is
				int nPostPlayerPos = pSave->GetWritePos();
				int nPlayerSize = nPostPlayerPos - nPrePlayerPos;
				pSave->SetWritePos( nPrePlayerPos );
				pSave->WriteInt( &nPlayerSize );
				pSave->SetWritePos( nPostPlayerPos );

				nNumActualClients++;
				nSizeActualClients += nPlayerSize;
			}
		}

		if ( m_pSaveData != NULL && m_fDoLoad )
		{
			// We loaded a save before and still have some players who haven't loaded in, add them to the list
			CopyPendingPlayersToNewSave( gpGlobals->pSaveData, pSave, nNumActualClients, nSizeActualClients );
		}

		// Now go back and write how many players we've saved
		int nEndPos = pSave->GetWritePos();
		pSave->SetWritePos( nStartPos );
		pSave->WriteShort( &nNumActualClients );
		pSave->WriteInt( &nSizeActualClients );
		pSave->SetWritePos( nEndPos );

		pSave->EndBlock();
	}

	//---------------------------------

	void PostSave()
	{
		m_PlayerDependentEnts.RemoveAll();
	}

	//---------------------------------

	void WriteSaveHeaders( ISave *pSave )
	{
		pSave->WriteShort( &MPPLAYER_SAVE_RESTORE_VERSION );
	}
	
	//---------------------------------

	void ReadRestoreHeaders( IRestore *pRestore )
	{
		short version;
		pRestore->ReadShort( &version );
		// only load if version matches and if we are loading a game, not a transition
		m_fDoLoad = ( version == MPPLAYER_SAVE_RESTORE_VERSION );
		m_RestoredPlayers.RemoveAll();
	}

	//---------------------------------

	void Restore( IRestore *pRestore, bool createPlayers )
	{
		if ( m_fDoLoad )
		{
			m_nInitialPos = pRestore->GetReadPos();

			pRestore->StartBlock();

			pRestore->ReadShort(); // nClientsSaved
			m_nClientsTotalSize = pRestore->ReadInt();

			pRestore->EndBlock();
		}
	}

	//---------------------------------

	void WritePlayerDependencyData( CGameSaveRestoreInfo *pSaveData, ISave *pSave, CBasePlayer *pPlayer )
	{
		// Get all of their weapons
		short nNumWeapons = 0;
		short nActiveWeapon = -1;
		int nWeaponStartPos = pSave->GetWritePos();
		pSave->WriteShort( &nNumWeapons );
		for ( int i = 0; i < pPlayer->WeaponCount(); i++ )
		{
			CBaseCombatWeapon *pWeapon = pPlayer->GetWeapon( i );
			if ( pWeapon )
			{
				short nClassnameLen = strlen( pWeapon->GetClassname() ) + 1;
				pSave->WriteShort( &nClassnameLen );
				pSave->WriteString( pWeapon->GetClassname() );

				//-------------------------------------

				pSaveData->SetCurrentEntityContext( pWeapon );
				pWeapon->Save( *pSave );
				pSaveData->SetCurrentEntityContext( NULL );

				//-------------------------------------

				if (pPlayer->GetActiveWeapon() == pWeapon)
					nActiveWeapon = nNumWeapons;

				nNumWeapons++;
			}
		}

		pSave->WriteShort( &nActiveWeapon );

		int nWeaponEndPos = pSave->GetWritePos();
		pSave->SetWritePos( nWeaponStartPos );
		pSave->WriteShort( &nNumWeapons );
		pSave->SetWritePos( nWeaponEndPos );

		// Viewmodels are currently recreated instead of saved
		/*for ( int i = 0; i < MAX_VIEWMODELS; i++ )
		{
		}*/
	}

	void RestorePlayerDependencyData( CGameSaveRestoreInfo *pSaveData, IRestore *pRestore, CBasePlayer *pPlayer, CUtlVector<CBaseEntity*> &vecRestoredEntList )
	{
		// Get all of their weapons
		short nNumWeapons = 0;
		pRestore->ReadShort( &nNumWeapons );
		for ( int i = 0; i < nNumWeapons; i++ )
		{
			char szClassname[32];
			short nClassnameLen = 0;
			pRestore->ReadShort( &nClassnameLen );
			pRestore->ReadString( szClassname, sizeof( szClassname ), nClassnameLen );
			CBaseEntity *pEnt = CreateEntityByName( szClassname ); //pPlayer->GiveNamedItem( szClassname );
			if ( pEnt && pEnt->IsBaseCombatWeapon() )
			{
				pSaveData->SetCurrentEntityContext( pEnt );
				pEnt->Restore( *pRestore );
				pSaveData->SetCurrentEntityContext( NULL );

				pEnt->Precache();

				pPlayer->RestoreWeapon( pEnt->MyCombatWeaponPointer(), i );

				vecRestoredEntList.AddToTail( pEnt );
			}
			else
			{
				Assert( 0 );
			}
		}

		short nActiveWeapon = 0;
		pRestore->ReadShort( &nActiveWeapon );
		if ( nActiveWeapon != -1 )
			pPlayer->Weapon_Switch( pPlayer->GetWeapon( nActiveWeapon ) ); // SetActiveWeapon
	}

	//---------------------------------

	bool RestorePlayer( CBasePlayer *pPlayer )
	{
		if (m_fDoLoad)
		{
			uint64 playerID = pPlayer->GetSteamIDAsUInt64();
			if ( pPlayer->IsFakeClient() )
				playerID = (uint64)pPlayer->entindex();

			if ( m_RestoredPlayers.HasElement( playerID ) )
				return false;

			//-------------------------------------

			gpGlobals->pSaveData = m_pSaveData;

			m_pSaveData->levelInfo.time = gpGlobals->curtime;	// Update time

			//-------------------------------------

			CRestore restoreHelper( m_pSaveData );

			int nStartPos = restoreHelper.GetReadPos();

			restoreHelper.StartBlock();

			int nNumClients = restoreHelper.ReadShort();
			restoreHelper.ReadInt(); // nTotalSize

			bool bRestored = false;
			for (int i = 0; i < nNumClients; i++)
			{
				int nPlayerSize;
				restoreHelper.ReadInt( &nPlayerSize );

				int64 steamID;
				restoreHelper.ReadInt64( &steamID );

				if ( (uint64)steamID == playerID )
				{
					m_pRestoringPlayer = pPlayer;

					//-------------------------------------

					m_pSaveData->SetCurrentEntityContext( pPlayer );
					bRestored = (pPlayer->Restore( restoreHelper ) > 0);
					m_pSaveData->SetCurrentEntityContext( NULL );

					// Make sure the player isn't in a solid
					if (UTIL_EntityInSolid( pPlayer ))
					{
						bool bTeleported = false;
						if (m_pSaveData->levelInfo.fUseLandmark)
						{
							// First, see if the changelevel has a point to put us at
							CBaseTrigger *pChangeLevel = NULL;
							const char *pszEscapePoint = GetEscapePointForLandmark( m_pSaveData->levelInfo.szLandmarkName, &pChangeLevel );
							if (pszEscapePoint)
							{
								Vector vecOrigin;
								CBaseEntity *pEscapePoint = g_MPSaveRestore.FindValidEscapePoint( pszEscapePoint, pPlayer, pChangeLevel, vecOrigin );
								if (pEscapePoint)
								{
									pPlayer->Teleport( &vecOrigin, NULL, NULL );
									bTeleported = true;
								}
							}
						}

						if (!bTeleported)
						{
							// Give up and spawn them at the spawn point
							CBaseEntity *pSpawnPoint = pPlayer->EntSelectSpawnPoint();
							pPlayer->SetLocalOrigin( pSpawnPoint->GetLocalOrigin() + Vector( 0,0,1 ) );
							pPlayer->SetLocalAngles( pSpawnPoint->GetLocalAngles() );
						}
					}

					//-------------------------------------

					CUtlVector<CBaseEntity *> vecRestoredEnts;
					RestorePlayerDependencyData( m_pSaveData, &restoreHelper, pPlayer, vecRestoredEnts );

					//-------------------------------------

					// Call OnRestore
					pPlayer->OnRestore();
					FOR_EACH_VEC( vecRestoredEnts, i )
					{
						vecRestoredEnts[i]->OnRestore();
					}

					m_RestoredPlayers.AddToTail( playerID );
					m_pRestoringPlayer = NULL;
					break;
				}
				else
				{
					// Skip to the next player (account for player size and steam ID)
					restoreHelper.SetReadPos( restoreHelper.GetReadPos() + nPlayerSize - sizeof( int ) - sizeof( int64 ) );
				}
			}

			restoreHelper.EndBlock();

			restoreHelper.SetReadPos( nStartPos );

			if ( m_RestoredPlayers.Count() == nNumClients )
			{
				// Restored all players
				g_MPSaveRestore.CleanupRestorePostPlayers( m_pSaveData );
				m_pSaveData = NULL;
				m_RestoredPlayers.RemoveAll();
			}

			return bRestored;
		}

		return false;
	}

	void CopyPendingPlayersToNewSave( CSaveRestoreData *pSaveData, ISave *pSave, short &nNumActualClients, int &nSizeActualClients )
	{
		CRestore restoreHelper( m_pSaveData );

		int nStartPos = restoreHelper.GetReadPos();

		restoreHelper.StartBlock();

		int nNumClients = restoreHelper.ReadShort();
		restoreHelper.ReadInt(); // nTotalSize

		for (int i = 0; i < nNumClients; i++)
		{
			int nPlayerSize;
			restoreHelper.ReadInt( &nPlayerSize );

			int64 steamID;
			restoreHelper.ReadInt64( &steamID );

			if ( !m_RestoredPlayers.HasElement( steamID ) )
			{
				memcpy( pSaveData->AccessCurPos(), m_pSaveData->AccessCurPos(), nPlayerSize );
				nSizeActualClients += nPlayerSize;
				nNumActualClients++;
			}
			else
			{
				// Skip to the next player (account for player size and steam ID)
				restoreHelper.SetReadPos( restoreHelper.GetReadPos() + nPlayerSize - sizeof( int ) - sizeof( int64 ) );
			}
		}

		restoreHelper.EndBlock();

		restoreHelper.SetReadPos( nStartPos );
	}

	void AddPlayerDependentEnt( CBaseEntity *pEnt, CBasePlayer *pPlayer )
	{
		m_PlayerDependentEnts.AddToTail( pEnt );
	}

	bool IsPlayerDependentEnt( CBaseEntity *pEnt ) const
	{
		return m_PlayerDependentEnts.HasElement( pEnt );
	}

	int GetInitialPos() const
	{
		return m_nInitialPos;
	}

	int GetClientsTotalSize() const
	{
		return m_nClientsTotalSize;
	}

	CBasePlayer *GetRestoringPlayer() const
	{
		return m_pRestoringPlayer;
	}

	CSaveRestoreData *GetSaveData() const
	{
		return m_pSaveData;
	}

	void SetSaveData( CSaveRestoreData *pSaveData )
	{
		m_pSaveData = pSaveData;

		if ( m_pSaveData == NULL )
			m_RestoredPlayers.RemoveAll();
	}

private:
	bool m_fDoLoad;

	// Entities which should load alongside players (e.g. vehicles)
	CUtlVector<CBaseEntity*>	m_PlayerDependentEnts;

	int m_nInitialPos;
	int m_nClientsTotalSize;
	CSaveRestoreData *m_pSaveData;
	CUtlVector<uint64> m_RestoredPlayers;
	CBasePlayer *m_pRestoringPlayer;	// The player we are currently restoring
};

//-----------------------------------------------------------------------------

CMPPlayerSaveRestoreBlockHandler g_MPPlayerSaveRestoreBlockHandler;

//-------------------------------------

ISaveRestoreBlockHandler *GetMPPlayerSaveRestoreBlockHandler()
{
	return &g_MPPlayerSaveRestoreBlockHandler;
}

//----------------------------------------------------------------------------

void CMPSaveRestore::LevelInitPreEntity()
{
}

void CMPSaveRestore::LevelShutdownPostEntity()
{
	if (g_MPPlayerSaveRestoreBlockHandler.GetSaveData())
	{
		CleanupRestorePostPlayers( g_MPPlayerSaveRestoreBlockHandler.GetSaveData() );
		g_MPPlayerSaveRestoreBlockHandler.SetSaveData( NULL );
	}
}

bool CMPSaveRestore::SaveInitEntities( CSaveRestoreData *pSaveData )
{
	int number_of_entities = m_SaveEntities.Count() > 0 ? m_SaveEntities.Count() : gEntList.NumberOfEntities();
	if (number_of_entities == 0)
		return false;

	if (m_SaveEntities.Count() <= 0)
	{
		// If saving all entities, then deduct the number of players and their dependencies
		for ( int i = 0; i < gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if (pPlayer != NULL)
			{
				number_of_entities--;

				for ( CBaseEntity *pChild = pPlayer->FirstMoveChild(); pChild != NULL; pChild = pChild->NextMovePeer() )
				{
					number_of_entities--;
					g_MPPlayerSaveRestoreBlockHandler.AddPlayerDependentEnt( pChild, pPlayer );
				}
			}
		}
	}

	entitytable_t *pEntityTable = (entitytable_t *)engine->SaveAllocMemory( (sizeof( entitytable_t ) * number_of_entities), sizeof( char ) );
	if (!pEntityTable)
		return false;

	pSaveData->InitEntityTable( pEntityTable, number_of_entities );

	// build the table of entities
	// this is used to turn pointers into savable indices
	// build up ID numbers for each entity, for use in pointer conversions
	// if an entity requires a certain edict number upon restore, save that as well
	int nSlot = 0;
	if (m_SaveEntities.Count() > 0)
	{
		for (int i = 0; i < m_SaveEntities.Count(); i++)
		{
			SaveEntityOnTable( m_SaveEntities[i], pSaveData, nSlot );
		}

		m_SaveEntities.RemoveAll();
	}
	else
	{
		CBaseEntity *pEnt = NULL;
		while ( (pEnt = gEntList.NextEnt( pEnt )) != NULL )
		{
			if ( pEnt->IsPlayer() || g_MPPlayerSaveRestoreBlockHandler.IsPlayerDependentEnt( pEnt ) )
				continue;

			SaveEntityOnTable( pEnt, pSaveData, nSlot );
		}
	}

	pSaveData->BuildEntityHash();

	Assert( nSlot == pSaveData->NumEntities() );
	return (nSlot == pSaveData->NumEntities());
}

void CMPSaveRestore::CleanupSave( CSaveRestoreData *pSaveData )
{
	engine->SaveFreeMemory( pSaveData->DetachEntityTable() );	// engine->SaveAllocMemory( (sizeof( entitytable_t ) * number_of_entities), sizeof( char ) );
	engine->SaveFreeMemory( pSaveData->DetachSymbolTable() );	// engine->SaveAllocMemory( nTokens, sizeof( char * ) );
	engine->SaveFreeMemory( pSaveData );						// engine->SaveAllocMemory( sizeof(CSaveRestoreData) + (sizeof(entitytable_t) * numentities) + size, sizeof(char) );
}

void CMPSaveRestore::CleanupRestore( CSaveRestoreData *pSaveData )
{
	CleanupSave( pSaveData );
}

void CMPSaveRestore::CleanupRestorePrePlayers( CSaveRestoreData *pSaveData )
{
	// Now create a new save data instance that restores players only
	pSaveData->Seek( g_MPPlayerSaveRestoreBlockHandler.GetInitialPos() );

	int nSymbolTableSize = pSaveData->SizeSymbolTable();
	int nPlayerSaveSize = (sizeof(short)*2) + g_MPPlayerSaveRestoreBlockHandler.GetClientsTotalSize();

	// Reserve memory needed for the player data + the symbol table
	g_MPPlayerRestoreMemory.EnsureCapacity( sizeof( CSaveRestoreData ) + nPlayerSaveSize + ( nSymbolTableSize * sizeof( char* ) ) );

	//void *pSaveMemory = engine->SaveAllocMemory( sizeof(CSaveRestoreData) + nPlayerSaveSize, sizeof(char));
	void *pSaveMemory = g_MPPlayerRestoreMemory.Base();
	if ( !pSaveMemory )
	{
		Warning( "Couldn't allocate enough memory to keep track of players\n" );
		engine->SaveFreeMemory( pSaveData );
		return;
	}

	char **pTokens = pSaveData->DetachSymbolTable();

	CSaveRestoreData *pNewSaveData = MakeSaveRestoreData( pSaveMemory );
	memcpy( pNewSaveData, pSaveData, sizeof( CSaveRestoreData ) );
	pNewSaveData->Init( (char *)(pNewSaveData + 1), nPlayerSaveSize );	// skip the save structure
	memcpy( pNewSaveData->AccessCurPos(), pSaveData->AccessCurPos(), nPlayerSaveSize );

	pSaveMemory = &g_MPPlayerRestoreMemory.Element(sizeof( CSaveRestoreData ) + nPlayerSaveSize);
	char **pNewTokens = (char **)pSaveMemory;

	// Now transfer the symbol table
	pNewSaveData->InitSymbolTable( pNewTokens, nSymbolTableSize );

	for (int i = 0; i < nSymbolTableSize; i++)
	{
		char *pszSymbol = pTokens[i];
		if (pszSymbol && pszSymbol[0] != '\0')
			pNewSaveData->DefineSymbol( pszSymbol, i );
	}

	// Free the original data once we have what we need
	engine->SaveFreeMemory( pSaveData->DetachEntityTable() );
	engine->SaveFreeMemory( pTokens );
	engine->SaveFreeMemory( pSaveData );

	g_MPPlayerSaveRestoreBlockHandler.SetSaveData( pNewSaveData );
}

void CMPSaveRestore::CleanupRestorePostPlayers( CSaveRestoreData *pSaveData )
{
	g_MPPlayerRestoreMemory.Purge();
}

void CMPSaveRestore::SaveSymbols( CSaveRestoreData *pSaveData, CUtlBuffer &buffer )
{
	buffer.PutInt( pSaveData->SizeSymbolTable() );
	for ( int i = 0; i < pSaveData->SizeSymbolTable(); i++ )
	{
		buffer.PutString( pSaveData->StringFromSymbol( i ) );
	}
}

void CMPSaveRestore::LoadSymbols( CSaveRestoreData *pSaveData, CUtlBuffer &buffer )
{
	int nCount = buffer.GetInt();
	for (int i = 0; i < nCount; i++)
	{
		// Tokens need to address directly to the buffer
		char *pszSymbol = (char *)buffer.PeekGet();
		if (pszSymbol && pszSymbol[0] != '\0')
			pSaveData->DefineSymbol( pszSymbol, i);

		int nLen = buffer.PeekStringLength();
		buffer.SeekGet( CUtlBuffer::SEEK_CURRENT, nLen );
	}

	pSaveData->MoveCurPos( buffer.TellGet() );
}

void CMPSaveRestore::ResolveSavePath( char *szPath, const char *pszSaveName, size_t nPathSize )
{
	ResolveMPSavePath( szPath, pszSaveName, nPathSize, IsTransitioning() );
}

bool CMPSaveRestore::SaveFile( CSaveRestoreData *pSaveData, CSave *pSave, const char *pszSaveName )
{
	// HACKHACK: Save headers are done afterwards but must be in the position from before
	int nSavePos = pSaveData->GetCurPos();

	if ( IsTransitioning() )
	{
		// Write these for when we transition back to this level
		pSave->WriteInt( &m_nLandmark );
		pSave->WriteVector( &pSaveData->levelInfo.vecLandmarkOffset );
	}

	g_ServerGameDLL.SaveGlobalState( pSaveData );
	g_pGameSaveRestoreBlockSet->Save( pSave );

	int nSaveSize = pSaveData->GetCurPos() - nSavePos;

	//---------------------------------------------------

	int nSaveHeaderPos = pSaveData->GetCurPos();
	g_pGameSaveRestoreBlockSet->WriteSaveHeaders( pSave );
	int nSaveHeaderSize = pSaveData->GetCurPos() - nSaveHeaderPos;

	g_pGameSaveRestoreBlockSet->PostSave();

	// Now begin saving the file itself
	CUtlBuffer outBuffer;

	// Write out all of the symbols first
	SaveSymbols( pSaveData, outBuffer );

	// Then save the data itself
	pSaveData->Rewind( nSaveHeaderSize );
	outBuffer.Put( ((const void *)pSaveData->AccessCurPos()), nSaveHeaderSize );

	pSaveData->Rewind( nSaveSize );
	outBuffer.Put( ((const void *)pSaveData->AccessCurPos()), nSaveSize );

	g_pFullFileSystem->CreateDirHierarchy( "save", "MOD" );

	char szPath[MAX_PATH];
	ResolveSavePath( szPath, pszSaveName, sizeof( szPath ) );

	if ( !g_pFullFileSystem->WriteFile( szPath, "MOD", outBuffer ) )
		return false;

	return true;
}

bool CMPSaveRestore::LoadFile( CSaveRestoreData *pSaveData )
{
	CUtlBuffer outBuffer( pSaveData->GetBuffer(), pSaveData->SizeBuffer() );
	if (g_pFullFileSystem->ReadFile( mp_save_dat_load.GetString(), "MOD", outBuffer))
	{
		// Read the symbols first
		LoadSymbols( pSaveData, outBuffer );

		// Now read the save itself
		g_ServerGameDLL.ReadRestoreHeaders( pSaveData );

		// Need to do this after reading restore headers because the save assumed it was first
		pSaveData->Rebase();

		g_ServerGameDLL.RestoreGlobalState( pSaveData );

		g_ServerGameDLL.Restore( pSaveData, false );

		g_MPSaveRestore.CleanupRestorePrePlayers( pSaveData );
		return true;
	}

	g_MPSaveRestore.CleanupRestore( pSaveData );
	return false;
}

bool CMPSaveRestore::SaveTransitionFile( CSaveRestoreData *pSaveData, CSave *pSave, const char *pszTargetMap, const char *pLandmarkName, int nLandmark )
{
	m_nLandmark = nLandmark;
	if ( !SaveFile( pSaveData, pSave, STRING( gpGlobals->mapname ) ) )
		return false;

	return true;
}

bool CMPSaveRestore::RestoreTransitionFile( CSaveRestoreData *pSaveData, const char *pszThisMap, const char *pszOldMap, const char *pLandmarkName )
{
	char szPath[MAX_PATH];
	//ResolveSavePath( szPath, m_szMapTransitionFrom, sizeof( szPath ) );
	ResolveSavePath( szPath, mp_save_dat_map_from.GetString(), sizeof( szPath ) );

	CUtlBuffer outBuffer( pSaveData->GetBuffer(), pSaveData->SizeBuffer() );
	if (g_pFullFileSystem->ReadFile( szPath, "MOD", outBuffer ))
	{
		// Read the symbols first
		LoadSymbols( pSaveData, outBuffer );

		// Now read the save itself
		g_ServerGameDLL.ReadRestoreHeaders( pSaveData );

		// Need to do this after reading restore headers because the save assumed it was first
		pSaveData->Rebase();

		// Fill in landmark data
		V_strncpy( pSaveData->levelInfo.szCurrentMapName, pszOldMap, sizeof( pSaveData->levelInfo.szCurrentMapName ) );
		V_strncpy( pSaveData->levelInfo.szLandmarkName, pLandmarkName, sizeof( pSaveData->levelInfo.szLandmarkName ) );
		pSaveData->levelInfo.fUseLandmark = true;

		// Get the landmark's counterpart in this map
		CBaseEntity *pLandmark = gEntList.FindEntityByName( NULL, pLandmarkName );
		while (pLandmark)
		{
			if (pLandmark->ClassMatches( "info_landmark" ))
			{
				pSaveData->levelInfo.vecLandmarkOffset = pLandmark->GetAbsOrigin();
				break;
			}

			pLandmark = gEntList.FindEntityByName( pLandmark, pLandmarkName );
		}

		Assert( pLandmark != NULL );

		// Get the saved landmark data
		int nPrevLandmark;
		CRestore restoreHelper( pSaveData );
		restoreHelper.ReadInt( &nPrevLandmark );
		pSaveData->MoveCurPos( sizeof( Vector ) ); // Previous landmark pos is only used when transitioning to previous level

		g_ServerGameDLL.RestoreGlobalState( pSaveData );

		int nBase = restoreHelper.GetReadPos();

		g_ServerGameDLL.CreateEntityTransitionList( pSaveData, 1 << nPrevLandmark ); // FENTTABLE_LEVELMASK

		g_pGameSaveRestoreBlockSet->CallBlockHandlerRestore( GetMPPlayerSaveRestoreBlockHandler(), nBase, &restoreHelper, false );
		GetMPPlayerSaveRestoreBlockHandler()->PostRestore();

		g_MPSaveRestore.CleanupRestorePrePlayers( pSaveData );
		return true;
	}

	g_MPSaveRestore.CleanupRestore( pSaveData );
	return false;
}

bool CMPSaveRestore::RestoreNextLevelFile( CSaveRestoreData *pSaveData, const char *pszThisMap, const char *pszOldMap, const char *pLandmarkName )
{
	char szPath[MAX_PATH];
	ResolveSavePath( szPath, pszThisMap, sizeof( szPath ) );

	CUtlBuffer outBuffer( pSaveData->GetBuffer(), pSaveData->SizeBuffer() );
	if (g_pFullFileSystem->ReadFile( szPath, "MOD", outBuffer ))
	{
		// Read the symbols first
		LoadSymbols( pSaveData, outBuffer );

		// Now read the save itself
		g_ServerGameDLL.ReadRestoreHeaders( pSaveData );

		// Need to do this after reading restore headers because the save assumed it was first
		pSaveData->Rebase();

		// Fill in landmark data
		V_strncpy( pSaveData->levelInfo.szCurrentMapName, pszOldMap, sizeof( pSaveData->levelInfo.szCurrentMapName ) );
		V_strncpy( pSaveData->levelInfo.szLandmarkName, pLandmarkName, sizeof( pSaveData->levelInfo.szLandmarkName ) );
		pSaveData->levelInfo.fUseLandmark = true;

		// Get the saved landmark data
		int nLandmark;
		CRestore restoreHelper( pSaveData );
		restoreHelper.ReadInt( &nLandmark );
		restoreHelper.ReadVector( &pSaveData->levelInfo.vecLandmarkOffset, 1 );

		g_ServerGameDLL.RestoreGlobalState( pSaveData );

		// Remove entities that already transitioned
		for ( int i = 0; i < pSaveData->NumEntities(); i++ )
		{
			entitytable_t *pEntInfo = pSaveData->GetEntityInfo( i );
			if ( pEntInfo->size == 0 || pEntInfo->edictindex == 0 || pEntInfo->classname == NULL_STRING )
				continue;

			if (pEntInfo->flags & (1 << nLandmark) && !(pEntInfo->flags & FENTTABLE_GLOBAL))
			{
				// Ensures this entity is not spawned on restore
				pEntInfo->flags = FENTTABLE_REMOVED;
			}
		}

		//g_ServerGameDLL.CreateEntityTransitionList( pSaveData, FENTTABLE_LEVELMASK & ~(nLandmark) );
		g_ServerGameDLL.Restore( pSaveData, false );

		g_MPSaveRestore.CleanupRestore( pSaveData );
		return true;
	}

	g_MPSaveRestore.CleanupRestore( pSaveData );
	return false;
}

bool CMPSaveRestore::FindTransitionFile( const char *pszThisMap, const char **ppszOldMap, const char **ppLandmarkName )
{
	char szPath[MAX_PATH];
	//ResolveSavePath( szPath, m_szMapTransitionFrom, sizeof( szPath ) );
	ResolveSavePath( szPath, mp_save_dat_map_from.GetString(), sizeof(szPath));

	if (g_pFullFileSystem->FileExists( szPath ))
	{
		*ppszOldMap = mp_save_dat_map_from.GetString();
		*ppLandmarkName = mp_save_dat_landmark.GetString();
		return true;
	}

	return false;
}

void CMPSaveRestore::ClearTransitionFiles()
{
	FileFindHandle_t handle;
	const char *pszFileName = g_pFullFileSystem->FindFirst( "save/*.mptrs", &handle);
	while (pszFileName != NULL)
	{
		g_pFullFileSystem->RemoveFile( pszFileName, "MOD" );
		g_pFullFileSystem->FindNext( handle );
	}
}

bool CMPSaveRestore::HasPlayerData()
{
	return g_MPPlayerSaveRestoreBlockHandler.GetSaveData() != NULL;
}

bool CMPSaveRestore::RestorePlayer( CBasePlayer *pPlayer )
{
	if (g_MPPlayerSaveRestoreBlockHandler.GetSaveData() == NULL)
		return false;

	return g_MPPlayerSaveRestoreBlockHandler.RestorePlayer( pPlayer );
}

bool CMPSaveRestore::IsRestoringPlayer( CBasePlayer *pPlayer )
{
	if (g_MPPlayerSaveRestoreBlockHandler.GetSaveData() == NULL)
		return false;

	if ( pPlayer )
	{
		// Restoring this player
		return g_MPPlayerSaveRestoreBlockHandler.GetRestoringPlayer() == pPlayer;
	}
	else
	{
		// Restoring any player
		return g_MPPlayerSaveRestoreBlockHandler.GetRestoringPlayer() != NULL;
	}
}

//-----------------------------------------------------------------------------
//
// Transition Player Counting
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Timer counting down to the transition
//-----------------------------------------------------------------------------
class CMPTransitionTimer : public CGameTimer
{
public:
	DECLARE_CLASS( CMPTransitionTimer, CGameTimer );

	virtual int	ObjectCaps( void ) { return BaseClass::ObjectCaps() | FCAP_DONT_SAVE; }

	virtual void OnTimerFinished()
	{
		BaseClass::OnTimerFinished();
		m_bTimerFinished = true;
	}

public:

	bool m_bTimerFinished;
};

LINK_ENTITY_TO_CLASS( game_timer_transition, CMPTransitionTimer );

//-----------------------------------------------------------------------------

void CMPSaveRestore::AddPlayerToTransition( CBasePlayer *pPlayer, CBaseTrigger *pChangeLevelTrigger, const char *pszEscapePoint )
{
	if (m_PlayersInTransition.HasElement( pPlayer ))
		return;

	if (m_hChangeLevel == NULL)
	{
		m_hChangeLevel = pChangeLevelTrigger;
		m_pszEscapePoint = pszEscapePoint;
	}
	else if (pChangeLevelTrigger != m_hChangeLevel && pChangeLevelTrigger)
	{
		Assert( 0 );
		Warning( "Player %s touching a different transition trigger\n", pPlayer->GetPlayerName() );
		return;
	}

	if (m_PlayersInTransition.Count() == 0)
	{
		m_PlayersInTransition.AddToTail( pPlayer );

		// Don't bother setting up if we're already going to transition after this
		if (!AllPlayersReadyToTransition())
			BeginTransitionSetup();
	}
	else
	{
		m_PlayersInTransition.AddToTail( pPlayer );
	}

	if ( m_hTransitionTimer )
	{
		CMPTransitionTimer *pTimer = static_cast<CMPTransitionTimer *>(m_hTransitionTimer.Get());

		int nPlayersWaiting = PlayersWaitingToTransition();
		int nPlayersNotInTransition = PlayersNotInTransition();
		pTimer->SetProgressBarMaxSegments( nPlayersWaiting + nPlayersNotInTransition );
		pTimer->SetProgressBarOverride( nPlayersWaiting );
	}

	FreezePlayer( pPlayer );
}

bool CMPSaveRestore::RemovePlayerFromTransition( CBasePlayer *pPlayer, bool bTryTeleportOutside )
{
	bool bSuccess = false;
	int i = m_PlayersInTransition.Find( pPlayer );
	if (i != m_PlayersInTransition.InvalidIndex())
	{
		if (bTryTeleportOutside && m_hChangeLevel)
		{
			Vector vecOrigin;
			bSuccess = FindChangelevelExit( pPlayer, m_hChangeLevel, vecOrigin );

			if (!bSuccess)
			{
				// See if there's an escape point we can use
				if (m_pszEscapePoint && *m_pszEscapePoint)
				{
					CBaseEntity *pEscapePoint = FindValidEscapePoint( m_pszEscapePoint, pPlayer, m_hChangeLevel, vecOrigin );
					if (pEscapePoint)
					{
						bSuccess = true;
					}
				}
			}

			if (bSuccess)
			{
				pPlayer->Teleport( &vecOrigin, NULL, NULL );
			}
		}
		else
		{
			bSuccess = true;
		}
	}

	if (bSuccess)
	{
		UnfreezePlayer( pPlayer );

		m_PlayersInTransition.Remove( i );
	}

	if ( m_hTransitionTimer )
	{
		CMPTransitionTimer *pTimer = static_cast<CMPTransitionTimer *>(m_hTransitionTimer.Get());

		int nPlayersWaiting = PlayersWaitingToTransition();
		int nPlayersNotInTransition = PlayersNotInTransition();
		pTimer->SetProgressBarMaxSegments( nPlayersWaiting + nPlayersNotInTransition );
		pTimer->SetProgressBarOverride( nPlayersWaiting );
	}

	if (m_PlayersInTransition.Count() == 0)
		CleanupTransitionSetup();

	return bSuccess;
}

bool CMPSaveRestore::FindChangelevelExit( CBasePlayer *pPlayer, CBaseTrigger *pChangeLevel, Vector &vecOrigin )
{
	ICollideable *pCollide = pChangeLevel->CollisionProp();
	Vector vecPlayerOrigin = pPlayer->GetAbsOrigin();

	// Since the exact surface normal cannot be determined, we have to guess this
	Vector vecChangeLevelPos, vecChangeLevelMins, vecChangeLevelMaxs;
	pCollide->WorldSpaceSurroundingBounds( &vecChangeLevelMins, &vecChangeLevelMaxs );
	vecChangeLevelMins.z = vecChangeLevelMaxs.z = vecPlayerOrigin.z;
	CalcClosestPointOnLine( vecPlayerOrigin, vecChangeLevelMins, vecChangeLevelMaxs, vecChangeLevelPos, NULL );

	if (mp_save_debug_transition.GetBool())
		NDebugOverlay::Cross3D( vecChangeLevelPos, 10.0f, 0, 0, 255, true, 5.0f );

	//-------------------------------------------------------------------------

	// See if we can try a cardinal direction
	Vector2D vec2DDir = (vecPlayerOrigin.AsVector2D() - vecChangeLevelPos.AsVector2D());
	vec2DDir.NormalizeInPlace();
	vec2DDir.x = roundf( vec2DDir.x );
	vec2DDir.y = roundf( vec2DDir.y );

	if (vec2DDir.x != 0.0f || vec2DDir.y != 0.0f)
	{
		Ray_t ray;
		trace_t tr;

		// 2 passes for XY axis and two diagonal axes
		const int NUM_DIRS = 3;
		const int NUM_PASSES = 2;
		for (int j = 0; j < (NUM_PASSES*NUM_DIRS); j++)
		{
			Vector vecDirToTry = Vector( vec2DDir.x, vec2DDir.y, 0 );
			if (vec2DDir.x == 0.0f || vec2DDir.y == 0.0f)
			{
				// Starts centered
				vec_t addDir = 0.0f;
				switch (j % NUM_DIRS)
				{
					case 0:	break;					// Default pass
					case 1:	addDir = 1.0f; break;	// Go right
					case 2:	addDir = -1.0f; break;	// Go left
				}
				(vec2DDir.x == 0.0f ? vecDirToTry.x : vecDirToTry.y) = addDir;
			}
			else
			{
				// Starts diagonal
				switch (j % NUM_DIRS)
				{
					case 0:	break;						// Default pass
					case 1:	vecDirToTry.x = 0.0f; break;	// Go right
					case 2:	vecDirToTry.y = 0.0f; break;	// Go left
				}
			}

			//-------------------------------------------------------------------------

			Vector vecTest = vecPlayerOrigin + (vecDirToTry * (pPlayer->BoundingRadius() * (float)((j/(NUM_PASSES+1))+1)));

			// Make sure it's not in a solid
			UTIL_TraceHull( vecTest, vecTest + Vector(0,0,1), pPlayer->GetPlayerMins(), pPlayer->GetPlayerMaxs(), MASK_PLAYERSOLID, pPlayer, COLLISION_GROUP_PLAYER, &tr );
			if (!tr.DidHit())
			{
				// Test with just the eye pos to make sure it's not going through a wall
				UTIL_TraceLine( pPlayer->EyePosition(), vecTest + pPlayer->GetViewOffset(), MASK_PLAYERSOLID, pPlayer, COLLISION_GROUP_PLAYER, &tr );
				if (!tr.DidHit())
				{
					// Now just make sure it's not still in the trigger
					ray.Init( vecTest + pPlayer->GetPlayerMins(), vecTest + pPlayer->GetPlayerMaxs() );
					enginetrace->ClipRayToCollideable( ray, MASK_ALL, pCollide, &tr );
					if (!tr.DidHit())
					{
						vecOrigin = vecTest;
						if (mp_save_debug_transition.GetBool())
							NDebugOverlay::Cross3D( vecTest, 5.0f, 0, 255, 0, true, 5.0f );
						return true;
					}
				}
			}

			if (mp_save_debug_transition.GetBool())
				NDebugOverlay::Cross3D( vecTest, 3.0f, 255, 0, 0, true, 5.0f );
		}
	}

	return false;
}

CBaseEntity *CMPSaveRestore::FindValidEscapePoint( const char *pszName, CBasePlayer *pPlayer, CBaseTrigger *pChangeLevel, Vector &vecOrigin )
{
	trace_t tr;
	CBaseEntity *pEscapePoint = gEntList.FindEntityByName( NULL, pszName, pChangeLevel, pPlayer, pChangeLevel );
	while (pEscapePoint)
	{
		vecOrigin = pEscapePoint->GetAbsOrigin();
		UTIL_TraceHull( vecOrigin, vecOrigin + Vector(0,0,1), pPlayer->GetPlayerMins(), pPlayer->GetPlayerMaxs(), MASK_PLAYERSOLID, pPlayer, COLLISION_GROUP_PLAYER, &tr );
		if (!tr.DidHit())
			break;

		pEscapePoint = gEntList.FindEntityByName( pEscapePoint, pszName, pChangeLevel, pPlayer, pChangeLevel );
	}

	return pEscapePoint;
}

void CMPSaveRestore::FreezePlayer( CBasePlayer *pPlayer )
{
	pPlayer->LockPlayerInPlace();
	pPlayer->RemoveFlag( FL_FROZEN ); // Don't use FL_FROZEN - Let them look around and press buttons
	pPlayer->AddFlag( FL_NOTARGET );
	pPlayer->DisableButtons( IN_ATTACK | IN_ATTACK2 | IN_ATTACK3 );
	pPlayer->m_nRenderFX = kRenderFxHologram;
	pPlayer->SetAbsVelocity( Vector() );
	
#ifdef HL2_DLL
	if ( PlayerCanCancelTransition( pPlayer ) )
	{
		// Give a HUD hint on how to escape
		UTIL_HudHintText( pPlayer, "#Valve_Hint_ExitTransition" );
	}
#endif

	// No LockPlayerInPlace()
	//pPlayer->AddFlag( FL_GODMODE | FL_NOTARGET ); // Don't use FL_FROZEN - Let them look around and press buttons
	//pPlayer->SetMoveType( MOVETYPE_NONE );
}

void CMPSaveRestore::UnfreezePlayer( CBasePlayer *pPlayer )
{
	pPlayer->UnlockPlayer();
	pPlayer->RemoveFlag( FL_NOTARGET );
	pPlayer->EnableButtons( IN_ATTACK | IN_ATTACK2 | IN_ATTACK3 );
	pPlayer->m_nRenderFX = kRenderFxNone;

	// No LockPlayerInPlace()
	//pPlayer->RemoveFlag( FL_GODMODE | FL_NOTARGET );
	//pPlayer->SetMoveType( MOVETYPE_WALK );
}

//-----------------------------------------------------------------------------

// From triggers.cpp
#define SF_CHANGELEVEL_NOTOUCH		0x0002
#define SF_CHANGELEVEL_CHAPTER		0x0004

void CMPSaveRestore::BeginTransitionSetup()
{
	// Disable all changelevels that are not the one we're operating with
	CBaseEntity *pChangeLevel = gEntList.FindEntityByClassname( NULL, "trigger_changelevel" );
	while (pChangeLevel)
	{
		if (pChangeLevel != m_hChangeLevel && !pChangeLevel->HasSpawnFlags( SF_CHANGELEVEL_NOTOUCH ) &&
			( !pChangeLevel->HasSpawnFlags(SF_CHANGELEVEL_CHAPTER) || gpGlobals->eLoadType != MapLoad_NewGame ))
		{
			pChangeLevel->RemoveSolidFlags( FSOLID_NOT_SOLID | FSOLID_TRIGGER );
		}

		pChangeLevel = gEntList.FindEntityByClassname( pChangeLevel, "trigger_changelevel" );
	}

	// Create a timer
	CMPTransitionTimer *pTimer = static_cast<CMPTransitionTimer *>(CreateEntityByName( "game_timer_transition" ));
	if (pTimer)
	{
		pTimer->KeyValue( "timer_length", mp_save_transition_wait_time.GetString() );
		pTimer->KeyValue( "warn_time", mp_save_transition_wait_time_warn.GetString() );
		pTimer->KeyValue( "progress_bar_max", CNumStr( PlayersWaitingToTransition() + PlayersNotInTransition() ) );
		pTimer->KeyValue( "timer_caption", "#LoadingProgress_Changelevel" );
		pTimer->KeyValue( "StartDisabled", "0" );

		DispatchSpawn( pTimer );
		m_hTransitionTimer = pTimer;
	}
}

void CMPSaveRestore::CleanupTransitionSetup()
{
	// Re-enable all changelevels
	CBaseEntity *pChangeLevel = gEntList.FindEntityByClassname( NULL, "trigger_changelevel" );
	while (pChangeLevel)
	{
		if (pChangeLevel != m_hChangeLevel && !pChangeLevel->HasSpawnFlags( SF_CHANGELEVEL_NOTOUCH ) &&
			( !pChangeLevel->HasSpawnFlags(SF_CHANGELEVEL_CHAPTER) || gpGlobals->eLoadType != MapLoad_NewGame ))
		{
			pChangeLevel->AddSolidFlags( FSOLID_NOT_SOLID | FSOLID_TRIGGER );
		}

		pChangeLevel = gEntList.FindEntityByClassname( pChangeLevel, "trigger_changelevel" );
	}

	// Remove the timer
	if (m_hTransitionTimer)
	{
		UTIL_Remove( m_hTransitionTimer );
		m_hTransitionTimer = NULL;
	}

	// Remove players from transition list
	for (int i = m_PlayersInTransition.Count() - 1; i >= 0; i--)
	{
		UnfreezePlayer( m_PlayersInTransition[i] );
		m_PlayersInTransition.FastRemove( i );
	}

	m_hChangeLevel = NULL;
	m_bForceReadyTransition = false;
}

int CMPSaveRestore::PlayersWaitingToTransition() const
{
	return m_PlayersInTransition.Count();
}

int CMPSaveRestore::PlayersNotInTransition() const
{
	int nNumClients = 0;
	for ( int i = 0; i < gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pOtherPlayer = UTIL_PlayerByIndex( i );
		if ( !pOtherPlayer || !pOtherPlayer->IsConnected() || pOtherPlayer->GetObserverMode() > OBS_MODE_FREEZECAM )
			continue;

		if ( !m_PlayersInTransition.HasElement( pOtherPlayer ) )
		{
			nNumClients++;
		}
	}

	return nNumClients;
}

bool CMPSaveRestore::IsPlayerWaitingToTransition( CBasePlayer *pPlayer ) const
{
	return m_PlayersInTransition.HasElement( pPlayer );
}

bool CMPSaveRestore::PlayerCanCancelTransition( CBasePlayer *pPlayer ) const
{
	int i = m_PlayersInTransition.Find( pPlayer );
	if (i == m_PlayersInTransition.InvalidIndex())
		return false;

	// There is an escape point we can use
	if (m_pszEscapePoint && *m_pszEscapePoint)
		return true;

	if (m_hChangeLevel)
	{
		// Not if we came from above
		if (m_hChangeLevel->WorldSpaceCenter().z < pPlayer->GetAbsOrigin().z)
			return false;

		// Test whether player is fully or partially overlapping
		// If the player seems to be completely inside, then don't do it (it's probably scripted)
		Vector vecDir = (pPlayer->GetAbsOrigin() - m_hChangeLevel->WorldSpaceCenter());
		vecDir.z = 0.0f;
		if ( m_hChangeLevel->PointIsWithin( pPlayer->WorldSpaceCenter() + ( pPlayer->BoundingRadius() * vecDir ) ) )
			return false;
	}

	return true;
}

bool CMPSaveRestore::AllPlayersReadyToTransition() const
{
	if (m_hTransitionTimer)
	{
		// We're ready if the timer is up
		if (static_cast<CMPTransitionTimer *>(m_hTransitionTimer.Get())->m_bTimerFinished)
			return true;
	}

	if (m_bForceReadyTransition)
		return true;

	// Check if all players are in our transition
	for ( int i = 0; i < gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pOtherPlayer = UTIL_PlayerByIndex( i );
		if ( !pOtherPlayer || !pOtherPlayer->IsConnected() || pOtherPlayer->GetObserverMode() > OBS_MODE_FREEZECAM )
			continue;

		if ( !m_PlayersInTransition.HasElement( pOtherPlayer ) )
		{
			// Not yet
			return false;
		}
	}

	return true;
}

void CMPSaveRestore::ForceReadyToTransition()
{
	m_bForceReadyTransition = true;
}

#endif

//----------------------------------------------------------------------------

#ifndef CLIENT_DLL
CON_COMMAND( mp_save, "Saves in multiplayer" )
{
	if ( args.ArgC() < 2 )
	{
		Warning( "Format: mp_save <save>\n" );
		return;
	}

	if ( g_MPSaveRestore.PlayersWaitingToTransition() )
	{
		Msg( "Can't save during transitions\n" );
		return;
	}
	
	CSaveRestoreData *pSaveData = g_ServerGameDLL.SaveInit( 0 );
	if ( !pSaveData )
	{
		Warning( "Unable to allocate save data\n" );
		return;
	}

	g_MPSaveRestore.SetSaving( true );
	
	g_pGameSaveRestoreBlockSet->PreSave( pSaveData );

	CSave saveHelper( pSaveData );

	const char *pszSave = args.Arg( 1 );
	g_MPSaveRestore.SaveFile( pSaveData, &saveHelper, pszSave );

	g_MPSaveRestore.SetSaving( false );

	g_MPSaveRestore.CleanupSave( pSaveData );

	// Save metadata
	KeyValues *pKV = new KeyValues( "MPSaveData" );
	if ( pKV )
	{
		pKV->SetString( "map_from", STRING( gpGlobals->mapname ) );

		pKV->SaveToFile( g_pFullFileSystem, UTIL_VarArgs( "save/%s.mpsav.txt", pszSave ), "MOD" );
		pKV->deleteThis();
	}
}
#endif

// We have load-related commands on both the server and the client so that both can execute them

#ifdef CLIENT_DLL
CON_COMMAND_F( mp_load, "", FCVAR_HIDDEN )
#else
CON_COMMAND( mp_load, "Loads a save in multiplayer" )
#endif
{
	if ( args.ArgC() < 2 )
	{
		Warning( "Format: mp_load <save>\n" );
		return;
	}
	
#ifdef CLIENT_DLL
	char szPath[MAX_PATH];
	ResolveMPSavePath( szPath, args.Arg( 1 ), sizeof(szPath), false );

	KeyValues *pSaveMetadata = new KeyValues( "MPSaveData" );
	if ( pSaveMetadata && pSaveMetadata->LoadFromFile( g_pFullFileSystem, VarArgs( "%s.txt", szPath ), "MOD" ) )
	{
		const char *pszMap = pSaveMetadata->GetString( "map_from", NULL );
		if ( pszMap )
		{
			engine->ClientCmd_Unrestricted( VarArgs( "mp_save_dat_load \"%s\"; map %s", szPath, pszMap ) );
		}
	}
	pSaveMetadata->deleteThis();
#else
	const char *pszSave = args.Arg( 1 );
	g_MPSaveRestore.StartLoadingSave( pszSave );
#endif
}

#ifdef CLIENT_DLL
CON_COMMAND_F( mp_load_transition, "", FCVAR_HIDDEN )
#else
CON_COMMAND( mp_load_transition, "Loads an existing multiplayer transition save as if a level transition happened\nFormat: mp_load_transition <prev map> <next map> <landmark name>" )
#endif
{
	if ( args.ArgC() < 4 )
	{
		Warning( "Format: mp_load_transition <prev map> <next map> <landmark name>\n" );
		return;
	}

	const char *pszPrevMap = V_GetFileName( args.Arg( 1 ) );
	const char *pszNextMap = args.Arg( 2 );
	const char *pszLandmark = args.Arg( 3 );
	
#ifdef CLIENT_DLL
	engine->ClientCmd_Unrestricted( VarArgs( "mp_save_dat_map_from \"%s\"; mp_save_dat_landmark \"%s\"; map %s", pszPrevMap, pszLandmark, pszNextMap ) );
#else
	g_MPSaveRestore.StartTransition( pszLandmark );
	mp_save_dat_map_from.SetValue( pszPrevMap );

	engine->ChangeLevel( pszNextMap, NULL );
#endif
}

#ifndef CLIENT_DLL
CON_COMMAND( mp_save_transition_wait_cancel, "If players are currently waiting to transition levels, this forces it to happen immediately" )
{
	if (g_MPSaveRestore.PlayersWaitingToTransition())
		g_MPSaveRestore.ForceReadyToTransition();
}
#endif
