//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef MAP_H
#define MAP_H
#ifdef _WIN32
#pragma once
#endif


// All the brush sides referenced by info_no_dynamic_shadow entities.
extern CUtlVector<int> g_NoDynamicShadowSides;

#ifdef PROPPER
//Carl
//extern brush_texture_t	side_brushtextures[MAX_MAP_BRUSHSIDES];
#endif //PROPPER
#endif // MAP_H
