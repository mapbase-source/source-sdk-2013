//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 =================
//
// Purpose: General matching functions for wildcards and leading operators (e.g. '!=1').
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"

#include "matchers.h"
#include "fmtstr.h"

bool Matcher_Match(const char *pszQuery, const char *szValue)
{
#ifdef CLIENT_DLL
	// FIXME: There is no clientside equivalent to the RS code
	return Matcher_NamesMatch( pszQuery, szValue );
#else
	// This sends our comparison directly to the Response System, which is responsible for leading operator functions.
	// Then, if applicable, it passes it along to Matcher_NamesMatch to handle the wildcards.
	extern bool ResponseSystemCompare( const char *criterion, const char *value );
	return ResponseSystemCompare( pszQuery, szValue );
#endif
}

bool Matcher_Match(const char *pszQuery, int iValue) { return Matcher_Match(pszQuery, CNumStr(iValue)); }
bool Matcher_Match(const char *pszQuery, float flValue) { return Matcher_Match(pszQuery, CNumStr(flValue)); }
