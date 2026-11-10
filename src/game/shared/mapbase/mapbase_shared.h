//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose: Carries the Mapbase CAutoGameSystem that loads manifest among other things.
//			Also includes code that does not fit anywhere else.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include "igamesystem.h"
#include "filesystem.h"
#include "vstdlib/IKeyValuesSystem.h"

//-----------------------------------------------------------------------------
// Purpose: System used to load map-specific files, etc.
//-----------------------------------------------------------------------------
class CMapbaseSystem : public CAutoGameSystem
{
public:
	DECLARE_DATADESC();

	CMapbaseSystem() : CAutoGameSystem( "CMapbaseSystem" )
	{
	}

	inline bool GetGameInfoKeyValues(KeyValues *pKeyValues)
	{
		// TODO: If there's a custom GameUI override, draw from its KV instead
		return pKeyValues->LoadFromFile( filesystem, "gameinfo.txt", "MOD" );
	}

	virtual bool Init();
	virtual void LevelInitPreEntity();
	virtual void LevelInitPostEntity();
	virtual void LevelShutdownPreEntity();
	virtual void LevelShutdownPostEntity();

#ifdef CLIENT_DLL
	inline const char *MapNameNoExt() { return MapName(); }
#else
	inline const char *MapNameNoExt() { return STRING( gpGlobals->mapname ); }
#endif

	void RefreshMapName();
	void RefreshCustomTalker();

#ifdef CLIENT_DLL
	void InitializeRTs();
	void Shutdown();
#endif

	void ParseGenericManifest();

	void InitManifestFileSymbols();
	void AddManifestFile( const char *file );
	void LoadFromValue( const char *value, int type, bool bWarn );
	void LoadFromValue( const char *value, int type );
	void AddManifestFile( KeyValues *pKV, const char *pszMapName, bool bDontWarn = false );

private:

#ifdef CLIENT_DLL
	void ManifestLoadCustomCloseCaption( const char *pszFile );
	void ManifestLoadCustomScheme( const char *pszFile );
	void ManifestLoadCustomHudAnimations( const char *pszFile );
	void ManifestLoadCustomHudLayout( const char *pszFile );
#endif

public:

	void LoadCustomSoundscriptFile( const char *szScript )		{ LoadFromValue( szScript, MANIFEST_SOUNDSCRIPTS, false ); }
	void LoadCustomLocalizationFile( const char *szScript )		{ LoadFromValue( szScript, MANIFEST_LOCALIZATION, false ); }
	void LoadCustomSurfacePropsFile( const char *szScript )		{ LoadFromValue( szScript, MANIFEST_SURFACEPROPS, false ); }
#ifdef CLIENT_DLL
	void LoadCustomCloseCaptionFile( const char *szScript )		{ LoadFromValue( szScript, MANIFEST_CLOSECAPTION, false ); }
	void LoadCustomVGUIFile( const char *szScript )				{ LoadFromValue( szScript, MANIFEST_VGUI, false ); }
	void LoadCustomClientSchemeFile( const char *szScript )		{ LoadFromValue( szScript, MANIFEST_CLIENTSCHEME, false ); }
	void LoadCustomHUDAnimationsFile( const char *szScript )	{ LoadFromValue( szScript, MANIFEST_HUDANIMATIONS, false ); }
	void LoadCustomHUDLayoutFile( const char *szScript )		{ LoadFromValue( szScript, MANIFEST_HUDLAYOUT, false ); }
#else
	void LoadCustomTalkerFile( const char *szScript )			{ LoadFromValue( szScript, MANIFEST_TALKER, false ); }
	void LoadCustomActbusyFile( const char *szScript )			{ LoadFromValue( szScript, MANIFEST_ACTBUSY, false ); }
	void LoadCustomChoreoSentenceFile( const char *szScript )	{ LoadFromValue( szScript, MANIFEST_CHOREOSENTENCES, false ); }
#endif

	const char *GetModName() const;

#ifdef MAPBASE_VSCRIPT
	void ScriptAddManifestFile( const char *szScript ) { AddManifestFile( szScript ); }

	virtual void RegisterVScript()
	{
		g_pScriptVM->RegisterInstance( this, "Mapbase" );
	}
#endif

public:

	enum
	{
		MANIFEST_SOUNDSCRIPTS,
		//MANIFEST_PROPDATA,
		//MANIFEST_SOUNDSCAPES,
		MANIFEST_LOCALIZATION,
		MANIFEST_SURFACEPROPS,
#ifdef CLIENT_DLL
		MANIFEST_CLOSECAPTION,
		MANIFEST_VGUI,
		MANIFEST_CLIENTSCHEME,
		MANIFEST_HUDANIMATIONS,
		MANIFEST_HUDLAYOUT,
#else
		MANIFEST_TALKER,
		MANIFEST_CHOREOSENTENCES,
		MANIFEST_ACTBUSY,
#endif
#ifdef MAPBASE_VSCRIPT
		MANIFEST_VSCRIPT,
#endif
#if defined(HL2_DLL) || defined(HL2_CLIENT_DLL)
		MANIFEST_PROTAGONISTS,	// See protagonist_system.h
#endif

		// Must always be kept below
		MANIFEST_NUM_TYPES,
	};

	struct ManifestType_t
	{
		ManifestType_t( const char *_string, const char *cvarname, const char *cvardesc )
		{
			string = _string;
		}

		//int type;
		const char *string;
	};

private:

	HKeySymbol	m_iManifestFileSymbols[MANIFEST_NUM_TYPES] = { -1 };

#ifdef CLIENT_DLL
	bool m_bInitializedRTs = false;
	CUtlVector<CTextureReference> m_CameraTextures;

	CUtlVector<CUtlSymbol> m_CloseCaptionFileNames;
#endif
};

// 
// Dynamic mod-based mod title comments
// 
typedef struct
{
	char pBSPName[64];
	char pTitleName[64];
} MODTITLECOMMENT;

typedef struct
{
	int iChapter;
	char pChapterName[64];
} MODCHAPTER;

extern CUtlVector<MODTITLECOMMENT> *Mapbase_GetChapterMaps();
extern CUtlVector<MODCHAPTER> *Mapbase_GetChapterList();
