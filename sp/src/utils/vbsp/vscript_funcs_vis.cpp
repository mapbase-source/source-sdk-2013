//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "tier1/KeyValues.h"
#include "tier1/fmtstr.h"

#ifdef PROPPER
#include "../propper/propper.h"
#else
#include "vbsp.h"
#endif
#include "map.h"
#include "fgdlib/fgdlib.h"

#include "vscript_vbsp.h"
#include "vscript_funcs_vis.h"

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

// There are currently no vis-related functions, but it would be nice to have them in the future.

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

void RegisterVisScriptFunctions()
{
	//ScriptRegisterFunction( g_pScriptVM, VMFKV_CreateBlank, "Creates a CScriptKeyValues instance with VMF formatting." );
}
