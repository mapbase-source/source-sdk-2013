//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose:		Multiplayer save/restore for level transitions.
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef MAPBASE_MP_SAVERESTORE_H
#define MAPBASE_MP_SAVERESTORE_H
#ifdef _WIN32
#pragma once
#endif

#include "saverestore.h"
#include "mapbase/game_timer.h"
#ifndef CLIENT_DLL
#include "triggers.h"
#endif

#ifndef CLIENT_DLL

//=============================================================================
//=============================================================================
class CMPSaveRestore : public CAutoGameSystem
{
public:
	CMPSaveRestore();
	~CMPSaveRestore();

	bool EnabledTransitions() const;
	bool EnabledTransitionsInDeathmatch() const;	// CChangeLevel checks this separately

	void StartTransition( const char *pszLandmark );
	void EndTransition();
	bool IsTransitioning() const;

	void SetSaving( bool bSaving );
	bool IsSaving() const;

	void StartLoadingSave( const char *pszSave );
	void StopLoadingSave();
	bool IsLoadingSave() const;

	//-----------------------------------------------------------------------------

	bool HasSaveEnts() const { return m_SaveEntities.Count() > 0; }
	void AddSaveEnt( CBaseEntity *pEnt ) { m_SaveEntities.AddToTail( pEnt ); }

	void LevelInitPreEntity();
	void LevelShutdownPostEntity();

	bool SaveInitEntities( CSaveRestoreData *pSaveData );
	void CleanupSave( CSaveRestoreData *pSaveData );
	void CleanupRestore( CSaveRestoreData *pSaveData );
	void CleanupRestorePrePlayers( CSaveRestoreData *pSaveData );
	void CleanupRestorePostPlayers( CSaveRestoreData *pSaveData );

	bool SaveFile( CSaveRestoreData *pSaveData, CSave *pSave, const char *pszSaveName );
	bool LoadFile( CSaveRestoreData *pSaveData );	// Call StartLoadingSave() to load a specific file

	bool SaveTransitionFile( CSaveRestoreData *pSaveData, CSave *pSave, const char *pszTargetMap, const char *pLandmarkName, int nLandmark );
	bool FindTransitionFile( const char *pszThisMap, const char **ppszOldMap, const char **ppLandmarkName );
	bool RestoreTransitionFile( CSaveRestoreData *pSaveData, const char *pszThisMap, const char *pszOldMap, const char *pLandmarkName );
	bool RestoreNextLevelFile( CSaveRestoreData *pSaveData, const char *pszThisMap, const char *pszOldMap, const char *pLandmarkName );
	void ClearTransitionFiles();

	bool HasPlayerData();
	bool RestorePlayer( CBasePlayer *pPlayer );
	bool IsRestoringPlayer( CBasePlayer *pPlayer = NULL );

	//-----------------------------------------------------------------------------

	void AddPlayerToTransition( CBasePlayer *pPlayer, CBaseTrigger *pChangeLevelTrigger, const char *pszEscapePoint );
	bool RemovePlayerFromTransition( CBasePlayer *pPlayer, bool bTryTeleportOutside );
	void BeginTransitionSetup();
	void CleanupTransitionSetup();

	int PlayersWaitingToTransition() const;
	int PlayersNotInTransition() const;
	bool IsPlayerWaitingToTransition( CBasePlayer *pPlayer ) const;
	bool PlayerCanCancelTransition( CBasePlayer *pPlayer ) const;
	bool AllPlayersReadyToTransition() const;
	void ForceReadyToTransition();

	bool FindChangelevelExit( CBasePlayer *pPlayer, CBaseTrigger *pChangeLevel, Vector &vecOrigin );
	CBaseEntity *FindValidEscapePoint( const char *pszName, CBasePlayer *pPlayer, CBaseTrigger *pChangeLevel, Vector &vecOrigin );

	void FreezePlayer( CBasePlayer *pPlayer );
	void UnfreezePlayer( CBasePlayer *pPlayer );

	//-----------------------------------------------------------------------------

private:

	void SaveSymbols( CSaveRestoreData *pSaveData, CUtlBuffer &buffer );
	void LoadSymbols( CSaveRestoreData *pSaveData, CUtlBuffer &buffer );

	void ResolveSavePath( char *szPath, const char *pszSaveName, size_t nPathSize );

private:
	bool m_bSaving = false;

	// Included temporarily while saving transition file
	int m_nLandmark;

	CUtlVector<CBaseEntity *>	m_SaveEntities;

	// Transition player counting
	CUtlVector<CBasePlayer *>	m_PlayersInTransition;
	CHandle<CBaseTrigger>	m_hChangeLevel;
	const char *m_pszEscapePoint;
	EHANDLE	m_hTransitionTimer;
	bool m_bForceReadyTransition;
};

extern CMPSaveRestore g_MPSaveRestore;

ISaveRestoreBlockHandler *GetMPPlayerSaveRestoreBlockHandler();

#endif

#endif // MAPBASE_MP_SAVERESTORE_H
