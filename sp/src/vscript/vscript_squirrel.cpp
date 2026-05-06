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
#include "tier1/utlbuffer.h"
#include "tier1/utlmap.h"
#include "tier1/utlstring.h"

#include "squirrel.h"
#include "sqstdaux.h"
//#include "sqstdblob.h"
//#include "sqstdsystem.h"
#include "sqstdtime.h"
//#include "sqstdio.h"
#include "sqstdmath.h"
#include "sqstdstring.h"

// HACK: Include internal parts of squirrel for serialization
#include "squirrel/squirrel/sqvm.h"
#include "squirrel/squirrel/sqobject.h"
#include "squirrel/squirrel/sqstate.h"
#include "squirrel/squirrel/sqtable.h"
#include "squirrel/squirrel/sqarray.h"
#include "squirrel/squirrel/sqclass.h"
#include "squirrel/squirrel/sqfuncproto.h"
#include "squirrel/squirrel/squserdata.h"
#include "squirrel/squirrel/sqclosure.h"

#include "sqdbg.h"

#include "tier1/utlbuffer.h"
#include "tier1/mapbase_con_groups.h"
#include "tier1/convar.h"

#include "vscript_squirrel.nut"

#include <cstdarg>

extern ConVar developer;


struct WriteStateMap
{
	CUtlRBTree< void* > cache;

	WriteStateMap() : cache( DefLessFunc(void*) )
	{}

	bool CheckCache( void* ptr, CUtlBuffer* pBuffer )
	{
		int idx = cache.Find( ptr );
		if ( idx != cache.InvalidIndex() )
		{
			pBuffer->PutInt( idx );
			return true;
		}
		else
		{
			int newIdx = cache.Insert( ptr );
			pBuffer->PutInt( newIdx );
			return false;
		}
	}
};

struct ReadStateMap
{
	CUtlMap< int, SQObject > cache;

	ReadStateMap() : cache( DefLessFunc(int) )
	{}

	bool CheckCache( SQObject* ptr, CUtlBuffer* pBuffer, int* outmarker )
	{
		int marker = pBuffer->GetInt();
		int idx = cache.Find( marker );
		if ( idx != cache.InvalidIndex() )
		{
			const SQObject &o = cache[idx];

			Assert( o._type == ptr->_type );

			ptr->_type = o._type;
			ptr->_unVal.raw = o._unVal.raw;

			return true;
		}
		else
		{
			*outmarker = marker;
			return false;
		}
	}

	void StoreInCache( int marker, const SQObject &obj )
	{
		int idx = cache.Insert( marker );
		SQObject &o = cache[idx];
		o._type = obj._type;
		o._unVal.raw = obj._unVal.raw;
	}
};


class SquirrelVM : public IScriptVM
{
public:
	virtual bool Init() override;
	virtual void Shutdown() override;

	virtual bool ConnectDebugger( int port = 0, float timeout = 0.0f ) override;
	virtual void DisconnectDebugger() override;

	virtual ScriptLanguage_t GetLanguage() override;
	virtual const char* GetLanguageName() override;

	virtual void AddSearchPath(const char* pszSearchPath) override;

	//--------------------------------------------------------

	virtual bool Frame(float simTime) override;

	//--------------------------------------------------------
	// Simple script usage
	//--------------------------------------------------------
	virtual ScriptStatus_t Run(const char* pszScript, bool bWait = true) override;

	//--------------------------------------------------------
	// Compilation
	//--------------------------------------------------------
	virtual HSCRIPT CompileScript(const char* pszScript, const char* pszId = NULL) override;
	virtual void ReleaseScript(HSCRIPT) override;

	//--------------------------------------------------------
	// Execution of compiled
	//--------------------------------------------------------
	virtual ScriptStatus_t Run(HSCRIPT hScript, HSCRIPT hScope = NULL, bool bWait = true) override;
	virtual ScriptStatus_t Run(HSCRIPT hScript, bool bWait) override;

	//--------------------------------------------------------
	// Scope
	//--------------------------------------------------------
	virtual HSCRIPT CreateScope(const char* pszScope, HSCRIPT hParent = NULL) override;
	virtual void ReleaseScope(HSCRIPT hScript) override;

	//--------------------------------------------------------
	// Script functions
	//--------------------------------------------------------
	virtual HSCRIPT LookupFunction(const char* pszFunction, HSCRIPT hScope = NULL) override;
	virtual void ReleaseFunction(HSCRIPT hScript) override;

	//--------------------------------------------------------
	// Script functions (raw, use Call())
	//--------------------------------------------------------
	virtual ScriptStatus_t ExecuteFunction(HSCRIPT hFunction, ScriptVariant_t* pArgs, int nArgs, ScriptVariant_t* pReturn, HSCRIPT hScope, bool bWait) override;

	//--------------------------------------------------------
	// Hooks
	//--------------------------------------------------------
	virtual HScriptRaw HScriptToRaw( HSCRIPT val ) override;
	virtual ScriptStatus_t ExecuteHookFunction( const char *pszEventName, ScriptVariant_t *pArgs, int nArgs, ScriptVariant_t *pReturn, HSCRIPT hScope, bool bWait ) override;

	//--------------------------------------------------------
	// External functions
	//--------------------------------------------------------
	virtual void RegisterFunction(ScriptFunctionBinding_t* pScriptFunction) override;

	//--------------------------------------------------------
	// External classes
	//--------------------------------------------------------
	virtual bool RegisterClass(ScriptClassDesc_t* pClassDesc) override;

	//--------------------------------------------------------
	// External constants
	//--------------------------------------------------------
	virtual void RegisterConstant(ScriptConstantBinding_t *pScriptConstant) override;

	//--------------------------------------------------------
	// External enums
	//--------------------------------------------------------
	virtual void RegisterEnum(ScriptEnumDesc_t *pEnumDesc) override;
	
	//--------------------------------------------------------
	// External hooks
	//--------------------------------------------------------
	virtual void RegisterHook(ScriptHook_t *pHookDesc) override;

	//--------------------------------------------------------
	// External instances. Note class will be auto-registered.
	//--------------------------------------------------------

	virtual HSCRIPT RegisterInstance(ScriptClassDesc_t* pDesc, void* pInstance, bool bRefCounted = false) override;
	virtual void SetInstanceUniqeId(HSCRIPT hInstance, const char* pszId) override;
	virtual void RemoveInstance(HSCRIPT hInstance) override;

	virtual void* GetInstanceValue(HSCRIPT hInstance, ScriptClassDesc_t* pExpectedType = NULL) override;

	//----------------------------------------------------------------------------

	virtual bool GenerateUniqueKey(const char* pszRoot, char* pBuf, int nBufSize) override;

	//----------------------------------------------------------------------------

	virtual bool ValueExists(HSCRIPT hScope, const char* pszKey) override;

	virtual bool SetValue(HSCRIPT hScope, const char* pszKey, const char* pszValue) override;
	virtual bool SetValue(HSCRIPT hScope, const char* pszKey, const ScriptVariant_t& value) override;
	virtual bool SetValue(HSCRIPT hScope, const ScriptVariant_t& key, const ScriptVariant_t& val) override;

	virtual void CreateTable(ScriptVariant_t& Table) override;
	virtual int	GetNumTableEntries(HSCRIPT hScope) override;
	virtual int GetKeyValue(HSCRIPT hScope, int nIterator, ScriptVariant_t* pKey, ScriptVariant_t* pValue) override;

	virtual bool GetValue(HSCRIPT hScope, const char* pszKey, ScriptVariant_t* pValue) override;
	virtual bool GetValue(HSCRIPT hScope, ScriptVariant_t key, ScriptVariant_t* pValue) override;
	virtual void ReleaseValue(ScriptVariant_t& value) override;

	virtual bool ClearValue(HSCRIPT hScope, const char* pszKey) override;
	virtual bool ClearValue( HSCRIPT hScope, ScriptVariant_t pKey ) override;

	virtual void CreateArray(ScriptVariant_t &arr, int size = 0) override;
	virtual bool ArrayAppend(HSCRIPT hArray, const ScriptVariant_t &val) override;
	virtual HSCRIPT CopyObject(HSCRIPT obj) override;

	//----------------------------------------------------------------------------

	virtual void WriteState(CUtlBuffer* pBuffer) override;
	virtual void ReadState(CUtlBuffer* pBuffer) override;
	virtual void RemoveOrphanInstances() override;

	virtual void DumpState() override;

	virtual void SetOutputCallback(ScriptOutputFunc_t pFunc) override;
	virtual void SetErrorCallback(ScriptErrorFunc_t pFunc) override;

	//----------------------------------------------------------------------------

	virtual bool RaiseException(const char* pszExceptionText) override;

	void WriteObject( const SQObjectPtr &obj, CUtlBuffer* pBuffer, WriteStateMap& writeState );

	// Do not implicitly add/remove ref
	void WriteObject( const SQObject &obj, CUtlBuffer* pBuffer, WriteStateMap& writeState )
	{
		WriteObject( (const SQObjectPtr&)obj, pBuffer, writeState );
	}

	void WriteObject( SQTable *pObj, CUtlBuffer* pBuffer, WriteStateMap& writeState )
	{
		SQObject obj;
		obj._type = OT_TABLE;
		obj._unVal.pUserPointer = pObj;
		WriteObject( (const SQObjectPtr&)obj, pBuffer, writeState );
	}

	void WriteObject( SQClass *pObj, CUtlBuffer* pBuffer, WriteStateMap& writeState )
	{
		SQObject obj;
		obj._type = OT_CLASS;
		obj._unVal.pUserPointer = pObj;
		WriteObject( (const SQObjectPtr&)obj, pBuffer, writeState );
	}

	void WriteObject( SQWeakRef *pObj, CUtlBuffer* pBuffer, WriteStateMap& writeState )
	{
		SQObject obj;
		obj._type = OT_WEAKREF;
		obj._unVal.pUserPointer = pObj;
		WriteObject( (const SQObjectPtr&)obj, pBuffer, writeState );
	}

	void WriteObject( SQFunctionProto *pObj, CUtlBuffer* pBuffer, WriteStateMap& writeState )
	{
		SQObject obj;
		obj._type = OT_FUNCPROTO;
		obj._unVal.pUserPointer = pObj;
		WriteObject( (const SQObjectPtr&)obj, pBuffer, writeState );
	}

	void WriteObject( SQGenerator *pObj, CUtlBuffer* pBuffer, WriteStateMap& writeState )
	{
		SQObject obj;
		obj._type = OT_GENERATOR;
		obj._unVal.pUserPointer = pObj;
		WriteObject( (const SQObjectPtr&)obj, pBuffer, writeState );
	}

	void ReadObject( SQObjectPtr &obj, CUtlBuffer* pBuffer, ReadStateMap& readState );

	// Do not implicity add/remove ref
	void ReadObject( SQObject &obj, CUtlBuffer* pBuffer, ReadStateMap& readState )
	{
		obj._type = OT_NULL;
		obj._unVal.pUserPointer = 0;
		ReadObject( (SQObjectPtr&)obj, pBuffer, readState );
	}

	void WriteVM( SQVM *pThis, CUtlBuffer *pBuffer, WriteStateMap &writeState );
	void ReadVM( SQVM *pThis, CUtlBuffer *pBuffer, ReadStateMap &readState );

	HSQUIRRELVM vm_ = nullptr;
	HSQOBJECT lastError_;
	HSQOBJECT vectorClass_;
	HSQOBJECT regexpClass_;
	HSQDEBUGSERVER debugger_ = nullptr;
};

static char TYPETAG_VECTOR[] = "VectorTypeTag";

namespace SQVector
{
	SQInteger Construct(HSQUIRRELVM vm)
	{
		// TODO: There must be a nicer way to store the data with the actual instance, there are
		// default slots but they are really just pointers anyway and you need to hold a member
		// pointer which is yet another pointer dereference
		int numParams = sq_gettop(vm);

		float x = 0;
		float y = 0;
		float z = 0;

		if ((numParams >= 2 && SQ_FAILED(sq_getfloat(vm, 2, &x))) ||
			(numParams >= 3 && SQ_FAILED(sq_getfloat(vm, 3, &y))) ||
			(numParams >= 4 && SQ_FAILED(sq_getfloat(vm, 4, &z))))
		{
			return sq_throwerror(vm, "Expected Vector(float x, float y, float z)");
		}

		SQUserPointer p;
		if (SQ_FAILED(sq_getinstanceup(vm, 1, &p, 0)))
		{
			return SQ_ERROR;
		}

		if (!p)
		{
			return sq_throwerror(vm, "Accessed null instance");
		}

		new (p) Vector(x, y, z);

		return 0;
	}

	SQInteger _get(HSQUIRRELVM vm)
	{
		const char* key = nullptr;
		sq_getstring(vm, 2, &key);

		if (key == nullptr)
		{
			return sq_throwerror(vm, "Expected Vector._get(string)");
		}

		if (key[0] < 'x' || key[0] > 'z' || key[1] != '\0')
		{
			return sqstd_throwerrorf(vm, "the index '%.50s' does not exist", key);
		}

		Vector* v = nullptr;
		if (SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v, TYPETAG_VECTOR)))
		{
			return sq_throwerror(vm, "Unable to get Vector");
		}

		int idx = key[0] - 'x';
		sq_pushfloat(vm, (*v)[idx]);
		return 1;
	}

	SQInteger _set(HSQUIRRELVM vm)
	{
		const char* key = nullptr;
		sq_getstring(vm, 2, &key);

		if (key == nullptr)
		{
			return sq_throwerror(vm, "Expected Vector._set(string)");
		}

		if (key[0] < 'x' || key[0] > 'z' || key[1] != '\0')
		{
			return sqstd_throwerrorf(vm, "the index '%.50s' does not exist", key);
		}

		Vector* v = nullptr;
		if (SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v, TYPETAG_VECTOR)))
		{
			return sq_throwerror(vm, "Unable to get Vector");
		}

		float val = 0;
		if (SQ_FAILED(sq_getfloat(vm, 3, &val)))
		{
			return sqstd_throwerrorf(vm, "Vector.%s expects float", key);
		}

		int idx = key[0] - 'x';
		(*v)[idx] = val;
		return 0;
	}

	SQInteger _add(HSQUIRRELVM vm)
	{
		Vector* v1 = nullptr;
		Vector* v2 = nullptr;

		if (sq_gettop(vm) != 2 ||
			SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v1, TYPETAG_VECTOR)) ||
			SQ_FAILED(sq_getinstanceup(vm, 2, (SQUserPointer*)&v2, TYPETAG_VECTOR)))
		{
			return sq_throwerror(vm, "Expected (Vector, Vector)");
		}

		sq_getclass(vm, 1);
		sq_createinstance(vm, -1);
		SQUserPointer p;
		sq_getinstanceup(vm, -1, &p, 0);
		new(p) Vector((*v1) + (*v2));
		sq_remove(vm, -2);

		return 1;
	}

	SQInteger _sub(HSQUIRRELVM vm)
	{
		Vector* v1 = nullptr;
		Vector* v2 = nullptr;

		if (sq_gettop(vm) != 2 ||
			SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v1, TYPETAG_VECTOR)) ||
			SQ_FAILED(sq_getinstanceup(vm, 2, (SQUserPointer*)&v2, TYPETAG_VECTOR)))
		{
			return sq_throwerror(vm, "Expected (Vector, Vector)");
		}

		sq_getclass(vm, 1);
		sq_createinstance(vm, -1);
		SQUserPointer p;
		sq_getinstanceup(vm, -1, &p, 0);
		new(p) Vector((*v1) - (*v2));
		sq_remove(vm, -2);

		return 1;
	}

	SQInteger _multiply(HSQUIRRELVM vm)
	{
		Vector* v1 = nullptr;

		if (sq_gettop(vm) != 2 ||
			SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v1, TYPETAG_VECTOR)))
		{
			return sq_throwerror(vm, "Expected (Vector, Vector|float)");
		}

		float s = 0.0;
		Vector* v2 = nullptr;

		if ( SQ_SUCCEEDED(sq_getfloat(vm, 2, &s)) )
		{
			sq_getclass(vm, 1);
			sq_createinstance(vm, -1);
			SQUserPointer p;
			sq_getinstanceup(vm, -1, &p, 0);
			new(p) Vector((*v1) * s);
			sq_remove(vm, -2);

			return 1;
		}
		else if ( SQ_SUCCEEDED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v2, TYPETAG_VECTOR)) )
		{
			sq_getclass(vm, 1);
			sq_createinstance(vm, -1);
			SQUserPointer p;
			sq_getinstanceup(vm, -1, &p, 0);
			new(p) Vector((*v1) * (*v2));
			sq_remove(vm, -2);

			return 1;
		}
		else
		{
			return sq_throwerror(vm, "Expected (Vector, Vector|float)");
		}
	}

	SQInteger _divide(HSQUIRRELVM vm)
	{
		Vector* v1 = nullptr;

		if (sq_gettop(vm) != 2 ||
			SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v1, TYPETAG_VECTOR)))
		{
			return sq_throwerror(vm, "Expected (Vector, Vector|float)");
		}

		float s = 0.0;
		Vector* v2 = nullptr;

		if ( SQ_SUCCEEDED(sq_getfloat(vm, 2, &s)) )
		{
			sq_getclass(vm, 1);
			sq_createinstance(vm, -1);
			SQUserPointer p;
			sq_getinstanceup(vm, -1, &p, 0);
			new(p) Vector((*v1) / s);
			sq_remove(vm, -2);

			return 1;
		}
		else if ( SQ_SUCCEEDED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v2, TYPETAG_VECTOR)) )
		{
			sq_getclass(vm, 1);
			sq_createinstance(vm, -1);
			SQUserPointer p;
			sq_getinstanceup(vm, -1, &p, 0);
			new(p) Vector((*v1) / (*v2));
			sq_remove(vm, -2);

			return 1;
		}
		else
		{
			return sq_throwerror(vm, "Expected (Vector, Vector|float)");
		}
	}

	SQInteger _unm(HSQUIRRELVM vm)
	{
		Vector* v1 = nullptr;

		if (sq_gettop(vm) != 1 ||
			SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v1, TYPETAG_VECTOR)))
		{
			return sq_throwerror(vm, "Expected (Vector)");
		}

		sq_getclass(vm, 1);
		sq_createinstance(vm, -1);
		SQUserPointer p;
		sq_getinstanceup(vm, -1, &p, 0);
		new(p) Vector(-v1->x, -v1->y, -v1->z);
		sq_remove(vm, -2);

		return 1;
	}

	SQInteger weakref(HSQUIRRELVM vm)
	{
		sq_weakref(vm, 1);
		return 1;
	}

	SQInteger getclass(HSQUIRRELVM vm)
	{
		sq_getclass(vm, 1);
		sq_push(vm, -1);
		return 1;
	}

	// multi purpose - copy from input vector, or init with 3 float input
	SQInteger Set(HSQUIRRELVM vm)
	{
		SQInteger top = sq_gettop(vm);
		Vector* v1 = nullptr;

		if ( top < 2 || SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v1, TYPETAG_VECTOR)) )
		{
			return sq_throwerror(vm, "Expected (Vector, Vector)");
		}

		Vector* v2 = nullptr;

		if ( SQ_SUCCEEDED(sq_getinstanceup(vm, 2, (SQUserPointer*)&v2, TYPETAG_VECTOR)) )
		{
			if ( top != 2 )
				return sq_throwerror(vm, "Expected (Vector, Vector)");

			VectorCopy( *v2, *v1 );
			sq_remove( vm, -1 );

			return 1;
		}

		float x, y, z;

		if ( top == 4 &&
			SQ_SUCCEEDED(sq_getfloat(vm, 2, &x)) &&
			SQ_SUCCEEDED(sq_getfloat(vm, 3, &y)) &&
			SQ_SUCCEEDED(sq_getfloat(vm, 4, &z)) )
		{
			v1->Init( x, y, z );
			sq_pop( vm, 3 );

			return 1;
		}

		return sq_throwerror(vm, "Expected (Vector, Vector)");
	}

	SQInteger Add(HSQUIRRELVM vm)
	{
		Vector* v1 = nullptr;
		Vector* v2 = nullptr;

		if (sq_gettop(vm) != 2 ||
			SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v1, TYPETAG_VECTOR)) ||
			SQ_FAILED(sq_getinstanceup(vm, 2, (SQUserPointer*)&v2, TYPETAG_VECTOR)))
		{
			return sq_throwerror(vm, "Expected (Vector, Vector)");
		}

		VectorAdd( *v1, *v2, *v1 );
		sq_remove( vm, -1 );

		return 1;
	}

	SQInteger Subtract(HSQUIRRELVM vm)
	{
		Vector* v1 = nullptr;
		Vector* v2 = nullptr;

		if (sq_gettop(vm) != 2 ||
			SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v1, TYPETAG_VECTOR)) ||
			SQ_FAILED(sq_getinstanceup(vm, 2, (SQUserPointer*)&v2, TYPETAG_VECTOR)))
		{
			return sq_throwerror(vm, "Expected (Vector, Vector)");
		}

		VectorSubtract( *v1, *v2, *v1 );
		sq_remove( vm, -1 );

		return 1;
	}

	SQInteger Multiply(HSQUIRRELVM vm)
	{
		Vector* v1 = nullptr;

		if (sq_gettop(vm) != 2 ||
			SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v1, TYPETAG_VECTOR)))
		{
			return sq_throwerror(vm, "Expected (Vector, Vector|float)");
		}

		Vector* v2 = nullptr;

		if ( SQ_SUCCEEDED(sq_getinstanceup( vm, 2, (SQUserPointer*)&v2, TYPETAG_VECTOR )) )
		{
			VectorMultiply( *v1, *v2, *v1 );
			sq_remove( vm, -1 );

			return 1;
		}

		float flInput;

		if ( SQ_SUCCEEDED(sq_getfloat( vm, 2, &flInput )) )
		{
			VectorMultiply( *v1, flInput, *v1 );
			sq_remove( vm, -1 );

			return 1;
		}

		return sq_throwerror(vm, "Expected (Vector, Vector|float)");
	}

	SQInteger Divide(HSQUIRRELVM vm)
	{
		Vector* v1 = nullptr;

		if (sq_gettop(vm) != 2 ||
			SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v1, TYPETAG_VECTOR)))
		{
			return sq_throwerror(vm, "Expected (Vector, Vector|float)");
		}

		Vector* v2 = nullptr;

		if ( SQ_SUCCEEDED(sq_getinstanceup( vm, 2, (SQUserPointer*)&v2, TYPETAG_VECTOR )) )
		{
			VectorDivide( *v1, *v2, *v1 );
			sq_remove( vm, -1 );

			return 1;
		}

		float flInput;

		if ( SQ_SUCCEEDED(sq_getfloat( vm, 2, &flInput )) )
		{
			VectorDivide( *v1, flInput, *v1 );
			sq_remove( vm, -1 );

			return 1;
		}

		return sq_throwerror(vm, "Expected (Vector, Vector|float)");
	}

	SQInteger DistTo(HSQUIRRELVM vm)
	{
		Vector* v1 = nullptr;
		Vector* v2 = nullptr;

		if (sq_gettop(vm) != 2 ||
			SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v1, TYPETAG_VECTOR)) ||
			SQ_FAILED(sq_getinstanceup(vm, 2, (SQUserPointer*)&v2, TYPETAG_VECTOR)))
		{
			return sq_throwerror(vm, "Expected (Vector, Vector)");
		}

		sq_pushfloat( vm, v1->DistTo(*v2) );

		return 1;
	}

	SQInteger DistToSqr(HSQUIRRELVM vm)
	{
		Vector* v1 = nullptr;
		Vector* v2 = nullptr;

		if (sq_gettop(vm) != 2 ||
			SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v1, TYPETAG_VECTOR)) ||
			SQ_FAILED(sq_getinstanceup(vm, 2, (SQUserPointer*)&v2, TYPETAG_VECTOR)))
		{
			return sq_throwerror(vm, "Expected (Vector, Vector)");
		}

		sq_pushfloat( vm, v1->DistToSqr(*v2) );

		return 1;
	}

	SQInteger IsEqualTo(HSQUIRRELVM vm)
	{
		Vector* v1 = nullptr;
		Vector* v2 = nullptr;

		if (sq_gettop(vm) < 2 || // bother checking > 3?
			SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v1, TYPETAG_VECTOR)) ||
			SQ_FAILED(sq_getinstanceup(vm, 2, (SQUserPointer*)&v2, TYPETAG_VECTOR)) )
		{
			return sq_throwerror(vm, "Expected (Vector, Vector, float)");
		}

		float tolerance = 0.0f;
		sq_getfloat( vm, 3, &tolerance );

		sq_pushbool( vm, VectorsAreEqual( *v1, *v2, tolerance ) );

		return 1;
	}

	SQInteger Length(HSQUIRRELVM vm)
	{
		Vector* v1 = nullptr;

		if (sq_gettop(vm) != 1 ||
			SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v1, TYPETAG_VECTOR)))
		{
			return sq_throwerror(vm, "Expected (Vector)");
		}

		sq_pushfloat(vm, v1->Length());
		return 1;
	}

	SQInteger LengthSqr(HSQUIRRELVM vm)
	{
		Vector* v1 = nullptr;

		if (sq_gettop(vm) != 1 ||
			SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v1, TYPETAG_VECTOR)))
		{
			return sq_throwerror(vm, "Expected (Vector)");
		}

		sq_pushfloat(vm, v1->LengthSqr());
		return 1;
	}

	SQInteger Length2D(HSQUIRRELVM vm)
	{
		Vector* v1 = nullptr;

		if (sq_gettop(vm) != 1 ||
			SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v1, TYPETAG_VECTOR)))
		{
			return sq_throwerror(vm, "Expected (Vector)");
		}

		sq_pushfloat(vm, v1->Length2D());
		return 1;
	}

	SQInteger Length2DSqr(HSQUIRRELVM vm)
	{
		Vector* v1 = nullptr;

		if (sq_gettop(vm) != 1 ||
			SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v1, TYPETAG_VECTOR)))
		{
			return sq_throwerror(vm, "Expected (Vector)");
		}

		sq_pushfloat(vm, v1->Length2DSqr());
		return 1;
	}

	SQInteger Normalized(HSQUIRRELVM vm)
	{
		Vector* v1 = nullptr;

		if (sq_gettop(vm) != 1 ||
			SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v1, TYPETAG_VECTOR)))
		{
			return sq_throwerror(vm, "Expected (Vector)");
		}

		sq_getclass(vm, 1);
		sq_createinstance(vm, -1);
		SQUserPointer p;
		sq_getinstanceup(vm, -1, &p, 0);
		new(p) Vector((*v1).Normalized());
		sq_remove(vm, -2);

		return 1;
	}

	SQInteger Norm(HSQUIRRELVM vm)
	{
		Vector* v1 = nullptr;

		if (sq_gettop(vm) != 1 ||
			SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v1, TYPETAG_VECTOR)))
		{
			return sq_throwerror(vm, "Expected (Vector)");
		}

		float len = v1->NormalizeInPlace();
		sq_pushfloat(vm, len);

		return 1;
	}

	SQInteger Scale(HSQUIRRELVM vm)
	{
		Vector* v1 = nullptr;
		float s = 0.0f;

		if (sq_gettop(vm) != 2 ||
			SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v1, TYPETAG_VECTOR)) ||
			SQ_SUCCEEDED(sq_getfloat(vm, 2, &s)))
		{
			return sq_throwerror(vm, "Expected (Vector, float)");
		}

		sq_getclass(vm, 1);
		sq_createinstance(vm, -1);
		SQUserPointer p;
		sq_getinstanceup(vm, -1, &p, 0);
		new(p) Vector((*v1) * s);
		sq_remove(vm, -2);

		return 1;
	}

	SQInteger Dot(HSQUIRRELVM vm)
	{
		Vector* v1 = nullptr;
		Vector* v2 = nullptr;

		if (sq_gettop(vm) != 2 ||
			SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v1, TYPETAG_VECTOR)) ||
			SQ_FAILED(sq_getinstanceup(vm, 2, (SQUserPointer*)&v2, TYPETAG_VECTOR)))
		{
			return sq_throwerror(vm, "Expected (Vector, Vector)");
		}

		sq_pushfloat(vm, v1->Dot(*v2));
		return 1;
	}

	SQInteger ToKVString(HSQUIRRELVM vm)
	{
		Vector* v1 = nullptr;

		if (sq_gettop(vm) != 1 ||
			SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v1, TYPETAG_VECTOR)))
		{
			return sq_throwerror(vm, "Expected (Vector)");
		}

		sqstd_pushstringf(vm, "%f %f %f", v1->x, v1->y, v1->z);
		return 1;
	}

	SQInteger FromKVString(HSQUIRRELVM vm)
	{
		Vector* v1 = nullptr;
		const char* szInput;

		if (sq_gettop(vm) != 2 ||
			SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v1, TYPETAG_VECTOR)) ||
			SQ_FAILED(sq_getstring(vm, 2, &szInput)) )
		{
			return sq_throwerror(vm, "Expected (Vector, string)");
		}

		float x = 0.0f, y = 0.0f, z = 0.0f;

		if ( sscanf( szInput, "%f %f %f", &x, &y, &z ) < 3 )
		{
			// Return null while invalidating the input vector.
			// This allows the user to easily check for input errors without halting.

			sq_pushnull(vm);
			*v1 = vec3_invalid;

			return 1;
		}

		v1->x = x;
		v1->y = y;
		v1->z = z;

		// return input vector
		sq_remove( vm, -1 );

		return 1;
	}

	SQInteger Cross(HSQUIRRELVM vm)
	{
		Vector* v1 = nullptr;
		Vector* v2 = nullptr;

		if (sq_gettop(vm) != 2 ||
			SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v1, TYPETAG_VECTOR)) ||
			SQ_FAILED(sq_getinstanceup(vm, 2, (SQUserPointer*)&v2, TYPETAG_VECTOR)))
		{
			return sq_throwerror(vm, "Expected (Vector, Vector)");
		}

		sq_getclass(vm, 1);
		sq_createinstance(vm, -1);
		SQUserPointer p;
		sq_getinstanceup(vm, -1, &p, 0);
		new(p) Vector((*v1).Cross(*v2));
		sq_remove(vm, -2);

		return 1;
	}

	SQInteger WithinAABox(HSQUIRRELVM vm)
	{
		Vector* v1 = nullptr;
		Vector* mins = nullptr;
		Vector* maxs = nullptr;

		if (sq_gettop(vm) != 3 ||
			SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v1, TYPETAG_VECTOR)) ||
			SQ_FAILED(sq_getinstanceup(vm, 2, (SQUserPointer*)&mins, TYPETAG_VECTOR)) ||
			SQ_FAILED(sq_getinstanceup(vm, 3, (SQUserPointer*)&maxs, TYPETAG_VECTOR)))
		{
			return sq_throwerror(vm, "Expected (Vector, Vector, Vector)");
		}

		sq_pushbool( vm, v1->WithinAABox( *mins, *maxs ) );

		return 1;
	}

	SQInteger ToString(HSQUIRRELVM vm)
	{
		Vector* v1 = nullptr;

		if (sq_gettop(vm) != 1 ||
			SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v1, TYPETAG_VECTOR)))
		{
			return sq_throwerror(vm, "Expected (Vector)");
		}

		sqstd_pushstringf(vm, "(Vector %p [%f %f %f])", (void*)v1, v1->x, v1->y, v1->z);
		return 1;
	}

	SQInteger TypeOf(HSQUIRRELVM vm)
	{
		sq_pushstring(vm, "Vector", -1);
		return 1;
	}

	SQInteger Nexti(HSQUIRRELVM vm)
	{
		Vector* v1 = nullptr;

		if (sq_gettop(vm) != 2 ||
			SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&v1, TYPETAG_VECTOR)))
		{
			return sq_throwerror(vm, "Expected (Vector)");
		}

		HSQOBJECT obj;
		sq_resetobject(&obj);
		sq_getstackobj(vm, 2, &obj);

		const char* curkey = nullptr;

		if (sq_isnull(obj))
		{
			curkey = "w";
		}
		else if (sq_isstring(obj))
		{
			curkey = sq_objtostring(&obj);
		}
		else
		{
			return sq_throwerror(vm, "Invalid iterator");
		}

		Assert(curkey && curkey[0] >= 'w' && curkey[0] <= 'z');

		if (curkey[0] == 'z')
		{
			// Reached the end
			sq_pushnull(vm);
			return 1;
		}

		char newkey = curkey[0] + 1;
		sq_pushstring(vm, &newkey, 1);

		return 1;
	}

	static const SQRegFunction funcs[] = {
		{_SC("constructor"), Construct,0,nullptr},
		{_SC("_get"), _get, 2, _SC(".s")},
		{_SC("_set"), _set, 3, _SC(".sn")},
		{_SC("_add"), _add, 2, _SC("..")},
		{_SC("_sub"), _sub, 2, _SC("..")},
		{_SC("_mul"), _multiply, 2, _SC("..")},
		{_SC("_div"), _divide, 2, _SC("..")},
		{_SC("_unm"), _unm, 1, _SC(".")},
		{_SC("weakref"), weakref, 1, _SC(".")},
		{_SC("getclass"), getclass, 1, _SC(".")},
		{_SC("Set"), Set, -2, _SC("..nn")},
		{_SC("Add"), Add, 2, _SC("..")},
		{_SC("Subtract"), Subtract, 2, _SC("..")},
		{_SC("Multiply"), Multiply, 2, _SC("..")},
		{_SC("Divide"), Divide, 2, _SC("..")},
		{_SC("DistTo"), DistTo, 2, _SC("..")},
		{_SC("DistToSqr"), DistToSqr, 2, _SC("..")},
		{_SC("IsEqualTo"), IsEqualTo, -2, _SC("..n")},
		{_SC("Length"), Length, 1, _SC(".")},
		{_SC("LengthSqr"), LengthSqr, 1, _SC(".")},
		{_SC("Length2D"), Length2D, 1, _SC(".")},
		{_SC("Length2DSqr"), Length2DSqr, 1, _SC(".")},
		{_SC("Normalized"), Normalized, 1, _SC(".")},
		{_SC("Norm"), Norm, 1, _SC(".")},
		{_SC("Scale"), Scale, 2, _SC(".n")},		// identical to _multiply
		{_SC("Dot"), Dot, 2, _SC("..")},
		{_SC("Cross"), Cross, 2, _SC("..")},
		{_SC("WithinAABox"), WithinAABox, 3, _SC("...")},
		{_SC("ToKVString"), ToKVString, 1, _SC(".")},
		{_SC("FromKVString"), FromKVString, 2, _SC(".s")},
		{_SC("_tostring"), ToString, 1, _SC(".")},
		{_SC("_typeof"), TypeOf, 1, _SC(".")},
		{_SC("_nexti"), Nexti, 2, _SC("..")},

		{nullptr,(SQFUNCTION)0,0,nullptr}
	};

	HSQOBJECT register_class(HSQUIRRELVM v)
	{
		sq_pushstring(v, _SC("Vector"), -1);
		sq_newclass(v, SQFalse);
		sq_settypetag(v, -1, TYPETAG_VECTOR);
		sq_setclassudsize(v, -1, sizeof(Vector));
		SQInteger i = 0;
		while (funcs[i].name != 0) {
			const SQRegFunction& f = funcs[i];
			sq_pushstring(v, f.name, -1);
			sq_newclosure(v, f.f, 0);
			sq_setparamscheck(v, f.nparamscheck, f.typemask);
			sq_setnativeclosurename(v, -1, f.name);
			sq_newslot(v, -3, SQFalse);
			i++;
		}
		HSQOBJECT klass;
		sq_resetobject(&klass);
		sq_getstackobj(v, -1, &klass);
		sq_addref(v, &klass);
		sq_newslot(v, -3, SQFalse);
		return klass;
	}
} // SQVector

struct ClassInstanceData
{
	ClassInstanceData(void* instance, ScriptClassDesc_t* desc, const char* instanceId = nullptr, bool refCounted = false) :
		instance(instance),
		desc(desc),
		instanceId(instanceId),
		refCounted(refCounted)
	{}

	void* instance;
	ScriptClassDesc_t* desc;
	CUtlConstString instanceId;

	// keep for setting instance release hook in save/restore
	bool refCounted;
};

bool CreateParamCheck(const ScriptFunctionBinding_t& func, char* output)
{
	*output++ = '.';
	for (int i = 0; i < func.m_desc.m_Parameters.Count(); ++i)
	{
		switch (func.m_desc.m_Parameters[i])
		{
		case FIELD_FLOAT:
		case FIELD_INTEGER:
			*output++ = 'n';
			break;
		case FIELD_CSTRING:
			*output++ = 's';
			break;
		case FIELD_VECTOR:
			*output++ = 'x'; // Generic instance, we validate on arrival
			break;
		case FIELD_BOOLEAN:
			*output++ = 'b';
			break;
		case FIELD_CHARACTER:
			*output++ = 's';
			break;
		case FIELD_HSCRIPT:
			*output++ = '.';
			break;
		default:
			Assert(!"Unsupported type");
			return false;
		};
	}
	*output++ = 0;
	return true;
}

void PushVariant(HSQUIRRELVM vm, const ScriptVariant_t& value)
{
	switch (value.m_type)
	{
	case FIELD_VOID:
		sq_pushnull(vm);
		break;
	case FIELD_FLOAT:
		sq_pushfloat(vm, value);
		break;
	case FIELD_CSTRING:
		if ( value.m_pszString )
			sq_pushstring(vm, value, -1);
		else
			sq_pushnull(vm);
		break;
	case FIELD_VECTOR:
	{
		SquirrelVM* pSquirrelVM = (SquirrelVM*)sq_getsharedforeignptr(vm);
		Assert(pSquirrelVM);
		sq_pushobject(vm, pSquirrelVM->vectorClass_);
		sq_createinstance(vm, -1);
		SQUserPointer p;
		sq_getinstanceup(vm, -1, &p, 0);
		new(p) Vector(static_cast<const Vector&>(value));
		sq_remove(vm, -2);
		break;
	}
	case FIELD_INTEGER:
		sq_pushinteger(vm, value.m_int);
		break;
	case FIELD_BOOLEAN:
		sq_pushbool(vm, value.m_bool);
		break;
	case FIELD_CHARACTER:
	{
		char buf[2] = { value.m_char, 0 };
		sq_pushstring(vm, buf, 1);
		break;
	}
	case FIELD_HSCRIPT:
		if (value.m_hScript)
		{
			sq_pushobject(vm, *((HSQOBJECT*)value.m_hScript));
		}
		else
		{
			sq_pushnull(vm);
		}
		break;
	}
}

void GetVariantScriptString(const ScriptVariant_t& value, char *szValue, int iSize)
{
	switch (value.m_type)
	{
		case FIELD_VOID:
			V_strncpy( szValue, "null", iSize );
			break;
		case FIELD_FLOAT:
			V_snprintf( szValue, iSize, "%f", value.m_float );
			break;
		case FIELD_CSTRING:
			V_snprintf( szValue, iSize, "\"%s\"", value.m_pszString );
			break;
		case FIELD_VECTOR:
			V_snprintf( szValue, iSize, "Vector( %f, %f, %f )", value.m_pVector->x, value.m_pVector->y, value.m_pVector->z );
			break;
		case FIELD_INTEGER:
			V_snprintf( szValue, iSize, "%i", value.m_int );
			break;
		case FIELD_BOOLEAN:
			V_snprintf( szValue, iSize, "%d", value.m_bool );
			break;
		case FIELD_CHARACTER:
			//char buf[2] = { value.m_char, 0 };
			V_snprintf( szValue, iSize, "\"%c\"", value.m_char );
			break;
	}
}

bool getVariant(HSQUIRRELVM vm, SQInteger idx, ScriptVariant_t& variant)
{
	switch (sq_gettype(vm, idx))
	{
	case OT_NULL:
	{
		variant.Free();
		variant.m_flags = 0;
		// TODO: Should this be (HSCRIPT)nullptr
		variant.m_type = FIELD_VOID;
		return true;
	}
	case OT_INTEGER:
	{
		SQInteger val;
		if (SQ_FAILED(sq_getinteger(vm, idx, &val)))
		{
			return false;
		}
		variant.Free();
		variant = (int)val;
		return true;
	}
	case OT_FLOAT:
	{
		SQFloat val;
		if (SQ_FAILED(sq_getfloat(vm, idx, &val)))
		{
			return false;
		}
		variant.Free();
		variant = (float)val;
		return true;
	}
	case OT_BOOL:
	{
		SQBool val;
		if (SQ_FAILED(sq_getbool(vm, idx, &val)))
		{
			return false;
		}
		variant.Free();
		variant = val ? true : false;
		return true;
	}
	case OT_STRING:
	{
		const char* val;
		SQInteger size = 0;
		if (SQ_FAILED(sq_getstringandsize(vm, idx, &val, &size)))
		{
			return false;
		}
		variant.Free();
		char* buffer = (char*)malloc(size + 1);
		V_memcpy(buffer, val, size);
		buffer[size] = 0;
		variant = buffer;
		variant.m_flags |= SV_FREE;
		return true;
	}
	case OT_INSTANCE:
	{
		Vector* v = nullptr;
		if (SQ_SUCCEEDED(sq_getinstanceup(vm, idx, (SQUserPointer*)&v, TYPETAG_VECTOR)))
		{
			variant.Free();
			variant = (Vector*)malloc(sizeof(Vector));
			variant.EmplaceAllocedVector(*v);
			variant.m_flags |= SV_FREE;
			return true;
		}
		// fall-through for non-vector
	}
	default:
	{
		variant.Free();
		HSQOBJECT* obj = new HSQOBJECT;
		sq_resetobject(obj);
		sq_getstackobj(vm, idx, obj);
		sq_addref(vm, obj);
		variant = (HSCRIPT)obj;
	}
	};


	return true;
}

SQInteger function_stub(HSQUIRRELVM vm)
{
	SQInteger top = sq_gettop(vm);

	ScriptFunctionBinding_t* pFunc = nullptr;
	sq_getuserpointer(vm, top, (SQUserPointer*)&pFunc);

	Assert(pFunc);

	int nargs = pFunc->m_desc.m_Parameters.Count();
	int nLastHScriptIdx = -1;

	if (nargs > top)
	{
		// TODO: Handle optional parameters?
		return sq_throwerror(vm, "Invalid number of parameters");
	}

	CUtlVector<ScriptVariant_t> params;
	params.SetCount(nargs);

	for (int i = 0; i < nargs; ++i)
	{
		switch (pFunc->m_desc.m_Parameters[i])
		{
		case FIELD_FLOAT:
		{
			float val = 0.0;
			if (SQ_FAILED(sq_getfloat(vm, i + 2, &val)))
				return sq_throwerror(vm, "Expected float");
			params[i] = val;
			break;
		}
		case FIELD_CSTRING:
		{
			const char* val;
			if (SQ_FAILED(sq_getstring(vm, i + 2, &val)))
				return sq_throwerror(vm, "Expected string");
			params[i] = val;
			break;
		}
		case FIELD_VECTOR:
		{
			Vector* val;
			if (SQ_FAILED(sq_getinstanceup(vm, i + 2, (SQUserPointer*)&val, TYPETAG_VECTOR)))
				return sq_throwerror(vm, "Expected Vector");
			params[i] = *val;
			break;
		}
		case FIELD_INTEGER:
		{
			SQInteger val = 0;
			if (SQ_FAILED(sq_getinteger(vm, i + 2, &val)))
				return sq_throwerror(vm, "Expected integer");
			params[i] = (int)val;
			break;
		}
		case FIELD_BOOLEAN:
		{
			SQBool val = 0;
			if (SQ_FAILED(sq_getbool(vm, i + 2, &val)))
				return sq_throwerror(vm, "Expected bool");
			params[i] = val ? true : false;
			break;
		}
		case FIELD_CHARACTER:
		{
			const char* val;
			if (SQ_FAILED(sq_getstring(vm, i + 2, &val)))
				return sq_throwerror(vm, "Expected string");
			params[i] = val[i];
			break;
		}
		case FIELD_HSCRIPT:
		{
			HSQOBJECT val;
			if (SQ_FAILED(sq_getstackobj(vm, i + 2, &val)))
				return sq_throwerror(vm, "Expected handle");

			if (sq_isnull(val))
			{
				params[i] = (HSCRIPT)nullptr;
			}
			else
			{
				HSQOBJECT* pObject = new HSQOBJECT;
				*pObject = val;
				params[i] = (HSCRIPT)pObject;
			}

			nLastHScriptIdx = i;
			break;
		}
		default:
			Assert(!"Unsupported type");
			return false;
		}
	}

	void* instance = nullptr;

	if (pFunc->m_flags & SF_MEMBER_FUNC)
	{
		ClassInstanceData* classInstanceData;
		if (SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&classInstanceData, 0)))
		{
			return SQ_ERROR;
		}

		if (!classInstanceData)
		{
			return sq_throwerror(vm, "Accessed null instance");
		}

		// check that the type of self, or any basetype, matches the function description
		ScriptClassDesc_t *selfType = classInstanceData->desc;
		while (selfType != pFunc->m_desc.m_pScriptClassDesc)
		{
			if (!selfType)
			{
				return sq_throwerror(vm, "Mismatched instance type");
			}
			selfType = selfType->m_pBaseDesc;
			Assert(selfType != classInstanceData->desc); // there should be no infinite loop
		}

		instance = classInstanceData->instance;
	}

	ScriptVariant_t script_retval;
	ScriptVariantTemporaryStorage_t script_retval_storage;

	SquirrelVM* pSquirrelVM = (SquirrelVM*)sq_getsharedforeignptr(vm);
	Assert(pSquirrelVM);

	bool call_success = (*pFunc->m_pfnBinding)(pFunc->m_pFunction, instance, params.Base(), nargs,
		pFunc->m_desc.m_ReturnType == FIELD_VOID ? nullptr : &script_retval, script_retval_storage);
	Assert(call_success);
	(void)call_success;

	SQInteger sq_retval;
	if (!sq_isnull(pSquirrelVM->lastError_))
	{
		sq_pushobject(vm, pSquirrelVM->lastError_);
		sq_release(vm, &pSquirrelVM->lastError_);
		sq_resetobject(&pSquirrelVM->lastError_);
		sq_retval = sq_throwobject(vm);
	}
	else
	{
		Assert(script_retval.m_type == pFunc->m_desc.m_ReturnType);
		Assert( ( pFunc->m_desc.m_ReturnType != FIELD_VOID ) || !( pFunc->m_flags & SF_REFCOUNTED_RET ) );

		if (pFunc->m_desc.m_ReturnType != FIELD_VOID)
		{
			PushVariant(vm, script_retval);

			if ( ( pFunc->m_flags & SF_REFCOUNTED_RET ) && script_retval.m_hScript )
			{
				Assert( script_retval.m_type == FIELD_HSCRIPT );

				// Release the intermediary ref held from RegisterInstance
				sq_release(vm, (HSQOBJECT*)script_retval.m_hScript);
				delete (HSQOBJECT*)script_retval.m_hScript;
			}

			sq_retval = 1;
		}
		else
		{
			sq_retval = 0;
		}
	}

	// strings never get copied here, Vector and QAngle are stored in script_retval_storage
	// everything else is stored inline, so there should be no memory to free
	Assert(!(script_retval.m_flags & SV_FREE));

	for ( int i = 0; i <= nLastHScriptIdx; ++i )
	{
		if ( pFunc->m_desc.m_Parameters[i] == FIELD_HSCRIPT )
			delete (HSQOBJECT*)params[i].m_hScript;
	}

	return sq_retval;
}


SQInteger destructor_stub(SQUserPointer p, SQInteger size)
{
	auto classInstanceData = (ClassInstanceData*)p;

	// if instance is not deleted, then it's leaking
	// this should never happen
	Assert( classInstanceData->desc->m_pfnDestruct );

	if (classInstanceData->desc->m_pfnDestruct)
		classInstanceData->desc->m_pfnDestruct(classInstanceData->instance);

	classInstanceData->~ClassInstanceData();
	return 0;
}

SQInteger destructor_stub_instance(SQUserPointer p, SQInteger size)
{
	auto classInstanceData = (ClassInstanceData*)p;
	// This instance is owned by the game, don't delete it
	classInstanceData->~ClassInstanceData();
	return 0;
}

SQInteger constructor_stub(HSQUIRRELVM vm)
{
	ScriptClassDesc_t* pClassDesc = nullptr;
	if (SQ_FAILED(sq_gettypetag(vm, 1, (SQUserPointer*)&pClassDesc)))
	{
		return sq_throwerror(vm, "Expected native class");
	}

	if (!pClassDesc || (void*)pClassDesc == TYPETAG_VECTOR)
	{
		return sq_throwerror(vm, "Unable to obtain native class description");
	}

	if (!pClassDesc->m_pfnConstruct)
	{
		return sqstd_throwerrorf(vm, "Unable to construct instances of %s", pClassDesc->m_pszScriptName);
	}

	SQUserPointer p;
	if (SQ_FAILED(sq_getinstanceup(vm, 1, &p, 0)))
	{
		return SQ_ERROR;
	}

	if (!p)
	{
		return sq_throwerror(vm, "Accessed null instance");
	}

	void* instance = pClassDesc->m_pfnConstruct();

#ifdef DBGFLAG_ASSERT
	SquirrelVM* pSquirrelVM = (SquirrelVM*)sq_getsharedforeignptr(vm);
	Assert(pSquirrelVM);
	// expect construction to always succeed
	Assert(sq_isnull(pSquirrelVM->lastError_));
#endif

	new(p) ClassInstanceData(instance, pClassDesc, nullptr, true);

	sq_setreleasehook(vm, 1, &destructor_stub);

	return 0;
}

SQInteger tostring_stub(HSQUIRRELVM vm)
{
	ClassInstanceData* classInstanceData = nullptr;
	if (SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&classInstanceData, 0)))
	{
		return SQ_ERROR;
	}

	char buffer[128] = "";

	if (classInstanceData &&
		classInstanceData->instance &&
		classInstanceData->desc->pHelper &&
		classInstanceData->desc->pHelper->ToString(classInstanceData->instance, buffer, sizeof(buffer)))
	{
		sq_pushstring(vm, buffer, -1);
	}
	else if (classInstanceData)
	{
		sqstd_pushstringf(vm, "(%s : 0x%p)", classInstanceData->desc->m_pszScriptName, classInstanceData->instance);
	}
	else
	{
		HSQOBJECT obj;
		sq_resetobject(&obj);
		sq_getstackobj(vm, 1, &obj);
		// Semi-based on SQVM::ToString default case
		sqstd_pushstringf(vm, "(%s: 0x%p)", IdType2Name(obj._type), (void*)_rawval(obj));
	}

	return 1;
}

SQInteger get_stub(HSQUIRRELVM vm)
{
	ClassInstanceData* classInstanceData = nullptr;
	if (SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&classInstanceData, 0)))
	{
		return SQ_ERROR;
	}

	const char* key = nullptr;
	sq_getstring(vm, 2, &key);

	if (key == nullptr)
	{
		return sq_throwerror(vm, "Expected _get(string)");
	}

	ScriptVariant_t var;
	SQInteger sq_retval = 0;
	if (classInstanceData &&
		classInstanceData->instance &&
		classInstanceData->desc->pHelper &&
		classInstanceData->desc->pHelper->Get(classInstanceData->instance, key, var))
	{
		PushVariant(vm, var);
		sq_retval = 1;
	}
	else
	{
		// Fallback
		// Extra stack variables don't need to be popped, they are cleaned up on exit
		sq_pushroottable(vm);
		sq_push(vm, -2);

		if ( SQ_SUCCEEDED( sq_rawget(vm, -2) ) )
		{
			sq_retval = 1;
		}
		else
		{
			sq_retval = sqstd_throwerrorf(vm, "the index '%.50s' does not exist", key);
		}
	}

	var.Free();
	return sq_retval;
}

SQInteger set_stub(HSQUIRRELVM vm)
{
	ClassInstanceData* classInstanceData = nullptr;
	if (SQ_FAILED(sq_getinstanceup(vm, 1, (SQUserPointer*)&classInstanceData, 0)))
	{
		return SQ_ERROR;
	}

	const char* key = nullptr;
	sq_getstring(vm, 2, &key);

	if (key == nullptr)
	{
		return sq_throwerror(vm, "Expected _set(string)");
	}

	ScriptVariant_t var;
	SQInteger sq_retval = 0;
	getVariant( vm, -1, var );

	if (!(
		classInstanceData &&
		classInstanceData->instance &&
		classInstanceData->desc->pHelper &&
		classInstanceData->desc->pHelper->Set(classInstanceData->instance, key, var)
	))
	{
		// Fallback
		sq_pushroottable(vm);
		sq_push(vm, -3);
		sq_push(vm, -3);

		if ( SQ_SUCCEEDED( sq_rawset(vm, -3) ) )
		{
			// rawset doesn't return correctly, pop env to return val
			sq_pop(vm, 1);
			sq_retval = 1;
		}
		else
		{
			sq_retval = sqstd_throwerrorf(vm, "the index '%.50s' does not exist", key);
		}
	}

	var.Free();
	return sq_retval;
}

SQInteger IsValid_stub(HSQUIRRELVM vm)
{
	ClassInstanceData* classInstanceData = nullptr;
	sq_getinstanceup(vm, 1, (SQUserPointer*)&classInstanceData, 0);
	sq_pushbool(vm, classInstanceData != nullptr);
	return 1;
}

SQInteger weakref_stub(HSQUIRRELVM vm)
{
	sq_weakref(vm, 1);
	return 1;
}

SQInteger getclass_stub(HSQUIRRELVM vm)
{
	sq_getclass(vm, 1);
	sq_push(vm, -1);
	return 1;
}

struct SquirrelSafeCheck
{
	SquirrelSafeCheck(HSQUIRRELVM vm, int outputCount = 0) :
		vm_{ vm },
		top_{ sq_gettop(vm) },
		outputCount_{ outputCount }
	{}

	~SquirrelSafeCheck()
	{
		SQInteger curtop = sq_gettop(vm_);
		SQInteger diff = curtop - outputCount_;
		if ( top_ != diff )
		{
			Assert(!"Squirrel VM stack is not consistent");
			Error("Squirrel VM stack is not consistent\n");
		}

		// TODO: Handle error state checks
	}

	HSQUIRRELVM vm_;
	SQInteger top_;
	SQInteger outputCount_;
};


void printfunc(HSQUIRRELVM SQ_UNUSED_ARG(v), const SQChar* format, ...)
{
	va_list args;
	char buffer[2048];
	va_start(args, format);
	V_vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	CGMsg(0, CON_GROUP_VSCRIPT_PRINT, "%s", buffer);
}

void errorfunc(HSQUIRRELVM SQ_UNUSED_ARG(v), const SQChar* format, ...)
{
	va_list args;
	char buffer[2048];
	va_start(args, format);
	V_vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	Warning("%s", buffer);
}

const char * ScriptDataTypeToName(ScriptDataType_t datatype)
{
	switch (datatype)
	{
	case FIELD_VOID:		return "void";
	case FIELD_FLOAT:		return "float";
	case FIELD_CSTRING:		return "string";
	case FIELD_VECTOR:		return "Vector";
	case FIELD_INTEGER:		return "int";
	case FIELD_BOOLEAN:		return "bool";
	case FIELD_CHARACTER:	return "char";
	case FIELD_HSCRIPT:		return "handle";
	case FIELD_VARIANT:		return "variant";
	default:				return "<unknown>";
	}
}


#define PushDocumentationRegisterFunction( szName )	\
	sq_pushroottable(vm);							\
	sq_pushstring(vm, "__Documentation", -1);		\
	sq_get(vm, -2);									\
	sq_pushstring(vm, szName, -1);					\
	sq_get(vm, -2);									\
	sq_push(vm, -2);

#define CallDocumentationRegisterFunction( paramcount )	\
	sq_call(vm, paramcount+1, SQFalse, SQFalse);		\
	sq_pop(vm, 3);

void RegisterDocumentation(HSQUIRRELVM vm, const ScriptFuncDescriptor_t& pFuncDesc, ScriptClassDesc_t* pClassDesc = nullptr)
{
	if ( !developer.GetInt() )
		return;

	SquirrelSafeCheck safeCheck(vm);

	if (pFuncDesc.m_pszDescription && pFuncDesc.m_pszDescription[0] == SCRIPT_HIDE[0])
		return;

	char name[256] = "";

	if (pClassDesc)
	{
		V_strcat_safe(name, pClassDesc->m_pszScriptName);
		V_strcat_safe(name, "::");
	}

	V_strcat_safe(name, pFuncDesc.m_pszScriptName);


	char signature[256] = "";
	V_snprintf(signature, sizeof(signature), "%s %s(", ScriptDataTypeToName(pFuncDesc.m_ReturnType), name);

	for (int i = 0; i < pFuncDesc.m_Parameters.Count(); ++i)
	{
		if (i != 0)
			V_strcat_safe(signature, ", ");

		V_strcat_safe(signature, ScriptDataTypeToName(pFuncDesc.m_Parameters[i]));
	}

	V_strcat_safe(signature, ")");

	// RegisterHelp(name, signature, description)
	PushDocumentationRegisterFunction( "RegisterHelp" );
		sq_pushstring(vm, name, -1);
		sq_pushstring(vm, signature, -1);
		sq_pushstring(vm, pFuncDesc.m_pszDescription ? pFuncDesc.m_pszDescription : "", -1);
	CallDocumentationRegisterFunction( 3 );
}

void RegisterClassDocumentation(HSQUIRRELVM vm, const ScriptClassDesc_t* pClassDesc)
{
	if ( !developer.GetInt() )
		return;

	SquirrelSafeCheck safeCheck(vm);

	const char *name = pClassDesc->m_pszScriptName;
	const char *base = "";
	if (pClassDesc->m_pBaseDesc)
	{
		base = pClassDesc->m_pBaseDesc->m_pszScriptName;
	}

	const char *description = pClassDesc->m_pszDescription;
	if (description)
	{
		if (description[0] == SCRIPT_HIDE[0])
			return;
		if (description[0] == SCRIPT_SINGLETON[0])
			description++;
	}
	else
	{
		description = "";
	}

	// RegisterClassHelp(name, base, description)
	PushDocumentationRegisterFunction( "RegisterClassHelp" );
		sq_pushstring(vm, name, -1);
		sq_pushstring(vm, base, -1);
		sq_pushstring(vm, description, -1);
	CallDocumentationRegisterFunction( 3 );
}

void RegisterEnumDocumentation(HSQUIRRELVM vm, const ScriptEnumDesc_t* pClassDesc)
{
	if ( !developer.GetInt() )
		return;

	SquirrelSafeCheck safeCheck(vm);

	if (pClassDesc->m_pszDescription && pClassDesc->m_pszDescription[0] == SCRIPT_HIDE[0])
		return;

	const char *name = pClassDesc->m_pszScriptName;

	// RegisterEnumHelp(name, description)
	PushDocumentationRegisterFunction( "RegisterEnumHelp" );
		sq_pushstring(vm, name, -1);
		sq_pushinteger(vm, pClassDesc->m_ConstantBindings.Count());
		sq_pushstring(vm, pClassDesc->m_pszDescription ? pClassDesc->m_pszDescription : "", -1);
	CallDocumentationRegisterFunction( 3 );
}

void RegisterConstantDocumentation( HSQUIRRELVM vm, const ScriptConstantBinding_t* pConstDesc, const char *pszAsString, ScriptEnumDesc_t* pEnumDesc = nullptr )
{
	if ( !developer.GetInt() )
		return;

	SquirrelSafeCheck safeCheck(vm);

	if (pConstDesc->m_pszDescription && pConstDesc->m_pszDescription[0] == SCRIPT_HIDE[0])
		return;

	char name[256] = "";

	if (pEnumDesc)
	{
		V_strcat_safe(name, pEnumDesc->m_pszScriptName);
		V_strcat_safe(name, ".");
	}

	V_strcat_safe(name, pConstDesc->m_pszScriptName);

	char signature[256] = "";
	V_snprintf(signature, sizeof(signature), "%s (%s)", pszAsString, ScriptDataTypeToName(pConstDesc->m_data.m_type));

	// RegisterConstHelp(name, signature, description)
	PushDocumentationRegisterFunction( "RegisterConstHelp" );
		sq_pushstring(vm, name, -1);
		sq_pushstring(vm, signature, -1);
		sq_pushstring(vm, pConstDesc->m_pszDescription ? pConstDesc->m_pszDescription : "", -1);
	CallDocumentationRegisterFunction( 3 );
}

void RegisterHookDocumentation(HSQUIRRELVM vm, const ScriptHook_t* pHook, const ScriptFuncDescriptor_t& pFuncDesc, ScriptClassDesc_t* pClassDesc = nullptr)
{
	if ( !developer.GetInt() )
		return;

	SquirrelSafeCheck safeCheck(vm);

	if (pFuncDesc.m_pszDescription && pFuncDesc.m_pszDescription[0] == SCRIPT_HIDE[0])
		return;

	char name[256] = "";

	if (pClassDesc)
	{
		V_strcat_safe(name, pClassDesc->m_pszScriptName);
		V_strcat_safe(name, " -> ");
	}

	V_strcat_safe(name, pFuncDesc.m_pszScriptName);


	char signature[256] = "";
	V_snprintf(signature, sizeof(signature), "%s %s(", ScriptDataTypeToName(pFuncDesc.m_ReturnType), name);

	for (int i = 0; i < pFuncDesc.m_Parameters.Count(); ++i)
	{
		if (i != 0)
			V_strcat_safe(signature, ", ");

		V_strcat_safe(signature, ScriptDataTypeToName(pFuncDesc.m_Parameters[i]));
		V_strcat_safe(signature, " [");
		V_strcat_safe(signature, pHook->m_pszParameterNames[i]);
		V_strcat_safe(signature, "]");
	}

	V_strcat_safe(signature, ")");

	// RegisterHookHelp(name, signature, description)
	PushDocumentationRegisterFunction( "RegisterHookHelp" );
		sq_pushstring(vm, name, -1);
		sq_pushstring(vm, signature, -1);
		sq_pushstring(vm, pFuncDesc.m_pszDescription ? pFuncDesc.m_pszDescription : "", -1);
	CallDocumentationRegisterFunction( 3 );
}

void RegisterMemberDocumentation(HSQUIRRELVM vm, const ScriptMemberDesc_t& pDesc, ScriptClassDesc_t* pClassDesc = nullptr)
{
	if ( !developer.GetInt() )
		return;

	SquirrelSafeCheck safeCheck(vm);

	if (pDesc.m_pszDescription && pDesc.m_pszDescription[0] == SCRIPT_HIDE[0])
		return;

	char name[256] = "";

	if (pClassDesc)
	{
		V_strcat_safe(name, pClassDesc->m_pszScriptName);
		V_strcat_safe(name, ".");
	}

	if (pDesc.m_pszScriptName)
		V_strcat_safe(name, pDesc.m_pszScriptName);

	char signature[256] = "";
	V_snprintf(signature, sizeof(signature), "%s %s", ScriptDataTypeToName(pDesc.m_ReturnType), name);

	// RegisterMemberHelp(name, signature, description)
	PushDocumentationRegisterFunction( "RegisterMemberHelp" );
		sq_pushstring(vm, name, -1);
		sq_pushstring(vm, signature, -1);
		sq_pushstring(vm, pDesc.m_pszDescription ? pDesc.m_pszDescription : "", -1);
	CallDocumentationRegisterFunction( 3 );
}

SQInteger GetDeveloperLevel(HSQUIRRELVM vm)
{
	sq_pushinteger( vm, developer.GetInt() );
	return 1;
}


bool SquirrelVM::Init()
{
	vm_ = sq_open(1024); //creates a VM with initial stack size 1024

	if (vm_ == nullptr)
		return false;

	sq_setsharedforeignptr(vm_, this);
	sq_resetobject(&lastError_);

	sq_setprintfunc(vm_, printfunc, errorfunc);


	{
		sq_pushroottable(vm_);

		sqstd_register_mathlib(vm_);
		sqstd_register_stringlib(vm_);
		vectorClass_ = SQVector::register_class(vm_);

		// We exclude these libraries as they create a security risk on the client even
		// though I'm sure if someone tried hard enough they could achieve all sorts of
		// things with the other APIs, this just makes it a little bit harder for a map
		// created by someone in the community causing a bunch of security vulnerablilties.
		//
		// Pretty sure DoIncludeScript() is already a vulnerability vector here, however
		// that also depends on compile errors not showing up and relies on IFilesystem with
		// a path prefix.
		//
		//sqstd_register_bloblib(vm_);
		//sqstd_register_iolib(vm_);
		//sqstd_register_systemlib(vm_);

		// There is no vulnerability in getting time.
		sqstd_register_timelib(vm_);


		sqstd_seterrorhandlers(vm_);

		{
			// Unfortunately we can not get the pattern from a regexp instance
			// so we need to wrap it with our own to get it.
			if (Run(R"script(
				class regexp extends regexp  
				{
					constructor(pattern) 
					{
						base.constructor(pattern);
						this.pattern_ = pattern;
					}
					pattern_ = "";
				}
			)script") == SCRIPT_ERROR)
			{
				this->Shutdown();
				return false;
			}

			sq_resetobject(&regexpClass_);
			sq_pushstring(vm_, "regexp", -1);
			sq_rawget(vm_, -2);
			sq_getstackobj(vm_, -1, &regexpClass_);
			sq_addref(vm_, &regexpClass_);
			sq_pop(vm_, 1);
		}

		sq_pushstring( vm_, "developer", -1 );
		sq_newclosure( vm_, &GetDeveloperLevel, 0 );
		//sq_setnativeclosurename( vm_, -1, "developer" );
		sq_newslot( vm_, -3, SQFalse );

		sq_pop(vm_, 1);
	}

	if (Run(g_Script_vscript_squirrel) != SCRIPT_DONE)
	{
		this->Shutdown();
		return false;
	}

	return true;
}

void SquirrelVM::Shutdown()
{
	if (vm_)
	{
		sq_release(vm_, &vectorClass_);
		sq_release(vm_, &regexpClass_);

		sq_close(vm_);
		vm_ = nullptr;
	}
}

bool VScriptRunScript( const char *pszScriptName, HSCRIPT hScope, bool bWarnMissing );

bool SquirrelVM::ConnectDebugger( int port, float timeout )
{
	if ( !debugger_ )
	{
		debugger_ = sqdbg_attach_debugger( vm_ );

		if ( sqdbg_listen_socket( debugger_, port ) == 0 && timeout )
		{
			float startTime = Plat_FloatTime();

			while ( !sqdbg_is_client_connected( debugger_ ) )
			{
				float time = Plat_FloatTime();
				if ( time - startTime > timeout )
					break;

				ThreadSleep( 50 );

				sqdbg_frame( debugger_ );
			}
		}
	}
	else
	{
		sqdbg_frame( debugger_ );
	}

	VScriptRunScript( "sqdbg_definitions.nut", NULL, false );
	return true;
}

void SquirrelVM::DisconnectDebugger()
{
	if ( debugger_ )
	{
		sqdbg_destroy_debugger( vm_ );
		debugger_ = nullptr;
	}
}

ScriptLanguage_t SquirrelVM::GetLanguage()
{
	return SL_SQUIRREL;
}

const char* SquirrelVM::GetLanguageName()
{
	return "squirrel";
}

void SquirrelVM::AddSearchPath(const char* pszSearchPath)
{
	// TODO: Search path support
}

bool SquirrelVM::Frame(float simTime)
{
	if ( debugger_ )
	{
		sqdbg_frame( debugger_ );
	}
	return false;
}

ScriptStatus_t SquirrelVM::Run(const char* pszScript, bool bWait)
{
	SquirrelSafeCheck safeCheck(vm_);
	if (SQ_FAILED(sq_compilebuffer(vm_, pszScript, strlen(pszScript), "<run>", SQTrue)))
	{
		return SCRIPT_ERROR;
	}

	sq_pushroottable(vm_);
	if (SQ_FAILED(sq_call(vm_, 1, SQFalse, SQTrue)))
	{
		sq_pop(vm_, 1);
		return SCRIPT_ERROR;
	}

	sq_pop(vm_, 1);
	return SCRIPT_DONE;
}

HSCRIPT SquirrelVM::CompileScript(const char* pszScript, const char* pszId)
{
	SquirrelSafeCheck safeCheck(vm_);

	bool bUnnamed = ( pszId == nullptr );
	if ( bUnnamed )
	{
		pszId = "<unnamed>";
	}

	int nScriptLen = strlen(pszScript);

	if (SQ_FAILED(sq_compilebuffer(vm_, pszScript, nScriptLen, pszId, SQTrue)))
	{
		return nullptr;
	}

	if ( debugger_ && !bUnnamed )
	{
		sqdbg_on_script_compile( debugger_, pszScript, nScriptLen, pszId, strlen(pszId) );
	}

	HSQOBJECT* obj = new HSQOBJECT;
	sq_resetobject(obj);
	sq_getstackobj(vm_, -1, obj);
	sq_addref(vm_, obj);
	sq_pop(vm_, 1);

	return (HSCRIPT)obj;
}

void SquirrelVM::ReleaseScript(HSCRIPT hScript)
{
	SquirrelSafeCheck safeCheck(vm_);
	if (!hScript) return;
	HSQOBJECT* obj = (HSQOBJECT*)hScript;
	sq_release(vm_, obj);
	delete obj;
}

ScriptStatus_t SquirrelVM::Run(HSCRIPT hScript, HSCRIPT hScope, bool bWait)
{
	SquirrelSafeCheck safeCheck(vm_);
	HSQOBJECT* obj = (HSQOBJECT*)hScript;
	sq_pushobject(vm_, *obj);

	if (hScope)
	{
		Assert(hScope != INVALID_HSCRIPT);
		HSQOBJECT* scope = (HSQOBJECT*)hScope;
		sq_pushobject(vm_, *scope);
	}
	else
	{
		sq_pushroottable(vm_);
	}

	auto result = sq_call(vm_, 1, false, true);
	sq_pop(vm_, 1);
	if (SQ_FAILED(result))
	{
		return SCRIPT_ERROR;
	}
	return SCRIPT_DONE;
}

ScriptStatus_t SquirrelVM::Run(HSCRIPT hScript, bool bWait)
{
	SquirrelSafeCheck safeCheck(vm_);
	HSQOBJECT* obj = (HSQOBJECT*)hScript;
	sq_pushobject(vm_, *obj);
	sq_pushroottable(vm_);
	auto result = sq_call(vm_, 1, false, true);
	sq_pop(vm_, 1);
	if (SQ_FAILED(result))
	{
		return SCRIPT_ERROR;
	}
	return SCRIPT_DONE;
}

HSCRIPT SquirrelVM::CreateScope(const char* pszScope, HSCRIPT hParent)
{
	SquirrelSafeCheck safeCheck(vm_);

	sq_newtable(vm_);

	if (hParent)
	{
		HSQOBJECT* parent = (HSQOBJECT*)hParent;
		Assert(hParent != INVALID_HSCRIPT);
		sq_pushobject(vm_, *parent);
	}
	else
	{
		sq_pushroottable(vm_);
	}

	sq_pushstring(vm_, pszScope, -1);
	sq_push(vm_, -3);
	sq_rawset(vm_, -3);

	sq_pushstring(vm_, "__vname", -1);
	sq_pushstring(vm_, pszScope, -1);
	sq_rawset(vm_, -4);

	if (SQ_FAILED(sq_setdelegate(vm_, -2)))
	{
		sq_pop(vm_, 2);
		return nullptr;
	}

	HSQOBJECT* obj = new HSQOBJECT;
	sq_resetobject(obj);
	sq_getstackobj(vm_, -1, obj);
	sq_addref(vm_, obj);
	sq_pop(vm_, 1);

	return (HSCRIPT)obj;
}

void SquirrelVM::ReleaseScope(HSCRIPT hScript)
{
	SquirrelSafeCheck safeCheck(vm_);
	if (!hScript) return;
	HSQOBJECT* obj = (HSQOBJECT*)hScript;
	sq_pushobject(vm_, *obj);

	sq_getdelegate(vm_, -1);

	sq_pushstring(vm_, "__vname", -1);
	sq_rawdeleteslot(vm_, -2, SQFalse);

	sq_pop(vm_, 2);

	sq_release(vm_, obj);
	delete obj;
}

HSCRIPT SquirrelVM::LookupFunction(const char* pszFunction, HSCRIPT hScope)
{
	SquirrelSafeCheck safeCheck(vm_);
	if (hScope)
	{
		HSQOBJECT* scope = (HSQOBJECT*)hScope;
		Assert(hScope != INVALID_HSCRIPT);
		sq_pushobject(vm_, *scope);
	}
	else
	{
		sq_pushroottable(vm_);
	}
	sq_pushstring(vm_, _SC(pszFunction), -1);

	HSQOBJECT obj;
	sq_resetobject(&obj);

	if (sq_get(vm_, -2) == SQ_OK)
	{
		sq_getstackobj(vm_, -1, &obj);
		sq_addref(vm_, &obj);
		sq_pop(vm_, 1);
	}
	sq_pop(vm_, 1);

	if (!sq_isclosure(obj))
	{
		sq_release(vm_, &obj);
		return nullptr;
	}

	HSQOBJECT* pObj = new HSQOBJECT;
	*pObj = obj;
	return (HSCRIPT)pObj;
}

void SquirrelVM::ReleaseFunction(HSCRIPT hScript)
{
	SquirrelSafeCheck safeCheck(vm_);
	if (!hScript) return;
	HSQOBJECT* obj = (HSQOBJECT*)hScript;
	sq_release(vm_, obj);
	delete obj;
}

ScriptStatus_t SquirrelVM::ExecuteFunction(HSCRIPT hFunction, ScriptVariant_t* pArgs, int nArgs, ScriptVariant_t* pReturn, HSCRIPT hScope, bool bWait)
{
	SquirrelSafeCheck safeCheck(vm_);
	if (!hFunction)
		return SCRIPT_ERROR;

	if (hFunction == INVALID_HSCRIPT)
		return SCRIPT_ERROR;

	HSQOBJECT* pFunc = (HSQOBJECT*)hFunction;
	sq_pushobject(vm_, *pFunc);

	if (hScope)
	{
		Assert(hScope != INVALID_HSCRIPT);
		HSQOBJECT* scope = (HSQOBJECT*)hScope;
		sq_pushobject(vm_, *scope);
	}
	else
	{
		sq_pushroottable(vm_);
	}

	for (int i = 0; i < nArgs; ++i)
	{
		PushVariant(vm_, pArgs[i]);
	}

	bool hasReturn = pReturn != nullptr;

	if (SQ_FAILED(sq_call(vm_, nArgs + 1, hasReturn, SQTrue)))
	{
		sq_pop(vm_, 1);
		return SCRIPT_ERROR;
	}

	if (hasReturn)
	{
		if (!getVariant(vm_, -1, *pReturn))
		{
			sq_pop(vm_, 1);
			return SCRIPT_ERROR;
		}

		sq_pop(vm_, 1);
	}

	sq_pop(vm_, 1);
	return SCRIPT_DONE;
}

HScriptRaw SquirrelVM::HScriptToRaw( HSCRIPT val )
{
	Assert( val );
	Assert( val != INVALID_HSCRIPT );

	HSQOBJECT *obj = (HSQOBJECT*)val;
#if 0
	if ( sq_isweakref(*obj) )
		return obj->_unVal.pWeakRef->_obj._unVal.raw;
#endif
	return obj->_unVal.raw;
}

ScriptStatus_t SquirrelVM::ExecuteHookFunction(const char *pszEventName, ScriptVariant_t* pArgs, int nArgs, ScriptVariant_t* pReturn, HSCRIPT hScope, bool bWait)
{
	SquirrelSafeCheck safeCheck(vm_);

	HSQOBJECT* pFunc = (HSQOBJECT*)GetScriptHookManager().GetHookFunction();
	sq_pushobject(vm_, *pFunc);

	// The call environment of the Hooks::Call function does not matter
	// as the function does not access any member variables.
	sq_pushroottable(vm_);

	sq_pushstring(vm_, pszEventName, -1);

	if (hScope)
		sq_pushobject(vm_, *((HSQOBJECT*)hScope));
	else
		sq_pushnull(vm_); // global hook

	for (int i = 0; i < nArgs; ++i)
	{
		PushVariant(vm_, pArgs[i]);
	}

	bool hasReturn = pReturn != nullptr;

	if (SQ_FAILED(sq_call(vm_, nArgs + 3, hasReturn, SQTrue)))
	{
		sq_pop(vm_, 1);
		return SCRIPT_ERROR;
	}

	if (hasReturn)
	{
		if (!getVariant(vm_, -1, *pReturn))
		{
			sq_pop(vm_, 1);
			return SCRIPT_ERROR;
		}

		sq_pop(vm_, 1);
	}

	sq_pop(vm_, 1);
	return SCRIPT_DONE;
}

void SquirrelVM::RegisterFunction(ScriptFunctionBinding_t* pScriptFunction)
{
	SquirrelSafeCheck safeCheck(vm_);
	Assert(pScriptFunction);

	if (!pScriptFunction)
		return;

	char typemask[64];
	Assert(pScriptFunction->m_desc.m_Parameters.Count() < sizeof(typemask));
	if (!CreateParamCheck(*pScriptFunction, typemask))
	{
		return;
	}

	sq_pushroottable(vm_);

	sq_pushstring(vm_, pScriptFunction->m_desc.m_pszScriptName, -1);

	sq_pushuserpointer(vm_, pScriptFunction);
	sq_newclosure(vm_, function_stub, 1);

	sq_setnativeclosurename(vm_, -1, pScriptFunction->m_desc.m_pszScriptName);

	sq_setparamscheck(vm_, pScriptFunction->m_desc.m_Parameters.Count() + 1, typemask);
	bool isStatic = false;
	sq_newslot(vm_, -3, isStatic);

	sq_pop(vm_, 1);

	RegisterDocumentation(vm_, pScriptFunction->m_desc);
}

bool SquirrelVM::RegisterClass(ScriptClassDesc_t* pClassDesc)
{
	SquirrelSafeCheck safeCheck(vm_);

	sq_pushroottable(vm_);
	sq_pushstring(vm_, pClassDesc->m_pszScriptName, -1);

	// Check if class name is already taken
	if (sq_get(vm_, -2) == SQ_OK)
	{
		HSQOBJECT obj;
		sq_resetobject(&obj);
		sq_getstackobj(vm_, -1, &obj);
		if (!sq_isnull(obj))
		{
			sq_pop(vm_, 2);
			return false;
		}

		sq_pop(vm_, 1);
	}

	// Register base in case it doesn't exist
	if (pClassDesc->m_pBaseDesc)
	{
		RegisterClass(pClassDesc->m_pBaseDesc);

		// Check if the base class can be
		sq_pushstring(vm_, pClassDesc->m_pBaseDesc->m_pszScriptName, -1);
		if (sq_get(vm_, -2) != SQ_OK)
		{
			sq_pop(vm_, 1);
			return false;
		}
	}

	if (SQ_FAILED(sq_newclass(vm_, pClassDesc->m_pBaseDesc ? SQTrue : SQFalse)))
	{
		sq_pop(vm_, 2 + (pClassDesc->m_pBaseDesc ? 1 : 0));
		return false;
	}

	sq_settypetag(vm_, -1, pClassDesc);

	sq_setclassudsize(vm_, -1, sizeof(ClassInstanceData));

	sq_pushstring(vm_, "constructor", -1);
	sq_newclosure(vm_, constructor_stub, 0);
	sq_newslot(vm_, -3, SQFalse);

	sq_pushstring(vm_, "_tostring", -1);
	sq_newclosure(vm_, tostring_stub, 0);
	sq_newslot(vm_, -3, SQFalse);

	if ( pClassDesc->pHelper )
	{
		sq_pushstring(vm_, "_get", -1);
		sq_newclosure(vm_, get_stub, 0);
		sq_newslot(vm_, -3, SQFalse);

		sq_pushstring(vm_, "_set", -1);
		sq_newclosure(vm_, set_stub, 0);
		sq_newslot(vm_, -3, SQFalse);
	}

	sq_pushstring(vm_, "IsValid", -1);
	sq_newclosure(vm_, IsValid_stub, 0);
	sq_newslot(vm_, -3, SQFalse);

	sq_pushstring(vm_, "weakref", -1);
	sq_newclosure(vm_, weakref_stub, 0);
	sq_newslot(vm_, -3, SQFalse);

	sq_pushstring(vm_, "getclass", -1);
	sq_newclosure(vm_, getclass_stub, 0);
	sq_newslot(vm_, -3, SQFalse);


	for (int i = 0; i < pClassDesc->m_FunctionBindings.Count(); ++i)
	{
		auto& scriptFunction = pClassDesc->m_FunctionBindings[i];

		char typemask[64];
		Assert(scriptFunction.m_desc.m_Parameters.Count() < sizeof(typemask));
		if (!CreateParamCheck(scriptFunction, typemask))
		{
			Warning("Unable to create param check for %s.%s\n",
				pClassDesc->m_pszClassname, scriptFunction.m_desc.m_pszFunction);
			break;
		}

		sq_pushstring(vm_, scriptFunction.m_desc.m_pszScriptName, -1);

		sq_pushuserpointer(vm_, &scriptFunction);
		sq_newclosure(vm_, function_stub, 1);

		sq_setnativeclosurename(vm_, -1, scriptFunction.m_desc.m_pszScriptName);

		sq_setparamscheck(vm_, scriptFunction.m_desc.m_Parameters.Count() + 1, typemask);
		bool isStatic = false;
		sq_newslot(vm_, -3, isStatic);

		RegisterDocumentation(vm_, scriptFunction.m_desc, pClassDesc);
	}

	for (int i = 0; i < pClassDesc->m_Hooks.Count(); ++i)
	{
		auto& scriptHook = pClassDesc->m_Hooks[i];

		RegisterHookDocumentation(vm_, scriptHook, scriptHook->m_desc, pClassDesc);
	}

	for (int i = 0; i < pClassDesc->m_Members.Count(); ++i)
	{
		auto& member = pClassDesc->m_Members[i];

		RegisterMemberDocumentation(vm_, member, pClassDesc);
	}

	sq_pushstring(vm_, pClassDesc->m_pszScriptName, -1);
	sq_push(vm_, -2);

	if (SQ_FAILED(sq_newslot(vm_, -4, SQFalse)))
	{
		sq_pop(vm_, 4);
		return false;
	}

	sq_pop(vm_, 2);

	RegisterClassDocumentation(vm_, pClassDesc);

	return true;
}

void SquirrelVM::RegisterConstant(ScriptConstantBinding_t* pScriptConstant)
{
	SquirrelSafeCheck safeCheck(vm_);
	Assert(pScriptConstant);

	if (!pScriptConstant)
		return;

	sq_pushconsttable(vm_);
	sq_pushstring(vm_, pScriptConstant->m_pszScriptName, -1);

	PushVariant(vm_, pScriptConstant->m_data);

	sq_newslot(vm_, -3, SQFalse);

	sq_pop(vm_, 1);

	char szValue[64];
	GetVariantScriptString( pScriptConstant->m_data, szValue, sizeof(szValue) );
	RegisterConstantDocumentation(vm_, pScriptConstant, szValue);
}

void SquirrelVM::RegisterEnum(ScriptEnumDesc_t* pEnumDesc)
{
	SquirrelSafeCheck safeCheck(vm_);
	Assert(pEnumDesc);

	if (!pEnumDesc)
		return;

	sq_newtableex(vm_, pEnumDesc->m_ConstantBindings.Count());

	sq_pushconsttable(vm_);

	sq_pushstring(vm_, pEnumDesc->m_pszScriptName, -1);
	sq_push(vm_, -3);
	sq_rawset(vm_, -3);

	for (int i = 0; i < pEnumDesc->m_ConstantBindings.Count(); ++i)
	{
		auto& scriptConstant = pEnumDesc->m_ConstantBindings[i];

		sq_pushstring(vm_, scriptConstant.m_pszScriptName, -1);
		PushVariant(vm_, scriptConstant.m_data);
		sq_rawset(vm_, -4);
		
		char szValue[64];
		GetVariantScriptString( scriptConstant.m_data, szValue, sizeof(szValue) );
		RegisterConstantDocumentation(vm_, &scriptConstant, szValue, pEnumDesc);
	}

	sq_pop(vm_, 2);

	RegisterEnumDocumentation(vm_, pEnumDesc);
}

void SquirrelVM::RegisterHook(ScriptHook_t* pHookDesc)
{
	SquirrelSafeCheck safeCheck(vm_);
	Assert(pHookDesc);

	if (!pHookDesc)
		return;

	RegisterHookDocumentation(vm_, pHookDesc, pHookDesc->m_desc, nullptr);
}

HSCRIPT SquirrelVM::RegisterInstance(ScriptClassDesc_t* pDesc, void* pInstance, bool bRefCounted)
{
	SquirrelSafeCheck safeCheck(vm_);

	this->RegisterClass(pDesc);

	sq_pushroottable(vm_);
	sq_pushstring(vm_, pDesc->m_pszScriptName, -1);

	if (sq_get(vm_, -2) != SQ_OK)
	{
		sq_pop(vm_, 1);
		return nullptr;
	}

	if (SQ_FAILED(sq_createinstance(vm_, -1)))
	{
		sq_pop(vm_, 2);
		return nullptr;
	}

	{
		ClassInstanceData *self;
		sq_getinstanceup(vm_, -1, (SQUserPointer*)&self, 0);
		new(self) ClassInstanceData(pInstance, pDesc, nullptr, bRefCounted);

		// can't delete the instance if it doesn't have a destructor
		// if the instance doesn't have a constructor,
		// the class needs to register the destructor with DEFINE_SCRIPT_REFCOUNTED_INSTANCE()
		Assert( !bRefCounted || self->desc->m_pfnDestruct );
	}

	sq_setreleasehook(vm_, -1, bRefCounted ? &destructor_stub : &destructor_stub_instance);

	HSQOBJECT* obj = new HSQOBJECT;
	sq_getstackobj(vm_, -1, obj);
	sq_addref(vm_, obj);
	sq_pop(vm_, 3);

	return (HSCRIPT)obj;
}

void SquirrelVM::SetInstanceUniqeId(HSCRIPT hInstance, const char* pszId)
{
	SquirrelSafeCheck safeCheck(vm_);

	if (!hInstance) return;
	HSQOBJECT* obj = (HSQOBJECT*)hInstance;
	sq_pushobject(vm_, *obj);

	ClassInstanceData* classInstanceData;
	sq_getinstanceup(vm_, -1, (SQUserPointer*)&classInstanceData, nullptr);

	classInstanceData->instanceId = pszId;

	sq_poptop(vm_);
}

void SquirrelVM::RemoveInstance(HSCRIPT hInstance)
{
	if (!hInstance)
		return;

	SquirrelSafeCheck safeCheck(vm_);

	HSQOBJECT* obj = (HSQOBJECT*)hInstance;
	ClassInstanceData *self;

	sq_pushobject(vm_, *obj);
	sq_getinstanceup(vm_, -1, (SQUserPointer*)&self, nullptr);
	sq_setinstanceup(vm_, -1, nullptr);
	sq_setreleasehook(vm_, -1, nullptr);
	sq_pop(vm_, 1);
	sq_release(vm_, obj);

	self->~ClassInstanceData();
	delete obj;
}

void* SquirrelVM::GetInstanceValue(HSCRIPT hInstance, ScriptClassDesc_t* pExpectedType)
{
	SquirrelSafeCheck safeCheck(vm_);

	if (!hInstance) return nullptr;
	HSQOBJECT* obj = (HSQOBJECT*)hInstance;

	if (pExpectedType)
	{
		sq_pushroottable(vm_);
		sq_pushstring(vm_, pExpectedType->m_pszScriptName, -1);

		if (sq_get(vm_, -2) != SQ_OK)
		{
			sq_pop(vm_, 1);
			return nullptr;
		}

		sq_pushobject(vm_, *obj);

		if (sq_instanceof(vm_) != SQTrue)
		{
			sq_pop(vm_, 3);
			return nullptr;
		}

		sq_pop(vm_, 3);
	}

	sq_pushobject(vm_, *obj);
	ClassInstanceData* classInstanceData;
	sq_getinstanceup(vm_, -1, (SQUserPointer*)&classInstanceData, nullptr);
	sq_pop(vm_, 1);


	if (!classInstanceData)
	{
		return nullptr;
	}


	return classInstanceData->instance;
}

bool SquirrelVM::GenerateUniqueKey(const char* pszRoot, char* pBuf, int nBufSize)
{
	static unsigned keyIdx = 0;
	// This gets used for script scope, still confused why it needs to be inside IScriptVM
	// is it just to be a compatible name for CreateScope?
	V_snprintf(pBuf, nBufSize, "%08X_%s", ++keyIdx, pszRoot);
	return true;
}

bool SquirrelVM::ValueExists(HSCRIPT hScope, const char* pszKey)
{
	SquirrelSafeCheck safeCheck(vm_);
	if (hScope)
	{
		HSQOBJECT* scope = (HSQOBJECT*)hScope;
		Assert(hScope != INVALID_HSCRIPT);
		sq_pushobject(vm_, *scope);
	}
	else
	{
		sq_pushroottable(vm_);
	}

	sq_pushstring(vm_, pszKey, -1);

	if (sq_get(vm_, -2) != SQ_OK)
	{
		sq_pop(vm_, 1);
		return false;
	}

	sq_pop(vm_, 2);
	return true;
}

bool SquirrelVM::SetValue(HSCRIPT hScope, const char* pszKey, const char* pszValue)
{
	SquirrelSafeCheck safeCheck(vm_);
	if (hScope)
	{
		HSQOBJECT* scope = (HSQOBJECT*)hScope;
		Assert(hScope != INVALID_HSCRIPT);
		sq_pushobject(vm_, *scope);
	}
	else
	{
		sq_pushroottable(vm_);
	}

	sq_pushstring(vm_, pszKey, -1);
	sq_pushstring(vm_, pszValue, -1);

	sq_newslot(vm_, -3, SQFalse);

	sq_pop(vm_, 1);
	return true;
}

bool SquirrelVM::SetValue(HSCRIPT hScope, const char* pszKey, const ScriptVariant_t& value)
{
	SquirrelSafeCheck safeCheck(vm_);
	if (hScope)
	{
		HSQOBJECT* scope = (HSQOBJECT*)hScope;
		Assert(hScope != INVALID_HSCRIPT);
		sq_pushobject(vm_, *scope);
	}
	else
	{
		sq_pushroottable(vm_);
	}

	sq_pushstring(vm_, pszKey, -1);
	PushVariant(vm_, value);

	sq_newslot(vm_, -3, SQFalse);

	sq_pop(vm_, 1);
	return true;
}

bool SquirrelVM::SetValue( HSCRIPT hScope, const ScriptVariant_t& key, const ScriptVariant_t& val )
{
	SquirrelSafeCheck safeCheck(vm_);
	HSQOBJECT obj = *(HSQOBJECT*)hScope;
	if (hScope)
	{
		Assert(hScope != INVALID_HSCRIPT);
		sq_pushobject(vm_, obj);
	}
	else
	{
		sq_pushroottable(vm_);
	}

	if ( sq_isarray(obj) )
	{
		Assert( key.m_type == FIELD_INTEGER );

		sq_pushinteger(vm_, key.m_int);
		PushVariant(vm_, val);

		sq_set(vm_, -3);
	}
	else
	{
		PushVariant(vm_, key);
		PushVariant(vm_, val);

		sq_newslot(vm_, -3, SQFalse);
	}

	sq_pop(vm_, 1);
	return true;
}

void SquirrelVM::CreateTable(ScriptVariant_t& Table)
{
	SquirrelSafeCheck safeCheck(vm_);

	sq_newtable(vm_);

	HSQOBJECT* obj = new HSQOBJECT;
	sq_resetobject(obj);
	sq_getstackobj(vm_, -1, obj);
	sq_addref(vm_, obj);
	sq_pop(vm_, 1);

	Table = (HSCRIPT)obj;
}

//
// input table/array/class/instance/string
//
int	SquirrelVM::GetNumTableEntries(HSCRIPT hScope)
{
	SquirrelSafeCheck safeCheck(vm_);

	if (!hScope)
	{
		// sq_getsize returns -1 on invalid input
		return -1;
	}

	HSQOBJECT* scope = (HSQOBJECT*)hScope;
	Assert(hScope != INVALID_HSCRIPT);
	sq_pushobject(vm_, *scope);

	int count = sq_getsize(vm_, -1);

	sq_pop(vm_, 1);

	return count;
}

int SquirrelVM::GetKeyValue(HSCRIPT hScope, int nIterator, ScriptVariant_t* pKey, ScriptVariant_t* pValue)
{
	SquirrelSafeCheck safeCheck(vm_);

	if (hScope)
	{
		Assert(hScope != INVALID_HSCRIPT);
		HSQOBJECT* scope = (HSQOBJECT*)hScope;
		sq_pushobject(vm_, *scope);
	}
	else
	{
		sq_pushroottable(vm_);
	}

	if (nIterator == -1)
	{
		sq_pushnull(vm_);
	}
	else
	{
		sq_pushinteger(vm_, nIterator);
	}

	SQInteger nextiter = -1;

	if (SQ_SUCCEEDED(sq_next(vm_, -2)))
	{
		if (pKey) getVariant(vm_, -2, *pKey);
		if (pValue) getVariant(vm_, -1, *pValue);

		sq_getinteger(vm_, -3, &nextiter);
		sq_pop(vm_, 2);
	}
	sq_pop(vm_, 2);

	return nextiter;
}

bool SquirrelVM::GetValue(HSCRIPT hScope, const char* pszKey, ScriptVariant_t* pValue)
{
#ifdef _DEBUG
	AssertMsg( pszKey, "FATAL: cannot get NULL" );

	// Don't crash on debug
	if ( !pszKey )
		return GetValue( hScope, ScriptVariant_t(0), pValue );
#endif

	SquirrelSafeCheck safeCheck(vm_);

	Assert(pValue);
	if (!pValue)
	{
		return false;
	}

	if (hScope)
	{
		HSQOBJECT* scope = (HSQOBJECT*)hScope;
		Assert(hScope != INVALID_HSCRIPT);
		sq_pushobject(vm_, *scope);
	}
	else
	{
		sq_pushroottable(vm_);
	}

	sq_pushstring(vm_, pszKey, -1);

	if (sq_get(vm_, -2) != SQ_OK)
	{
		sq_pop(vm_, 1);
		return false;
	}

	if (!getVariant(vm_, -1, *pValue))
	{
		sq_pop(vm_, 2);
		return false;
	}

	sq_pop(vm_, 2);

	return true;
}

bool SquirrelVM::GetValue( HSCRIPT hScope, ScriptVariant_t key, ScriptVariant_t* pValue )
{
	SquirrelSafeCheck safeCheck(vm_);

	Assert(pValue);
	if (!pValue)
	{
		return false;
	}

	if (hScope)
	{
		HSQOBJECT* scope = (HSQOBJECT*)hScope;
		Assert(hScope != INVALID_HSCRIPT);
		sq_pushobject(vm_, *scope);
	}
	else
	{
		sq_pushroottable(vm_);
	}

	PushVariant(vm_, key);

	if (sq_get(vm_, -2) != SQ_OK)
	{
		sq_pop(vm_, 1);
		return false;
	}

	if (!getVariant(vm_, -1, *pValue))
	{
		sq_pop(vm_, 2);
		return false;
	}

	sq_pop(vm_, 2);

	return true;
}


void SquirrelVM::ReleaseValue(ScriptVariant_t& value)
{
	SquirrelSafeCheck safeCheck(vm_);
	if (value.m_type == FIELD_HSCRIPT)
	{
		HSCRIPT hScript = value;
		HSQOBJECT* obj = (HSQOBJECT*)hScript;
		sq_release(vm_, obj);
		delete obj;
	}
	else
	{
		value.Free();
	}

	// Let's prevent this being called again and giving some UB
	value.m_type = FIELD_VOID;
	value.m_flags = 0;
}

bool SquirrelVM::ClearValue(HSCRIPT hScope, const char* pszKey)
{
	SquirrelSafeCheck safeCheck(vm_);

	if (hScope)
	{
		HSQOBJECT* scope = (HSQOBJECT*)hScope;
		Assert(hScope != INVALID_HSCRIPT);
		sq_pushobject(vm_, *scope);
	}
	else
	{
		sq_pushroottable(vm_);
	}

	sq_pushstring(vm_, pszKey, -1);
	if (SQ_FAILED(sq_deleteslot(vm_, -2, SQFalse)))
	{
		sq_pop(vm_, 1);
		return false;
	}

	sq_pop(vm_, 1);
	return true;
}

bool SquirrelVM::ClearValue(HSCRIPT hScope, ScriptVariant_t pKey)
{
	SquirrelSafeCheck safeCheck(vm_);

	if (hScope)
	{
		HSQOBJECT* scope = (HSQOBJECT*)hScope;
		Assert(hScope != INVALID_HSCRIPT);
		sq_pushobject(vm_, *scope);
	}
	else
	{
		sq_pushroottable(vm_);
	}

	PushVariant(vm_, pKey);
	if (SQ_FAILED(sq_deleteslot(vm_, -2, SQFalse)))
	{
		sq_pop(vm_, 1);
		return false;
	}

	sq_pop(vm_, 1);
	return true;
}


void SquirrelVM::CreateArray(ScriptVariant_t &arr, int size)
{
	SquirrelSafeCheck safeCheck(vm_);

	HSQOBJECT *obj = new HSQOBJECT;
	sq_resetobject(obj);

	sq_newarray(vm_,size);
	sq_getstackobj(vm_, -1, obj);
	sq_addref(vm_, obj);
	sq_pop(vm_, 1);

	arr = (HSCRIPT)obj;
}

bool SquirrelVM::ArrayAppend(HSCRIPT hArray, const ScriptVariant_t &val)
{
	SquirrelSafeCheck safeCheck(vm_);

	HSQOBJECT *arr = (HSQOBJECT*)hArray;

	sq_pushobject(vm_, *arr);
	PushVariant(vm_, val);
	bool ret = sq_arrayappend(vm_, -2) == SQ_OK;
	sq_pop(vm_, 1);

	return ret;
}

HSCRIPT SquirrelVM::CopyObject(HSCRIPT obj)
{
	if ( !obj )
		return NULL;

	HSQOBJECT *ret = new HSQOBJECT;
	*ret = *(HSQOBJECT*)obj;
	sq_addref( vm_, ret );
	return (HSCRIPT)ret;
}

//-------------------------------------------------------------
//-------------------------------------------------------------

enum ClassType
{
	VectorClassType = 0,
	NativeClassType = 1,
	ScriptClassType = 2
};

// Use iterator as SQTable::_nodes is private
#define FOREACH_SQTABLE( pTable, key, val )\
	SQInteger i = 0;\
	SQObjectPtr pi = i;\
	for ( ; (i = pTable->Next( false, pi, key, val )) != -1; pi._unVal.nInteger = i )


#define NATIVE_NAME_READBUF_SIZE 128

void SquirrelVM::WriteObject( const SQObjectPtr &obj, CUtlBuffer* pBuffer, WriteStateMap& writeState )
{
	pBuffer->PutInt( obj._type );

	switch ( obj._type )
	{
	case OT_NULL:
		break;

	case OT_INTEGER:
#ifdef _SQ64
		pBuffer->PutInt64( obj._unVal.nInteger );
#else
		pBuffer->PutInt( obj._unVal.nInteger );
#endif
		break;

	case OT_FLOAT:
#ifdef SQUSEDOUBLE
		pBuffer->PutDouble( obj._unVal.fFloat );
#else
		pBuffer->PutFloat( obj._unVal.fFloat );
#endif
		break;

	case OT_BOOL:
		Assert( ( obj._unVal.nInteger & -2 ) == 0 );
		pBuffer->PutChar( obj._unVal.nInteger );
		break;

	case OT_STRING:
		pBuffer->PutInt( obj._unVal.pString->_len );
		pBuffer->Put( obj._unVal.pString->_val, obj._unVal.pString->_len );
		break;

	case OT_TABLE:
	{
		SQTable *pThis = obj._unVal.pTable;

		if ( writeState.CheckCache( pThis, pBuffer ) )
			break;

		int count = pThis->CountUsed();
		DBG_CODE_NOSCOPE( int nCountPut = pBuffer->TellPut(); )
		pBuffer->PutInt( count );

		pBuffer->PutChar( pThis->_delegate != NULL );
		if ( pThis->_delegate )
		{
			WriteObject( pThis->_delegate, pBuffer, writeState );
		}

		{
			SQObjectPtr key, val;
			FOREACH_SQTABLE( pThis, key, val )
			{
				WriteObject( key, pBuffer, writeState );
				WriteObject( val, pBuffer, writeState );
				DBG_CODE( count--; );
			}
		}

#ifdef _DEBUG
		if ( count )
		{
			int prev = *(int*)( (char*)pBuffer->Base() + nCountPut );
			*(int*)( (char*)pBuffer->Base() + nCountPut ) = prev - count;
			Warning( "Table serialisation error, changed count %d->%d\n", prev, prev - count );
			Assert(0);
		}
#endif
		break;
	}
	case OT_ARRAY:
	{
		SQArray *pThis = obj._unVal.pArray;

		if ( writeState.CheckCache( pThis, pBuffer ) )
			break;

		int count = pThis->_values.size();
		DBG_CODE_NOSCOPE(
			int counter = count;
			int nCountPut = pBuffer->TellPut();
		)
		pBuffer->PutInt( count );

		for ( int i = 0; i < count; ++i )
		{
			WriteObject( pThis->_values[i], pBuffer, writeState );
			DBG_CODE( counter--; );
		}

#ifdef _DEBUG
		if ( counter )
		{
			int prev = *(int*)( (char*)pBuffer->Base() + nCountPut );
			*(int*)( (char*)pBuffer->Base() + nCountPut ) = prev - counter;
			Warning( "Array serialisation error, changed count %d->%d\n", prev, prev - count );
			Assert(0);
		}
#endif
		break;
	}
	case OT_CLOSURE:
	{
		SQClosure *pThis = obj._unVal.pClosure;

		if ( writeState.CheckCache( pThis, pBuffer ) )
			break;

		Assert( pThis->_function );

		WriteObject( pThis->_function, pBuffer, writeState );

		pBuffer->PutChar( pThis->_root != NULL );
		if ( pThis->_root )
		{
			WriteObject( pThis->_root, pBuffer, writeState );
		}

		pBuffer->PutChar( pThis->_env != NULL );
		if ( pThis->_env )
		{
			WriteObject( pThis->_env, pBuffer, writeState );
		}

		// NOTE: Used for class functions to access the base class via 'base' keyword.
		// This is assigned in SQClass::NewSlot().
		// The alternative to writing SQClosure::_base would be always using SQClass::NewSlot() on class restore.
		pBuffer->PutChar( pThis->_base != NULL );
		if ( pThis->_base )
		{
			WriteObject( pThis->_base, pBuffer, writeState );
		}

		int count = pThis->_function->_noutervalues;
		pBuffer->PutInt( count );
		for ( int i = 0; i < count; ++i )
		{
			WriteObject( pThis->_outervalues[i], pBuffer, writeState );
		}

		count = pThis->_function->_ndefaultparams;
		pBuffer->PutInt( count );
		for ( int i = 0; i < count; ++i )
		{
			WriteObject( pThis->_defaultparams[i], pBuffer, writeState );
		}

		break;
	}
	// TODO: Nameless and non-global native closures
	// if _name == NULL and _noutervalues == 0 then it is member IsValid etc.
	// if _name != NULL and _outervalues[0] == USERPOINTER then global or member functions
	// if this is a non-bound (!_env) member native closure, it will be written but won't be read,
	// or wrong closure will be read if a closure with the same name exists in the root table.
	//
	// An identifier can be added to script descriptions to determine the type of this native closure to read/write
	case OT_NATIVECLOSURE:
	{
		SQNativeClosure *pThis = obj._unVal.pNativeClosure;

#ifdef _DEBUG
		bool bAsserted = false;

		if ( pThis->_noutervalues && pThis->_name._type == OT_STRING && pThis->_name._unVal.pString &&
				pThis->_outervalues[0]._type == OT_USERPOINTER )
		{
			Assert( pThis->_noutervalues == 1 );
			Assert( pThis->_outervalues[0]._type == OT_USERPOINTER );

			const SQUserPointer userpointer = pThis->_outervalues[0]._unVal.pUserPointer;
			const CUtlVector< ScriptClassDesc_t* > &classes = ScriptClassDesc_t::AllClassesDesc();
			FOR_EACH_VEC( classes, i )
			{
				const CUtlVector< ScriptFunctionBinding_t > &funcs = classes[i]->m_FunctionBindings;
				FOR_EACH_VEC( funcs, j )
				{
					if ( &funcs[j] == userpointer )
					{
						AssertMsg( 0, "SquirrelVM: Native closure is not saved! '%s' -> %s::%s",
								pThis->_name._unVal.pString->_val,
								classes[i]->m_pszScriptName,
								funcs[j].m_desc.m_pszScriptName );
						bAsserted = true;
						goto done;
					}
				}
			}
			done:;
		}
#endif

		if ( pThis->_name._type == OT_STRING && pThis->_name._unVal.pString && !pThis->_env )
		{
			Assert( pThis->_name._unVal.pString->_len < NATIVE_NAME_READBUF_SIZE );
			pBuffer->Put( pThis->_name._unVal.pString->_val, pThis->_name._unVal.pString->_len + 1 );
			break;
		}

#ifdef _DEBUG
		if ( pThis->_name._type == OT_STRING && pThis->_name._unVal.pString )
		{
			if ( !bAsserted )
				AssertMsg( 0, "SquirrelVM: Native closure is not saved! '%s'", pThis->_name._unVal.pString->_val );
		}
		else
		{
			AssertMsg( 0, "SquirrelVM: Native closure is not saved!" );
		}
#endif

		Assert( *(int*)pBuffer->PeekPut( -(int)sizeof(int) ) == OT_NATIVECLOSURE );
		*(int*)pBuffer->PeekPut( -(int)sizeof(int) ) = OT_NULL;

		break;
	}
	case OT_FUNCPROTO:
	{
		SQFunctionProto *pThis = obj._unVal.pFunctionProto;

		if ( writeState.CheckCache( pThis, pBuffer ) )
			break;

		// NOTE: SQFunctionProto::Save cannot save non-primitive literals. Save everything manually!
		pBuffer->PutInt( pThis->_nliterals );
		pBuffer->PutInt( pThis->_nparameters );
		pBuffer->PutInt( pThis->_noutervalues );
		pBuffer->PutInt( pThis->_nlocalvarinfos );
		pBuffer->PutInt( pThis->_nlineinfos );
		pBuffer->PutInt( pThis->_ndefaultparams );
		pBuffer->PutInt( pThis->_ninstructions );
		pBuffer->PutInt( pThis->_nfunctions );

		WriteObject( pThis->_sourcename, pBuffer, writeState );
		WriteObject( pThis->_name, pBuffer, writeState );

		for ( int i = 0; i < pThis->_nliterals; ++i )
		{
			WriteObject( pThis->_literals[i], pBuffer, writeState );
		}

		for ( int i = 0; i < pThis->_nparameters; ++i )
		{
			WriteObject( pThis->_parameters[i], pBuffer, writeState );
		}

		for ( int i = 0; i < pThis->_noutervalues; ++i )
		{
			SQOuterVar &p = pThis->_outervalues[i];
			pBuffer->PutUnsignedInt( p._type );
			WriteObject( p._src, pBuffer, writeState );
			WriteObject( p._name, pBuffer, writeState );
		}

		for ( int i = 0; i < pThis->_nlocalvarinfos; ++i )
		{
			SQLocalVarInfo &p = pThis->_localvarinfos[i];
			WriteObject( p._name, pBuffer, writeState );
			pBuffer->PutUnsignedInt( p._pos );
			pBuffer->PutUnsignedInt( p._start_op );
			pBuffer->PutUnsignedInt( p._end_op );
		}

		pBuffer->Put( pThis->_lineinfos, sizeof(SQLineInfo) * pThis->_nlineinfos );
		pBuffer->Put( pThis->_defaultparams, sizeof(SQInteger) * pThis->_ndefaultparams );
		pBuffer->Put( pThis->_instructions, sizeof(SQInstruction) * pThis->_ninstructions );

		for ( int i = 0; i < pThis->_nfunctions; ++i )
		{
			WriteObject( pThis->_functions[i], pBuffer, writeState );
		}

		pBuffer->PutInt( pThis->_stacksize );
		pBuffer->PutChar( pThis->_bgenerator );
		pBuffer->PutInt( pThis->_varparams );

		break;
	}
	case OT_CLASS:
	{
		SQClass *pThis = obj._unVal.pClass;

		if ( writeState.CheckCache( pThis, pBuffer ) )
			break;

		SQUserPointer typetag = pThis->_typetag;

		if ( typetag != NULL )
		{
			if ( typetag == TYPETAG_VECTOR )
			{
				pBuffer->PutChar( VectorClassType );
			}
			else
			{
				ScriptClassDesc_t *pDesc = (ScriptClassDesc_t*)typetag;
				pBuffer->PutChar( NativeClassType );
				pBuffer->PutString( pDesc->m_pszScriptName );
				Assert( strlen(pDesc->m_pszScriptName) < NATIVE_NAME_READBUF_SIZE );
			}

			break;
		}

		if ( pThis == _class(regexpClass_) )
		{
			pBuffer->PutChar( NativeClassType );
			pBuffer->PutString( "regexp" );

			break;
		}

		pBuffer->PutChar( ScriptClassType );

		pBuffer->PutChar( pThis->_base != NULL );
		if ( pThis->_base )
		{
			WriteObject( pThis->_base, pBuffer, writeState );
		}

		// FIXME: This is inefficient for inheritance, broken for native class inheritance

		WriteObject( pThis->_members, pBuffer, writeState );

		int count = pThis->_defaultvalues.size();
		pBuffer->PutInt( count );

		for ( int i = 0; i < count; ++i )
		{
			WriteObject( pThis->_defaultvalues[i].val, pBuffer, writeState );
		}

		count = pThis->_methods.size();
		pBuffer->PutInt( count );

		for ( int i = 0; i < count; ++i )
		{
			WriteObject( pThis->_methods[i].val, pBuffer, writeState );
		}

		// only write valid metmathods instead of writing 18 nulls every time
		int mask = 0;
		int nMaskPut = pBuffer->TellPut();
		pBuffer->PutInt( mask );

		// all metamethods can fit in an int32 mask
		Assert( SQMetaMethod::MT_LAST <= (sizeof(int) << 3) );

		for ( unsigned i = 0; i < SQMetaMethod::MT_LAST; ++i )
		{
			if ( pThis->_metamethods[i]._type != OT_NULL )
			{
				mask |= 1 << i;
				WriteObject( pThis->_metamethods[i], pBuffer, writeState );
			}
		}

		if ( mask )
		{
			*(int*)( (char*)pBuffer->Base() + nMaskPut ) = mask;
		}

		pBuffer->PutInt( pThis->_constructoridx );

		break;
	}
	case OT_INSTANCE:
	{
		SQInstance *pThis = obj._unVal.pInstance;

		if ( writeState.CheckCache( pThis, pBuffer ) )
			break;

		WriteObject( pThis->_class, pBuffer, writeState );

		if ( pThis->_class == _class(regexpClass_) )
		{
			SQObjectPtr key = SQString::Create( _ss(vm_), "pattern_" );
			SQObjectPtr val;
			pThis->Get( key, val );
			WriteObject( val, pBuffer, writeState );

			break;
		}

		SQUserPointer typetag = pThis->_class->_typetag;
		if ( typetag )
		{
			if ( typetag == TYPETAG_VECTOR )
			{
				Vector *v = (Vector *)pThis->_userpointer;
				Assert(v);
				pBuffer->PutFloat( v->x );
				pBuffer->PutFloat( v->y );
				pBuffer->PutFloat( v->z );
			}
			else
			{
				ClassInstanceData *pData = (ClassInstanceData *)pThis->_userpointer;
				if ( pData )
				{
					if ( pData->desc->m_pszDescription[0] == SCRIPT_SINGLETON[0] )
					{
						// Do nothing, singleton should be created from just the class
					}
					else if ( !pData->instanceId.IsEmpty() )
					{
						Assert( strlen(pData->instanceId.Get()) < NATIVE_NAME_READBUF_SIZE );
						pBuffer->PutString( pData->instanceId );
						pBuffer->PutChar( pData->refCounted ? 1 : 0 );
					}
					else
					{
						DevWarning( "SquirrelVM::WriteObject: no instanceID for '%s', unable to serialize\n",
							pData->desc->m_pszClassname );

						pBuffer->PutString( "" );
					}
				}
				else
				{
					pBuffer->PutString( "" );
				}
			}

			break;
		}

		// NOTE: move this up if native classes can have default values
		int count = pThis->_class->_defaultvalues.size();
		pBuffer->PutInt( count );
		for ( int i = 0; i < count; ++i )
		{
			WriteObject( pThis->_values[i], pBuffer, writeState );
		}

		break;
	}
	case OT_WEAKREF:
	{
		SQWeakRef *pThis = obj._unVal.pWeakRef;
		WriteObject( pThis->_obj, pBuffer, writeState );
		break;
	}
	case OT_OUTER:
	{
		SQOuter *pThis = obj._unVal.pOuter;

		if ( writeState.CheckCache( pThis, pBuffer ) )
			break;

		Assert( pThis->_valptr == &pThis->_value ); // otherwise untested

		WriteObject( pThis->_value, pBuffer, writeState );
		break;
	}
	case OT_THREAD:
	{
		SQVM *pThis = obj._unVal.pThread;

		if ( writeState.CheckCache( pThis, pBuffer ) )
			break;

		pBuffer->PutInt( pThis->_stack.size() );
		pBuffer->PutInt( pThis->_callsstacksize );

		if ( pThis->_callsstacksize )
		{
			for ( int i = 0; i < pThis->_callsstacksize; i++ )
			{
				const SQVM::CallInfo *ci = &pThis->_callsstack[i];

				Assert( ci->_ip >= ci->_closure._unVal.pClosure->_function->_instructions &&
						ci->_ip < ci->_closure._unVal.pClosure->_function->_instructions +
							ci->_closure._unVal.pClosure->_function->_ninstructions );
				Assert( pThis->_etraps.size() >= (SQUnsignedInteger)ci->_etraps );
				Assert( ci->_closure._type == OT_CLOSURE && ci->_closure._unVal.pClosure );

				WriteObject( ci->_closure, pBuffer, writeState );

				Assert( ci->_ip - ci->_closure._unVal.pClosure->_function->_instructions <= INT_MAX );

				pBuffer->PutChar( ci->_generator != 0 );
				if ( ci->_generator )
					WriteObject( ci->_generator, pBuffer, writeState );

				int offset = ci->_ip - ci->_closure._unVal.pClosure->_function->_instructions;
				pBuffer->PutInt( offset );
				pBuffer->PutInt( ci->_etraps );
				pBuffer->PutInt( ci->_prevstkbase );
				pBuffer->PutInt( ci->_prevtop );
				pBuffer->PutInt( ci->_target );
				pBuffer->PutInt( ci->_ncalls );
				pBuffer->PutChar( ci->_root );

				for ( int j = 0; j < ci->_etraps; j++ )
				{
					const SQExceptionTrap &et = pThis->_etraps[j];
					pBuffer->PutInt( et._stacksize );
					pBuffer->PutInt( et._stackbase );
					Assert( et._ip - ci->_ip <= INT_MAX );
					pBuffer->PutInt( et._ip - ci->_ip );
					pBuffer->PutInt( et._extarget );
				}
			}

			int stackidx = pThis->ci - pThis->_callsstack;
			Assert( stackidx >= 0 && stackidx < pThis->_callsstacksize );
			pBuffer->PutInt( stackidx );
		}

		pBuffer->PutInt( pThis->_nnativecalls );
		pBuffer->PutInt( pThis->_nmetamethodscall );

		pBuffer->PutChar( pThis->_suspended );
		pBuffer->PutChar( pThis->_suspended_root );
		pBuffer->PutInt( pThis->_suspended_target );
		pBuffer->PutInt( pThis->_suspended_traps );

		WriteVM( pThis, pBuffer, writeState );

		break;
	}
	case OT_GENERATOR:
	{
		SQGenerator *pThis = obj._unVal.pGenerator;

		if ( writeState.CheckCache( pThis, pBuffer ) )
			break;

		pBuffer->PutChar( pThis->_state );

		if ( pThis->_state == SQGenerator::SQGeneratorState::eDead )
			break;

		WriteObject( pThis->_closure, pBuffer, writeState );

		const SQVM::CallInfo *ci = &pThis->_ci;

		Assert( pThis->_closure._unVal.pClosure == ci->_closure._unVal.pClosure );
		Assert( ci->_ip >= ci->_closure._unVal.pClosure->_function->_instructions &&
				ci->_ip < ci->_closure._unVal.pClosure->_function->_instructions +
					ci->_closure._unVal.pClosure->_function->_ninstructions );
		Assert( pThis->_etraps.size() >= (SQUnsignedInteger)ci->_etraps );

		Assert( ci->_ip - ci->_closure._unVal.pClosure->_function->_instructions <= INT_MAX );

		pBuffer->PutChar( ci->_generator != 0 );
		if ( ci->_generator )
			WriteObject( ci->_generator, pBuffer, writeState );

		int offset = ci->_ip - ci->_closure._unVal.pClosure->_function->_instructions;
		pBuffer->PutInt( offset );
		pBuffer->PutInt( ci->_etraps );
		pBuffer->PutInt( ci->_prevstkbase );
		pBuffer->PutInt( ci->_prevtop );
		pBuffer->PutInt( ci->_target );
		pBuffer->PutInt( ci->_ncalls );
		pBuffer->PutChar( ci->_root );

		for ( int j = 0; j < ci->_etraps; j++ )
		{
			const SQExceptionTrap &et = pThis->_etraps[j];
			pBuffer->PutInt( et._stacksize );
			pBuffer->PutInt( et._stackbase );
			Assert( et._ip - ci->_ip <= INT_MAX );
			pBuffer->PutInt( et._ip - ci->_ip );
			pBuffer->PutInt( et._extarget );
		}

		int stacksize = pThis->_stack.size();
		pBuffer->PutInt( stacksize );
		for ( int i = 0; i < stacksize; ++i )
		{
			WriteObject( pThis->_stack[i], pBuffer, writeState );
		}

		break;
	}
	case OT_USERDATA:
	case OT_USERPOINTER:
		break;
	default:
		AssertMsgAlways( 0, "SquirrelVM::WriteObject: unknown type" );
	}
}


void SquirrelVM::ReadObject( SQObjectPtr &pObj, CUtlBuffer* pBuffer, ReadStateMap& readState )
{
	SQObject obj;
	obj._type = (SQObjectType)pBuffer->GetInt();
	DBG_CODE( obj._unVal.raw = 0xCC; );

	switch ( obj._type )
	{
	case OT_NULL:
		obj._unVal.raw = 0;
		break;

	case OT_INTEGER:
#ifdef _SQ64
		obj._unVal.nInteger = (SQInteger)pBuffer->GetInt64();
#else
		obj._unVal.nInteger = (SQInteger)pBuffer->GetInt();
#endif
		break;

	case OT_FLOAT:
#ifdef SQUSEDOUBLE
		obj._unVal.fFloat = (SQFloat)pBuffer->GetDouble();
#else
		obj._unVal.fFloat = (SQFloat)pBuffer->GetFloat();
#endif
		break;

	case OT_BOOL:
		obj._unVal.nInteger = pBuffer->GetChar();
		Assert( ( obj._unVal.nInteger & -2 ) == 0 );
		break;

	case OT_STRING:
	{
		int len = pBuffer->GetInt();
		char *psz = (char*)pBuffer->PeekGet( 0 );
		pBuffer->SeekGet( CUtlBuffer::SEEK_CURRENT, len );
		Assert( pBuffer->IsValid() );
		obj._unVal.pString = SQString::Create( _ss(vm_), psz, len );
		break;
	}
	case OT_TABLE:
	{
		int marker;
		if ( readState.CheckCache( &obj, pBuffer, &marker ) )
			break;

		SQTable *pThis;

		int count = pBuffer->GetInt();

		// NOTE: Hack for VM load - do not release the current roottable
		// which would remove native funcs which are looked up from the root
		if ( pObj._unVal.pTable != vm_->_roottable._unVal.pTable )
		{
			pThis = SQTable::Create( _ss(vm_), count );
		}
		else
		{
			pThis = pObj._unVal.pTable;
		}

		obj._unVal.pTable = pThis;
		readState.StoreInCache( marker, obj );

		if ( pBuffer->GetChar() )
		{
			SQObject delegate;
			ReadObject( delegate, pBuffer, readState );
			pThis->_delegate = delegate._unVal.pTable;
		}

		SQObjectPtr key, val;
		while ( count-- )
		{
			ReadObject( key, pBuffer, readState );
			ReadObject( val, pBuffer, readState );
			Assert( key._type != OT_NULL );
			pThis->NewSlot( key, val );
		}

		break;
	}
	case OT_ARRAY:
	{
		int marker;
		if ( readState.CheckCache( &obj, pBuffer, &marker ) )
			break;

		int count = pBuffer->GetInt();

		SQArray *pThis = SQArray::Create( _ss(vm_), count );
		obj._unVal.pArray = pThis;
		readState.StoreInCache( marker, obj );

		for ( int i = 0; i < count; ++i )
		{
			ReadObject( pThis->_values[i], pBuffer, readState );
		}

		break;
	}
	case OT_CLOSURE:
	{
		int marker;
		if ( readState.CheckCache( &obj, pBuffer, &marker ) )
			break;

		SQObjectPtr func, root;
		ReadObject( func, pBuffer, readState );
		Assert( func._type == OT_FUNCPROTO && func._unVal.pFunctionProto );

		if ( pBuffer->GetChar() )
		{
			ReadObject( root, pBuffer, readState );
			Assert( root._type != OT_NULL && root._unVal.pWeakRef );
		}
		else
		{
			root = vm_->_roottable._unVal.pTable->GetWeakRef( OT_TABLE );
		}

		SQClosure *pThis = SQClosure::Create( _ss(vm_), func._unVal.pFunctionProto, root._unVal.pWeakRef );
		obj._unVal.pClosure = pThis;
		readState.StoreInCache( marker, obj );

		if ( pBuffer->GetChar() )
		{
			SQObject env;
			ReadObject( env, pBuffer, readState );
			pThis->_env = env._unVal.pWeakRef;
		}

		if ( pBuffer->GetChar() )
		{
			SQObjectPtr base;
			ReadObject( base, pBuffer, readState );
			pThis->_base = base._unVal.pClass;
		}

		int count = pBuffer->GetInt();
		for ( int i = 0; i < count; ++i )
		{
			ReadObject( pThis->_outervalues[i], pBuffer, readState );
		}

		count = pBuffer->GetInt();
		for ( int i = 0; i < count; ++i )
		{
			ReadObject( pThis->_defaultparams[i], pBuffer, readState );
		}

		break;
	}
	case OT_NATIVECLOSURE:
	{
		char psz[NATIVE_NAME_READBUF_SIZE] = "";
		pBuffer->GetString( psz, sizeof(psz) );

		SQObjectPtr key = SQString::Create( _ss(vm_), psz );
		SQObjectPtr val;

		if ( !vm_->_roottable._unVal.pTable->Get( key, val ) )
		{
			Warning( "SquirrelVM::ReadObject: failed to find native closure '%s'\n", psz );
			obj._type = OT_NULL;
			obj._unVal.raw = 0;
			break;
		}

#ifdef _DEBUG
		// If it's a script closure which references this native closure,
		// these references will stack up on each load-save
		if ( val._type != OT_NATIVECLOSURE )
			Warning( "SquirrelVM::ReadObject: '%s' is not nativeclosure\n", psz );
#endif

		AssertMsg( val._type == OT_NATIVECLOSURE || val._type == OT_CLOSURE, "'%s' is not a closure", psz );

		obj._type = val._type;
		obj._unVal.pNativeClosure = val._unVal.pNativeClosure;

		break;
	}
	case OT_FUNCPROTO:
	{
		int marker;
		if ( readState.CheckCache( &obj, pBuffer, &marker ) )
			break;

		int nliterals = pBuffer->GetInt();
		int nparameters = pBuffer->GetInt();
		int noutervalues = pBuffer->GetInt();
		int nlocalvarinfos = pBuffer->GetInt();
		int nlineinfos = pBuffer->GetInt();
		int ndefaultparams = pBuffer->GetInt();
		int ninstructions = pBuffer->GetInt();
		int nfunctions = pBuffer->GetInt();

		SQFunctionProto *pThis = SQFunctionProto::Create( _ss(vm_), ninstructions, nliterals, nparameters,
			nfunctions, noutervalues, nlineinfos, nlocalvarinfos, ndefaultparams );
		obj._unVal.pFunctionProto = pThis;
		readState.StoreInCache( marker, obj );

		ReadObject( pThis->_sourcename, pBuffer, readState );
		ReadObject( pThis->_name, pBuffer, readState );

		for ( int i = 0; i < pThis->_nliterals; ++i )
		{
			ReadObject( pThis->_literals[i], pBuffer, readState );
		}

		for ( int i = 0; i < pThis->_nparameters; ++i )
		{
			ReadObject( pThis->_parameters[i], pBuffer, readState );
		}

		for ( int i = 0; i < pThis->_noutervalues; ++i )
		{
			SQOuterVar &p = pThis->_outervalues[i];
			p._type = (SQOuterType)pBuffer->GetUnsignedInt();
			ReadObject( p._src, pBuffer, readState );
			ReadObject( p._name, pBuffer, readState );
		}

		for ( int i = 0; i < pThis->_nlocalvarinfos; ++i )
		{
			SQLocalVarInfo &p = pThis->_localvarinfos[i];
			ReadObject( p._name, pBuffer, readState );
			p._pos = pBuffer->GetUnsignedInt();
			p._start_op = pBuffer->GetUnsignedInt();
			p._end_op = pBuffer->GetUnsignedInt();
		}

		pBuffer->Get( pThis->_lineinfos, sizeof(SQLineInfo) * pThis->_nlineinfos );
		pBuffer->Get( pThis->_defaultparams, sizeof(SQInteger) * pThis->_ndefaultparams );
		pBuffer->Get( pThis->_instructions, sizeof(SQInstruction) * pThis->_ninstructions );

		for ( int i = 0; i < pThis->_nfunctions; ++i )
		{
			ReadObject( pThis->_functions[i], pBuffer, readState );
		}

		pThis->_stacksize = pBuffer->GetInt();
		pThis->_bgenerator = ( pBuffer->GetChar() != 0 );
		pThis->_varparams = pBuffer->GetInt();

		break;
	}
	case OT_CLASS:
	{
		int marker;
		if ( readState.CheckCache( &obj, pBuffer, &marker ) )
			break;

		ClassType type = (ClassType)pBuffer->GetChar();

		if ( type == VectorClassType )
		{
			obj._unVal.pClass = vectorClass_._unVal.pClass;
			readState.StoreInCache( marker, obj );
		}
		else if ( type == NativeClassType )
		{
			char psz[NATIVE_NAME_READBUF_SIZE] = "";
			pBuffer->GetString( psz, sizeof(psz) );

			SQObjectPtr key = SQString::Create( _ss(vm_), psz );
			SQObjectPtr val;

			if ( !vm_->_roottable._unVal.pTable->Get( key, val ) )
			{
				Warning( "SquirrelVM::ReadObject: failed to find native class '%s'\n", psz );
				obj._type = OT_NULL;
				obj._unVal.raw = 0;
				break;
			}

			Assert( val._type == OT_CLASS );

			obj._unVal.pClass = val._unVal.pClass;
			readState.StoreInCache( marker, obj );
		}
		else if ( type == ScriptClassType )
		{
			SQObjectPtr base, members;

			if ( pBuffer->GetChar() )
			{
				ReadObject( base, pBuffer, readState );
				Assert( base._type == OT_CLASS );
			}

			SQClass *pThis = SQClass::Create( _ss(vm_), base._unVal.pClass );
			obj._unVal.pClass = pThis;
			readState.StoreInCache( marker, obj );

			ReadObject( members, pBuffer, readState );

			Assert( members._type == OT_TABLE );

			// Replace with restored members
			pThis->_members->Release();
			pThis->_members = members._unVal.pTable;
			__ObjAddRef( pThis->_members );

			int count = pBuffer->GetInt();
			pThis->_defaultvalues.resize( count );

			for ( int i = 0; i < count; ++i )
			{
				ReadObject( pThis->_defaultvalues[i].val, pBuffer, readState );
			}

			count = pBuffer->GetInt();
			pThis->_methods.resize( count );

			for ( int i = 0; i < count; ++i )
			{
				ReadObject( pThis->_methods[i].val, pBuffer, readState );
			}

			int mask = pBuffer->GetInt();
			if ( mask )
			{
				for ( unsigned i = 0; i < SQMetaMethod::MT_LAST; ++i )
					if ( mask & (1 << i) )
						ReadObject( pThis->_metamethods[i], pBuffer, readState );
			}

			pThis->_constructoridx = pBuffer->GetInt();

			Assert( pThis->_constructoridx == -1 || (SQUnsignedInteger)pThis->_constructoridx < pThis->_methods.size() );
		}
		else
		{
			Assert( !"SquirrelVM::ReadObject: unknown class type" );
		}

		break;
	}
	case OT_INSTANCE:
	{
		int marker;
		if ( readState.CheckCache( &obj, pBuffer, &marker ) )
			break;

		SQInstance *pThis;

		SQObjectPtr pClass;
		ReadObject( pClass, pBuffer, readState );

		Assert( pClass._type == OT_CLASS && pClass._unVal.pClass );

		if ( _class(pClass) == _class(regexpClass_) )
		{
			SQObjectPtr key = SQString::Create( _ss(vm_), "pattern_" );
			SQObjectPtr val;
			ReadObject( val, pBuffer, readState );

			pThis = _class(pClass)->CreateInstance();
			pThis->Set( key, val );
			obj._unVal.pInstance = pThis;
			readState.StoreInCache( marker, obj );

			break;
		}

		SQUserPointer typetag = _class(pClass)->_typetag;

		// singleton
		if ( typetag && typetag != TYPETAG_VECTOR )
		{
			ScriptClassDesc_t *pDesc = (ScriptClassDesc_t *)typetag;
			if ( pDesc->m_pszDescription[0] == SCRIPT_SINGLETON[0] )
			{
				bool bFoundSingleton = false;

				SQObjectPtr key, val;
				FOREACH_SQTABLE( vm_->_roottable._unVal.pTable, key, val )
				{
					if ( sq_isinstance(val) && _instance(val)->_class == _class(pClass) )
					{
						pThis = val._unVal.pInstance;
						obj._unVal.pInstance = pThis;
						readState.StoreInCache( marker, obj );

						bFoundSingleton = true;
						break;
					}
				}

				if ( !bFoundSingleton )
				{
					Warning( "SquirrelVM::ReadObject: failed to find native singleton of '%s'\n", pDesc->m_pszClassname );
					Assert(0);
					obj._type = OT_NULL;
					obj._unVal.raw = 0;
				}

				break;
			}
		}

		pThis = SQInstance::Create( _ss(vm_), _class(pClass) );
		obj._unVal.pInstance = pThis;
		readState.StoreInCache( marker, obj );

		if ( typetag )
		{
			if ( typetag == TYPETAG_VECTOR )
			{
				float x = pBuffer->GetFloat();
				float y = pBuffer->GetFloat();
				float z = pBuffer->GetFloat();
				new ( pThis->_userpointer ) Vector( x, y, z );
			}
			else
			{
				ScriptClassDesc_t *pDesc = (ScriptClassDesc_t *)typetag;

				char pszInstanceName[NATIVE_NAME_READBUF_SIZE] = "";
				pBuffer->GetString( pszInstanceName, sizeof(pszInstanceName) );

				if ( pszInstanceName[0] )
				{
					bool refCounted = ( pBuffer->GetChar() != 0 );

					HSQOBJECT *hInstance = new HSQOBJECT;
					hInstance->_type = OT_INSTANCE;
					hInstance->_unVal.pInstance = pThis;

					Assert( pDesc->pHelper );

					void *pInstance = pDesc->pHelper->BindOnRead( (HSCRIPT)hInstance, NULL, pszInstanceName );
					if ( pInstance )
					{
						sq_addref( vm_, hInstance );
						new( pThis->_userpointer ) ClassInstanceData( pInstance, pDesc, pszInstanceName, refCounted );
						pThis->_hook = refCounted ? &destructor_stub : &destructor_stub_instance;
					}
					else
					{
						delete hInstance;
						pThis->_userpointer = NULL;
					}
				}
				else
				{
					pThis->_userpointer = NULL;
				}
			}

			break;
		}

		int count = pBuffer->GetInt();
#ifdef _DEBUG
		int size = pThis->_class->_defaultvalues.size();
		if ( count != size )
		{
			Warning("SquirrelVM::ReadObject: Non-matching class default value count!\n");
			Assert(0);
		}
#endif
		for ( int i = 0; i < count; ++i )
		{
			ReadObject( pThis->_values[i], pBuffer, readState );
		}

		break;
	}
	case OT_WEAKREF:
	{
		SQObject ref;
		ReadObject( ref, pBuffer, readState );
		if ( !ref._unVal.pRefCounted )
		{
			obj._type = OT_NULL;
			obj._unVal.raw = 0;
			break;
		}

		Assert( ISREFCOUNTED( ref._type ) );
		Assert( ref._unVal.pRefCounted->_uiRef > 0 );
		ref._unVal.pRefCounted->_uiRef--;

		SQWeakRef *pThis = ref._unVal.pRefCounted->GetWeakRef( ref._type );
		obj._unVal.pWeakRef = pThis;

		break;
	}
	case OT_OUTER:
	{
		int marker;
		if ( readState.CheckCache( &obj, pBuffer, &marker ) )
			break;

		SQOuter *pThis = SQOuter::Create( _ss(vm_), NULL );
		obj._unVal.pOuter = pThis;
		readState.StoreInCache( marker, obj );

		ReadObject( pThis->_value, pBuffer, readState );
		pThis->_valptr = &(pThis->_value);

		break;
	}
	case OT_THREAD:
	{
		int marker;
		if ( readState.CheckCache( &obj, pBuffer, &marker ) )
			break;

		SQVM *pThis = sq_newthread( vm_, pBuffer->GetInt() );
		pThis->_uiRef++;
		vm_->Pop();
		pThis->_uiRef--;
		obj._unVal.pThread = pThis;
		readState.StoreInCache( marker, obj );

		pThis->_callsstacksize = pBuffer->GetInt();

		if ( pThis->_callsstacksize )
		{
			while ( pThis->_callsstacksize >= pThis->_alloccallsstacksize )
				pThis->GrowCallStack();

			for ( int i = 0; i < pThis->_callsstacksize; i++ )
			{
				SQVM::CallInfo *ci = &pThis->_callsstack[i];

				SQObjectPtr closure;
				ReadObject( closure, pBuffer, readState );
				Assert( closure._type == OT_CLOSURE && closure._unVal.pClosure );

				if ( pBuffer->GetChar() )
				{
					SQObject generator;
					ReadObject( generator, pBuffer, readState );
					Assert( generator._type == OT_GENERATOR && generator._unVal.pGenerator );
					ci->_generator = generator._unVal.pGenerator;
				}
				else
				{
					ci->_generator = NULL;
				}

				int offset = pBuffer->GetInt();
				SQInstruction *start = closure._unVal.pClosure->_function->_instructions;
				SQInstruction *end = start + closure._unVal.pClosure->_function->_ninstructions;
				SQInstruction *pos = start + offset;

				Assert( pos >= start && pos < end );

				if ( pos < start || pos >= end )
					pos = start;

				ci->_ip = pos;
				ci->_literals = closure._unVal.pClosure->_function->_literals;
				ci->_closure = closure;
				ci->_etraps = pBuffer->GetInt();
				ci->_prevstkbase = pBuffer->GetInt();
				ci->_prevtop = pBuffer->GetInt();
				ci->_target = pBuffer->GetInt();
				ci->_ncalls = pBuffer->GetInt();
				ci->_root = pBuffer->GetChar();

				pThis->_etraps.resize( ci->_etraps );

				for ( int j = 0; j < ci->_etraps; j++ )
				{
					SQExceptionTrap &et = pThis->_etraps[j];
					et._stacksize = pBuffer->GetInt();
					et._stackbase = pBuffer->GetInt();
					et._ip = ci->_ip + pBuffer->GetInt();
					et._extarget = pBuffer->GetInt();
				}
			}

			int stackidx = pBuffer->GetInt();
			Assert( stackidx >= 0 && stackidx < pThis->_callsstacksize );
			pThis->ci = &pThis->_callsstack[ stackidx ];
		}

		pThis->_nnativecalls = pBuffer->GetInt();
		pThis->_nmetamethodscall = pBuffer->GetInt();

		pThis->_suspended = pBuffer->GetChar();
		pThis->_suspended_root = pBuffer->GetChar();
		pThis->_suspended_target = pBuffer->GetInt();
		pThis->_suspended_traps = pBuffer->GetInt();

		ReadVM( pThis, pBuffer, readState );

		break;
	}
	case OT_GENERATOR:
	{
		int marker;
		if ( readState.CheckCache( &obj, pBuffer, &marker ) )
			break;

		int state = pBuffer->GetChar();
		if ( state == SQGenerator::SQGeneratorState::eDead )
		{
			// SQGenerator doesn't allow creating dead generators, write null.
			// This is much simpler than creating a dummy closure only to tell the user it's dead.
			obj._type = OT_NULL;
			obj._unVal.raw = 0;
			break;
		}

		SQObjectPtr closure;
		ReadObject( closure, pBuffer, readState );
		Assert( closure._type == OT_CLOSURE && closure._unVal.pClosure );

		SQGenerator *pThis = SQGenerator::Create( _ss(vm_), closure._unVal.pClosure );
		obj._unVal.pGenerator = pThis;
		readState.StoreInCache( marker, obj );

		pThis->_state = (SQGenerator::SQGeneratorState)state;

		SQVM::CallInfo *ci = &pThis->_ci;

		if ( pBuffer->GetChar() )
		{
			SQObject generator;
			ReadObject( generator, pBuffer, readState );
			Assert( generator._type == OT_GENERATOR && generator._unVal.pGenerator );
			ci->_generator = generator._unVal.pGenerator;
		}
		else
		{
			ci->_generator = NULL;
		}

		int offset = pBuffer->GetInt();
		SQInstruction *start = closure._unVal.pClosure->_function->_instructions;
		SQInstruction *end = start + closure._unVal.pClosure->_function->_ninstructions;
		SQInstruction *pos = start + offset;

		Assert( pos >= start && pos < end );

		if ( pos < start || pos >= end )
			pos = start;

		ci->_ip = pos;
		ci->_literals = closure._unVal.pClosure->_function->_literals;
		ci->_closure = closure;
		ci->_etraps = pBuffer->GetInt();
		ci->_prevstkbase = pBuffer->GetInt();
		ci->_prevtop = pBuffer->GetInt();
		ci->_target = pBuffer->GetInt();
		ci->_ncalls = pBuffer->GetInt();
		ci->_root = pBuffer->GetChar();

		pThis->_etraps.resize( ci->_etraps );

		for ( int j = 0; j < ci->_etraps; j++ )
		{
			SQExceptionTrap &et = pThis->_etraps[j];
			et._stacksize = pBuffer->GetInt();
			et._stackbase = pBuffer->GetInt();
			et._ip = ci->_ip + pBuffer->GetInt();
			et._extarget = pBuffer->GetInt();
		}

		int stacksize = pBuffer->GetInt();
		pThis->_stack.resize( stacksize );
		for ( int i = 0; i < stacksize; ++i )
		{
			ReadObject( pThis->_stack[i], pBuffer, readState );
		}

		break;
	}
	case OT_USERDATA:
	case OT_USERPOINTER:
		break;
	default:
		AssertMsgAlways( 0, "SquirrelVM::ReadObject: serialisation error" );
	}

	Assert( !ISREFCOUNTED(obj._type) || obj._unVal.raw != 0xCC );

	pObj = obj;
}

void SquirrelVM::WriteVM( SQVM *pThis, CUtlBuffer *pBuffer, WriteStateMap &writeState )
{
	// Let others access the current VM inside ReadObject
	SQVM *pPrevVM = vm_;
	vm_ = pThis;

	Assert( pThis->_roottable._unVal.pTable->_uiRef > 0 );

	WriteObject( pThis->_roottable, pBuffer, writeState );

	Assert( pThis->_roottable._unVal.pTable->_uiRef > 0 );

	pBuffer->PutInt( pThis->_top );
	pBuffer->PutInt( pThis->_stackbase );
	pBuffer->PutInt( pThis->_stack.size() );
	for ( int i = 0; i < pThis->_top; ++i ) // write up to top
	{
		WriteObject( pThis->_stack[i], pBuffer, writeState );
	}

	vm_ = pPrevVM; // restore
}

void SquirrelVM::ReadVM( SQVM *pThis, CUtlBuffer *pBuffer, ReadStateMap &readState )
{
	// Let others access the current VM inside ReadObject
	SQVM *pPrevVM = vm_;
	vm_ = pThis;

	ReadObject( pThis->_roottable, pBuffer, readState );

	Assert( pThis->_roottable._unVal.pTable->_uiRef > 0 );

	pThis->_top = pBuffer->GetInt();
	pThis->_stackbase = pBuffer->GetInt();
	pThis->_stack.resize( pBuffer->GetInt() );
	for ( int i = 0; i < pThis->_top; ++i )
	{
		ReadObject( pThis->_stack[i], pBuffer, readState );
	}

	vm_ = pPrevVM; // restore
}

void SquirrelVM::WriteState( CUtlBuffer* pBuffer )
{
	// If the main VM can be suspended, WriteVM/ReadVM would need to include the code inside OT_THREAD r/w
	Assert( !vm_->ci );

	WriteStateMap writeState;
	WriteVM( vm_, pBuffer, writeState );
}

void SquirrelVM::ReadState( CUtlBuffer* pBuffer )
{
	ReadStateMap readState;
	ReadVM( vm_, pBuffer, readState );
}

void SquirrelVM::RemoveOrphanInstances()
{
	SquirrelSafeCheck safeCheck(vm_);
	// TODO: Is this the right thing to do here? It's not really removing orphan instances
	sq_collectgarbage(vm_);
}

void SquirrelVM::DumpState()
{
	SquirrelSafeCheck safeCheck(vm_);
	// TODO: Dump state
}

void SquirrelVM::SetOutputCallback(ScriptOutputFunc_t pFunc)
{
	SquirrelSafeCheck safeCheck(vm_);
	// TODO: Support output callbacks
}

void SquirrelVM::SetErrorCallback(ScriptErrorFunc_t pFunc)
{
	SquirrelSafeCheck safeCheck(vm_);
	// TODO: Support error callbacks
}

bool SquirrelVM::RaiseException(const char* pszExceptionText)
{
	SquirrelSafeCheck safeCheck(vm_);
	sq_pushstring(vm_, pszExceptionText, -1);
	sq_resetobject(&lastError_);
	sq_getstackobj(vm_, -1, &lastError_);
	sq_addref(vm_, &lastError_);
	sq_pop(vm_, 1);
	return true;
}


IScriptVM* makeSquirrelVM()
{
	return new SquirrelVM;
}
