//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose:		HL2 implementation of CBaseClientRenderTargets for custom RT textures.
//
//				(Added by Mapbase for generalized use; most code will act as though
//				this is a default feature)
//
// Author:		Blixibon
//
//=============================================================================//
#ifndef HL2_RENDERTARGETS_H
#define HL2_RENDERTARGETS_H
#ifdef _WIN32
#pragma once
#endif

#include "baseclientrendertargets.h"

class CHL2RenderTargets : public CBaseClientRenderTargets
{
	// no networked vars
	DECLARE_CLASS_GAMEROOT( CHL2RenderTargets, CBaseClientRenderTargets );
public:
	void InitClientRenderTargets( IMaterialSystem *pMaterialSystem, IMaterialSystemHardwareConfig *pHardwareConfig );
	void ShutdownClientRenderTargets ( void );

protected:

	// 
	// Define custom RT textures below. See baseclientrendertargets.h for examples
	// 
};

extern CHL2RenderTargets *g_pHL2RenderTargets;

#endif // HL2_RENDERTARGETS_H