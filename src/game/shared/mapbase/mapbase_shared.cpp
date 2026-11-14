//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose: Carries the Mapbase CAutoGameSystem that loads manifest among other things.
//			Also includes code that does not fit anywhere else.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include "mapbase_shared.h"
#include "tier0/icommandline.h"
#include "tier1/mapbase_matchers_base.h"
#include <vgui_controls/Controls.h> 
#include <vgui/ILocalize.h>
#include "saverestore_utlvector.h"
#include "props_shared.h"
#include "utlbuffer.h"
#include "usermessages.h"
#ifdef CLIENT_DLL
#include "hud_closecaption.h"
#include "panelmetaclassmgr.h"
#include "c_soundscape.h"
#include "hud_macros.h"
#include "clientmode_shared.h"
#else
#include "soundscape_system.h"
#include "AI_ResponseSystem.h"
#include "mapbase/SystemConvarMod.h"
#include "gameinterface.h"
#include "mapbase/choreosentence.h"
#endif
#if defined(HL2_DLL) || defined(HL2_CLIENT_DLL)
#include "protagonist_system.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Mod Data
//-----------------------------------------------------------------------------

#ifdef CLIENT_DLL
// This constant should change with each Mapbase update
ConVar mapbase_version_client( "mapbase_version_client", MAPBASE_VERSION, FCVAR_NONE, "The version of Mapbase currently being used in this mod's client.dll" );
#else
// This constant should change with each Mapbase update
ConVar mapbase_version( "mapbase_version", MAPBASE_VERSION, FCVAR_NONE, "The version of Mapbase currently being used in this mod's server.dll" );
#endif

// The game's name found in gameinfo.txt. Mostly used for Discord RPC.
char g_iszGameName[128];

#ifdef GAME_DLL
// Default player configuration
char g_szDefaultPlayerModel[MAX_PATH];
bool g_bDefaultPlayerLegs;
bool g_bDefaultPlayerDrawExternally;

char g_szDefaultHandsModel[MAX_PATH];
int g_iDefaultHandsSkin;
int g_iDefaultHandsBody;

#ifdef HL2_DLL
// See protagonist_system.h
char g_szDefaultProtagonist[MAX_PROTAGONIST_NAME];
#endif
#endif

//-----------------------------------------------------------------------------
// Map-Specific File Manifests
//-----------------------------------------------------------------------------

#define GENERIC_MANIFEST_FILE "scripts/mapbase_default_manifest.txt"

const char *g_MapName;

extern ISoundEmitterSystemBase *soundemitterbase;

#ifdef GAME_DLL
ConVar mapbase_load_default_manifest( "mapbase_load_default_manifest", "1", FCVAR_NONE, "Should we automatically load our default manifest file? (\"maps/%mapname%_manifest.txt\")" );

ConVar mapbase_flush_talker("mapbase_flush_talker", "1", FCVAR_NONE, "Normally, when a map with custom talker files is unloaded, the response system resets to rid itself of the custom file(s). Turn this convar off to prevent that from happening.");

#ifdef HL2_DLL
extern void ParseCustomActbusyFile(const char *file);
#endif

extern bool LoadResponseSystemFile(const char *scriptfile);
extern void ReloadResponseSystem();

// Reloads the response system when the map changes to avoid custom talker leaking
static bool g_bMapContainsCustomTalker;
#else
ConVar mapbase_load_default_manifest( "mapbase_load_default_manifest_client", "1", FCVAR_NONE, "Should we automatically load our default manifest file? (\"maps/%mapname%_manifest.txt\")" );

// This is from the vgui_controls library
extern vgui::HScheme g_iCustomClientSchemeOverride;

extern bool g_bUsingCustomHudAnimations;
bool g_bUsingCustomHudLayout = false;
#endif

extern void AddSurfacepropFile( const char *pFileName, IPhysicsSurfaceProps *pProps, IFileSystem *pFileSystem );

//-----------------------------------------------------------------------------

CMapbaseSystem	g_MapbaseSystem;

BEGIN_DATADESC_NO_BASE( CMapbaseSystem )

	//DEFINE_UTLVECTOR( m_StoredManifestFiles, FIELD_STRING ),

END_DATADESC()

#ifdef MAPBASE_VSCRIPT
BEGIN_SCRIPTDESC_ROOT( CMapbaseSystem, SCRIPT_SINGLETON "All-purpose Mapbase system primarily used for map-specific files." )

	DEFINE_SCRIPTFUNC_NAMED( ScriptAddManifestFile, "AddManifestFile", "Loads a manifest file." )
	DEFINE_SCRIPTFUNC( LoadCustomSoundscriptFile, "Loads a custom soundscript file." )
	DEFINE_SCRIPTFUNC( LoadCustomLocalizationFile, "Loads a custom localization file." )
	DEFINE_SCRIPTFUNC( LoadCustomSurfacePropsFile, "Loads a custom surface properties file." )
#ifdef CLIENT_DLL
	DEFINE_SCRIPTFUNC( LoadCustomCloseCaptionFile, "Loads a custom closed captions file." )
	DEFINE_SCRIPTFUNC( LoadCustomVGUIFile, "Loads a custom VGUI definitions file." )
	DEFINE_SCRIPTFUNC( LoadCustomClientSchemeFile, "Loads a custom ClientScheme.res override file." )
	DEFINE_SCRIPTFUNC( LoadCustomHUDAnimationsFile, "Loads a custom HUD animations override file." )
	DEFINE_SCRIPTFUNC( LoadCustomHUDLayoutFile, "Loads a custom HUD layout override file." )
#else
	DEFINE_SCRIPTFUNC( LoadCustomTalkerFile, "Loads a custom talker file." )
	DEFINE_SCRIPTFUNC( LoadCustomActbusyFile, "Loads a custom actbusy file." )
	DEFINE_SCRIPTFUNC( LoadCustomChoreoSentenceFile, "Loads a custom choreo sentence file." )
#endif

	DEFINE_SCRIPTFUNC( GetModName, "Gets the name of the mod. This is the name which shows up on Steam, RPC, etc." )

	// Legacy
	DEFINE_SCRIPTFUNC_NAMED( LoadCustomSoundscriptFile, "LoadSoundscriptFile", SCRIPT_HIDE )
#ifndef CLIENT_DLL
	DEFINE_SCRIPTFUNC_NAMED( LoadCustomTalkerFile, "LoadTalkerFile", SCRIPT_HIDE )
	DEFINE_SCRIPTFUNC_NAMED( LoadCustomActbusyFile, "LoadActbusyFile", SCRIPT_HIDE )
#endif

END_SCRIPTDESC();
#endif

//-----------------------------------------------------------------------------

bool CMapbaseSystem::Init()
{
	InitConsoleGroups( g_pFullFileSystem, &g_GameConGroups );

	// Checks gameinfo.txt for additional command line options
	KeyValues *gameinfo = new KeyValues("GameInfo");
	if (GetGameInfoKeyValues(gameinfo))
	{
		KeyValues *pCommandLineList = gameinfo->FindKey("CommandLine", false);
		if (pCommandLineList)
		{
			for (KeyValues *pKey = pCommandLineList->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey())
			{
				CommandLine()->AppendParm( pKey->GetName(), pKey->GetString() );
			}
		}
	}
	gameinfo->deleteThis();

#ifdef CLIENT_DLL
	// Shared Mapbase scripts to avoid overwriting mod files
	g_pVGuiLocalize->AddFile( "resource/mapbase_%language%.txt" );
#ifdef HL2_CLIENT_DLL
	g_pVGuiLocalize->AddFile( "resource/mapbase_hl2_%language%.txt" );
#endif
#ifdef TF_CLIENT_DLL
	g_pVGuiLocalize->AddFile( "resource/mapbase_tf_%language%.txt" );
#endif
#endif
	return true;
}

//-----------------------------------------------------------------------------

void CMapbaseSystem::LevelInitPreEntity()
{
#ifdef GAME_DLL
	CGMsg( 0, ConGroup_MapbaseMisc, "Mapbase system loaded (v%s)\n", MAPBASE_VERSION );
#endif

	// Checks gameinfo.txt for Mapbase-specific options
	KeyValues *gameinfo = new KeyValues("GameInfo");
	if (GetGameInfoKeyValues(gameinfo))
	{
		// Indicates this is a core Mapbase mod and not a mod using its code
		//g_bMapbaseCore = gameinfo->GetBool("mapbase_core", false);

		if (!gameinfo->GetBool("hide_mod_name", false))
		{
			// Store the game's name
			const char *pszGameName = gameinfo->GetString("game_rpc", NULL);
			if (pszGameName == NULL)
				pszGameName = gameinfo->GetString("game");

			Q_strncpy(g_iszGameName, pszGameName, sizeof(g_iszGameName));
		}

#ifdef GAME_DLL
		Q_strncpy( g_szDefaultPlayerModel, gameinfo->GetString( "player_default_model", "models/player.mdl" ), sizeof( g_szDefaultPlayerModel ) );
		g_bDefaultPlayerLegs = gameinfo->GetBool( "player_default_legs", false );
		g_bDefaultPlayerDrawExternally = gameinfo->GetBool( "player_default_draw_externally", false );

		Q_strncpy( g_szDefaultHandsModel, gameinfo->GetString( "player_default_hands", "models/weapons/v_hands.mdl" ), sizeof( g_szDefaultHandsModel ) );
		g_iDefaultHandsSkin = gameinfo->GetInt( "player_default_hands_skin", 0 );
		g_iDefaultHandsBody = gameinfo->GetInt( "player_default_hands_body", 0 );

#ifdef HL2_DLL
		Q_strncpy( g_szDefaultProtagonist, gameinfo->GetString( "player_default_protagonist", "" ), sizeof( g_szDefaultProtagonist ) );
#endif
#endif
	}
	gameinfo->deleteThis();

	// Shared Mapbase scripts to avoid overwriting mod files
#ifdef CLIENT_DLL
	PanelMetaClassMgr()->LoadMetaClassDefinitionFile( "scripts/vgui_screens_mapbase.txt" );
#endif

	if ( m_iManifestFileSymbols[0] == -1 )
		InitManifestFileSymbols();
}

//-----------------------------------------------------------------------------

void CMapbaseSystem::LevelInitPostEntity()
{
	// Load the generic script.
	ParseGenericManifest();
}

void CMapbaseSystem::LevelShutdownPreEntity()
{
	// How would we make sure they don't last between maps?
	// TODO: Investigate ReloadLocalizationFiles()
	//g_pVGuiLocalize->RemoveAll();
	//g_pVGuiLocalize->ReloadLocalizationFiles();
}

void CMapbaseSystem::LevelShutdownPostEntity()
{
	RefreshCustomTalker();

#ifdef CLIENT_DLL
	CHudCloseCaption *hudCloseCaption = GET_HUDELEMENT( CHudCloseCaption );
	FOR_EACH_VEC( m_CloseCaptionFileNames, i )
	{
		hudCloseCaption->RemoveCaptionDictionary( m_CloseCaptionFileNames[i] );
	}
	m_CloseCaptionFileNames.RemoveAll();

	if (g_iCustomClientSchemeOverride != 0 || g_bUsingCustomHudAnimations || g_bUsingCustomHudLayout)
	{
		CGMsg( 1, ConGroup_MapbaseMisc, "Mapbase: Reloading client mode and viewport scheme\n" );

		// TODO: We currently have no way of actually cleaning up custom schemes upon level unload.
		// That may or may not be sustainable if there's a ton of custom schemes loaded at once
		g_iCustomClientSchemeOverride = 0;

		g_bUsingCustomHudAnimations = false;
		g_bUsingCustomHudLayout = false;

		// Reload scheme
		ClientModeShared *mode = ( ClientModeShared * )GetClientModeNormal();
		if ( mode )
		{
			mode->ReloadScheme( true );

			// We need to reload default values, so load a special "hudlayout_mapbase.res" file that only contains
			// default Mapbase definitions identical to the defaults in the code
			CBaseViewport *pViewport = dynamic_cast<CBaseViewport *>(g_pClientMode->GetViewport());
			if (pViewport)
			{
				KeyValuesAD pConditions( "conditions" );
				g_pClientMode->ComputeVguiResConditions( pConditions );

				// reload the .res file from disk
				pViewport->LoadControlSettings( "scripts/hudlayout_mapbase.res", NULL, NULL, pConditions );
			}
		}
	}
#endif
}

//-----------------------------------------------------------------------------

void CMapbaseSystem::RefreshCustomTalker()
{
#ifdef GAME_DLL
	if (g_bMapContainsCustomTalker && mapbase_flush_talker.GetBool())
	{
		CGMsg( 1, ConGroup_MapbaseMisc, "Mapbase: Reloading response system to flush custom talker\n" );
		ReloadResponseSystem();
		g_bMapContainsCustomTalker = false;
	}
#endif
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Initialize custom RT textures if necessary
//-----------------------------------------------------------------------------
void CMapbaseSystem::InitializeRTs()
{
	if (!m_bInitializedRTs)
	{
		SetDefLessFunc( m_RenderTargetData );

		KeyValues *pKV = new KeyValues("RenderTargets");
		if (pKV->LoadFromFile( filesystem, "scripts/render_targets.txt" ))
		{
			HKeySymbol hShadowDepthTextures = KeyValuesSystem()->GetSymbolForString( "ShadowDepthTextures", false );
			for (KeyValues *pKey = pKV->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey())
			{
				if ( pKey->GetNameSymbol() == hShadowDepthTextures )
				{
					// Depth textures are split into subkeys
					for (KeyValues *pSub = pKey->GetFirstSubKey(); pSub; pSub = pSub->GetNextKey())
					{
						RenderTargetData_t data;
						data.nRes = pSub->GetInt( "res" );
						data.nNum = pSub->GetInt( "num" );
						m_RenderTargetData.Insert( pSub->GetNameSymbol(), data );
					}
				}
				else
				{
					RenderTargetData_t data;
					data.nRes = pKey->GetInt( "res" );
					data.nNum = pKey->GetInt( "num" );
					m_RenderTargetData.Insert( pKey->GetNameSymbol(), data );
				}
			}
		}
		pKV->deleteThis();

		int iNumCameras = CommandLine()->ParmValue( "-numcameratextures", 3 );

		materials->BeginRenderTargetAllocation();

		for (int i = 0; i < iNumCameras; i++)
		{
			char szName[32];
			Q_snprintf( szName, sizeof(szName), "_rt_Camera%i", i );

			int iRefIndex = m_CameraTextures.AddToTail();

			//m_CameraTextures[iRefIndex].InitRenderTarget(
			//	256, 256, RT_SIZE_DEFAULT,
			//	g_pMaterialSystem->GetBackBufferFormat(),
			//	MATERIAL_RT_DEPTH_SHARED, true, szName );

			m_CameraTextures[iRefIndex].Init( g_pMaterialSystem->CreateNamedRenderTargetTextureEx2(
				szName,
				256, 256, RT_SIZE_DEFAULT,
				g_pMaterialSystem->GetBackBufferFormat(),
				MATERIAL_RT_DEPTH_SHARED,
				0,
				CREATERENDERTARGETFLAGS_HDR ) );
		}

		materials->EndRenderTargetAllocation();

		m_bInitializedRTs = true;
	}
}

void CMapbaseSystem::Shutdown()
{
	if (m_bInitializedRTs)
	{
		for (int i = 0; i < m_CameraTextures.Count(); i++)
		{
			m_CameraTextures[i].Shutdown();
		}
		m_bInitializedRTs = false;
	}
}
#endif

// Get a generic, hardcoded manifest with hardcoded names.
void CMapbaseSystem::ParseGenericManifest()
{
	if (!mapbase_load_default_manifest.GetBool())
		return;

	KeyValues *pKV = new KeyValues("DefaultManifest");
	pKV->LoadFromFile(filesystem, GENERIC_MANIFEST_FILE);

	AddManifestFile(pKV, MapNameNoExt()/*, true*/);

	pKV->deleteThis();
}

inline HKeySymbol AddKeySymbolForName( const char *pszName )
{
	return KeyValuesSystem()->GetSymbolForString( pszName );
}

void CMapbaseSystem::InitManifestFileSymbols()
{
	// Keep in sync with the enum
	const char *szManifestFileStrings[MANIFEST_NUM_TYPES] = {
		"soundscripts",
		//"propdata",
		//"soundscapes",
		"localization",
		"surfaceprops",
#ifdef CLIENT_DLL
		"closecaption",
		"vgui",
		"clientscheme",
		"hudanimations",
		"hudlayout",
#else
		"talker",
		"choreosentences",
		"actbusy",
#endif
#ifdef MAPBASE_VSCRIPT
		"vscript",
#endif
#if defined(HL2_DLL) || defined(HL2_CLIENT_DLL)
		"protagonists",
#endif
	};

	for (int i = 0; i < MANIFEST_NUM_TYPES; i++)
	{
		m_iManifestFileSymbols[i] = KeyValuesSystem()->GetSymbolForString( szManifestFileStrings[i] );
	}
}

void CMapbaseSystem::AddManifestFile( const char *file )
{
	KeyValues *pKV = new KeyValues(file);
	if ( !pKV->LoadFromFile( filesystem, file ) )
	{
		Warning("Mapbase Manifest: \"%s\" is unreadable or missing (can't load KV, check for syntax errors)\n", file);
		pKV->deleteThis();
		return;
	}

	//CGMsg( 1, ConGroup_MapbaseMisc, "===== Mapbase Manifest: Loading manifest file %s =====\n", file );

	AddManifestFile(pKV, MapNameNoExt(), false);

	//CGMsg( 1, ConGroup_MapbaseMisc, "==============================================================================\n" );

	pKV->deleteThis();
}

void CMapbaseSystem::LoadFromValue( const char *value, int type, bool bDontWarn )
{
	if (type != MANIFEST_VSCRIPT && !filesystem->FileExists(value, "MOD"))
	{
		if (!bDontWarn)
		{
			Warning("Mapbase Manifest: WARNING! \"%s\" does not exist!\n", value);
		}
		return;
	}

	switch (type)
	{
		case MANIFEST_SOUNDSCRIPTS: { soundemitterbase->AddSoundOverrides(value); } break;
		//case MANIFEST_PROPDATA: { g_PropDataSystem.ParsePropDataFile(value); } break;
		case MANIFEST_LOCALIZATION: { g_pVGuiLocalize->AddFile( value, "MOD", true ); } break;
		case MANIFEST_SURFACEPROPS: { AddSurfacepropFile( value, physprops, filesystem ); } break;
#ifdef CLIENT_DLL
		case MANIFEST_CLOSECAPTION: { ManifestLoadCustomCloseCaption( value ); } break;
		case MANIFEST_VGUI:			{ PanelMetaClassMgr()->LoadMetaClassDefinitionFile( value ); } break;
		case MANIFEST_CLIENTSCHEME:	{ ManifestLoadCustomScheme( value ); } break;
		case MANIFEST_HUDANIMATIONS:	{ ManifestLoadCustomHudAnimations( value ); } break;
		case MANIFEST_HUDLAYOUT:	{ ManifestLoadCustomHudLayout( value ); } break;
		//case MANIFEST_SOUNDSCAPES: { Soundscape_AddFile(value); } break;
#else
		case MANIFEST_TALKER: {
				g_bMapContainsCustomTalker = true;
				LoadResponseSystemFile(value); //PrecacheCustomResponseSystem( value );
			} break;
		//case MANIFEST_SOUNDSCAPES: { g_SoundscapeSystem.AddSoundscapeFile(value); } break;
		case MANIFEST_CHOREOSENTENCES: { LoadChoreoSentenceFile(value); } break;
#ifdef HL2_DLL
		case MANIFEST_ACTBUSY: { ParseCustomActbusyFile(value); } break;
#endif
#endif
#ifdef MAPBASE_VSCRIPT
		case MANIFEST_VSCRIPT:		{ VScriptRunScript(value, false); } break;
#endif
#if defined(HL2_DLL) || defined(HL2_CLIENT_DLL)
		case MANIFEST_PROTAGONISTS: { g_ProtagonistSystem.LoadProtagonistFile( value ); } break;
#endif
	}
}

// This doesn't call deleteThis()!
void CMapbaseSystem::AddManifestFile(KeyValues *pKV, const char *pszMapName, bool bDontWarn)
{
	char value[MAX_PATH];
	int iNoErrorsSymbol = KeyValuesSystem()->GetSymbolForString( "NoErrors" );

	for (KeyValues *pKey = pKV->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey())
	{
		int iNameSymbol = pKey->GetNameSymbol();
		if (iNameSymbol == iNoErrorsSymbol)
		{
			bDontWarn = pKey->GetBool();
			continue;
		}

		value[0] = '\0';

		// Parse %mapname%, etc.
		bool inparam = false;
		CUtlStringList outStrings;
		V_SplitString( pKey->GetString(), "%", outStrings );
		FOR_EACH_VEC( outStrings, i )
		{
			if (inparam)
			{
				if (FStrEq( outStrings[i], "mapname" ))
				{
					Q_strncat( value, pszMapName, sizeof( value ) );
				}
				else if (FStrEq( outStrings[i], "language" ))
				{
#ifdef CLIENT_DLL
					char uilanguage[64];
					engine->GetUILanguage(uilanguage, sizeof(uilanguage));
					Q_strncat( value, uilanguage, sizeof( value ) );
#else
					// Give up, use English
					Q_strncat( value, "english", sizeof( value ) );
#endif
				}
			}
			else
			{
				Q_strncat( value, outStrings[i], sizeof( value ) );
			}

			inparam = !inparam;
		}

		outStrings.PurgeAndDeleteElements();

		for (int i = 0; i < MANIFEST_NUM_TYPES; i++)
		{
			if (iNameSymbol == m_iManifestFileSymbols[i])
			{
				LoadFromValue(value, i, bDontWarn);
				break;
			}
		}
	}
}

#ifdef CLIENT_DLL
void CMapbaseSystem::ManifestLoadCustomCloseCaption( const char *pszFile )
{
	if (GET_HUDELEMENT( CHudCloseCaption ))
		(GET_HUDELEMENT( CHudCloseCaption ))->AddCustomCaptionFile( pszFile, m_CloseCaptionFileNames );
}

// Custom scheme loading
void CMapbaseSystem::ManifestLoadCustomScheme( const char *pszFile )
{
	g_iCustomClientSchemeOverride = vgui::scheme()->LoadSchemeFromFile( pszFile, "CustomClientScheme" );

	// Reload scheme
	ClientModeShared *mode = ( ClientModeShared * )GetClientModeNormal();
	if ( mode )
	{
		mode->ReloadScheme( true );
	}
}

void CMapbaseSystem::ManifestLoadCustomHudAnimations( const char *pszFile )
{
	CBaseViewport *pViewport = dynamic_cast<CBaseViewport *>(g_pClientMode->GetViewport());
	if (pViewport)
	{
		g_bUsingCustomHudAnimations = true;
		if (!pViewport->LoadCustomHudAnimations( pszFile ))
		{
			g_bUsingCustomHudAnimations = false;
			CGWarning( 0, ConGroup_MapbaseMisc, "Custom HUD animations file \"%s\" failed to load\n", pszFile );
			pViewport->ReloadHudAnimations();
		}
		else
		{
			CGMsg( 1, ConGroup_MapbaseMisc, "Loaded custom HUD animations file \"%s\"\n", pszFile );;
		}
	}
}

void CMapbaseSystem::ManifestLoadCustomHudLayout( const char *pszFile )
{
	CBaseViewport *pViewport = dynamic_cast<CBaseViewport *>(g_pClientMode->GetViewport());
	if (pViewport)
	{
		g_bUsingCustomHudLayout = true;

		KeyValuesAD pConditions( "conditions" );
		g_pClientMode->ComputeVguiResConditions( pConditions );

		// reload the .res file from disk
		pViewport->LoadControlSettings( pszFile, NULL, NULL, pConditions );

		CGMsg( 1, ConGroup_MapbaseMisc, "Loaded custom HUD layout file \"%s\"\n", pszFile );;
	}
}
#endif

const char *CMapbaseSystem::GetModName() const
{
	return g_iszGameName;
}

//-----------------------------------------------------------------------------
// Chapters
//-----------------------------------------------------------------------------
#ifdef GAME_DLL
static CUtlVector<MODTITLECOMMENT> g_MapbaseChapterMaps;
static CUtlVector<MODCHAPTER> g_MapbaseChapterList;
CUtlVector<MODTITLECOMMENT> *Mapbase_GetChapterMaps()
{
	if (g_MapbaseChapterMaps.Count() == 0)
	{
		// Check the chapter list
		KeyValues *chapterlist = new KeyValues("ChapterList");
		if (chapterlist->LoadFromFile(filesystem, "scripts/chapters.txt", "MOD"))
		{
			KeyValues *pKey = chapterlist->GetFirstSubKey();
			if (pKey)
			{
				if (Q_stricmp( pKey->GetName(), "Chapters" ) == 0)
				{
					for (KeyValues *pChapters = pKey->GetFirstSubKey(); pChapters; pChapters = pChapters->GetNextKey())
					{
						int index = g_MapbaseChapterList.AddToTail();
						g_MapbaseChapterList[index].iChapter = atoi(pChapters->GetName());
						Q_strncpy(g_MapbaseChapterList[index].pChapterName, pChapters->GetString(), sizeof(g_MapbaseChapterList[index]));
					}
				}

				for (pKey = pKey->GetNextKey(); pKey; pKey = pKey->GetNextKey())
				{
					int index = g_MapbaseChapterMaps.AddToTail();
					Q_strncpy(g_MapbaseChapterMaps[index].pBSPName, pKey->GetName(), sizeof(g_MapbaseChapterMaps[index].pBSPName));
					Q_strncpy(g_MapbaseChapterMaps[index].pTitleName, pKey->GetString(), sizeof(g_MapbaseChapterMaps[index].pTitleName));

					//comment.pBSPName = pKey->GetName();
					//comment.pTitleName = pKey->GetString();
				}
			}
		}
		chapterlist->deleteThis();
	}

	return &g_MapbaseChapterMaps;
}

CUtlVector<MODCHAPTER> *Mapbase_GetChapterList()
{
	return &g_MapbaseChapterList;
}

int Mapbase_GetChapterCount()
{
	return g_MapbaseChapterList.Count();
}

ThreeState_t Flashlight_GetLegacyVersionKey()
{
	KeyValues *gameinfo = new KeyValues( "GameInfo" );
	if (g_MapbaseSystem.GetGameInfoKeyValues( gameinfo ))
	{
		// -1 = default
		int iUseLegacyFlashlight = gameinfo->GetInt( "use_legacy_flashlight", -1 );
		if (iUseLegacyFlashlight > -1)
			return iUseLegacyFlashlight != 0 ? TRS_TRUE : TRS_FALSE;
	}
	gameinfo->deleteThis();

	return TRS_NONE;
}
#endif

//-----------------------------------------------------------------------------

#ifndef CLIENT_DLL // TODO: Separate client cvars?
void CV_WildcardCvarChanged( IConVar *pConVar, const char *pOldString, float flOldValue );

ConVar mapbase_wildcards_enabled( "mapbase_wildcards_enabled", "1", FCVAR_NONE, "Toggles Mapbase's '?' wildcard and true '*' features. Useful for maps that have '?' in their targetnames.", CV_WildcardCvarChanged );
ConVar mapbase_regex_enabled( "mapbase_regex_enabled", "1", FCVAR_NONE, "Toggles Mapbase's regex matching handover.", CV_WildcardCvarChanged );

void CV_WildcardCvarChanged( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	SetMatchersEnabled( mapbase_wildcards_enabled.GetBool(), mapbase_regex_enabled.GetBool() );
}
#endif

//-----------------------------------------------------------------------------

void CV_IncludeNameChanged( IConVar *pConVar, const char *pOldString, float flOldValue );

#ifdef CLIENT_DLL
ConVar con_group_include_name_client( "con_group_include_name_client", "0", FCVAR_NONE, "Includes groups when printing on the client.", CV_IncludeNameChanged );

void CV_IncludeNameChanged( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	SetConsoleGroupIncludeNames( con_group_include_name_client.GetBool() );
}
#else
ConVar con_group_include_name( "con_group_include_name", "0", FCVAR_NONE, "Includes groups when printing.", CV_IncludeNameChanged );

void CV_IncludeNameChanged( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	SetConsoleGroupIncludeNames( con_group_include_name.GetBool() );
}
#endif

CON_COMMAND_SHARED( con_group_reload, "Reloads all console groups." )
{
#ifdef GAME_DLL
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;
#endif

	InitConsoleGroups( g_pFullFileSystem );
}

CON_COMMAND_SHARED( con_group_list, "Prints a list of all console groups." )
{
#ifdef GAME_DLL
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;
#endif

	PrintAllConsoleGroups();
}

CON_COMMAND_SHARED( con_group_toggle, "Toggles a console group." )
{
#ifdef GAME_DLL
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;
#endif

	ToggleConsoleGroups( args.Arg( 1 ) );
}

//-----------------------------------------------------------------------------

#ifndef CLIENT_DLL

// Used by CServerGameDLL::DLLInit to initialize Mapbase events.
// These are usually new events which are added separately so that mods don't need to manually add them to modevents.res.
// Events specific to a certain game are usually separated due to having game-specific uses and/or because they were ported from another
// game that already has it.
// modevents.res will still override events listed in these files.
void RegisterMapbaseGameEvents()
{
	gameeventmanager->LoadEventsFromFile( "resource/mapbaseevents.res" );

#ifdef MAPBASE_MP
	gameeventmanager->LoadEventsFromFile( "resource/mapbaseevents_mp.res" );
#endif
#ifdef HL2_DLL
	gameeventmanager->LoadEventsFromFile( "resource/mapbaseevents_hl2.res" );
#endif
#ifdef HL2MP
	gameeventmanager->LoadEventsFromFile( "resource/mapbaseevents_hl2mp.res" );
#endif
#ifdef TF_DLL
	gameeventmanager->LoadEventsFromFile( "resource/mapbaseevents_tf.res" );
#endif
}

#endif
