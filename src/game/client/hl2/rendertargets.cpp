//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose: Implements IClientRenderTargets
// 
// Author: Nooodles
//
//=============================================================================//
#include "cbase.h"
#include "rendertargets.h"

ConVar mapbase_water_resolution( "mapbase_water_resolution", "1024", FCVAR_NONE, "Needs to be set at game launch time to override." );
ConVar mapbase_monitor_resolution( "mapbase_monitor_resolution", "1024", FCVAR_NONE, "Needs to be set at game launch time to override." );

void CRenderTargets::InitClientRenderTargets( IMaterialSystem* pMaterialSystem, IMaterialSystemHardwareConfig* pHardwareConfig )
{
	BaseClass::InitClientRenderTargets( pMaterialSystem, pHardwareConfig, 
		mapbase_water_resolution.GetInt(), mapbase_monitor_resolution.GetInt() );
}

//-----------------------------------------------------------------------------
// Purpose: Shutdown client render targets. This gets called during shutdown in the engine
// Input  :  - 
//-----------------------------------------------------------------------------
void CRenderTargets::ShutdownClientRenderTargets()
{
	BaseClass::ShutdownClientRenderTargets();
}

EXPOSE_INTERFACE( CRenderTargets, IClientRenderTargets, CLIENTRENDERTARGETS_INTERFACE_VERSION );