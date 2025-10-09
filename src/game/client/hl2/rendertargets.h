//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose: Implements IClientRenderTargets
// 
// Author: Nooodles
//
//=============================================================================//

#ifndef RENDERTARGETS_H
#define RENDERTARGETS_H
#ifdef _WIN32
#pragma once
#endif

#include "baseclientrendertargets.h"

class CRenderTargets : public CBaseClientRenderTargets
{
	DECLARE_CLASS_GAMEROOT( CRenderTargets, CBaseClientRenderTargets );
public:
	virtual void InitClientRenderTargets( IMaterialSystem* pMaterialSystem, IMaterialSystemHardwareConfig* pHardwareConfig );
	virtual void ShutdownClientRenderTargets();
};
#endif
