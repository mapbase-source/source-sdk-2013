//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 =================
//
// Purpose: See mapbase_con_groups.cpp for more information
//
// $NoKeywords: $
//=============================================================================

#ifndef MAPBASE_CON_GROUPS_H
#define MAPBASE_CON_GROUPS_H
#ifdef _WIN32
#pragma once
#endif

#include "Color.h"
#include "utlvector.h"

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

struct ConGroup_t
{
	ConGroup_t( CUtlVector<ConGroup_t *> &list, const char *_pszName );
	~ConGroup_t();

	const char *pszName;
	Color clr;

	bool bDisabled;

	//----------------------------------------

	static bool g_bIncludeConGroupNames;
};

//-----------------------------------------------------------------------------

extern ConGroup_t ConGroup_MapbaseMisc;
extern ConGroup_t ConGroup_ResponseSys;
extern ConGroup_t ConGroup_VScript;
extern ConGroup_t ConGroup_VScriptPrint;

// Preprocessors for old naming convention

#define CON_GROUP_MAPBASE_MISC		ConGroup_MapbaseMisc
#define CON_GROUP_RESPONSE_SYSTEM	ConGroup_ResponseSys
#define CON_GROUP_VSCRIPT			ConGroup_VScript
#define CON_GROUP_VSCRIPT_PRINT		ConGroup_VScriptPrint

//-----------------------------------------------------------------------------

// Fixes up the message instead of passing it directly to ConColorMsg. (do not call directly; call CGMsg instead)
void _CGMsgFixup( int level, const ConGroup_t &group, PRINTF_FORMAT_STRING const tchar *pMsg, ... ) FMTFUNCTION( 3, 4 );

// Mapbase console group message.
// 
// - "level" is developer level (like in DevMsg; use 0 for Msg)
// - "group" is the console group (use a string in the "CON_GROUP" function, or use one of the defaults)
// - "pMsg" and "..." are the message contents
// 
// See below this macro for more information
#define CGMsg( level, group, pMsg, ... ) \
	do { \
		if ( !group.bDisabled ) \
		{ \
			if ( ConGroup_t::g_bIncludeConGroupNames ) \
				_CGMsgFixup( level, group, pMsg, ##__VA_ARGS__ ); \
			else \
				ConColorMsg( level, group.clr, pMsg, ##__VA_ARGS__ ); \
		} \
	} while(0)

// We use a macro so that we can pass the variadic arguments directly without using va_list or variadic functions.
// (since the former is slow and the latter bloats too much with this)
// The "do while" is so that we can directly plug the name into instances of Msg/DevMsg without having to change
// anything else, since it needs to have multiple lines but also end with a semicolon.
// 
// The necessity of this mess is arguable, but it's filling in for direct calls to Msg/DevMsg, so I wanted to
// minimize the performance difference.
// (although calling Msg more than a few times per frame is usually a sign of a larger problem, anyway...)

#define CGWarning CGMsg

//-----------------------------------------------------------------------------

class IBaseFileSystem;

void InitConsoleGroups( IBaseFileSystem *filesystem, CUtlVector<ConGroup_t *> *list = NULL );

void PrintAllConsoleGroups();
void ToggleConsoleGroups( const char *pszQuery );
void SetConsoleGroupIncludeNames( bool bToggle );

#endif
