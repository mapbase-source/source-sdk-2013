//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 =================
//
// Purpose: General matching functions for wildcards and leading operators (e.g. '!=1').
//
// $NoKeywords: $
//=============================================================================

#ifndef MAPBASE_MATCHERS_H
#define MAPBASE_MATCHERS_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/mapbase_matchers_base.h"

// Complex string comparison.
// Scans pszQuery for leading operators (!=, <, etc.) and then evaluates szValue against them,
// if applicable. Then, uses Matcher_NamesMatch to compare using wildcards if necessary.
bool Matcher_Match( const char *pszQuery, const char *szValue );
bool Matcher_Match( const char *pszQuery, int iValue );
bool Matcher_Match( const char *pszQuery, float flValue );

#endif
