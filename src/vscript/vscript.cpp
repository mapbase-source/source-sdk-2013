//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose: Custom implementation of VScript in Source 2013, created from scratch
//			using the Alien Swarm SDK as a reference for Valve's library.
// 
// Author(s): ReDucTor (header written by Blixibon)
//
// $NoKeywords: $
//=============================================================================//

#include "vscript/ivscript.h"

#include "vscript_bindings_base.h"

#include "tier1/tier1.h"

IScriptVM* makeSquirrelVM();

int vscript_token = 0;

class CScriptManager : public CTier1AppSystem<IScriptManager>
{
public:
	virtual IScriptVM* CreateVM(ScriptLanguage_t language) override
	{
		IScriptVM* pScriptVM = nullptr;
		if (language == SL_SQUIRREL)
		{
			pScriptVM = makeSquirrelVM();
		}
		else
		{
			return nullptr;
		}

		if (pScriptVM == nullptr)
		{
			return nullptr;
		}

		if (!pScriptVM->Init())
		{
			delete pScriptVM;
			return nullptr;
		}

		// Register base bindings for all VMs
		RegisterBaseBindings( pScriptVM );

		return pScriptVM;
	}

	virtual void DestroyVM(IScriptVM * pScriptVM) override
	{
		if (pScriptVM)
		{
			pScriptVM->Shutdown();
			delete pScriptVM;
		}
	}

	// Mapbase moves CScriptKeyValues into the library so it could be used elsewhere

	// if bBorrow is false, CScriptKeyValues owns pKV memory
	// Functions returning the result need to return HSCRIPT_RC
	// see comment on IScriptVM::RegisterInstance()
	virtual HSCRIPT CreateScriptKeyValues( IScriptVM *pVM, KeyValues *pKV, bool bBorrow ) override
	{
		CScriptKeyValues *pSKV = new CScriptKeyValues( pKV, bBorrow );
		HSCRIPT hSKV = pVM->RegisterInstance( pSKV, true );
		return hSKV;
	}

	virtual KeyValues *GetKeyValuesFromScriptKV( IScriptVM *pVM, HSCRIPT hSKV ) override
	{
		CScriptKeyValues *pSKV = (hSKV ? (CScriptKeyValues*)pVM->GetInstanceValue( hSKV, GetScriptDesc( (CScriptKeyValues*)NULL ) ) : nullptr);
		if (pSKV)
		{
			return pSKV->GetKeyValues();
		}

		return nullptr;
	}
};

EXPOSE_SINGLE_INTERFACE(CScriptManager, IScriptManager, VSCRIPT_INTERFACE_VERSION);
