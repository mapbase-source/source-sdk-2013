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

#include "cbase.h"
#include "hl2_rendertargets.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"

//-----------------------------------------------------------------------------
// Purpose: Called by the engine in material system init and shutdown.
//			Clients should override this in their inherited version, but the base
//			is to init all standard render targets for use.
// Input  : pMaterialSystem - the engine's material system (our singleton is not yet inited at the time this is called)
//			pHardwareConfig - the user hardware config, useful for conditional render target setup
//-----------------------------------------------------------------------------
void CHL2RenderTargets::InitClientRenderTargets( IMaterialSystem* pMaterialSystem, IMaterialSystemHardwareConfig* pHardwareConfig )
{
	BaseClass::InitClientRenderTargets( pMaterialSystem, pHardwareConfig );
}

//-----------------------------------------------------------------------------
// Purpose: Shut down each CTextureReference we created in InitClientRenderTargets.
//			Called by the engine in material system shutdown.
// Input  :  - 
//-----------------------------------------------------------------------------
void CHL2RenderTargets::ShutdownClientRenderTargets()
{
	BaseClass::ShutdownClientRenderTargets();
}

static CHL2RenderTargets g_HL2RenderTargets;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CHL2RenderTargets, IClientRenderTargets,
	CLIENTRENDERTARGETS_INTERFACE_VERSION, g_HL2RenderTargets );
CHL2RenderTargets* g_pHL2RenderTargets = &g_HL2RenderTargets;
