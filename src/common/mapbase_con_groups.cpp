//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 =================
//
// Purpose: Mapbase classifies certain types of console messages into groups with specific colors.
//			This is inspired by similar groups seen in CS:GO and Source 2 games.
//
//			See game_con_groups.cpp for the default client/server implementation.
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "basetypes.h"
#include "mapbase_con_groups.h"
#include "KeyValues.h"
#include "filesystem.h"
#include "mapbase_matchers_base.h"

bool ConGroup_t::g_bIncludeConGroupNames = false;

static CUtlVector<ConGroup_t *> g_ConGroups;

//-----------------------------------------------------------------------------

ConGroup_t::ConGroup_t( CUtlVector<ConGroup_t *> &list, const char *_pszName )
{
	pszName = _pszName;
	clr.SetColor( 224, 224, 224, 255 ); // Default to a shade of gray

	list.AddToTail( this );
}

ConGroup_t::~ConGroup_t()
{
	// Assume we've already been added to the main list
	g_ConGroups.FindAndRemove( this );
}

//-----------------------------------------------------------------------------

// Base console groups
ConGroup_t ConGroup_MapbaseMisc		( g_ConGroups, "Mapbase misc." );
ConGroup_t ConGroup_ResponseSys		( g_ConGroups, "Response System" );
ConGroup_t ConGroup_VScript			( g_ConGroups, "VScript" );
ConGroup_t ConGroup_VScriptPrint	( g_ConGroups, "VScript print" );

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

int FindConGroup( const char *pszName )
{
	for (int i = 0; i < g_ConGroups.Count(); i++)
	{
		if (Q_stricmp( pszName, g_ConGroups[i]->pszName ) == 0)
			return i;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Loads console groups
//-----------------------------------------------------------------------------
void LoadConsoleGroupsFromFile( IBaseFileSystem *filesystem, const char *pszFileName, const char *pathID = NULL )
{
	KeyValues *pGroupRoot = new KeyValues( "ConsoleGroups" );

	pGroupRoot->LoadFromFile( filesystem, pszFileName, pathID );

	KeyValues *pGroup = NULL;
	for ( pGroup = pGroupRoot->GetFirstTrueSubKey(); pGroup; pGroup = pGroup->GetNextTrueSubKey() )
	{
		int index = FindConGroup( pGroup->GetName() );
		if (index != -1)
		{
			Color msgClr = pGroup->GetColor( "MessageColor" );

			// Make sure the color isn't 0,0,0,0 before assigning
			if (msgClr.GetRawColor() != 0)
				g_ConGroups[index]->clr = msgClr;

			g_ConGroups[index]->bDisabled = pGroup->GetBool( "Disabled", false );
		}
		else
		{
			Warning( "Invalid console group %s (new groups should be defined in the code)\n", pGroup->GetName() );
		}
	}

	pGroupRoot->deleteThis();
}

void InitConsoleGroups( IBaseFileSystem *filesystem, CUtlVector<ConGroup_t *> *list )
{
	if ( list )
	{
		g_ConGroups.AddVectorToTail( *list );
		list->Purge();
	}

	LoadConsoleGroupsFromFile( filesystem, "scripts/mapbase_con_groups.txt" );
	LoadConsoleGroupsFromFile( filesystem, "scripts/mod_con_groups.txt" );
}

void PrintAllConsoleGroups()
{
	Msg( "============================================================\n" );
	for (int i = 0; i < g_ConGroups.Count(); i++)
	{
		ConColorMsg( g_ConGroups[i]->clr, "	# %s", g_ConGroups[i]->pszName );

		if (g_ConGroups[i]->bDisabled)
			Msg(" [DISABLED]");

		//Msg( " - %s ", g_ConGroups[i]->pszDesc );

		Msg("\n");
	}
	Msg( "============================================================\n" );
}

void ToggleConsoleGroups( const char *pszQuery )
{
	bool bMatched = false;

	for (int i = 0; i < g_ConGroups.Count(); i++)
	{
		if (Matcher_NamesMatch( pszQuery, g_ConGroups[i]->pszName ))
		{
			Msg( "%s is now %s\n", g_ConGroups[i]->pszName, g_ConGroups[i]->bDisabled ? "enabled" : "disabled" );
			g_ConGroups[i]->bDisabled = !g_ConGroups[i]->bDisabled;
			bMatched = true;
		}
	}

	if (!bMatched)
		Msg( "No groups matching \"%s\"\n", pszQuery );
}

void SetConsoleGroupIncludeNames( bool bToggle )
{
	ConGroup_t::g_bIncludeConGroupNames = bToggle;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

void _CGMsgFixup( int level, const ConGroup_t &group, const tchar* pMsg, ... )
{
	// Return early if we're not at this level
	if (!IsSpewActive("developer", level))
		return;

	char string[ 2048 ];
	va_list argptr;
	va_start( argptr, pMsg );
	Q_vsnprintf( string, sizeof(string), pMsg, argptr );
	va_end( argptr );

	// For now, this is the only reason we would be here
	Assert( ConGroup_t::g_bIncludeConGroupNames );

	ConColorMsg( level, group.clr, "[%s] %s", group.pszName, string );
}
