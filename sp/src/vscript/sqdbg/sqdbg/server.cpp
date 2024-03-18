//-----------------------------------------------------------------------
//                       github.com/samisalreadytaken/sqdbg
//-----------------------------------------------------------------------
//
// Squirrel Debugger
//

#define SQDBG_SV_VER 2

#define ___CAT(a, b) a##b
#define __CAT(a, b) ___CAT(a,b)

#include "sqdbg.h"
#include "net.h"

#ifndef _WIN32
#include <math.h> // isfinite
#include <limits.h> // INT_MIN
#endif
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <float.h>
#include <new>

#ifndef SQDBG_DISABLE_PROFILER
#include <chrono>
#endif

// For Squirrel headers
#ifndef assert
#define assert Assert
#endif

#include <squirrel.h>
#include <sqstdaux.h>
#include <sqobject.h>
#include <sqstate.h>
#include <sqcompiler.h>
#include <sqvm.h>
#include <sqarray.h>
#include <sqtable.h>
#include <sqfuncproto.h>
#include <sqfuncstate.h>
#include <sqlexer.h>
#include <sqclosure.h>
#include <sqclass.h>
#include <sqstring.h>
#include <squserdata.h>


#ifdef _WIN32
void sqdbg_sleep( int ms )
{
	::Sleep( (DWORD)ms );
}
#else
#include <time.h>
void sqdbg_sleep( int ms )
{
	timespec t;
	t.tv_nsec = ms * 1000000;
	t.tv_sec = 0;
	nanosleep( &t, NULL );
}
#endif

#if defined(SQDBG_DEBUGGER_ECHO_OUTPUT) && defined(_DEBUG) && defined(_WIN32)
	#ifdef SQUNICODE
		#define _OutputDebugString(s) OutputDebugStringW(s)
	#else
		#define _OutputDebugString(s) OutputDebugStringA(s)
	#endif
	#define _OutputDebugStringA(s) OutputDebugStringA(s)

	void _OutputDebugStringFmt( const SQChar *fmt, ... )
	{
		SQChar buf[256];
		va_list va;
		va_start( va, fmt );
	#ifdef SQUNICODE
		int len = vswprintf( buf, sizeof(buf)/sizeof(SQChar), fmt, va );
	#else
		int len = vsnprintf( buf, sizeof(buf)/sizeof(SQChar), fmt, va );
	#endif
		va_end( va );

		if ( len < 0 || len > (int)(sizeof(buf)/sizeof(SQChar))-1 )
			len = (int)(sizeof(buf)/sizeof(SQChar))-1;

		_OutputDebugString( buf );
	}
#else
	#define _OutputDebugString(s) (void)0
	#define _OutputDebugStringA(s) (void)0
	#define _OutputDebugStringFmt(...) (void)0
#endif

#define _ArraySize(p) (sizeof((p))/sizeof(*(p)))

#define memzero(p) memset( (char*)(p), 0, sizeof(*(p)) )

#define ALIGN(v, a) (((v) + ((a)-1)) & ~((a)-1))

#ifndef _WIN32
#undef offsetof
#define offsetof(a,b) ((size_t)(&(((a*)0)->b)))
#endif

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#undef _SC
#ifdef SQUNICODE
	#define _SC(s) __CAT( L, s )
#else
	#define _SC(s) s
#endif

#if defined(SQUNICODE) && !defined(WCHAR_SIZE)
#ifdef _WIN32
	#define WCHAR_SIZE 2
#else
	#define WCHAR_SIZE 4
#endif
#endif

#ifndef scstrlen
#ifdef SQUNICODE
	#define scstrlen wcslen
#else
	#define scstrlen strlen
#endif
#endif

#ifndef scstrchr
#ifdef SQUNICODE
	#define scstrchr wcschr
#else
	#define scstrchr strchr
#endif
#endif

#ifndef scstricmp
#ifdef SQUNICODE
	#ifdef _WIN32
		#define scstricmp _wcsicmp
	#else
		#define scstricmp wcscmp
	#endif
#else
	#ifdef _WIN32
		#define scstricmp _stricmp
	#else
		#define scstricmp strcasecmp
	#endif
#endif
#endif

#ifndef scsprintf
#ifdef SQUNICODE
	#define scsprintf swprintf
#else
	#define scsprintf snprintf
#endif
#endif

#ifndef scvsprintf
#ifdef SQUNICODE
	#define scvsprintf vswprintf
#else
	#define scvsprintf vsnprintf
#endif
#endif

#ifndef sq_rsl
#define sq_rsl(l) ((l)*sizeof(SQChar))
#endif

#define SQStringFromSQChar(_pch) \
	( (SQString*)( (char*)(_pch) - (char*)offsetof( SQString, _val ) ) )

#ifndef SQUIRREL_VERSION_NUMBER
#error "SQUIRREL_VERSION_NUMBER is undefined"
#endif

#if SQUIRREL_VERSION_NUMBER >= 300
	#define _fp(func) (func)
	#define _outervalptr(outervar) (_outer((outervar))->_valptr)
	#define _nativenoutervalues(p) (p)->_noutervalues
	#define NATIVE_DEBUG_HOOK

	#ifdef NATIVE_DEBUG_HOOK
		#define SUPPORTS_RESTART_FRAME
		#define DEBUG_HOOK_CACHED_SQDBG
	#endif

	#ifdef NATIVE_DEBUG_HOOK
		typedef SQDEBUGHOOK _SQDEBUGHOOK;
	#else
		typedef SQFUNCTION _SQDEBUGHOOK;
	#endif

	#define CLOSURE_ENV_ISVALID(env) (env)
	#define CLOSURE_ENV_OBJ(env) ((env)->_obj)

	#if SQUIRREL_VERSION_NUMBER >= 310
		#define CLOSURE_ROOT
	#endif
#else
	#define _fp(func) _funcproto(func)
	#define _outervalptr(outervar) (&(outervar))
	#define _nativenoutervalues(p) (p)->_outervalues.size()
	typedef SQFUNCTION _SQDEBUGHOOK;

	#if SQUIRREL_VERSION_NUMBER >= 212
		#define CLOSURE_ENV_ISVALID(env) (sq_type(env) == OT_WEAKREF && _weakref(env))
		#define CLOSURE_ENV_OBJ(env) (_weakref(env)->_obj)
	#endif

	#undef _rawval
	#define _rawval(o) (SQUnsignedInteger)((o)._unVal.pRefCounted)

	#undef type
	#undef is_delegable
	#define is_delegable(t) (sq_type(t) & SQOBJECT_DELEGABLE)

	#ifndef SQUNICODE
		#undef scvsprintf
		#define scvsprintf vsnprintf
	#endif

	#undef scsprintf
	#ifdef SQUNICODE
		#define scsprintf swprintf
	#else
		#define scsprintf snprintf
	#endif
#endif

#include "vec.h"
#include "str.h"
#include "json.h"
#include "protocol.h"

#define SQ_FOREACH_OP( obj, key, val ) \
		{ \
			int _jump; \
			for ( SQObjectPtr _pos, key, val; \
					m_pCurVM->FOREACH_OP( obj, key, val, _pos, 0, 666, _jump ) && \
					_jump != 666; ) \
			{

#define SQ_FOREACH_END() } }

#define FOREACH_SQTABLE( pTable, key, val )\
	SQInteger _i = 0;\
	for ( SQObjectPtr pi = _i; (_i = pTable->Next( false, pi, key, val )) != -1; pi._unVal.nInteger = _i )

#ifndef SQDBG_EXCLUDE_DEFAULT_MEMFUNCTIONS
inline void *sqdbg_malloc( unsigned int size )
{
	extern void *sq_vm_malloc( SQUnsignedInteger size );
	return sq_vm_malloc( size );
}

inline void *sqdbg_realloc( void *p, unsigned int oldsize, unsigned int size )
{
	extern void *sq_vm_realloc( void *p, SQUnsignedInteger oldsize, SQUnsignedInteger size );
	return sq_vm_realloc( p, oldsize, size );
}

inline void sqdbg_free( void *p, unsigned int size )
{
	extern void sq_vm_free( void *p, SQUnsignedInteger size );
	sq_vm_free( p, size );
}
#endif

template < typename T >
void CopyString( const T &src, T *dst )
{
	Assert( src.ptr );

	if ( src.len )
	{
		if ( src.len != dst->len )
		{
			if ( !dst->len )
			{
				*(void**)&dst->ptr = sqdbg_malloc( ( src.len + 1 ) * sizeof(*dst->ptr) );
			}
			else
			{
				*(void**)&dst->ptr = sqdbg_realloc( dst->ptr,
						( dst->len + 1 ) * sizeof(*dst->ptr),
						( src.len + 1 ) * sizeof(*dst->ptr) );
			}

			dst->len = src.len;
			dst->ptr[dst->len] = 0;
		}
		else
		{
			Assert( dst->ptr );
			Assert( dst->ptr[dst->len] == 0 );
		}

		memcpy( dst->ptr, src.ptr, dst->len * sizeof(*dst->ptr) );
	}
	else
	{
		if ( dst->len )
		{
			sqdbg_free( dst->ptr, ( dst->len + 1 ) * sizeof(*dst->ptr) );
			dst->ptr = NULL;
			dst->len = 0;
		}
		else
		{
			Assert( !dst->ptr );
		}
	}
}

#ifdef SQUNICODE
void CopyString( const string_t &src, sqstring_t *dst )
{
	Assert( src.ptr );

	int srclen = SQUnicodeLength( src.ptr, src.len );

	if ( srclen )
	{
		if ( srclen != dst->len )
		{
			if ( !dst->len )
			{
				*(void**)&dst->ptr = sqdbg_malloc( ( srclen + 1 ) * sizeof(*dst->ptr) );
			}
			else
			{
				*(void**)&dst->ptr = sqdbg_realloc( dst->ptr,
						( dst->len + 1 ) * sizeof(*dst->ptr),
						( srclen + 1 ) * sizeof(*dst->ptr) );
			}

			dst->len = srclen;
			dst->ptr[dst->len] = 0;
		}
		else
		{
			Assert( dst->ptr );
			Assert( dst->ptr[dst->len] == 0 );
		}

		UTF8ToSQUnicode( dst->ptr, dst->len * sizeof(*dst->ptr), src.ptr, src.len );
	}
	else
	{
		if ( dst->len )
		{
			sqdbg_free( dst->ptr, ( dst->len + 1 ) * sizeof(*dst->ptr) );
			dst->ptr = NULL;
			dst->len = 0;
		}
		else
		{
			Assert( !dst->ptr );
		}
	}
}

void CopyString( const sqstring_t &src, string_t *dst )
{
	Assert( src.ptr );

	int srclen = UTF8Length( src.ptr, src.len );

	if ( srclen )
	{
		if ( srclen != dst->len )
		{
			if ( !dst->len )
			{
				*(void**)&dst->ptr = sqdbg_malloc( ( srclen + 1 ) * sizeof(*dst->ptr) );
			}
			else
			{
				*(void**)&dst->ptr = sqdbg_realloc( dst->ptr,
						( dst->len + 1 ) * sizeof(*dst->ptr),
						( srclen + 1 ) * sizeof(*dst->ptr) );
			}

			dst->len = srclen;
			dst->ptr[dst->len] = 0;
		}
		else
		{
			Assert( dst->ptr );
			Assert( dst->ptr[dst->len] == 0 );
		}

		SQUnicodeToUTF8( dst->ptr, dst->len * sizeof(*dst->ptr), src.ptr, src.len );
	}
	else
	{
		if ( dst->len )
		{
			sqdbg_free( dst->ptr, ( dst->len + 1 ) * sizeof(*dst->ptr) );
			dst->ptr = NULL;
			dst->len = 0;
		}
		else
		{
			Assert( !dst->ptr );
		}
	}
}
#endif

template < typename T >
void FreeString( T *dst )
{
	if ( dst->len )
	{
		sqdbg_free( dst->ptr, ( dst->len + 1 ) * sizeof(*dst->ptr) );
		dst->ptr = NULL;
		dst->len = 0;
	}
}

#ifdef SQUNICODE
inline SQString *CreateSQString( HSQUIRRELVM vm, const sqstring_t &str )
{
	return SQString::Create( vm->_sharedstate, str.ptr, str.len );
}

inline SQString *CreateSQString( SQSharedState *ss, const sqstring_t &str )
{
	return SQString::Create( ss, str.ptr, str.len );
}
#endif

inline SQString *CreateSQString( SQSharedState *ss, const string_t &str )
{
#ifdef SQUNICODE
	Assert( SQUnicodeLength( str.ptr, str.len ) <= 1024 );

	SQChar tmp[1024];
	int len = UTF8ToSQUnicode( tmp, sizeof(tmp), str.ptr, str.len );
	tmp[ min( len, (int)(sizeof(tmp)/sizeof(SQChar))-1 ) ] = 0;
	return SQString::Create( ss, tmp, len );
#else
	return SQString::Create( ss, str.ptr, str.len );
#endif
}

inline SQString *CreateSQString( HSQUIRRELVM vm, const string_t &str )
{
	return CreateSQString( vm->_sharedstate, str );
}

inline int GetFunctionDeclarationLine( SQFunctionProto *func )
{
	// not exactly function declaration line, but close enough
	Assert( func->_nlineinfos > 0 );
	return func->_lineinfos[0]._line;
}

inline bool IsFalse( const SQObject &obj )
{
#if SQUIRREL_VERSION_NUMBER >= 225 || defined(_SQ64) == defined(SQUSEDOUBLE)
	return ( ( sq_type(obj) & SQOBJECT_CANBEFALSE ) && _rawval(obj) == 0 );
#else
#if defined(_SQ64)
	return ( ( sq_type(obj) & SQOBJECT_CANBEFALSE ) &&
			( ( _rawval(obj) == 0 ) || ( sq_type(obj) == OT_FLOAT && _float(obj) == 0.0 ) ) );
#else // SQUSEDOUBLE
	return ( ( sq_type(obj) & SQOBJECT_CANBEFALSE ) &&
			( ( _rawval(obj) == 0 ) || ( sq_type(obj) == OT_INTEGER && _integer(obj) == 0 ) ) );
#endif
#endif
}

inline void SetBool( SQObjectPtr &obj, int state )
{
	obj.Null();
	obj._type = OT_BOOL;
	obj._unVal.nInteger = state;
}

template < typename C >
inline void StripFileName( C **ptr, int *len )
{
	for ( C *c = *ptr + *len - 1; c >= *ptr; c-- )
	{
		if ( *c == '/' || *c == '\\' )
		{
			c++;
			*len = *ptr + *len - c;
			*ptr = c;
			break;
		}
	}
}

#ifdef _DEBUG
class CStackCheck
{
private:
	HSQUIRRELVM vm;
	int top;

public:
	CStackCheck( HSQUIRRELVM v )
	{
		vm = v;
		top = vm->_top;
	}

	~CStackCheck()
	{
		Assert( vm->_top == top );
	}
};
#else
class CStackCheck
{
public:
	CStackCheck( HSQUIRRELVM ) {}
};
#endif


#ifndef SQDBG_DISABLE_PROFILER
class CProfiler
{
public:
	typedef double sample_t;
	typedef int hnode_t;
	static const hnode_t INVALID_HANDLE = -1;

	#pragma pack(push, 4)
	struct node_t
	{
		void *func;
		hnode_t caller;
		unsigned int calls;
		sample_t samples;
		sample_t sampleStart;
		hnode_t id;
	};
	#pragma pack(pop)

	struct nodetag_t
	{
		SQString *funcsrc;
		SQString *funcname;
	};

	struct group_t
	{
		unsigned int hits;
		unsigned int peakHit;
		sample_t samples;
		sample_t peak;
		sample_t sampleStart;
		SQString *tag;
	};

	static sample_t Sample()
	{
		std::chrono::duration< double, std::micro > d =
			std::chrono::high_resolution_clock::now().time_since_epoch();
		return d.count();
	}

private:
	enum
	{
		kProfDisabled = 0,
		kProfActive,
		kProfPaused,
	};

	int m_State;
	int m_nPauseLevel;
	vector< node_t > m_Nodes;
	vector< hnode_t > m_CallStack;
	vector< group_t > m_Groups;
	vector< int > m_GroupStack;
	vector< nodetag_t > m_NodeTags;

private:
	node_t *FindNode( hnode_t caller, void *func, hnode_t *handle )
	{
		int count = m_Nodes.size();

		// Start searching from caller,
		// new nodes are added after them
		int i = caller != INVALID_HANDLE ? caller : 0;

		for ( ; i < count; i++ )
		{
			node_t &node = m_Nodes[i];
			if ( node.func == func && node.caller == caller )
			{
				Assert( (hnode_t)i == node.id );
				*handle = (hnode_t)i;
				return &node;
			}
		}

		return NULL;
	}

	group_t *FindGroup( SQString *tag, int *idx )
	{
		for ( int i = m_Groups.size(); i--; )
		{
			group_t &group = m_Groups[i];
			if ( group.tag == tag )
			{
				*idx = i;
				return &group;
			}
		}

		return NULL;
	}

public:
	bool IsEnabled()
	{
		return m_State != kProfDisabled;
	}

	bool IsActive()
	{
		return m_State == kProfActive;
	}

	void Start( HSQUIRRELVM vm )
	{
		Assert( !IsEnabled() );

		if ( m_State != kProfDisabled )
			return;

		m_State = kProfActive;

		Assert( m_Nodes.capacity() == 0 );
		Assert( m_NodeTags.capacity() == 0 );
		Assert( m_CallStack.capacity() == 0 );
		Assert( m_GroupStack.capacity() == 0 );

		m_Nodes.reserve( max( vm->_alloccallsstacksize, 128 ) );
		m_NodeTags.reserve( m_Nodes.capacity() );
		m_CallStack.reserve( max( vm->_alloccallsstacksize, 8 ) );

		for ( int idx = 0;
				idx < vm->_callsstacksize - 1; // - sqdbg_prof_start
				idx++ )
		{
			const SQVM::CallInfo &ci = vm->_callsstack[idx];
			if ( sq_type(ci._closure) == OT_CLOSURE )
				CallBegin( _fp(_closure(ci._closure)->_function) );
		}
	}

	void Stop()
	{
		Assert( IsEnabled() );

		if ( m_State == kProfDisabled )
			return;

		m_State = kProfDisabled;
		m_nPauseLevel = 0;

		for ( int i = 0; i < m_NodeTags.size(); i++ )
		{
			nodetag_t *node = &m_NodeTags[i];
			__ObjRelease( node->funcsrc );
			__ObjRelease( node->funcname );
		}

		for ( int i = 0; i < m_Groups.size(); i++ )
		{
			group_t *group = &m_Groups[i];
			__ObjRelease( group->tag );
		}

		m_Nodes.purge();
		m_NodeTags.purge();
		m_CallStack.purge();
		m_Groups.purge();
		m_GroupStack.purge();
	}

	void GroupBegin( SQString *tag )
	{
		Assert( IsActive() );

		int idx;
		group_t *group = FindGroup( tag, &idx );

		if ( group )
		{
			m_GroupStack.append( idx );
			group->hits++;
			group->sampleStart = Sample();
			return;
		}

		m_GroupStack.append( m_Groups.size() );

		group = &m_Groups.append();
		memzero( group );

		group->tag = tag;
		__ObjAddRef( tag );

		group->hits = 1;
		group->sampleStart = Sample();
	}

	void GroupEnd()
	{
		Assert( IsActive() );

		sample_t sample = Sample();

		if ( !m_GroupStack.size() )
		{
			Assert(!"profiler group mismatch");
			return;
		}

		int idx = m_GroupStack.top();
		m_GroupStack.pop();

		group_t *group = &m_Groups[idx];

		// group was ended while profiler was paused
		Assert( group->sampleStart != 0.0 );

		sample_t dt = sample - group->sampleStart;
		group->samples += dt;

		if ( group->peak < dt )
		{
			group->peak = dt;
			group->peakHit = group->hits;
		}
	}

	void Pause()
	{
		Assert( IsEnabled() );

		sample_t sample = Sample();

		++m_nPauseLevel;

		if ( m_State != kProfPaused )
		{
			m_State = kProfPaused;

			if ( m_CallStack.size() )
			{
				hnode_t caller = m_CallStack.top();
				node_t *node = &m_Nodes[caller];
				node->samples += sample - node->sampleStart;
#ifdef _DEBUG
				node->sampleStart = 0.0;
#endif
			}

			if ( m_GroupStack.size() )
			{
				int idx = m_GroupStack.top();
				group_t *group = &m_Groups[idx];
				sample_t dt = sample - group->sampleStart;
				group->samples += dt;
#ifdef _DEBUG
				group->sampleStart = 0.0;
#endif

				if ( group->peak < dt )
				{
					group->peak = dt;
					group->peakHit = group->hits;
				}
			}
		}
	}

	void Resume()
	{
		Assert( IsEnabled() );

		if ( m_State == kProfPaused && --m_nPauseLevel == 0 )
		{
			m_State = kProfActive;

			sample_t sample = Sample();

			if ( m_CallStack.size() )
			{
				hnode_t caller = m_CallStack.top();
				node_t *node = &m_Nodes[caller];
				node->sampleStart = sample;
			}

			if ( m_GroupStack.size() )
			{
				int idx = m_GroupStack.top();
				group_t *group = &m_Groups[idx];
				group->sampleStart = sample;
			}
		}
	}

	void CallBegin( SQFunctionProto *func )
	{
		Assert( IsActive() );

		hnode_t caller = m_CallStack.size() ? m_CallStack.top() : INVALID_HANDLE;

		hnode_t id;
		node_t *node = FindNode( caller, func, &id );

		if ( node )
		{
			m_CallStack.append( id );
			node->calls++;
			node->sampleStart = Sample();
			return;
		}

		SQString *funcname = ( sq_type(func->_name) == OT_STRING ) ?
			_string(func->_name) :
			NULL;

		SQString *funcsrc = ( sq_type(func->_sourcename) == OT_STRING ) ?
			_string(func->_sourcename) :
			NULL;

		id = m_Nodes.size();
		node = &m_Nodes.append();
		nodetag_t *tag = &m_NodeTags.append();

		m_CallStack.append( id );

		node->id = id;
		node->func = func;
		node->caller = caller;
		node->calls = 1;
		node->samples = 0.0;

#if SQUIRREL_VERSION_NUMBER >= 300
		SQSharedState *ss = func->_sharedstate;
#else
		// null sourcename is not possible in the compiler
		// if it is, get SS from 'this': _string(func->_localvarinfos[0]._name)->_sharedstate;
		// if there are no parameters either,
		// the sharedstate needs to be accessible to the profiler
		Assert( funcsrc );
		SQSharedState *ss = funcsrc->_sharedstate;
#endif

		if ( funcname )
		{
			tag->funcname = funcname;
		}
		else
		{
			SQChar *tmp = ss->GetScratchPad( sq_rsl(FMT_PTR_LEN + 1) );
			int len = printhex( tmp, FMT_PTR_LEN, (SQUnsignedInteger)func );
			tmp[len] = 0;
			tag->funcname = CreateSQString( ss, { tmp, FMT_PTR_LEN } );
		}

		if ( funcsrc )
		{
			const int size = funcsrc->_len + 1 + FMT_UINT32_LEN + 1;
			SQChar *tmp = ss->GetScratchPad( sq_rsl(size) );
			int len = funcsrc->_len;
			memcpy( tmp, funcsrc->_val, sq_rsl(len) );

#ifdef SQDBG_SOURCENAME_HAS_PATH
			StripFileName( &tmp, &len );
#endif

			int line = GetFunctionDeclarationLine( func );
			if ( line )
			{
				tmp[len++] = ':';
				len += printint( tmp + len, FMT_UINT32_LEN, line );
			}

			tmp[len] = 0;
			tag->funcsrc = CreateSQString( ss, { tmp, len } );
		}
		else
		{
			tag->funcsrc = CreateSQString( ss, _SC("??") );
		}

		__ObjAddRef( tag->funcname );
		__ObjAddRef( tag->funcsrc );

		node->sampleStart = Sample();
	}

	void CallEnd()
	{
		Assert( IsActive() );

		sample_t sample = Sample();

		if ( !m_CallStack.size() )
		{
			// metamethod calls don't execute debug hook pre-221
#if SQUIRREL_VERSION_NUMBER >= 221
			Assert(!"profiler call mismatch");
#endif
			return;
		}

		hnode_t id = m_CallStack.top();
		m_CallStack.pop();

		node_t *node = &m_Nodes[id];

		// call ended while profiler was paused
		Assert( node->sampleStart != 0.0 );

		node->samples += sample - node->sampleStart;
	}

	void CallEndAll()
	{
		while ( m_CallStack.size() )
			CallEnd();
	}

#define CALLGRAPH_MAX_DEPTH 10
#define PROF_OUTPUT_HEADER "   %   total time  time/call      calls  func\n"
//                         "100.00  100.00 ms  100.00 ms 0x7fffffff  func\n"

#define PROF_GROUP_OUTPUT_START \
	"(sqdbg) prof | "

#define PROF_GROUP_OUTPUT_TEMPLATE \
	"(sqdbg) prof | : " \
	"total 100.00 ms, avg 100.00 ms, peak 100.00 ms(0x7fffffff), hits 0x7fffffff\n"

	// Returns character length
	int GetMaxOutputLen( SQString *tag, int type )
	{
		if ( tag )
		{
			return STRLEN(PROF_GROUP_OUTPUT_TEMPLATE) +
				ALIGN( tag->_len, 16 ) +
				1;
		}

		switch ( type )
		{
			// call graph
			case 0:
			{
				const int header = STRLEN(PROF_OUTPUT_HEADER);
				const int bufsize = header + m_Nodes.size() *
					( header - STRLEN("func") +
					  // depth[CALLGRAPH_MAX_DEPTH*3]func, src (addr)\n
					  CALLGRAPH_MAX_DEPTH * 3 +
					  /*func*/ 2 +
					  /*src*/ 1 +
					  2 + FMT_PTR_LEN +
					  1 ) +
					1;

				int strlen = 0;

				for ( int i = 0; i < m_NodeTags.size(); i++ )
				{
					nodetag_t *node = &m_NodeTags[i];
					strlen += (int)node->funcsrc->_len;
					strlen += (int)node->funcname->_len;
				}

				return bufsize + strlen;
			}
			// flat
			case 1:
			{
				const int header = STRLEN(PROF_OUTPUT_HEADER);
				const int bufsize = header + m_Nodes.size() *
					( header - STRLEN("func") +
					  // func, src (addr)\n
					  /*func*/ 2 +
					  /*src*/ 1 +
					  2 + FMT_PTR_LEN +
					  1 ) +
					1;

				int strlen = 0;

				for ( int i = 0; i < m_NodeTags.size(); i++ )
				{
					nodetag_t *node = &m_NodeTags[i];
					strlen += (int)node->funcsrc->_len;
					strlen += (int)node->funcname->_len;
				}

				return bufsize + strlen;
			}
			default:
			{
				return 0;
			}
		}
	}

	// Returns character length
	int Output( SQString *tag, int type, SQChar *buf, int size )
	{
		if ( tag )
		{
			int idx;
			const group_t *group = FindGroup( tag, &idx );

			if ( !group )
				return 0;

			const SQChar *bufstart = buf;

			int len = STRLEN(PROF_GROUP_OUTPUT_START);
			memcpy( buf, _SC(PROF_GROUP_OUTPUT_START), sq_rsl(len) );
			buf += len; size -= len;

			len = group->tag->_len;
			memcpy( buf, group->tag->_val, sq_rsl(len) );
			buf += len; size -= len;

			for ( int i = ALIGN( len, 16 ) - len; i-- > 0; )
			{
				*buf++ = ' ';
				size--;
			}

			*buf++ = ':'; size--;
			*buf++ = ' '; size--;

			len = STRLEN("total ");
			memcpy( buf, _SC("total "), sq_rsl(len) );
			buf += len; size -= len;

			sample_t time;

			if ( group->hits != 1 )
			{
				time = group->samples - group->peak;
			}
			else
			{
				time = group->samples;
			}

			PrintTime( time, buf, size );

			*buf++ = ','; size--;
			*buf++ = ' '; size--;

			len = STRLEN("avg ");
			memcpy( buf, _SC("avg "), sq_rsl(len) );
			buf += len; size -= len;

			if ( group->hits != 1 )
			{
				time = (group->samples - group->peak) / (sample_t)(group->hits - 1);
			}
			else
			{
				time = group->samples / (sample_t)group->hits;
			}

			PrintTime( time, buf, size );

			*buf++ = ','; size--;
			*buf++ = ' '; size--;

			len = STRLEN("peak ");
			memcpy( buf, _SC("peak "), sq_rsl(len) );
			buf += len; size -= len;

			PrintTime( group->peak, buf, size );

			*buf++ = '('; size--;
			len = printint( buf, size, group->peakHit );
			buf += len; size -= len;
			*buf++ = ')'; size--;

			*buf++ = ','; size--;
			*buf++ = ' '; size--;

			len = STRLEN("hits ");
			memcpy( buf, _SC("hits "), sq_rsl(len) );
			buf += len; size -= len;

			len = printint( buf, size, group->hits );
			buf += len; size -= len;

			*buf++ = '\n';
			*buf = 0;

			return (int)( buf - bufstart );
		}

		vector< node_t > nodes( m_Nodes );

		switch ( type )
		{
			// call graph
			case 0:
			{
				break;
			}
			// flat
			// merge all calls of identical functions
			case 1:
			{
				int c = nodes.size();

				for ( int i = 0; i < c; i++ )
				{
					node_t &node = nodes[i];
					node.caller = INVALID_HANDLE;

					for ( int j = i + 1; j < c; j++ )
					{
						node_t &nj = nodes[j];

						if ( nj.func == node.func )
						{
							node.samples += nj.samples;
							node.calls += nj.calls;

							nodes.remove(j);
							j--;
							c--;
						}
					}
				}

				break;
			}
			default:
			{
				return 0;
			}
		}

		int nodecount = nodes.size();
		sample_t totalSamples = 0.0;
		const SQChar *bufstart = buf;

		nodes.sort( _sort );

		for ( int i = 0; i < nodecount; i++ )
		{
			const node_t &node = nodes[i];

			// Only accumulate parent call times
			if ( node.caller == INVALID_HANDLE )
			{
				if ( !m_CallStack.size() || m_CallStack.top() != node.id )
				{
					totalSamples += node.samples;
				}
				// Within the call frame, accumulate children for a rough estimate
				// This will miss any time spent in the body
				// of the function excluding subcalls
				else
				{
					for ( int j = i; j < nodecount; j++ )
					{
						const node_t &nj = nodes[j];
						if ( nj.caller == node.id )
							totalSamples += nj.samples;
					}
				}
			}
		}

		int len = STRLEN(PROF_OUTPUT_HEADER);
		memcpy( buf, _SC(PROF_OUTPUT_HEADER), sq_rsl(len) );
		buf += len; size -= len;

		for ( int i = 0; i < nodecount; i++ )
		{
			const node_t &node = nodes[i];
			if ( node.caller != INVALID_HANDLE )
				break;

			Assert( size > 0 );
			DoPrint( nodecount, nodes._base.Base(), i, totalSamples, 0, buf, size );
		}

		*buf = 0;

		Assert( (int)scstrlen( bufstart ) == (int)( buf - bufstart ) );

		return (int)( buf - bufstart );
	}

private:
	void DoPrint( int nodecount, const node_t *nodes, int i,
			sample_t totalSamples, int depth, SQChar *&buf, int &size )
	{
		const node_t &node = nodes[i];

		sample_t frac = ( node.samples / totalSamples ) * 100.0;
		sample_t avg = node.samples / (sample_t)node.calls;

		int len;

		if ( node.samples && isfinite(frac) )
		{
			if ( frac > 100.0 )
				frac = 100.0;

			len = scsprintf( buf, size, _SC("%6.2f"), frac );
			buf += len; size -= len;
		}
		else
		{
			*buf++ = ' '; size--;
			*buf++ = ' '; size--;
			*buf++ = ' '; size--;

			len = STRLEN("N/A");
			memcpy( buf, _SC("N/A"), sq_rsl(len) );
			buf += len; size -= len;
		}

		*buf++ = ' '; size--;
		*buf++ = ' '; size--;

		PrintTime( node.samples, buf, size );

		*buf++ = ' '; size--;
		*buf++ = ' '; size--;

		PrintTime( avg, buf, size );

		*buf++ = ' '; size--;

		// right align
		len = FMT_UINT32_LEN - countdigits( node.calls );

		while ( len-- )
		{
			*buf++ = ' ';
			size--;
		}

		len = printint( buf, size, node.calls );
		buf += len; size -= len;

		*buf++ = ' '; size--;
		*buf++ = ' '; size--;

		if ( depth <= CALLGRAPH_MAX_DEPTH )
		{
			for ( int d = depth; d--; )
			{
				*buf++ = '|';
				*buf++ = ' ';
				*buf++ = ' ';
			}

			size -= depth * 3;
		}
		else
		{
			for ( int d = CALLGRAPH_MAX_DEPTH - 2; d--; )
			{
				*buf++ = '|'; size--;
				*buf++ = ' '; size--;
				*buf++ = ' '; size--;
			}

			buf--; size++;

			len = printint( buf, size, depth );
			buf += len; size -= len;

			for ( int d = ( ( len + 2 ) / 3 ) * 3; d-- > len; )
			{
				*buf++ = ' ';
				size--;
			}

			*buf++ = ' '; size--;

			len = ( ( len + 2 ) / 3 ) * 3;

			for ( int d = ( ( 2 ) * 3 - len ) / 3; d--; )
			{
				*buf++ = '|'; size--;
				*buf++ = ' '; size--;
				*buf++ = ' '; size--;
			}

			*(buf-1) = '.';
			*(buf-2) = '.';
		}

		const nodetag_t &tag = m_NodeTags[node.id];

		len = tag.funcname->_len;
		memcpy( buf, tag.funcname->_val, sq_rsl(len) );
		buf += len; size -= len;

		*buf++ = ','; size--;
		*buf++ = ' '; size--;

		len = tag.funcsrc->_len;
		memcpy( buf, tag.funcsrc->_val, sq_rsl(len) );
		buf += len; size -= len;

		if ( tag.funcname->_val[0] != '0' )
		{
			*buf++ = ' '; size--;
			*buf++ = '('; size--;
			len = printhex( buf, size, (SQUnsignedInteger)node.func );
			buf += len; size -= len;
			*buf++ = ')'; size--;
		}

		*buf++ = '\n'; size--;

		int more = 0;
		avg = 0.0;

		for ( int j = i + 1; j < nodecount; j++ )
		{
			const node_t &nj = nodes[j];
			if ( nj.caller == node.id )
			{
				// Prevent stack overflow
				// Limit should be at most 3 digits for the depth print
				if ( depth < 100 )
				{
					DoPrint( nodecount, nodes, j, totalSamples, depth+1, buf, size );
				}
				else
				{
					avg = ( more * avg + ( nj.samples / (sample_t)nj.calls ) ) / ( more + 1 );
					more++;
				}
			}
		}

		if ( more )
		{
			*buf++ = ' '; size--;
			*buf++ = ' '; size--;
			*buf++ = ' '; size--;

			len = STRLEN("N/A");
			memcpy( buf, _SC("N/A"), sq_rsl(len) );
			buf += len; size -= len;

			*buf++ = ' '; size--;
			*buf++ = ' '; size--;

			PrintTime( NAN, buf, size );

			*buf++ = ' '; size--;
			*buf++ = ' '; size--;

			PrintTime( avg, buf, size );

			*buf++ = ' '; size--;

			// right align
			len = FMT_UINT32_LEN - STRLEN("N/A");

			while ( len-- )
			{
				*buf++ = ' ';
				size--;
			}

			len = STRLEN("N/A");
			memcpy( buf, _SC("N/A"), sq_rsl(len) );
			buf += len; size -= len;

			*buf++ = ' '; size--;
			*buf++ = ' '; size--;

			for ( int d = CALLGRAPH_MAX_DEPTH; d--; )
			{
				*buf++ = '|';
				*buf++ = ' ';
				*buf++ = ' ';
			}

			size -= CALLGRAPH_MAX_DEPTH * 3;

			*(buf-1) = '.';
			*(buf-2) = '.';

			*buf++ = ' '; size--;

			len = printint( buf, size, more );
			buf += len; size -= len;

			len = STRLEN(" more");
			memcpy( buf, _SC(" more"), sq_rsl(len) );
			buf += len; size -= len;

			*buf++ = '\n'; size--;
		}
	}

	// Print time and its unit to 9 chars: "000.00 ms"
	static void PrintTime( sample_t us, SQChar *&buf, int &size )
	{
		if ( us < 1.0 )
		{
			if ( us > 0.0 )
			{
				int len = scsprintf( buf, size, _SC("%6.2f ns"), us * 1.e3 );
				buf += len;
				size -= len;
			}
			else
			{
				goto LNAN;
			}
		}
		else if ( us < 1.e3 )
		{
			int len = scsprintf( buf, size, _SC("%6.2f us"), us );
			buf += len;
			size -= len;
		}
		else if ( us < 1.e6 )
		{
			int len = scsprintf( buf, size, _SC("%6.2f ms"), us / 1.e3 );
			buf += len;
			size -= len;
		}
		else if ( us < 60.e6 * 15.0 ) // 900s
		{
			int len = scsprintf( buf, size, _SC("%6.2f  s"), us / 1.e6 );
			buf += len;
			size -= len;
		}
		else if ( us < 36.e8 ) // 60m
		{
			int len = scsprintf( buf, size, _SC("%6.2f  m"), us / 6.e7 );
			buf += len;
			size -= len;
		}
		else if ( us >= 36.e8 )
		{
			if ( us < 6048.e8 ) // 24h * 7
			{
				int len = scsprintf( buf, size, _SC("%6.2f  h"), us / 36.e8 );
				buf += len;
				size -= len;
			}
			else
			{
				if ( !isfinite(us) )
					goto LNAN;

				int len = 9 - STRLEN("> 1 w");
				while ( len-- )
				{
					*buf++ = ' ';
					size--;
				}

				*buf++ = '>'; size--;
				*buf++ = ' '; size--;
				*buf++ = '1'; size--;
				*buf++ = ' '; size--;
				*buf++ = 'w'; size--;
			}
		}
		else
		{
LNAN:
			int len = 9 - STRLEN("N/A");
			while ( len-- )
			{
				*buf++ = ' ';
				size--;
			}

			*buf++ = 'N'; size--;
			*buf++ = '/'; size--;
			*buf++ = 'A'; size--;
		}
	}

	static int _sort( const node_t *a, const node_t *b )
	{
		if ( a->caller != b->caller )
		{
			if ( a->caller == INVALID_HANDLE )
				return -1;

			if ( b->caller == INVALID_HANDLE )
				return 1;
		}

		if ( a->samples > b->samples )
			return -1;

		if ( b->samples > a->samples )
			return 1;

		return 0;
	}
};
#endif // !SQDBG_DISABLE_PROFILER

//
// Longest return value is 16 bytes including nul byte
//
inline conststring_t GetType( const SQObjectPtr &obj )
{
	switch ( _RAW_TYPE( sq_type(obj) ) )
	{
		case _RT_NULL: return "null";
		case _RT_INTEGER: return "integer";
		case _RT_FLOAT: return "float";
		case _RT_BOOL: return "bool";
		case _RT_STRING: return "string";
		case _RT_TABLE: return "table";
		case _RT_ARRAY: return "array";
		case _RT_GENERATOR: return "generator";
		case _RT_CLOSURE: return "function";
		case _RT_NATIVECLOSURE: return "native function";
		case _RT_USERDATA:
		case _RT_USERPOINTER: return "userdata";
		case _RT_THREAD: return "thread";
		case _RT_FUNCPROTO: return "function";
		case _RT_CLASS: return "class";
		case _RT_INSTANCE: return "instance";
		case _RT_WEAKREF: return "weakref";
#if SQUIRREL_VERSION_NUMBER >= 300
		case _RT_OUTER: return "outer";
#endif
		default: Assert(!"unknown type"); return "unknown";
	}
}

#if SQUIRREL_VERSION_NUMBER >= 300
conststring_t const g_InstructionName[ _OP_CLOSE + 1 ]=
{
	"LINE",
	"LOAD",
	"LOADINT",
	"LOADFLOAT",
	"DLOAD",
	"TAILCALL",
	"CALL",
	"PREPCALL",
	"PREPCALLK",
	"GETK",
	"MOVE",
	"NEWSLOT",
	"DELETE",
	"SET",
	"GET",
	"EQ",
	"NE",
	"ADD",
	"SUB",
	"MUL",
	"DIV",
	"MOD",
	"BITW",
	"RETURN",
	"LOADNULLS",
	"LOADROOT",
	"LOADBOOL",
	"DMOVE",
	"JMP",
	"JCMP",
	"JZ",
	"SETOUTER",
	"GETOUTER",
	"NEWOBJ",
	"APPENDARRAY",
	"COMPARITH",
	"INC",
	"INCL",
	"PINC",
	"PINCL",
	"CMP",
	"EXISTS",
	"INSTANCEOF",
	"AND",
	"OR",
	"NEG",
	"NOT",
	"BWNOT",
	"CLOSURE",
	"YIELD",
	"RESUME",
	"FOREACH",
	"POSTFOREACH",
	"CLONE",
	"TYPEOF",
	"PUSHTRAP",
	"POPTRAP",
	"THROW",
	"NEWSLOTA",
	"GETBASE",
	"CLOSE",
};
#elif SQUIRREL_VERSION_NUMBER >= 212
conststring_t const g_InstructionName[ _OP_NEWSLOTA + 1 ]=
{
	"LINE",
	"LOAD",
	"LOADINT",
	"LOADFLOAT",
	"DLOAD",
	"TAILCALL",
	"CALL",
	"PREPCALL",
	"PREPCALLK",
	"GETK",
	"MOVE",
	"NEWSLOT",
	"DELETE",
	"SET",
	"GET",
	"EQ",
	"NE",
	"ARITH",
	"BITW",
	"RETURN",
	"LOADNULLS",
	"LOADROOTTABLE",
	"LOADBOOL",
	"DMOVE",
	"JMP",
	"JNZ",
	"JZ",
	"LOADFREEVAR",
	"VARGC",
	"GETVARGV",
	"NEWTABLE",
	"NEWARRAY",
	"APPENDARRAY",
	"GETPARENT",
	"COMPARITH",
	"COMPARITHL",
	"INC",
	"INCL",
	"PINC",
	"PINCL",
	"CMP",
	"EXISTS",
	"INSTANCEOF",
	"AND",
	"OR",
	"NEG",
	"NOT",
	"BWNOT",
	"CLOSURE",
	"YIELD",
	"RESUME",
	"FOREACH",
	"POSTFOREACH",
	"DELEGATE",
	"CLONE",
	"TYPEOF",
	"PUSHTRAP",
	"POPTRAP",
	"THROW",
	"CLASS",
	"NEWSLOTA",
};
#endif

conststring_t const g_MetaMethodName[ MT_LAST ] =
{
	"_add",
	"_sub",
	"_mul",
	"_div",
	"_unm",
	"_modulo",
	"_set",
	"_get",
	"_typeof",
	"_nexti",
	"_cmp",
	"_call",
	"_cloned",
	"_newslot",
	"_delslot",
#if SQUIRREL_VERSION_NUMBER >= 210
	"_tostring",
	"_newmember",
	"_inherited",
#endif
};

inline bool SQTable_Get( SQTable *table, const string_t &key, SQObjectPtr &val )
{
#if SQUIRREL_VERSION_NUMBER >= 300
	#ifdef SQUNICODE
		Assert( SQUnicodeLength( key.ptr, key.len ) < 256 );

		SQChar tmp[256];
		int len = UTF8ToSQUnicode( tmp, sizeof(tmp), key.ptr, key.len );
		tmp[ min( len, (int)(sizeof(tmp)/sizeof(SQChar))-1 ) ] = 0;
		return table->GetStr( tmp, len, val );
	#else
		// SQTable::GetStr ignores string length with strcmp, need a terminated string here.
		// If the string is not terminated, then it's a writable, non-const string
		// and its source is the network buffer
		char z = key.ptr[key.len];

		if ( z != 0 )
			key.ptr[key.len] = 0;

		bool r = table->GetStr( key.ptr, key.len, val );

		if ( z != 0 )
			key.ptr[key.len] = z;

		return r;
	#endif
#else
	SQObjectPtr str = CreateSQString( table->_sharedstate, key );
	return table->Get( str, val );
#endif
}

#ifdef SQUNICODE
inline bool SQTable_Get( SQTable *table, const sqstring_t &key, SQObjectPtr &val )
{
#if SQUIRREL_VERSION_NUMBER >= 300
	Assert( key.ptr[key.len] == 0 );
	return table->GetStr( key.ptr, key.len, val );
#else
	SQObjectPtr str = SQString::Create( table->_sharedstate, key.ptr, key.len );
	return table->Get( str, val );
#endif
}
#endif


#define KW_CALLFRAME "\xFF\xFF\xF0"
#define KW_DELEGATE "\xFF\xFF\xF1"
#ifdef CLOSURE_ROOT
#define KW_ROOT "\xFF\xFF\xF2"
#endif
#define KW_THIS "__this"
#define KW_VARGV "__vargv"
#define KW_VARGC "__vargc"

#define IS_INTERNAL_TAG( str ) ((str)[0] == '$')
#define INTERNAL_TAG( name ) "$" name

#define ANONYMOUS_FUNCTION_BREAKPOINT_NAME "()"

#define INVALID_ID -1

typedef enum
{
	kFS_None			= 0x0000,
	kFS_Hexadecimal		= 0x0001,
	kFS_Binary			= 0x0002,
	kFS_Decimal			= 0x0004,
	kFS_Float			= 0x0008,
	kFS_FloatE			= 0x0010,
	kFS_FloatG			= 0x0020,
	kFS_Octal			= 0x0040,
	kFS_Character		= 0x0080,
	kFS_NoQuote			= 0x0100,
	kFS_Uppercase		= 0x0200,
	kFS_Padding			= 0x0400,
	kFS_NoPrefix		= 0x0800,
	kFS_KeyVal			= 0x1000,
	kFS_NoAddr			= 0x2000,
	kFS_Lock			= 0x8000,
} VARSPEC;

struct breakreason_t
{
	typedef enum
	{
		None = 0,
		Step = 1,
		Breakpoint,
		Exception,
		Pause,
		Restart,
		Goto,
		FunctionBreakpoint,
		DataBreakpoint,
	} EBreakReason;

	EBreakReason reason;
	string_t text;
	int id;

	breakreason_t( EBreakReason r = None, const string_t &t = { 0, 0 }, int i = 0 ) :
		reason(r),
		text(t),
		id(i)
	{}
};

struct breakpoint_t
{
	int line;
	sqstring_t src;

	sqstring_t funcsrc;
	int funcline;

	SQObjectPtr conditionFn;
	SQObjectPtr conditionEnv;

	int hitsTarget;
	int hits;
	string_t logMessage;

	int id;
};

typedef enum
{
	VARREF_OBJ = 0,
	VARREF_SCOPE_LOCALS,
	VARREF_SCOPE_OUTERS,
	VARREF_STACK,
	VARREF_INSTRUCTIONS,
	VARREF_OUTERS,
	VARREF_LITERALS,
	VARREF_METAMETHODS,
	VARREF_ATTRIBUTES,
	VARREF_CALLSTACK,
	VARREF_MAX
} EVARREF;

inline bool IsScopeRef( EVARREF type )
{
	Assert( type >= 0 && type < VARREF_MAX );
	return ( type == VARREF_SCOPE_LOCALS ||
			type == VARREF_SCOPE_OUTERS ||
			type == VARREF_STACK );
}

inline bool IsObjectRef( EVARREF type )
{
	return !IsScopeRef( type );
}

struct varref_t
{
	EVARREF type;
	union
	{
		struct
		{
			SQWeakRef *thread;
			int frame;
		} scope;

		struct
		{
			union
			{
				SQWeakRef *weakref;
				SQObject obj;
			};

			bool isWeak;
			bool isStrong; // temporary strong reference for inspecting vars
			bool hasNonStringMembers;
		} obj;
	};

	int id;

	HSQUIRRELVM GetThread() const
	{
		Assert( IsScopeRef( type ) );
		Assert( scope.thread );
		Assert( sq_type(scope.thread->_obj) == OT_THREAD );
		return _thread(scope.thread->_obj);
	}

	const SQObject &GetVar() const
	{
		Assert( IsObjectRef( type ) );
		Assert( !obj.isWeak || ( obj.isWeak && obj.weakref ) );
		Assert( !obj.isStrong || ( obj.isStrong && ISREFCOUNTED( sq_type(obj.obj) ) ) );
		Assert( ( !obj.isWeak && !obj.isStrong ) || ( obj.isWeak != obj.isStrong ) );
		return obj.isWeak ? obj.weakref->_obj : obj.obj;
	}
};

struct watch_t
{
	string_t expression;
	SQWeakRef *thread;
	int frame;
};

struct classdef_t
{
	SQClass *base;
	string_t name;
	SQObjectPtr value;
	SQObjectPtr metamembers;
	SQObjectPtr custommembers;
};

struct frameid_t
{
	int frame;
	int threadId;
};

struct script_t
{
	char *sourceptr;
	char *scriptptr;
	int sourcelen;
	int scriptlen;
};

struct objref_t
{
	typedef enum
	{
		INVALID = 0,
		TABLE,
		INSTANCE,
		CLASS,
		ARRAY,
		DELEGABLE_META,
		CUSTOMMEMBER,
		INT,
		PTR = 0x1000,
		READONLY = 0x2000,
	} EOBJREF;

	EOBJREF type;

	union
	{
		SQObjectPtr *ptr;
		int val; // vargc
	};

	// Let src hold strong ref for compiler assignment expressions such as ( a.b = a = null )
	SQObjectPtr src;
	// Let key hold strong ref for compiler newslot target
	SQObjectPtr key;
};

struct datawatch_t
{
	SQWeakRef *container;
	objref_t obj;
	string_t name;

	// Hold strong ref to be able to print its value when it's a ref counted object
	// if the old value is to be released, it will be done in CheckDataBreakpoints
	SQObjectPtr oldvalue;

	int hitsTarget;
	int hits;

	SQObjectPtr condition;
	unsigned int condtype;

	int id;
};

struct returnvalue_t
{
	SQObjectPtr value;
	SQString *funcname;
};

struct cachedinstr_t
{
	SQInstruction *ip;
	SQInstruction instr;
};

#ifndef SQDBG_DISABLE_PROFILER
struct threadprofiler_t
{
	SQWeakRef *thread;
	CProfiler prof;
};
#endif

typedef enum
{
	ThreadState_Running,
	ThreadState_Suspended,
	ThreadState_NextStatement,
	ThreadState_StepOver,
	ThreadState_StepIn,
	ThreadState_StepOut,
	ThreadState_StepOverInstruction,
	ThreadState_StepInInstruction,
	ThreadState_StepOutInstruction,
	ThreadState_SuspendNow,
} EThreadState;

//
// Squirrel doesn't read files, it usually keeps file names passed in from host programs.
// DAP returns file path on breakpoints; try to construct file paths from these partial
// file names. This will not work for multiple files with identical names and for files
// where breakpoints were not set.
//
class CFilePathMap
{
public:
	struct pair_t
	{
		string_t name;
		string_t path;
	};

	vector< pair_t > map;

	~CFilePathMap()
	{
		Clear();
	}

	void Add( const string_t &name, const string_t &path )
	{
		for ( int i = 0; i < map.size(); i++ )
		{
			pair_t &v = map[i];
			if ( v.name.IsEqualTo( name ) )
			{
				if ( !v.path.IsEqualTo( path ) )
				{
					CopyString( path, &v.path );
				}

				return;
			}
		}

		pair_t &v = map.append();
		CopyString( name, &v.name );
		CopyString( path, &v.path );
	}

	pair_t *Get( const string_t &name )
	{
		for ( int i = 0; i < map.size(); i++ )
		{
			pair_t &v = map[i];
			if ( v.name.IsEqualTo( name ) )
				return &v;
		}

		return NULL;
	}

	void Clear()
	{
		for ( int i = 0; i < map.size(); i++ )
		{
			pair_t &v = map[i];
			FreeString( &v.name );
			FreeString( &v.path );
		}

		map.purge();
	}
};

#define Print(...) \
	{ \
		_OutputDebugStringFmt( __VA_ARGS__ ); \
		m_Print( m_pCurVM, __VA_ARGS__ ); \
	}

#define PrintError(...) \
	{ \
		_OutputDebugStringFmt( __VA_ARGS__ ); \
		m_PrintError( m_pCurVM, __VA_ARGS__ ); \
	}

struct SQDebugServer
{
private:
	EThreadState m_State;
	int m_nStateCalls;
	int m_nCalls;
	int m_Sequence;

	HSQUIRRELVM m_pRootVM;
	HSQUIRRELVM m_pCurVM;
	HSQUIRRELVM m_pStateVM;

	SQPRINTFUNCTION m_Print;
	SQPRINTFUNCTION m_PrintError;
	SQObjectPtr m_ErrorHandler;

#ifndef SQDBG_DISABLE_PROFILER
	CProfiler *m_pProfiler;
	vector< threadprofiler_t > m_Profilers;
	bool m_bProfilerEnabled;
#endif

	bool m_bBreakOnExceptions;
	bool m_bDebugHookGuard;
	bool m_bInREPL;
	bool m_bDebugHookGuardAlways;
#if SQUIRREL_VERSION_NUMBER < 300
	bool m_bInDebugHook;
#endif
	bool m_bExceptionPause;

	HSQUIRRELVM m_pPausedThread;

	// Ignore debug hook calls from debugger executed scripts
	class CCallGuard
	{
		SQDebugServer *dbg;
		HSQUIRRELVM vm;
		SQObjectPtr temp_reg;

	public:
		CCallGuard( SQDebugServer *p, HSQUIRRELVM v ) :
			dbg( p ),
			vm( v ),
			temp_reg( v->temp_reg )
		{
			if ( dbg->m_bDebugHookGuardAlways || !dbg->m_bInREPL )
			{
				dbg->m_bDebugHookGuard = true;
			}
		}

		~CCallGuard()
		{
			dbg->m_bDebugHookGuard = false;
			vm->temp_reg = temp_reg;
		}
	};

private:
	SQObjectPtr m_sqfnGet;
	SQObjectPtr m_sqfnSet;
	SQObjectPtr m_sqfnNewSlot;
	SQObjectPtr m_EnvGetVal;

	SQObjectPtr m_sqstrCallFrame;
	SQObjectPtr m_sqstrDelegate;
#ifdef CLOSURE_ROOT
	SQObjectPtr m_sqstrRoot;
#endif

	int m_nBreakpointIndex;
	int m_nVarRefIndex;
#ifndef SQDBG_DISABLE_COMPILER
	int m_nClientColumnOffset;
#endif

	unsigned int m_iYieldValues;

	vector< cachedinstr_t > m_CachedInstructions;
	vector< returnvalue_t > m_ReturnValues;
	vector< varref_t > m_Vars;
	vector< watch_t > m_LockedWatches;
	vector< breakpoint_t > m_Breakpoints;
	vector< datawatch_t > m_DataWatches;
	vector< classdef_t > m_ClassDefinitions;
	vector< SQWeakRef* > m_Threads;
	vector< frameid_t > m_FrameIDs;

	CFilePathMap m_FilePathMap;
	vector< script_t > m_Scripts;

	CDefaultAllocator< char > m_Scratch;

	CServerSocket m_Server;

public:
	char *ScratchPad( int size );
	char *ScratchPad() { return m_Scratch.Base(); }

public:
	SQDebugServer();

	void Attach( HSQUIRRELVM vm );
	void SetErrorHandler( bool state );
	void DoSetDebugHook( HSQUIRRELVM vm, _SQDEBUGHOOK fn );
	void SetDebugHook( _SQDEBUGHOOK fn );
	bool ListenSocket( unsigned short port );
	void Shutdown();
	void DisconnectClient();
	void OnClientConnected( const char *addr );
	void Frame();

	bool IsClientConnected() { return m_Server.IsClientConnected(); }

private:
	void PrintLastServerMessage()
	{
#ifdef SQUNICODE
		int len = 0;
		SQChar wcs[256];

#define _bs (int)( sizeof(wcs) - len * sizeof(SQChar) )
#define _bl (int)( sizeof(wcs) / sizeof(SQChar) )

		len = UTF8ToSQUnicode( wcs, _bs, m_Server.m_pszLastMsgFmt, strlen(m_Server.m_pszLastMsgFmt) );

		if ( m_Server.m_pszLastMsg && len < _bl - 2 )
		{
			wcs[len++] = ' ';
			wcs[len++] = '(';

			len += UTF8ToSQUnicode( wcs + len, _bs, m_Server.m_pszLastMsg, strlen(m_Server.m_pszLastMsg) );

			if ( len < _bl - 2 )
			{
				wcs[len++] = ')';
				wcs[len++] = '\n';
			}
		}
		else if ( len < _bl - 1 )
		{
			wcs[len++] = '\n';
		}

		wcs[ min( len, _bl - 1 ) ] = 0;

#undef _bs
#undef _bl

		PrintError( wcs );
#else
		stringbuf_t< 256 > buf;
		buf.Puts( { m_Server.m_pszLastMsgFmt, (int)strlen(m_Server.m_pszLastMsgFmt) } );

		if ( m_Server.m_pszLastMsg )
		{
			buf.Put(' ');
			buf.Put('(');
			buf.Puts( { m_Server.m_pszLastMsg, (int)strlen(m_Server.m_pszLastMsg) } );
			buf.Put(')');
		}

		buf.Put('\n');
		buf.Term();

		PrintError( buf.ptr );
#endif

		m_Server.m_pszLastMsg = NULL;
	}

	void Recv()
	{
		if ( !m_Server.Recv() )
		{
			PrintLastServerMessage();
			DisconnectClient();
		}
	}

	void Parse()
	{
		if ( !m_Server.Parse< DAP_ReadHeader >() )
		{
			PrintLastServerMessage();
			DisconnectClient();
		}
	}

	void Send( const char *buf, int len )
	{
		if ( m_Server.IsClientConnected() && !m_Server.Send( buf, len ) )
		{
			PrintLastServerMessage();
			DisconnectClient();
		}
	}

	void OnMessageReceived( char *ptr, int len );

	void ProcessRequest( const json_table_t &table, int seq );
	void ProcessResponse( const json_table_t &table, int seq );
	void ProcessEvent( const json_table_t &table );

	void OnRequest_Initialize( const json_table_t &arguments, int seq );
	void OnRequest_SetBreakpoints( const json_table_t &arguments, int seq );
	void OnRequest_SetFunctionBreakpoints( const json_table_t &arguments, int seq );
	void OnRequest_SetExceptionBreakpoints( const json_table_t &arguments, int seq );
	void OnRequest_SetDataBreakpoints( const json_table_t &arguments, int seq );
	void OnRequest_DataBreakpointInfo( const json_table_t &arguments, int seq );
#ifndef SQDBG_DISABLE_COMPILER
	void OnRequest_Completions( const json_table_t &arguments, int seq );
#endif
	void OnRequest_Evaluate( const json_table_t &arguments, int seq );
	void OnRequest_Scopes( const json_table_t &arguments, int seq );
	void OnRequest_Threads( int seq );
	void OnRequest_StackTrace( const json_table_t &arguments, int seq );
	void OnRequest_Variables( const json_table_t &arguments, int seq );
	void OnRequest_SetVariable( const json_table_t &arguments, int seq );
	void OnRequest_SetExpression( const json_table_t &arguments, int seq );
	void OnRequest_Disassemble( const json_table_t &arguments, int seq );
#ifdef SUPPORTS_RESTART_FRAME
	void OnRequest_RestartFrame( const json_table_t &arguments, int seq );
#endif
	void OnRequest_GotoTargets( const json_table_t &arguments, int seq );
	void OnRequest_Goto( const json_table_t &arguments, int seq );
	void OnRequest_Next( const json_table_t &arguments, int seq );
	void OnRequest_StepIn( const json_table_t &arguments, int seq );
	void OnRequest_StepOut( const json_table_t &arguments, int seq );

private:
	int AddBreakpoint( int line, const string_t &src,
			const string_t &condition, int hitsTarget, const string_t &logMessage );
	int AddFunctionBreakpoint( const string_t &func, const string_t &funcsrc, int line,
			const string_t &condition, int hitsTarget, const string_t &logMessage );

	breakpoint_t *GetBreakpoint( int line, const sqstring_t &src );
	breakpoint_t *GetFunctionBreakpoint( const sqstring_t &func, const sqstring_t &funcsrc, int line );

	void FreeBreakpoint( breakpoint_t &bp );
	static inline bool HasCondition( const breakpoint_t *bp );
	bool CheckBreakpointCondition( breakpoint_t *bp, HSQUIRRELVM vm, const SQVM::CallInfo *ci );

	int EvalAndWriteExpr( HSQUIRRELVM vm, const SQVM::CallInfo *ci, string_t &expression, char *buf, int size );
	void TracePoint( breakpoint_t *bp, HSQUIRRELVM vm, int frame );

	enum DataBreakpointConditionType
	{
		DBC_NONE = 0,
		DBC_EQ = 0x01,
		DBC_NE = 0x02,
		DBC_G = 0x04,
		DBC_GE = DBC_G | DBC_EQ,
		DBC_L = 0x08,
		DBC_LE = DBC_L | DBC_EQ,
		DBC_BWA = 0x10,
		DBC_BWAZ = 0x20,
		DBC_BWAEQ = 0x40,
	};

	int CompareObj( const SQObjectPtr &lhs, const SQObjectPtr &rhs );

	bool CompileDataBreakpointCondition( string_t condition, SQObjectPtr &out, unsigned int &type );
	int AddDataBreakpoint( const string_t &dataId, const string_t &condition, int hitsTarget );
	void CheckDataBreakpoints( HSQUIRRELVM vm );
	void FreeDataWatch( datawatch_t &dw );

	inline void RemoveAllBreakpoints();
	inline void RemoveBreakpoints( const string_t &source );
	inline void RemoveFunctionBreakpoints();
	inline void RemoveDataBreakpoints();

	bool InstructionStep( HSQUIRRELVM vm, SQVM::CallInfo *ci, int instroffset = 0 );
	bool Step( HSQUIRRELVM vm, SQVM::CallInfo *ci );

	void CacheInstruction( SQInstruction *instr );
	void ClearCachedInstructions();
	void RestoreCachedInstructions();
	void UndoRestoreCachedInstructions();

	void SetSource( json_table_t &source, SQString *sourcename );

public:
	static inline bool IsJumpOp( const SQInstruction *instr );
	static inline int GetJumpCount( const SQInstruction *instr );
	static int DeduceJumpCount( SQInstruction *instr );

private:
	static inline bool IsValidStackFrame( HSQUIRRELVM vm, int id );
	static inline int GetStackBase( HSQUIRRELVM vm, const SQVM::CallInfo *ci );

	static HSQUIRRELVM GetThread( SQWeakRef *wr )
	{
		Assert( sq_type(wr->_obj) == OT_THREAD );
		return _thread(wr->_obj);
	}

	static SQWeakRef *GetWeakRef( HSQUIRRELVM vm ) { return GetWeakRef( vm, OT_THREAD ); }
	static SQWeakRef *GetWeakRef( SQRefCounted *obj, SQObjectType type )
	{
		return obj->GetWeakRef( type );
	}

	string_t GetValue( const SQObject &obj, int flags = 0 );

	string_t GetValue( SQInteger val, int flags = 0 )
	{
		SQObject obj;
		obj._type = OT_INTEGER;
		obj._unVal.nInteger = val;
		return GetValue( obj, flags );
	}

	static SQObject ToSQObject( SQClass *val )
	{
		SQObject obj;
		obj._type = OT_CLASS;
		obj._unVal.pClass = val;
		return obj;
	}

	static SQObject ToSQObject( SQTable *val )
	{
		SQObject obj;
		obj._type = OT_TABLE;
		obj._unVal.pTable = val;
		return obj;
	}

	void JSONSetString( json_table_t &elem, const string_t &key, const SQObject &obj, int flags = 0 );

	void DescribeInstruction( SQInstruction *instr, SQFunctionProto *func, stringbufext_t &buf );

#ifndef SQDBG_DISABLE_COMPILER
public:
	class CCompiler;
	enum ECompileReturnCode
	{
		CompileReturnCode_Success,
		// Lookup failed
		CompileReturnCode_DoesNotExist,
		// String/number parsing failed
		CompileReturnCode_SyntaxError,
		CompileReturnCode_Fallback,
		CompileReturnCode_NoValue,
		// Unrecognised token or token sequences
		CompileReturnCode_Unsupported,
		// Valid but too many parameters in a function call
		CompileReturnCode_CallBufferFull,
		// Valid but too many unary operators in a row
		CompileReturnCode_OpBufferFull,
	};

	// NOTE: Expression string will be modified if it contains a string with escape characters
	// This may cause RunExpression fallback after Evaluate to fail
	// This can be avoided by exiting compilation before escaped strings are parsed
	// by putting "0," at the beginning of the expression - this is valid squirrel while not allowed here
	ECompileReturnCode Evaluate( string_t &expression, HSQUIRRELVM vm, const SQVM::CallInfo *ci, SQObjectPtr &ret );
	ECompileReturnCode Evaluate( string_t &expression, HSQUIRRELVM vm, int frame, SQObjectPtr &ret )
	{
		return Evaluate( expression, vm, IsValidStackFrame( vm, frame ) ? &vm->_callsstack[frame] : NULL, ret );
	}
	ECompileReturnCode Evaluate( string_t &expression, HSQUIRRELVM vm, const SQVM::CallInfo *ci, SQObjectPtr &ret,
			objref_t &obj );

private:
	static SQTable *GetDefaultDelegate( HSQUIRRELVM vm, SQObjectType type );
	bool ArithOp( char op, const SQObjectPtr &lhs, const SQObjectPtr &rhs, SQObjectPtr &out );
	bool NewSlot( const objref_t &obj, const SQObjectPtr &value );
	bool Delete( const objref_t &obj, SQObjectPtr &value );
	bool Increment( const objref_t &obj, int amt );
#endif

private:
	static inline void ConvertPtr( objref_t &obj );

	bool GetObj_VarRef( string_t &expression, const varref_t *ref,
			objref_t &out, SQObjectPtr &value );
	bool GetObj_Var( const SQObjectPtr &key, const SQObjectPtr &var,
			objref_t &out, SQObjectPtr &value );
	bool GetObj_Var( string_t &expression, bool identifierIsString, const SQObjectPtr &var,
			objref_t &out, SQObjectPtr &value );
	bool GetObj_Frame( const string_t &expression, HSQUIRRELVM vm, const SQVM::CallInfo *ci,
			objref_t &out, SQObjectPtr &value );
	bool GetObj_Frame( const string_t &expression, HSQUIRRELVM vm, int frame,
			objref_t &out, SQObjectPtr &value )
	{
		return GetObj_Frame(
				expression,
				vm,
				IsValidStackFrame( vm, frame ) ? &vm->_callsstack[frame] : NULL,
				out, value );
	}

	bool Get( const objref_t &obj, SQObjectPtr &value );
	bool Set( const objref_t &obj, const SQObjectPtr &value );

private:
	bool RunExpression( const string_t &expression, HSQUIRRELVM vm, const SQVM::CallInfo *ci,
		SQObjectPtr &out, bool multiline = false );

	bool RunExpression( const string_t &expression, HSQUIRRELVM vm, int frame,
		SQObjectPtr &out, bool multiline = false )
	{
		return RunExpression(
				expression,
				vm,
				IsValidStackFrame( vm, frame ) ? &vm->_callsstack[frame] : NULL,
				out,
				multiline );
	}

	bool CompileScript( const string_t &script, SQObjectPtr &out );
	bool RunScript( HSQUIRRELVM vm, const string_t &script,
#ifdef CLOSURE_ROOT
			SQWeakRef *root,
#endif
			const SQObject *env, SQObjectPtr &out, bool multiline = false  );

	bool RunClosure( const SQObjectPtr &closure, const SQObject *env,
			SQObjectPtr &ret )
	{
		CCallGuard cg( this, m_pCurVM );
		return RunClosure( m_pCurVM, closure, env, ret );
	}

	bool RunClosure( const SQObjectPtr &closure, const SQObject *env,
			const SQObjectPtr &p1, SQObjectPtr &ret )
	{
		CCallGuard cg( this, m_pCurVM );
		return RunClosure( m_pCurVM, closure, env, p1, ret );
	}

	bool RunClosure( const SQObjectPtr &closure, const SQObject *env,
			const SQObjectPtr &p1, const SQObjectPtr &p2, SQObjectPtr &ret )
	{
		CCallGuard cg( this, m_pCurVM );
		return RunClosure( m_pCurVM, closure, env, p1, p2, ret );
	}

	bool RunClosure( const SQObjectPtr &closure, const SQObject *env,
			const SQObjectPtr &p1, const SQObjectPtr &p2, const SQObjectPtr &p3, SQObjectPtr &ret )
	{
		CCallGuard cg( this, m_pCurVM );
		return RunClosure( m_pCurVM, closure, env, p1, p2, p3, ret );
	}

	bool RunClosure( const SQObjectPtr &closure, const SQObject *env,
			const SQObjectPtr &p1, const SQObjectPtr &p2, const SQObjectPtr &p3, const SQObjectPtr &p4,
			SQObjectPtr &ret )
	{
		CCallGuard cg( this, m_pCurVM );
		return RunClosure( m_pCurVM, closure, env, p1, p2, p3, p4, ret );
	}

	bool RunClosure( const SQObjectPtr &closure, const SQObject *env,
			const SQObjectPtr &p1, const SQObjectPtr &p2, const SQObjectPtr &p3, const SQObjectPtr &p4,
			const SQObjectPtr &p5, SQObjectPtr &ret )
	{
		CCallGuard cg( this, m_pCurVM );
		return RunClosure( m_pCurVM, closure, env, p1, p2, p3, p4, p5, ret );
	}

	static inline bool RunClosure( HSQUIRRELVM vm, const SQObjectPtr &closure, const SQObject *env,
			SQObjectPtr &ret );
	static inline bool RunClosure( HSQUIRRELVM vm, const SQObjectPtr &closure, const SQObject *env,
			const SQObjectPtr &p1, SQObjectPtr &ret );
	static inline bool RunClosure( HSQUIRRELVM vm, const SQObjectPtr &closure, const SQObject *env,
			const SQObjectPtr &p1, const SQObjectPtr &p2, SQObjectPtr &ret );
	static inline bool RunClosure( HSQUIRRELVM vm, const SQObjectPtr &closure, const SQObject *env,
			const SQObjectPtr &p1, const SQObjectPtr &p2, const SQObjectPtr &p3, SQObjectPtr &ret );
	static inline bool RunClosure( HSQUIRRELVM vm, const SQObjectPtr &closure, const SQObject *env,
			const SQObjectPtr &p1, const SQObjectPtr &p2, const SQObjectPtr &p3, const SQObjectPtr &p4,
			SQObjectPtr &ret );
	static inline bool RunClosure( HSQUIRRELVM vm, const SQObjectPtr &closure, const SQObject *env,
			const SQObjectPtr &p1, const SQObjectPtr &p2, const SQObjectPtr &p3, const SQObjectPtr &p4,
			const SQObjectPtr &p5, SQObjectPtr &ret );

	static SQInteger SQMM_Get( HSQUIRRELVM vm );
	static SQInteger SQMM_Set( HSQUIRRELVM vm );
	static SQInteger SQMM_NewSlot( HSQUIRRELVM vm );

	static bool GetVariable( HSQUIRRELVM vm, const SQVM::CallInfo *ci,
			const SQObject &mtenv, const SQObject &index, SQObjectPtr &out );
	static bool SetVariable( HSQUIRRELVM vm, const SQVM::CallInfo *ci,
			const SQObject &mtenv, const SQObject &index, const SQObject &value );
	static bool NewSlot( HSQUIRRELVM vm, const SQObject &mtenv, const SQObject &index, const SQObject &value );

private:
	script_t *GetScript( const string_t &source );
	void RemoveScripts();

public:
	void OnScriptCompile( const SQChar *script, int scriptlen,
			const SQChar *sourcename, int sourcenamelen );

private:
	static bool ParseEvaluateName( const string_t &expression, HSQUIRRELVM vm, int frame,
		objref_t &out, SQObjectPtr &value );
	static int ParseFormatSpecifiers( string_t &expression, char **ppComma = NULL );
	static bool ParseBinaryNumber( const string_t &value, SQObject &out );
	static inline bool ShouldParseEvaluateName( const string_t &expression );
	static inline bool ShouldPageArray( const SQObject &obj, unsigned int limit );
#ifndef SQDBG_DISABLE_COMPILER
	void FillCompletions( const SQObjectPtr &target, HSQUIRRELVM vm, const SQVM::CallInfo *ci,
		int token, const string_t &partial, int start, int length, json_array_t &targets );
#endif

private:
	void InitEnv_GetVal( SQObjectPtr &env );
	void SetCallFrame( SQObjectPtr &env, HSQUIRRELVM vm, const SQVM::CallInfo *ci );
	void SetEnvDelegate( SQObjectPtr &env, const SQObject &delegate );
	void ClearEnvDelegate( SQObjectPtr &env );
#ifdef CLOSURE_ROOT
	void SetEnvRoot( SQObjectPtr &env, const SQObjectPtr &root );
#endif

private:
	static inline bool ShouldIgnoreStackFrame( const SQVM::CallInfo &ci );
	int ConvertToFrameID( int threadId, int stackFrame );
	bool TranslateFrameID( int frameId, HSQUIRRELVM *thread, int *stackFrame );

	int ThreadToID( HSQUIRRELVM vm );
	HSQUIRRELVM ThreadFromID( int id );
	inline void RemoveThreads();

private:
	int ToVarRef( EVARREF type, HSQUIRRELVM vm, int frame );
	int ToVarRef( EVARREF type, const SQObject &obj, bool isWeak = false, bool isStrong = false );
	int ToVarRef( const SQObject &obj, bool isWeak = false, bool isStrong = false )
	{
		return ToVarRef( VARREF_OBJ, obj, isWeak, isStrong );
	}

	static inline void ConvertToWeakRef( varref_t &v );

	inline varref_t *FromVarRef( int i );
	inline void RemoveVarRefs( bool all );
	inline void RemoveLockedWatches();

	void Suspend();
	void Break( HSQUIRRELVM vm, breakreason_t reason );
	void Continue( HSQUIRRELVM vm );

	classdef_t *FindClassDef( SQClass *base );

	// Fallback to base class
	const SQObjectPtr *GetClassDefValue( SQClass *base );
	const SQObjectPtr *GetClassDefMetaMembers( SQClass *base );
	const SQObjectPtr *GetClassDefCustomMembers( SQClass *base );

	bool CallCustomMembersGetFunc( const SQObjectPtr &closure, const SQObject *env,
			const SQObjectPtr &key, SQObjectPtr &ret );
	bool CallCustomMembersSetFunc( const SQObjectPtr &closure, const SQObject *env,
			const SQObjectPtr &key, const SQObjectPtr &val, SQObjectPtr &ret );

	void DefineClass( SQClass *target, SQTable *params );
	inline void RemoveClassDefs();

private:
	static SQUnsignedInteger s_nTargetAssignment;
	static SQString *GetLocalVarName( SQFunctionProto *func, SQInstruction *instr, unsigned int pos );

	static inline void PrintStackVar( SQFunctionProto *func, SQInstruction *instr, unsigned int pos,
			stringbufext_t &buf );
	static inline void PrintOuter( SQFunctionProto *func, int pos, stringbufext_t &buf );
	inline void PrintLiteral( SQFunctionProto *func, int pos, stringbufext_t &buf );
	static inline void PrintStackTarget( SQFunctionProto *func, SQInstruction *instr,
			stringbufext_t &buf );
	static inline void PrintStackTargetVar( SQFunctionProto *func, SQInstruction *instr,
			stringbufext_t &buf );
	static inline void PrintDeref( SQFunctionProto *func, SQInstruction *instr,
			unsigned int self, unsigned int key, stringbufext_t &buf );

	int DisassemblyBufLen( SQClosure *target );
	sqstring_t PrintDisassembly( SQClosure *target, SQChar *scratch, int bufsize );

#ifndef SQDBG_DISABLE_PROFILER
public:
	bool IsProfilerEnabled() { return m_bProfilerEnabled; }
	CProfiler *GetProfiler( HSQUIRRELVM vm );
	inline CProfiler *GetProfilerFast( HSQUIRRELVM vm );
	void ProfSwitchThread( HSQUIRRELVM vm );
	void ProfStart();
	void ProfStop();
	void ProfPause( HSQUIRRELVM vm );
	void ProfResume( HSQUIRRELVM vm );
	void ProfGroupBegin( HSQUIRRELVM vm, SQString *tag );
	void ProfGroupEnd( HSQUIRRELVM vm );
	sqstring_t ProfGet( HSQUIRRELVM vm, SQString *tag, int type );
	void ProfPrint( HSQUIRRELVM vm, SQString *tag, int type );
#endif

private:
	void ErrorHandler( HSQUIRRELVM vm );
	void DebugHook( HSQUIRRELVM vm, SQInteger type,
			const SQChar *sourcename, SQInteger line, const SQChar *funcname );
#ifndef SQDBG_DISABLE_PROFILER
	void ProfHook( HSQUIRRELVM vm, SQInteger type );
#endif
	void OnSQPrint( HSQUIRRELVM vm, const SQChar *buf, int len );
	void OnSQError( HSQUIRRELVM vm, const SQChar *buf, int len );

	template < typename T >
	void SendEvent_OutputStdOut( const T &strOutput, const SQVM::CallInfo *ci );

public:
	static SQInteger SQDefineClass( HSQUIRRELVM vm );
	static SQInteger SQPrintDisassembly( HSQUIRRELVM vm );
	static SQInteger SQBreak( HSQUIRRELVM vm );
#ifndef SQDBG_DISABLE_PROFILER
	static SQInteger SQProfStart( HSQUIRRELVM vm );
	static SQInteger SQProfStop( HSQUIRRELVM vm );
	static SQInteger SQProfPause( HSQUIRRELVM vm );
	static SQInteger SQProfResume( HSQUIRRELVM vm );
	static SQInteger SQProfGroupBegin( HSQUIRRELVM vm );
	static SQInteger SQProfGroupEnd( HSQUIRRELVM vm );
	static SQInteger SQProfGet( HSQUIRRELVM vm );
	static SQInteger SQProfPrint( HSQUIRRELVM vm );
#endif

	static void SQPrint( HSQUIRRELVM vm, const SQChar *fmt, ... );
	static void SQError( HSQUIRRELVM vm, const SQChar *fmt, ... );

#ifndef SQDBG_CALL_DEFAULT_ERROR_HANDLER
	static void SQErrorAtFrame( HSQUIRRELVM vm, const SQVM::CallInfo *ci, const SQChar *fmt, ... );
	static void PrintVar( HSQUIRRELVM vm, const SQChar *name, const SQObjectPtr &obj );
	static void PrintStack( HSQUIRRELVM vm );
#endif

	static SQInteger SQErrorHandler( HSQUIRRELVM vm );
#ifdef NATIVE_DEBUG_HOOK
	static void SQDebugHook( HSQUIRRELVM vm, SQInteger type,
			const SQChar *sourcename, SQInteger line, const SQChar *funcname );
#else
	static SQInteger SQDebugHook( HSQUIRRELVM vm );
#endif
#ifndef SQDBG_DISABLE_PROFILER
#ifdef NATIVE_DEBUG_HOOK
	static void SQProfHook( HSQUIRRELVM vm, SQInteger type,
			const SQChar *sourcename, SQInteger line, const SQChar *funcname );
#else
	static SQInteger SQProfHook( HSQUIRRELVM vm );
#endif
#endif
};

SQDebugServer::SQDebugServer() :
	m_State( ThreadState_Running ),
	m_nCalls( 0 ),
	m_Sequence( 0 ),
	m_pRootVM( NULL ),
	m_pCurVM( NULL ),
	m_Print( NULL ),
	m_PrintError( NULL ),
#ifndef SQDBG_DISABLE_PROFILER
	m_pProfiler( NULL ),
	m_bProfilerEnabled( 0 ),
#endif
	m_bBreakOnExceptions( 0 ),
	m_bDebugHookGuard( 0 ),
	m_bInREPL( 0 ),
	m_bDebugHookGuardAlways( 0 ),
#if SQUIRREL_VERSION_NUMBER < 300
	m_bInDebugHook( 0 ),
#endif
	m_bExceptionPause( 0 ),
	m_pPausedThread( NULL ),
	m_nBreakpointIndex( 0 ),
	m_nVarRefIndex( 0 ),
	m_iYieldValues( 0 )
{
}

char *SQDebugServer::ScratchPad( int size )
{
	size = ALIGN( size, 128 ) << 1;

	if ( m_Scratch.Base() )
	{
		if ( size <= m_Scratch.Size() )
			return m_Scratch.Base();

		m_Scratch.Alloc( size );
		return m_Scratch.Base();
	}

	m_Scratch.Alloc( max( 256, size ) );
	return m_Scratch.Base();
}

static inline void sqdbg_get_debugger_ref( HSQUIRRELVM vm, SQObjectPtr &ref );

void SQDebugServer::Attach( HSQUIRRELVM vm )
{
	if ( m_pRootVM )
	{
		if ( m_pRootVM == _thread(_ss(vm)->_root_vm) )
		{
			Print(_SC("(sqdbg) Debugger is already attached to this VM\n"));
		}
		else
		{
			Print(_SC("(sqdbg) Debugger is already attached to another VM\n"));
		}

		return;
	}

	m_pRootVM = _thread(_ss(vm)->_root_vm);
	m_pCurVM = vm;

	Assert( m_Threads.size() == 0 );
	ThreadToID( m_pRootVM );

#if SQUIRREL_VERSION_NUMBER >= 300
	m_Print = sq_getprintfunc( m_pRootVM );
	m_PrintError = sq_geterrorfunc( m_pRootVM );
#else
	m_Print = sq_getprintfunc( m_pRootVM );
	m_PrintError = m_Print;
#endif

	Assert( m_Print && m_PrintError );

	m_ErrorHandler = m_pRootVM->_errorhandler;

	if ( sq_type(m_ErrorHandler) != OT_NULL )
		sq_addref( m_pRootVM, &m_ErrorHandler );

	sq_enabledebuginfo( m_pRootVM, 1 );

	SQString *cached = CreateSQString( m_pRootVM, _SC("sqdbg") );
	__ObjAddRef( cached );
	m_sqstrCallFrame = CreateSQString( m_pRootVM, _SC(KW_CALLFRAME) );
	m_sqstrDelegate = CreateSQString( m_pRootVM, _SC(KW_DELEGATE) );
#ifdef CLOSURE_ROOT
	m_sqstrRoot = CreateSQString( m_pRootVM, _SC(KW_ROOT) );
#endif

#if SQUIRREL_VERSION_NUMBER >= 300
	m_sqfnGet = SQNativeClosure::Create( _ss(m_pRootVM), SQMM_Get, 0 );
	m_sqfnSet = SQNativeClosure::Create( _ss(m_pRootVM), SQMM_Set, 0 );
	m_sqfnNewSlot = SQNativeClosure::Create( _ss(m_pRootVM), SQMM_NewSlot, 0 );
#else
	m_sqfnGet = SQNativeClosure::Create( _ss(m_pRootVM), SQMM_Get );
	m_sqfnSet = SQNativeClosure::Create( _ss(m_pRootVM), SQMM_Set );
	m_sqfnNewSlot = SQNativeClosure::Create( _ss(m_pRootVM), SQMM_NewSlot );
#endif
	_nativeclosure(m_sqfnGet)->_nparamscheck = 2;
	_nativeclosure(m_sqfnSet)->_nparamscheck = 3;
	_nativeclosure(m_sqfnNewSlot)->_nparamscheck = 3;

	InitEnv_GetVal( m_EnvGetVal );
	sq_addref( m_pRootVM, &m_EnvGetVal );

	{
		CStackCheck stackcheck( m_pRootVM );

		SQObjectPtr ref;
		sqdbg_get_debugger_ref( m_pRootVM, ref );

		sq_pushroottable( m_pRootVM );

		sq_pushstring( m_pRootVM, _SC("sqdbg_define_class"), STRLEN("sqdbg_define_class") );
		sq_pushobject( m_pRootVM, ref );
		sq_newclosure( m_pRootVM, &SQDebugServer::SQDefineClass, 1 );
		sq_setnativeclosurename( m_pRootVM, -1, _SC("sqdbg_define_class") );
		sq_setparamscheck( m_pRootVM, 3, _SC(".yt") );
		sq_newslot( m_pRootVM, -3, SQFalse );

		sq_pushstring( m_pRootVM, _SC("sqdbg_disassemble"), STRLEN("sqdbg_disassemble") );
		sq_pushobject( m_pRootVM, ref );
		sq_newclosure( m_pRootVM, &SQDebugServer::SQPrintDisassembly, 1 );
		sq_setnativeclosurename( m_pRootVM, -1, _SC("sqdbg_disassemble") );
		sq_setparamscheck( m_pRootVM, 2, _SC(".c") );
		sq_newslot( m_pRootVM, -3, SQFalse );

		sq_pushstring( m_pRootVM, _SC("sqdbg_break"), STRLEN("sqdbg_break") );
		sq_pushobject( m_pRootVM, ref );
		sq_newclosure( m_pRootVM, &SQDebugServer::SQBreak, 1 );
		sq_setnativeclosurename( m_pRootVM, -1, _SC("sqdbg_break") );
		sq_setparamscheck( m_pRootVM, 1, _SC(".") );
		sq_newslot( m_pRootVM, -3, SQFalse );

#ifndef SQDBG_DISABLE_PROFILER
		sq_pushstring( m_pRootVM, _SC("sqdbg_prof_start"), STRLEN("sqdbg_prof_start") );
		sq_pushobject( m_pRootVM, ref );
		sq_newclosure( m_pRootVM, &SQDebugServer::SQProfStart, 1 );
		sq_setnativeclosurename( m_pRootVM, -1, _SC("sqdbg_prof_start") );
		sq_setparamscheck( m_pRootVM, 1, _SC(".") );
		sq_newslot( m_pRootVM, -3, SQFalse );

		sq_pushstring( m_pRootVM, _SC("sqdbg_prof_stop"), STRLEN("sqdbg_prof_stop") );
		sq_pushobject( m_pRootVM, ref );
		sq_newclosure( m_pRootVM, &SQDebugServer::SQProfStop, 1 );
		sq_setnativeclosurename( m_pRootVM, -1, _SC("sqdbg_prof_stop") );
		sq_setparamscheck( m_pRootVM, 1, _SC(".") );
		sq_newslot( m_pRootVM, -3, SQFalse );

		sq_pushstring( m_pRootVM, _SC("sqdbg_prof_pause"), STRLEN("sqdbg_prof_pause") );
		sq_pushobject( m_pRootVM, ref );
		sq_newclosure( m_pRootVM, &SQDebugServer::SQProfPause, 1 );
		sq_setnativeclosurename( m_pRootVM, -1, _SC("sqdbg_prof_pause") );
		sq_setparamscheck( m_pRootVM, 1, _SC(".") );
		sq_newslot( m_pRootVM, -3, SQFalse );

		sq_pushstring( m_pRootVM, _SC("sqdbg_prof_resume"), STRLEN("sqdbg_prof_resume") );
		sq_pushobject( m_pRootVM, ref );
		sq_newclosure( m_pRootVM, &SQDebugServer::SQProfResume, 1 );
		sq_setnativeclosurename( m_pRootVM, -1, _SC("sqdbg_prof_resume") );
		sq_setparamscheck( m_pRootVM, 1, _SC(".") );
		sq_newslot( m_pRootVM, -3, SQFalse );

		sq_pushstring( m_pRootVM, _SC("sqdbg_prof_begin"), STRLEN("sqdbg_prof_begin") );
		sq_pushobject( m_pRootVM, ref );
		sq_newclosure( m_pRootVM, &SQDebugServer::SQProfGroupBegin, 1 );
		sq_setnativeclosurename( m_pRootVM, -1, _SC("sqdbg_prof_begin") );
		sq_setparamscheck( m_pRootVM, 2, _SC(".s") );
		sq_newslot( m_pRootVM, -3, SQFalse );

		sq_pushstring( m_pRootVM, _SC("sqdbg_prof_end"), STRLEN("sqdbg_prof_end") );
		sq_pushobject( m_pRootVM, ref );
		sq_newclosure( m_pRootVM, &SQDebugServer::SQProfGroupEnd, 1 );
		sq_setnativeclosurename( m_pRootVM, -1, _SC("sqdbg_prof_end") );
		sq_setparamscheck( m_pRootVM, 1, _SC(".") );
		sq_newslot( m_pRootVM, -3, SQFalse );

		sq_pushstring( m_pRootVM, _SC("sqdbg_prof_get"), STRLEN("sqdbg_prof_get") );
		sq_pushobject( m_pRootVM, ref );
		sq_newclosure( m_pRootVM, &SQDebugServer::SQProfGet, 1 );
		sq_setnativeclosurename( m_pRootVM, -1, _SC("sqdbg_prof_get") );
		sq_setparamscheck( m_pRootVM, -1, _SC(".v|i|si|s") );
		sq_newslot( m_pRootVM, -3, SQFalse );

		sq_pushstring( m_pRootVM, _SC("sqdbg_prof_print"), STRLEN("sqdbg_prof_print") );
		sq_pushobject( m_pRootVM, ref );
		sq_newclosure( m_pRootVM, &SQDebugServer::SQProfPrint, 1 );
		sq_setnativeclosurename( m_pRootVM, -1, _SC("sqdbg_prof_print") );
		sq_setparamscheck( m_pRootVM, -1, _SC(".v|i|si|s") );
		sq_newslot( m_pRootVM, -3, SQFalse );
#endif

		sq_pop( m_pRootVM, 1 );
	}

	Print(_SC("(sqdbg) [%d] Attached\n"), SQDBG_SV_VER);
}

#define FOREACH_THREAD_BEGIN( _vm ) \
	for ( int i = m_Threads.size(); i--; ) \
	{ \
		SQWeakRef *wr = m_Threads[i]; \
		if ( wr && sq_type(wr->_obj) == OT_THREAD ) \
		{ \
			HSQUIRRELVM _vm = _thread(wr->_obj);

#define FOREACH_THREAD_END() \
		} \
	}

void SQDebugServer::SetErrorHandler( bool state )
{
	FOREACH_THREAD_BEGIN( vm )
	if ( state )
	{
		sq_newclosure( vm, &SQErrorHandler, 0 );
#ifdef SQDBG_CALL_DEFAULT_ERROR_HANDLER
		sq_setnativeclosurename( vm, -1, _SC("sqdbg") );
#endif
		sq_seterrorhandler( vm );
	}
	else
	{
		sq_pushobject( vm, m_ErrorHandler );
		sq_seterrorhandler( vm );
	}
	FOREACH_THREAD_END()
}

#ifdef NATIVE_DEBUG_HOOK
#ifdef DEBUG_HOOK_CACHED_SQDBG
static inline void sqdbg_set_debugger_cached_debughook( HSQUIRRELVM vm, bool state );
#else
#define sqdbg_set_debugger_cached_debughook( vm, state ) (void)0
#endif
#endif

void SQDebugServer::DoSetDebugHook( HSQUIRRELVM vm, _SQDEBUGHOOK fn )
{
#ifdef NATIVE_DEBUG_HOOK
	sq_setnativedebughook( vm, fn );
	sqdbg_set_debugger_cached_debughook( vm, fn != NULL );
#else
	if ( fn )
	{
		SQObjectPtr ref;
		sqdbg_get_debugger_ref( vm, ref );
		sq_pushobject( vm, ref );
		sq_newclosure( vm, fn, 1 );
	}
	else
	{
		sq_pushnull( vm );
	}

	sq_setdebughook( vm );
#endif
}

void SQDebugServer::SetDebugHook( _SQDEBUGHOOK fn )
{
	FOREACH_THREAD_BEGIN( vm )
		DoSetDebugHook( vm, fn );
	FOREACH_THREAD_END()
}

bool SQDebugServer::ListenSocket( unsigned short port )
{
	Assert( m_pRootVM );

	if ( m_Server.IsListening() )
	{
		port = m_Server.GetServerPort();

		if ( port )
		{
			Print(_SC("(sqdbg) Socket already open on port %d\n"), port);
		}
		else
		{
			Print(_SC("(sqdbg) Socket already open\n"));
		}

		return true;
	}

	if ( !m_Server.ListenSocket( port ) )
	{
		PrintLastServerMessage();
		return false;
	}

	port = m_Server.GetServerPort();

	Print(_SC("(sqdbg) Listening for connections on port %d\n"), port);
	return true;
}

void SQDebugServer::Shutdown()
{
	if ( !m_pRootVM )
		return;

	Print(_SC("(sqdbg) Shutdown\n"));

	if ( IsClientConnected() )
	{
		m_Server.Execute< SQDebugServer, &SQDebugServer::OnMessageReceived >( this );

		DAP_START_EVENT( ++m_Sequence, "terminated" );
		DAP_SEND();
	}

	m_Server.Shutdown();

#ifndef SQDBG_DISABLE_PROFILER
	if ( IsProfilerEnabled() )
		ProfStop();

	Assert( m_Profilers.size() == 0 );
	m_Profilers.purge();
#endif

	SetErrorHandler( false );
	SetDebugHook( NULL );

	m_State = ThreadState_Running;
	m_Sequence = 0;
	m_nBreakpointIndex = 0;
	m_nVarRefIndex = 0;
	m_iYieldValues = 0;
	m_nCalls = 0;

	m_bInREPL = false;
	m_bDebugHookGuardAlways = false;
	m_bDebugHookGuard = false;
#if SQUIRREL_VERSION_NUMBER < 300
	m_bInDebugHook = false;
#endif
	m_bExceptionPause = false;
	m_pPausedThread = NULL;

	m_ReturnValues.purge();
	RemoveVarRefs( true );
	RemoveLockedWatches();
	RemoveAllBreakpoints();
	RemoveDataBreakpoints();
	m_Breakpoints.purge();
	m_DataWatches.purge();
	RestoreCachedInstructions();
	ClearCachedInstructions();

	RemoveThreads();
	RemoveClassDefs();
	RemoveScripts();
	m_FrameIDs.purge();

	m_Scratch.Free();

	m_sqfnGet.Null();
	m_sqfnSet.Null();
	m_sqfnNewSlot.Null();

	sq_release( m_pRootVM, &m_EnvGetVal );
	m_EnvGetVal.Null();

	SQString *cached = CreateSQString( m_pRootVM, _SC("sqdbg") );
	__ObjRelease( cached );
	m_sqstrCallFrame.Null();
	m_sqstrDelegate.Null();
#ifdef CLOSURE_ROOT
	m_sqstrRoot.Null();
#endif

#if SQUIRREL_VERSION_NUMBER >= 300
	sq_setprintfunc( m_pRootVM, m_Print, m_PrintError );
#else
	sq_setprintfunc( m_pRootVM, m_Print );
#endif

	m_Print = m_PrintError = NULL;

	if ( sq_type(m_ErrorHandler) != OT_NULL )
	{
		sq_release( m_pRootVM, &m_ErrorHandler );
		m_ErrorHandler.Null();
	}

	sq_enabledebuginfo( m_pRootVM, 0 );
	sq_notifyallexceptions( m_pRootVM, 0 );

	m_pRootVM = m_pCurVM = NULL;
}

void SQDebugServer::DisconnectClient()
{
	if ( IsClientConnected() )
	{
		Print(_SC("(sqdbg) Client disconnected\n"));

		DAP_START_EVENT( ++m_Sequence, "terminated" );
		DAP_SEND();
	}

	m_Server.DisconnectClient();

	SetErrorHandler( false );
#ifndef SQDBG_DISABLE_PROFILER
	SetDebugHook( IsProfilerEnabled() ? &SQProfHook : NULL );
#else
	SetDebugHook( NULL );
#endif

	m_State = ThreadState_Running;
	m_Sequence = 0;
	m_nBreakpointIndex = 0;
	m_nVarRefIndex = 0;
	m_iYieldValues = 0;
	m_nCalls = 0;

	m_bInREPL = false;
	m_bDebugHookGuardAlways = false;
	m_bDebugHookGuard = false;
#if SQUIRREL_VERSION_NUMBER < 300
	m_bInDebugHook = false;
#endif
	m_bExceptionPause = false;
	m_pPausedThread = NULL;

	m_ReturnValues.purge();
	RemoveVarRefs( true );
	RemoveLockedWatches();
	RemoveAllBreakpoints();
	RemoveDataBreakpoints();
	m_Breakpoints.purge();
	m_DataWatches.purge();
	RestoreCachedInstructions();
	ClearCachedInstructions();

	ClearEnvDelegate( m_EnvGetVal );

	m_Scratch.Free();

#if SQUIRREL_VERSION_NUMBER >= 300
	sq_setprintfunc( m_pRootVM, m_Print, m_PrintError );
#else
	sq_setprintfunc( m_pRootVM, m_Print );
#endif
}

void SQDebugServer::OnClientConnected( const char *addr )
{
	Print(_SC("(sqdbg) Client connected from " FMT_CSTR "\n"), addr);

#if SQUIRREL_VERSION_NUMBER >= 300
	sq_setprintfunc( m_pRootVM, SQPrint, SQError );
#else
	sq_setprintfunc( m_pRootVM, SQPrint );
#endif

	SetErrorHandler( true );
	SetDebugHook( &SQDebugHook );

	// Validate if user has manually ruined it
	InitEnv_GetVal( m_EnvGetVal );

#define _check( var, size ) \
	if ( var.capacity() < size ) \
		var.reserve( size );

	_check( m_ReturnValues, 1 );
	_check( m_Vars, 64 );
	_check( m_FrameIDs, 8 );

#undef _check
}

void SQDebugServer::Frame()
{
	if ( m_Server.IsClientConnected() )
	{
		Recv();
		Parse();
		m_Server.Execute< SQDebugServer, &SQDebugServer::OnMessageReceived >( this );
	}
	else if ( m_Server.Listen() )
	{
		OnClientConnected( m_Server.m_pszLastMsg );
		m_Server.m_pszLastMsg = NULL;
	}
}

#define GET_OR_FAIL( _base, _val ) \
		if ( !(_base).Get( #_val, &_val ) ) \
		{ \
			PrintError(_SC("(sqdbg) invalid DAP message, could not find '" #_val "'\n")); \
			return; \
		}

#define GET_OR_ERROR_RESPONSE( _cmd, _base, _val ) \
		if ( !(_base).Get( #_val, &_val ) ) \
		{ \
			PrintError(_SC("(sqdbg) invalid DAP message, could not find '" #_val "'\n")); \
			DAP_ERROR_RESPONSE( seq, _cmd ); \
			DAP_ERROR_BODY( 0, "invalid DAP message", 0 ); \
			DAP_SEND(); \
			return; \
		}

void SQDebugServer::OnMessageReceived( char *ptr, int len )
{
	json_table_t table;
	JSONParser parser( ptr, len, &table );

	if ( parser.GetError() )
	{
		PrintError(_SC("(sqdbg) invalid DAP body : " FMT_CSTR "\n"), parser.GetError());
		Assert(!"invalid DAP body");
		DisconnectClient();
		return;
	}

	string_t type;
	table.GetString( "type", &type );

	if ( type.IsEqualTo( "request" ) )
	{
		int seq;
		GET_OR_FAIL( table, seq );

		ProcessRequest( table, seq );
	}
	else if ( type.IsEqualTo( "response" ) )
	{
		int request_seq;
		GET_OR_FAIL( table, request_seq );

		string_t command;
		table.GetString( "command", &command );

		PrintError(_SC("(sqdbg) Unrecognised response '" FMT_CSTR "'\n"), command.ptr);
		AssertMsg1( 0, "Unrecognised response '%s'", command.ptr );
	}
	else if ( type.IsEqualTo( "event" ) )
	{
		string_t event;
		table.GetString( "event", &event );

		PrintError(_SC("(sqdbg) Unrecognised event '" FMT_CSTR "'\n"), event.ptr);
		AssertMsg1( 0, "Unrecognised event '%s'", event.ptr );
	}
	else
	{
		PrintError(_SC("(sqdbg) invalid DAP type : '" FMT_CSTR "'\n"), type.ptr);
		Assert(!"invalid DAP type");
	}
}

void SQDebugServer::ProcessRequest( const json_table_t &table, int seq )
{
	string_t command;
	table.GetString( "command", &command );

	if ( command.IsEqualTo( "setBreakpoints" ) )
	{
		json_table_t *arguments;
		GET_OR_ERROR_RESPONSE( "setBreakpoints", table, arguments );

		OnRequest_SetBreakpoints( *arguments, seq );
	}
	else if ( command.IsEqualTo( "setFunctionBreakpoints" ) )
	{
		json_table_t *arguments;
		GET_OR_ERROR_RESPONSE( "setFunctionBreakpoints", table, arguments );

		OnRequest_SetFunctionBreakpoints( *arguments, seq );
	}
	else if ( command.IsEqualTo( "setExceptionBreakpoints" ) )
	{
		json_table_t *arguments;
		GET_OR_ERROR_RESPONSE( "setExceptionBreakpoints", table, arguments );

		OnRequest_SetExceptionBreakpoints( *arguments, seq );
	}
	else if ( command.IsEqualTo( "setDataBreakpoints" ) )
	{
		json_table_t *arguments;
		GET_OR_ERROR_RESPONSE( "setDataBreakpoints", table, arguments );

		OnRequest_SetDataBreakpoints( *arguments, seq );
	}
	else if ( command.IsEqualTo( "dataBreakpointInfo" ) )
	{
		json_table_t *arguments;
		GET_OR_ERROR_RESPONSE( "dataBreakpointInfo", table, arguments );

		OnRequest_DataBreakpointInfo( *arguments, seq );
	}
	else if ( command.IsEqualTo( "evaluate" ) )
	{
		json_table_t *arguments;
		GET_OR_ERROR_RESPONSE( "evaluate", table, arguments );

		OnRequest_Evaluate( *arguments, seq );
	}
#ifndef SQDBG_DISABLE_COMPILER
	else if ( command.IsEqualTo( "completions" ) )
	{
		json_table_t *arguments;
		GET_OR_ERROR_RESPONSE( "completions", table, arguments );

		OnRequest_Completions( *arguments, seq );
	}
#endif
	else if ( command.IsEqualTo( "scopes" ) )
	{
		json_table_t *arguments;
		GET_OR_ERROR_RESPONSE( "scopes", table, arguments );

		OnRequest_Scopes( *arguments, seq );
	}
	else if ( command.IsEqualTo( "threads" ) )
	{
		OnRequest_Threads( seq );
	}
	else if ( command.IsEqualTo( "stackTrace" ) )
	{
		json_table_t *arguments;
		GET_OR_ERROR_RESPONSE( "stackTrace", table, arguments );

		OnRequest_StackTrace( *arguments, seq );
	}
	else if ( command.IsEqualTo( "variables" ) )
	{
		json_table_t *arguments;
		GET_OR_ERROR_RESPONSE( "variables", table, arguments );

		OnRequest_Variables( *arguments, seq );
	}
	else if ( command.IsEqualTo( "setVariable" ) )
	{
		json_table_t *arguments;
		GET_OR_ERROR_RESPONSE( "setVariable", table, arguments );

		OnRequest_SetVariable( *arguments, seq );
	}
	else if ( command.IsEqualTo( "setExpression" ) )
	{
		json_table_t *arguments;
		GET_OR_ERROR_RESPONSE( "setExpression", table, arguments );

		OnRequest_SetExpression( *arguments, seq );
	}
	else if ( command.IsEqualTo( "setHitCount" ) )
	{
		json_table_t *arguments;
		GET_OR_ERROR_RESPONSE( "setHitCount", table, arguments );

		int breakpointId, hitCount;
		arguments->GetInt( "breakpointId", &breakpointId );
		arguments->GetInt( "hitCount", &hitCount );

		if ( breakpointId > 0 && breakpointId < m_nBreakpointIndex )
		{
#define _check( vec, type ) \
			for ( int i = 0; i < vec.size(); i++ ) \
			{ \
				type &bp = vec[i]; \
				if ( bp.id == breakpointId ) \
				{ \
					Assert( bp.hitsTarget ); \
					bp.hits = hitCount; \
					DAP_START_RESPONSE( seq, "setHitCount" ); \
					DAP_SEND(); \
					return; \
				} \
			}

			_check( m_Breakpoints, breakpoint_t );
			_check( m_DataWatches, datawatch_t );
#undef _check
		}

		DAP_ERROR_RESPONSE( seq, "setHitCount" );
		DAP_ERROR_BODY( 0, "invalid breakpoint {id}", 1 );
			json_table_t &variables = error.SetTable( "variables", 1 );
			variables.SetIntString( "id", breakpointId );
		DAP_SEND();
	}
	else if ( command.IsEqualTo( "source" ) )
	{
		json_table_t *arguments;
		GET_OR_ERROR_RESPONSE( "source", table, arguments );

		json_table_t *source;
		if ( arguments->GetTable( "source", &source ) )
		{
			string_t srcname;

			if ( ( !source->GetString( "name", &srcname ) || srcname.IsEmpty() ) &&
					source->GetString( "path", &srcname ) )
			{
				StripFileName( &srcname.ptr, &srcname.len );
			}

			script_t *scr = GetScript( srcname );
			if ( scr )
			{
				DAP_START_RESPONSE( seq, "source" );
				DAP_SET_TABLE( body, 1 );
					body.SetStringNoCopy( "content", { scr->scriptptr, scr->scriptlen } );
				DAP_SEND();
				return;
			}
		}

		DAP_ERROR_RESPONSE( seq, "source" );
		DAP_SEND();
	}
	else if ( command.IsEqualTo( "disassemble" ) )
	{
		json_table_t *arguments;
		GET_OR_ERROR_RESPONSE( "disassemble", table, arguments );

		OnRequest_Disassemble( *arguments, seq );
	}
#ifdef SUPPORTS_RESTART_FRAME
	else if ( command.IsEqualTo( "restartFrame" ) )
	{
		json_table_t *arguments;
		GET_OR_ERROR_RESPONSE( "restartFrame", table, arguments );

		if ( m_bExceptionPause )
		{
			DAP_START_RESPONSE( seq, "restartFrame" );
			DAP_SEND();

			Continue( m_pCurVM );
			return;
		}

		OnRequest_RestartFrame( *arguments, seq );

		RestoreCachedInstructions();
		ClearCachedInstructions();

		m_ReturnValues.clear();
		m_iYieldValues = 0;
	}
#endif
	else if ( command.IsEqualTo( "gotoTargets" ) )
	{
		json_table_t *arguments;
		GET_OR_ERROR_RESPONSE( "gotoTargets", table, arguments );

		OnRequest_GotoTargets( *arguments, seq );
	}
	else if ( command.IsEqualTo( "goto" ) )
	{
		json_table_t *arguments;
		GET_OR_ERROR_RESPONSE( "goto", table, arguments );

		if ( m_bExceptionPause )
		{
			DAP_START_RESPONSE( seq, "goto" );
			DAP_SEND();

			Continue( m_pCurVM );
			return;
		}

		OnRequest_Goto( *arguments, seq );

		RestoreCachedInstructions();
		ClearCachedInstructions();

		m_ReturnValues.clear();
		m_iYieldValues = 0;
	}
	else if ( command.IsEqualTo( "next" ) )
	{
		json_table_t *arguments;
		GET_OR_ERROR_RESPONSE( "next", table, arguments );

		if ( m_bExceptionPause )
		{
			DAP_START_RESPONSE( seq, "next" );
			DAP_SEND();

			Continue( m_pCurVM );
			return;
		}

		RestoreCachedInstructions();
		ClearCachedInstructions();

		m_ReturnValues.clear();
		m_iYieldValues = 0;

		OnRequest_Next( *arguments, seq );
	}
	else if ( command.IsEqualTo( "stepIn" ) )
	{
		json_table_t *arguments;
		GET_OR_ERROR_RESPONSE( "stepIn", table, arguments );

		if ( m_bExceptionPause )
		{
			DAP_START_RESPONSE( seq, "stepIn" );
			DAP_SEND();

			Continue( m_pCurVM );
			return;
		}

		RestoreCachedInstructions();
		ClearCachedInstructions();

		m_ReturnValues.clear();
		m_iYieldValues = 0;

		OnRequest_StepIn( *arguments, seq );
	}
	else if ( command.IsEqualTo( "stepOut" ) )
	{
		json_table_t *arguments;
		GET_OR_ERROR_RESPONSE( "stepOut", table, arguments );

		if ( m_bExceptionPause )
		{
			DAP_START_RESPONSE( seq, "stepOut" );
			DAP_SEND();

			Continue( m_pCurVM );
			return;
		}

		RestoreCachedInstructions();
		ClearCachedInstructions();

		m_ReturnValues.clear();
		m_iYieldValues = 0;

		OnRequest_StepOut( *arguments, seq );
	}
	else if ( command.IsEqualTo( "continue" ) )
	{
		DAP_START_RESPONSE( seq, "continue" );
		DAP_SET_TABLE( body, 1 );
			body.SetBool( "allThreadsContinued", true );
		DAP_SEND();

		Continue( m_pCurVM );
	}
	else if ( command.IsEqualTo( "pause" ) )
	{
		json_table_t *arguments;
		GET_OR_ERROR_RESPONSE( "pause", table, arguments );

		int threadId;
		arguments->GetInt( "threadId", &threadId, -1 );

		HSQUIRRELVM vm = ThreadFromID( threadId );

		if ( vm )
		{
			DAP_START_RESPONSE( seq, "pause" );
			DAP_SEND();

			if ( m_State != ThreadState_Suspended )
			{
				if ( m_pPausedThread )
				{
					RestoreCachedInstructions();
					ClearCachedInstructions();
				}

				m_pPausedThread = vm;
			}
		}
		else
		{
			DAP_ERROR_RESPONSE( seq, "pause" );
			DAP_ERROR_BODY( 0, "invalid thread", 0 );
			DAP_SEND();
		}
	}
	else if ( command.IsEqualTo( "attach" ) )
	{
		Print(_SC("(sqdbg) Client attached\n"));

		DAP_START_RESPONSE( seq, "attach" );
		DAP_SEND();

		DAP_START_EVENT( seq, "process" );
		DAP_SET_TABLE( body, 3 );
			body.SetString( "name", "" );
			body.SetString( "startMethod", "attach" );
			body.SetInt( "pointerSize", (int)sizeof(void*) );
		DAP_SEND();
	}
	else if ( command.IsEqualTo( "disconnect" ) || command.IsEqualTo( "terminate" ) )
	{
		DAP_START_RESPONSE( seq, command );
		DAP_SEND();

		DisconnectClient();
	}
	else if ( command.IsEqualTo( "initialize" ) )
	{
		json_table_t *arguments;
		GET_OR_ERROR_RESPONSE( "initialize", table, arguments );

		OnRequest_Initialize( *arguments, seq );
	}
	else if ( command.IsEqualTo( "configurationDone" ) )
	{
		DAP_START_RESPONSE( seq, "configurationDone" );
		DAP_SEND();
	}
	else
	{
		DAP_ERROR_RESPONSE( seq, command );
		DAP_ERROR_BODY( 0, "Unrecognised request '{command}'", 1 );
			json_table_t &variables = error.SetTable( "variables", 1 );
			variables.SetString( "command", command );
		DAP_SEND();
		AssertMsg1( 0, "Unrecognised request '%s'", command.ptr );
	}
}

void SQDebugServer::OnScriptCompile( const SQChar *script, int scriptlen,
		const SQChar *sourcename, int sourcenamelen )
{
	if ( !script || !scriptlen || !sourcename || !sourcenamelen )
		return;

#ifdef SQDBG_SOURCENAME_HAS_PATH
	StripFileName( &sourcename, &sourcenamelen );
#endif

#ifdef SQUNICODE
	Assert( UTF8Length( sourcename, sourcenamelen ) <= 256 );
#else
	Assert( sourcenamelen <= 256 );
#endif

	stringbuf_t< 256 > source;
	source.Puts( { sourcename, sourcenamelen } );

	script_t *scr = GetScript( source );

#ifdef SQUNICODE
	int scriptbufsize = ALIGN( UTF8Length( script, scriptlen ), 64 );
#else
	int scriptbufsize = ALIGN( scriptlen, 64 );
#endif

	if ( !scr )
	{
		scr = &m_Scripts.append();
		scr->sourceptr = (char*)sqdbg_malloc( source.len );
		scr->scriptptr = (char*)sqdbg_malloc( scriptbufsize );
		memcpy( scr->sourceptr, source.ptr, source.len );
		scr->sourcelen = source.len;
	}
	else
	{
		int oldsize = ALIGN( scr->scriptlen, 64 );
		if ( oldsize != scriptbufsize )
		{
			scr->scriptptr = (char*)sqdbg_realloc( scr->scriptptr, oldsize, scriptbufsize );
		}
	}

	scr->scriptlen = scstombs( scr->scriptptr, scriptbufsize, script, scriptlen );
}

script_t *SQDebugServer::GetScript( const string_t &source )
{
	for ( int i = 0; i < m_Scripts.size(); i++ )
	{
		script_t &scr = m_Scripts[i];
		if ( source.IsEqualTo( scr.sourceptr, scr.sourcelen ) )
			return &scr;
	}

	return NULL;
}

void SQDebugServer::RemoveScripts()
{
	for ( int i = 0; i < m_Scripts.size(); i++ )
	{
		script_t &scr = m_Scripts[i];
		sqdbg_free( scr.sourceptr, scr.sourcelen );
		sqdbg_free( scr.scriptptr, ALIGN( scr.scriptlen, 64 ) );
	}

	m_Scripts.purge();
}

void SQDebugServer::OnRequest_Initialize( const json_table_t &arguments, int seq )
{
	string_t clientID, clientName;
	arguments.GetString( "clientID", &clientID, "<unknown>" );
	arguments.GetString( "clientName", &clientName, "<unknown>" );

	if ( clientName.IsEqualTo( clientID ) )
	{
		Print(_SC("(sqdbg) Client initialised: " FMT_CSTR "\n"),
				clientName.ptr);
	}
	else
	{
		Print(_SC("(sqdbg) Client initialised: " FMT_CSTR " (" FMT_CSTR ")\n"),
				clientName.ptr, clientID.ptr);
	}

#ifndef SQDBG_DISABLE_COMPILER
	bool columnsStartAt1;
	arguments.GetBool( "columnsStartAt1", &columnsStartAt1 );
	m_nClientColumnOffset = (int)columnsStartAt1;
#endif

	DAP_START_RESPONSE( seq, "initialize" );
	DAP_SET_TABLE( body, 21 );
		body.SetBool( "supportsConfigurationDoneRequest", true );
		body.SetBool( "supportsFunctionBreakpoints", true );
		body.SetBool( "supportsConditionalBreakpoints", true );
		body.SetBool( "supportsHitConditionalBreakpoints", true );
		body.SetBool( "supportsEvaluateForHovers", true );
		json_array_t &exceptionBreakpointFilters = body.SetArray( "exceptionBreakpointFilters", 2 );
		{
			json_table_t &filter = exceptionBreakpointFilters.AppendTable(4);
			filter.SetString( "filter", "unhandled" );
			filter.SetString( "label", "Unhandled exceptions" );
			filter.SetString( "description", "Break on uncaught exceptions" );
			filter.SetBool( "default", true );
		}
		{
			json_table_t &filter = exceptionBreakpointFilters.AppendTable(3);
			filter.SetString( "filter", "all" );
			filter.SetString( "label", "All exceptions" );
			filter.SetString( "description", "Break on both caught and uncaught exceptions" );
		}
		body.SetBool( "supportsSetVariable", true );
#ifdef SUPPORTS_RESTART_FRAME
		body.SetBool( "supportsRestartFrame", true );
#endif
		body.SetBool( "supportsGotoTargetsRequest", true );
#ifndef SQDBG_DISABLE_COMPILER
		body.SetBool( "supportsCompletionsRequest", true );
#endif
		body.SetBool( "supportsSetExpression", true );
		body.SetBool( "supportsSetHitCount", true );
		body.SetArray( "supportedChecksumAlgorithms" );
		body.SetBool( "supportsValueFormattingOptions", true );
		body.SetBool( "supportsDelayedStackTraceLoading", true );
		body.SetBool( "supportsLogPoints", true );
		body.SetBool( "supportsTerminateRequest", true );
		body.SetBool( "supportsDataBreakpoints", true );
		body.SetBool( "supportsDisassembleRequest", true );
		body.SetBool( "supportsClipboardContext", true );
		body.SetBool( "supportsSteppingGranularity", true );
	DAP_SEND();

	DAP_START_EVENT( seq, "initialized" );
	DAP_SEND();
}

void SQDebugServer::SetSource( json_table_t &source, SQString *sourcename )
{
	sqstring_t srcname;
	srcname.Assign( sourcename );

#ifdef SQDBG_SOURCENAME_HAS_PATH
	StripFileName( &srcname.ptr, &srcname.len );
#endif

#ifdef SQUNICODE
	Assert( UTF8Length( srcname.ptr, srcname.len ) <= 256 );

	char pName[256];
	string_t strName;
	strName.Assign( pName, SQUnicodeToUTF8( pName, sizeof(pName), srcname.ptr, srcname.len ) );
#endif

#ifdef SQUNICODE
	CFilePathMap::pair_t *pair = m_FilePathMap.Get( strName );
#else
	CFilePathMap::pair_t *pair = m_FilePathMap.Get( srcname );
#endif

	if ( pair )
	{
		source.SetStringNoCopy( "path", pair->path );
#ifdef SQUNICODE
		source.SetStringNoCopy( "name", pair->name );
#else
		source.SetStringNoCopy( "name", srcname );
#endif
	}
	else
	{
#ifdef SQUNICODE
		source.SetString( "name", strName );
#else
		source.SetStringNoCopy( "name", srcname );
#endif
	}
}

void SQDebugServer::OnRequest_SetBreakpoints( const json_table_t &arguments, int seq )
{
	json_array_t *breakpoints;
	json_table_t *source;

	GET_OR_ERROR_RESPONSE( "setBreakpoints", arguments, breakpoints );
	GET_OR_ERROR_RESPONSE( "setBreakpoints", arguments, source );

	string_t srcname, srcpath;
	source->GetString( "path", &srcpath );

	if ( ( !source->GetString( "name", &srcname ) || srcname.IsEmpty() ) &&
			!srcpath.IsEmpty() )
	{
		srcname = srcpath;
		StripFileName( &srcname.ptr, &srcname.len );
	}

	if ( !srcname.IsEmpty() && !srcpath.IsEmpty() )
	{
		m_FilePathMap.Add( srcname, srcpath );
	}
	else if ( srcname.IsEmpty() && srcpath.IsEmpty() )
	{
		DAP_ERROR_RESPONSE( seq, "setBreakpoints" );
		DAP_ERROR_BODY( 0, "invalid source", 0 );
		DAP_SEND();
		return;
	}

	RemoveBreakpoints( srcname );

	for ( int i = 0; i < breakpoints->size(); i++ )
	{
		if ( !breakpoints->Get(i)->IsType( JSON_TABLE ) )
			continue;

		json_table_t &bp = breakpoints->Get(i)->AsTable();

		int line, hitsTarget = 0;
		string_t condition, hitCondition, logMessage;

		bp.GetInt( "line", &line );
		bp.GetString( "condition", &condition );
		bp.GetString( "logMessage", &logMessage );

		if ( bp.GetString( "hitCondition", &hitCondition ) && !hitCondition.IsEmpty() )
		{
			if ( !hitCondition.StartsWith("0x") )
			{
				atoi( hitCondition, &hitsTarget );
			}
			else
			{
				atox( hitCondition, &hitsTarget );
			}
		}

		int id = AddBreakpoint( line, srcname, condition, hitsTarget, logMessage );

		bp.SetInt( "id", id );
		bp.SetBool( "verified", id != INVALID_ID );

		if ( id != INVALID_ID )
		{
			bp.SetInt( "line", line );
		}
		else
		{
			bp.SetString( "reason", "failed" );
			bp.SetString( "message", GetValue( m_pCurVM->_lasterror, kFS_NoQuote ) );
		}
	}

	DAP_START_RESPONSE( seq, "setBreakpoints" );
	DAP_SET_TABLE( body, 1 );
		body.SetArray( "breakpoints", *breakpoints );
	DAP_SEND();
}

void SQDebugServer::OnRequest_SetFunctionBreakpoints( const json_table_t &arguments, int seq )
{
	json_array_t *breakpoints;
	GET_OR_ERROR_RESPONSE( "setFunctionBreakpoints", arguments, breakpoints );

	RemoveFunctionBreakpoints();

	for ( int i = 0; i < breakpoints->size(); i++ )
	{
		if ( !breakpoints->Get(i)->IsType( JSON_TABLE ) )
			continue;

		json_table_t &bp = breakpoints->Get(i)->AsTable();

		int hitsTarget = 0;
		string_t name, condition, hitCondition, logMessage;

		bp.GetString( "name", &name );
		bp.GetString( "condition", &condition );
		bp.GetString( "logMessage", &logMessage );

		if ( bp.GetString( "hitCondition", &hitCondition ) && !hitCondition.IsEmpty() )
		{
			if ( !hitCondition.StartsWith("0x") )
			{
				atoi( hitCondition, &hitsTarget );
			}
			else
			{
				atox( hitCondition, &hitsTarget );
			}
		}

		string_t funcsrc( "" );
		int line = 0;

		// function source: funcname,filename:line
		for ( int j = name.len - 1; j > 1; j-- )
		{
			if ( !line && name.ptr[j] == ':' )
			{
				string_t sLine;
				sLine.ptr = name.ptr + j + 1;
				sLine.len = name.len - j - 1;
				name.len = j;

				if ( !sLine.len || !atoi( sLine, &line ) || line <= 0 )
					line = -1;
			}
			else if ( name.ptr[j] == ',' )
			{
				funcsrc.ptr = name.ptr + j + 1;
				funcsrc.len = name.len - j - 1;
				name.len = j;
				break;
			}
		}

		if ( name.StartsWith( ANONYMOUS_FUNCTION_BREAKPOINT_NAME ) )
			name.Assign( "" );

		int id = AddFunctionBreakpoint( name, funcsrc, line, condition, hitsTarget, logMessage );

		bp.SetInt( "id", id );
		bp.SetBool( "verified", id != INVALID_ID );

		if ( id == INVALID_ID )
		{
			bp.SetString( "reason", "failed" );
			bp.SetString( "message", GetValue( m_pCurVM->_lasterror, kFS_NoQuote ) );
		}
	}

	DAP_START_RESPONSE( seq, "setFunctionBreakpoints" );
	DAP_SET_TABLE( body, 1 );
		body.SetArray( "breakpoints", *breakpoints );
	DAP_SEND();
}

void SQDebugServer::OnRequest_SetExceptionBreakpoints( const json_table_t &arguments, int seq )
{
	bool bCaught = false, bUncaught = false;

	json_array_t *filters;
	GET_OR_ERROR_RESPONSE( "setExceptionBreakpoints", arguments, filters );

	for ( int i = 0; i < filters->size(); i++ )
	{
		if ( !filters->Get(i)->IsType( JSON_STRING ) )
			continue;

		const string_t &filter = filters->Get(i)->AsString();

		if ( filter.IsEqualTo( "unhandled" ) )
		{
			bUncaught = true;
		}
		else if ( filter.IsEqualTo( "all" ) )
		{
			bCaught = true;
		}
	}

	if ( filters->size() == 0 )
	{
		m_bBreakOnExceptions = false;
		sq_notifyallexceptions( m_pRootVM, 0 );
	}
	else
	{
		m_bBreakOnExceptions = true;

		if ( bCaught )
		{
			sq_notifyallexceptions( m_pRootVM, 1 );
		}
		else
		{
			Assert( bUncaught );
			(void)bUncaught;
			sq_notifyallexceptions( m_pRootVM, 0 );
		}
	}

	DAP_START_RESPONSE( seq, "setExceptionBreakpoints" );
	DAP_SEND();
}

void SQDebugServer::OnRequest_SetDataBreakpoints( const json_table_t &arguments, int seq )
{
	json_array_t *breakpoints;
	GET_OR_ERROR_RESPONSE( "setDataBreakpoints", arguments, breakpoints );

	RemoveDataBreakpoints();

	for ( int i = 0; i < breakpoints->size(); i++ )
	{
		if ( !breakpoints->Get(i)->IsType( JSON_TABLE ) )
			continue;

		json_table_t &bp = breakpoints->Get(i)->AsTable();

		int hitsTarget = 0;
		string_t dataId, condition, hitCondition;

		bp.GetString( "dataId", &dataId );
		bp.GetString( "condition", &condition );

		condition.Strip();

		if ( bp.GetString( "hitCondition", &hitCondition ) && !hitCondition.IsEmpty() )
		{
			if ( !hitCondition.StartsWith("0x") )
			{
				atoi( hitCondition, &hitsTarget );
			}
			else
			{
				atox( hitCondition, &hitsTarget );
			}
		}

		int id = AddDataBreakpoint( dataId, condition, hitsTarget );

		bp.SetInt( "id", id );
		bp.SetBool( "verified", id != INVALID_ID );

		if ( id == INVALID_ID )
		{
			bp.SetString( "reason", "failed" );
			bp.SetString( "message", GetValue( m_pCurVM->_lasterror, kFS_NoQuote ) );
		}
	}

	DAP_START_RESPONSE( seq, "setDataBreakpoints" );
	DAP_SET_TABLE( body, 1 );
		body.SetArray( "breakpoints", *breakpoints );
	DAP_SEND();
}

void SQDebugServer::OnRequest_DataBreakpointInfo( const json_table_t &arguments, int seq )
{
	int variablesReference;
	string_t name;

	arguments.GetString( "name", &name );
	arguments.GetInt( "variablesReference", &variablesReference );

	if ( variablesReference )
	{
		objref_t obj;
		SQObjectPtr dummy;
		varref_t *ref = FromVarRef( variablesReference );

		// don't modify name in GetObj
		Assert( name.len < 512 );
		stringbuf_t< 512 > tmpbuf;
		tmpbuf.Puts( name );
		tmpbuf.Term();
		string_t tmp = tmpbuf;

		if ( !ref || ref->type != VARREF_OBJ ||
				!GetObj_VarRef( tmp, ref, obj, dummy ) )
		{
			DAP_START_RESPONSE( seq, "dataBreakpointInfo" );
			DAP_SET_TABLE( body, 2 );
				body.SetNull( "dataId" );
				body.SetString( "description", "" );
			DAP_SEND();
			return;
		}

		stringbuf_t< 512 > bufId, bufName;

		DAP_START_RESPONSE( seq, "dataBreakpointInfo" );
		DAP_SET_TABLE( body, 3 );
			{
				// 1:varref:name
				bufId.Put('1');
				bufId.Put(':');
				bufId.PutInt( variablesReference );
				bufId.Put(':');
				bufId.Puts( name );

				body.SetStringNoCopy( "dataId", bufId );
			}
			{
				bufName.Put('[');
				bufName.PutHex( _rawval(ref->GetVar()) );
				bufName.Put(' ');
				bufName.Puts( GetType( ref->GetVar() ) );
				bufName.Put(']');
				bufName.Put('-');
				bufName.Put('>');
				bufName.Puts( name );

				body.SetStringNoCopy( "description", bufName );
			}
			body.SetArray( "accessTypes", 1 ).Append( "write" );
		DAP_SEND();
	}
	else
	{
#ifndef SQDBG_DISABLE_COMPILER
		// don't modify name in CCompiler::ParseString
		Assert( name.len < 512 );
		stringbuf_t< 512 > tmpbuf;
		tmpbuf.Puts( name );
		tmpbuf.Term();
		string_t tmp = tmpbuf;

		objref_t obj;
		SQObjectPtr value;
		ECompileReturnCode r = Evaluate( tmp, m_pCurVM, m_pCurVM->ci ? m_pCurVM->ci : NULL, value, obj );

		// Check value again to see if the compiled expression was really a reference
		SQObjectPtr val;

		if ( r != CompileReturnCode_Success ||
				obj.type == objref_t::INVALID || obj.type == objref_t::PTR ||
				!ISREFCOUNTED( sq_type(obj.src) ) ||
				!Get( obj, val ) || sq_type(val) != sq_type(value) || _rawval(val) != _rawval(value) )
		{
			DAP_START_RESPONSE( seq, "dataBreakpointInfo" );
			DAP_SET_TABLE( body, 2 );
				body.SetNull( "dataId" );
				body.SetString( "description", "" );
			DAP_SEND();
			return;
		}

		stringbuf_t< 512 > bufId, bufName;

		DAP_START_RESPONSE( seq, "dataBreakpointInfo" );
		DAP_SET_TABLE( body, 3 );
			{
				// 0:expr
				bufId.Put('0');
				bufId.Put(':');
				bufId.Puts( name );

				body.SetStringNoCopy( "dataId", bufId );
			}
			{
				bufName.Put('`');
				bufName.Puts( name );
				bufName.Put('`');

				body.SetStringNoCopy( "description", bufName );
			}
			body.SetArray( "accessTypes", 1 ).Append( "write" );
		DAP_SEND();
#else
		DAP_START_RESPONSE( seq, "dataBreakpointInfo" );
		DAP_SET_TABLE( body, 2 );
			body.SetNull( "dataId" );
			body.SetString( "description", "expression data breakpoints not implemented" );
		DAP_SEND();
#endif
	}
}

int SQDebugServer::AddDataBreakpoint( const string_t &dataId, const string_t &strCondition, int hitsTarget )
{
	if ( dataId.len < 2 || dataId.ptr[1] != ':' )
		return INVALID_ID;

	string_t name;
	objref_t obj;
	SQObjectPtr value;
	SQWeakRef *pContainer;

	if ( dataId.ptr[0] == '1' )
	{
		char *pEnd = strchr( dataId.ptr + 1 + 1, ':' );
		if ( !pEnd )
			return INVALID_ID;

		string_t container;
		container.ptr = dataId.ptr + 1 + 1;
		container.len = pEnd - container.ptr;

		name.ptr = pEnd + 1;
		name.len = ( dataId.ptr + dataId.len ) - name.ptr;

		int variablesReference;

		if ( !atoi( container, &variablesReference ) )
		{
			m_pCurVM->_lasterror = CreateSQString( m_pCurVM, _SC("invalid object") );
			return INVALID_ID;
		}

		varref_t *ref = FromVarRef( variablesReference );

		// don't modify name in GetObj
		Assert( name.len < 512 );
		stringbuf_t< 512 > tmpbuf;
		tmpbuf.Puts( name );
		tmpbuf.Term();
		string_t tmp = tmpbuf;

		if ( !ref || ref->type != VARREF_OBJ ||
				!GetObj_VarRef( tmp, ref, obj, value ) )
		{
			m_pCurVM->_lasterror = CreateSQString( m_pCurVM, _SC("invalid object") );
			return INVALID_ID;
		}

		ConvertPtr( obj );
		Assert( obj.type != objref_t::PTR );

		ConvertToWeakRef( *ref );
		pContainer = ref->obj.weakref;
	}
#ifndef SQDBG_DISABLE_COMPILER
	else if ( dataId.ptr[0] == '0' )
	{
		name.ptr = dataId.ptr + 1 + 1;
		name.len = ( dataId.ptr + dataId.len ) - name.ptr;

		// don't modify name in CCompiler::ParseString
		Assert( name.len < 512 );
		stringbuf_t< 512 > tmpbuf;
		tmpbuf.Puts( name );
		tmpbuf.Term();
		string_t tmp = tmpbuf;

		ECompileReturnCode r = Evaluate( tmp, m_pCurVM, m_pCurVM->ci ? m_pCurVM->ci : NULL, value, obj );

		// Check value again to see if the compiled expression was really a reference
		SQObjectPtr val;

		if ( r != CompileReturnCode_Success ||
				obj.type == objref_t::INVALID || obj.type == objref_t::PTR ||
				!ISREFCOUNTED( sq_type(obj.src) ) ||
				!Get( obj, val ) || sq_type(val) != sq_type(value) || _rawval(val) != _rawval(value) )
		{
			m_pCurVM->_lasterror = CreateSQString( m_pCurVM, _SC("invalid expression") );
			return INVALID_ID;
		}

		pContainer = NULL;
	}
#endif
	else
	{
		return INVALID_ID;
	}

	bool duplicate = false;

	// Duplicate?
	for ( int i = m_DataWatches.size(); i--; )
	{
		const datawatch_t &dw = m_DataWatches[i];
		if ( dw.container == pContainer &&
				_refcounted(dw.obj.src) == _refcounted(obj.src) &&
				dw.obj.type == obj.type &&
				_rawval(dw.obj.key) == _rawval(obj.key) &&
				sq_type(dw.obj.key) == sq_type(obj.key) )
		{
			// Allow duplicate watches with different conditions
			if ( dw.condtype != DBC_NONE || !strCondition.IsEmpty() )
			{
				duplicate = true;
				break;
			}
			else
			{
				m_pCurVM->_lasterror = CreateSQString( m_pCurVM, _SC("duplicate breakpoint") );
				return INVALID_ID;
			}
		}
	}

	unsigned int condtype = DBC_NONE;
	SQObjectPtr condition;

	if ( !strCondition.IsEmpty() &&
			!CompileDataBreakpointCondition( strCondition, condition, condtype ) )
		return INVALID_ID;

	if ( duplicate )
	{
		// Re-iterate through watches to match this exact compiled condition
		for ( int i = m_DataWatches.size(); i--; )
		{
			const datawatch_t &dw = m_DataWatches[i];
			if ( dw.container == pContainer &&
					_refcounted(dw.obj.src) == _refcounted(obj.src) &&
					dw.obj.type == obj.type &&
					_rawval(dw.obj.key) == _rawval(obj.key) &&
					sq_type(dw.obj.key) == sq_type(obj.key) &&
					dw.condtype == condtype &&
					_rawval(dw.condition) == _rawval(condition) &&
					sq_type(dw.condition) == sq_type(condition) )
			{
				m_pCurVM->_lasterror = CreateSQString( m_pCurVM, _SC("duplicate breakpoint") );
				return INVALID_ID;
			}
		}
	}

	datawatch_t &dw = m_DataWatches.append();
	memzero( &dw );
	dw.id = ++m_nBreakpointIndex;
	CopyString( name, &dw.name );

	if ( pContainer )
	{
		dw.container = pContainer;
		__ObjAddRef( dw.container );
	}

	dw.obj = obj;
	dw.oldvalue = value;
	dw.hitsTarget = hitsTarget;
	dw.condition._type = OT_NULL;

	if ( condtype != DBC_NONE )
	{
		dw.condtype = condtype;
		dw.condition = condition;

		if ( is_delegable(dw.condition) && _delegable(dw.condition)->_delegate )
		{
			sq_addref( m_pRootVM, &dw.condition );
		}
	}

	return dw.id;
}

//
// Parse out condition type and evaluate the value.
// It would be possible to let the user do ops on the data with a pseudovariable like '$data',
// or allow late evaluation by making the condition a function to be executed or parsing it manually,
// but that would add too much complexity and runtime overhead.
//
bool SQDebugServer::CompileDataBreakpointCondition( string_t condition, SQObjectPtr &out, unsigned int &type )
{
	if ( condition.StartsWith("==") )
	{
		type = DBC_EQ;
		condition.ptr += 2;
		condition.len -= 2;
	}
	else if ( condition.StartsWith("!=") )
	{
		type = DBC_NE;
		condition.ptr += 2;
		condition.len -= 2;
	}
	else if ( condition.StartsWith(">=") )
	{
		type = DBC_GE;
		condition.ptr += 2;
		condition.len -= 2;
	}
	else if ( condition.StartsWith("<=") )
	{
		type = DBC_LE;
		condition.ptr += 2;
		condition.len -= 2;
	}
	else if ( condition.StartsWith("=&") )
	{
		type = DBC_BWAEQ;
		condition.ptr += 2;
		condition.len -= 2;
	}
	else if ( condition.StartsWith("!&") )
	{
		type = DBC_BWAZ;
		condition.ptr += 2;
		condition.len -= 2;
	}
	else if ( condition.StartsWith(">") )
	{
		type = DBC_G;
		condition.ptr += 1;
		condition.len -= 1;
	}
	else if ( condition.StartsWith("<") )
	{
		type = DBC_L;
		condition.ptr += 1;
		condition.len -= 1;
	}
	else if ( condition.StartsWith("&") )
	{
		type = DBC_BWA;
		condition.ptr += 1;
		condition.len -= 1;
	}
	else
	{
		type = DBC_EQ;
	}

	// Compile the condition in current stack frame
#ifdef NATIVE_DEBUG_HOOK
	if ( !RunExpression( condition, m_pCurVM, m_pCurVM->ci, out ) )
#else
	if ( !RunExpression( condition,
				m_pCurVM,
				// client could send this request while not suspended
				m_State == ThreadState_Suspended ? m_pCurVM->ci - 1 : m_pCurVM->ci,
				out ) )
#endif
		return false;

	// Condition can only be of certain types
	// for comparisons and bitwise ops
	switch ( type )
	{
		case DBC_G:
		case DBC_GE:
		case DBC_L:
		case DBC_LE:
			switch ( sq_type(out) )
			{
				case OT_INTEGER:
				case OT_FLOAT:
					return true;

				default:
					if ( is_delegable(out) )
					{
						// Should have valid cmp metamethod
						if ( _delegable(out)->_delegate )
						{
							SQObjectPtr mm;
							if ( _delegable(out)->GetMetaMethod( m_pRootVM, MT_CMP, mm ) )
							{
								SQObjectPtr ret;
								if ( RunClosure( mm, &out, out, ret) &&
										sq_type(ret) == OT_INTEGER )
								{
									return true;
								}
							}
						}

						m_pCurVM->_lasterror = CreateSQString( m_pCurVM,
								_SC("invalid cmp metamethod") );
						return false;
					}

					m_pCurVM->_lasterror = CreateSQString( m_pCurVM,
							_SC("invalid type for comparator, expected integer|float|instance|table") );
					return false;
			}
		case DBC_BWA:
		case DBC_BWAZ:
		case DBC_BWAEQ:
			if ( sq_type(out) == OT_INTEGER )
				return true;

			m_pCurVM->_lasterror = CreateSQString( m_pCurVM,
					_SC("invalid type for bitwise op, expected integer") );
			return false;
	}

	return true;
}

int SQDebugServer::CompareObj( const SQObjectPtr &lhs, const SQObjectPtr &rhs )
{
	SQObjectType tl = sq_type(lhs);
	SQObjectType tr = sq_type(rhs);

	if ( tl == tr )
	{
		if ( _rawval(lhs) == _rawval(rhs) )
			return DBC_EQ;

		switch ( tl )
		{
			case OT_INTEGER:
			case OT_BOOL:
				return _integer(lhs) < _integer(rhs) ? DBC_L : DBC_G;

			case OT_FLOAT:
				return _float(lhs) < _float(rhs) ? DBC_L : DBC_G;

			default:
				if ( is_delegable(lhs) && _delegable(lhs)->_delegate )
				{
					SQObjectPtr mm;
					if ( _delegable(lhs)->GetMetaMethod( m_pRootVM, MT_CMP, mm ) )
					{
						SQObjectPtr ret;
						if ( RunClosure( mm, &lhs, rhs, ret ) &&
								sq_type(ret) == OT_INTEGER )
						{
							if ( _integer(ret) == 0 )
								return DBC_EQ;

							if ( _integer(ret) < 0 )
								return DBC_L;

							return DBC_G;
						}
					}
				}

				// pointer comparison
				return _rawval(lhs) < _rawval(rhs) ? DBC_L : DBC_G;
		}
	}
	else
	{
		if ( tl == OT_INTEGER )
		{
			if ( tr == OT_FLOAT )
			{
				if ( _integer(lhs) == (SQInteger)_float(rhs) )
					return DBC_EQ;

				if ( _integer(lhs) < (SQInteger)_float(rhs) )
					return DBC_L;

				return DBC_G;
			}
		}
		else if ( tl == OT_FLOAT )
		{
			if ( tr == OT_INTEGER )
			{
				if ( (SQInteger)_float(lhs) == _integer(rhs) )
					return DBC_EQ;

				if ( (SQInteger)_float(lhs) < _integer(rhs) )
					return DBC_L;

				return DBC_G;
			}
		}

		// uncomparable
		return DBC_NONE;
	}
}

void SQDebugServer::CheckDataBreakpoints( HSQUIRRELVM vm )
{
	for ( int i = m_DataWatches.size(); i--; )
	{
		datawatch_t &dw = m_DataWatches[i];

		if ( dw.container && sq_type(dw.container->_obj) == OT_NULL )
		{
			DAP_START_EVENT( ++m_Sequence, "breakpoint" );
			DAP_SET_TABLE( body, 2 );
				body.SetString( "reason", "removed" );
				json_table_t &bp = body.SetTable( "breakpoint", 2 );
				bp.SetInt( "id", dw.id );
				bp.SetBool( "verified", false );
			DAP_SEND();

			FreeDataWatch( dw );
			m_DataWatches.remove(i);
			continue;
		}

		SQObjectPtr value;

		if ( Get( dw.obj, value ) )
		{
			if ( _rawval(dw.oldvalue) == _rawval(value) )
				continue;

			SQObjectPtr oldvalue = dw.oldvalue;
			dw.oldvalue = value;

			switch ( dw.condtype )
			{
				case DBC_NONE:
					break;

				case DBC_EQ:
					if ( _rawval(value) == _rawval(dw.condition) &&
							sq_type(value) == sq_type(dw.condition) )
						break;
					continue;

				case DBC_NE:
					if ( _rawval(value) != _rawval(dw.condition) ||
							sq_type(value) != sq_type(dw.condition) )
						break;
					continue;

				case DBC_G:
				case DBC_GE:
				case DBC_L:
				case DBC_LE:
					if ( CompareObj( value, dw.condition ) & dw.condtype )
						break;
					continue;

				case DBC_BWA:
					if ( sq_type(value) == OT_INTEGER &&
							( _integer(value) & _integer(dw.condition) ) != 0 )
						break;
					continue;

				case DBC_BWAZ:
					if ( sq_type(value) == OT_INTEGER &&
							( _integer(value) & _integer(dw.condition) ) == 0 )
						break;
					continue;

				case DBC_BWAEQ:
					if ( sq_type(value) == OT_INTEGER &&
							( _integer(value) & _integer(dw.condition) ) == _integer(dw.condition) )
						break;
					continue;

				default: UNREACHABLE();
			}

			if ( dw.hitsTarget )
			{
				if ( ++dw.hits < dw.hitsTarget )
					continue;

				dw.hits = 0;
			}

			stringbuf_t< 256 > buf;

			if ( dw.container )
			{
				buf.Put('[');
				buf.PutHex( _rawval(dw.container->_obj) );
				buf.Put(' ');
				buf.Puts( GetType( dw.container->_obj ) );
				buf.Put(']');
				buf.Put('-');
				buf.Put('>');
				buf.Puts( dw.name );
			}
			else
			{
				buf.Put('`');
				buf.Puts( dw.name );
				buf.Put('`');
			}

			buf.Puts(" changed (");
			buf.Puts( GetValue( oldvalue ) );
			buf.Puts(")->(");
			buf.Puts( GetValue( value ) );
			buf.Put(')');
			buf.Term();

			SQPrint( vm, _SC("(sqdbg) Data breakpoint hit: " FMT_CSTR "\n"), buf.ptr );

			Break( vm, { breakreason_t::DataBreakpoint, buf, dw.id } );
		}
		else
		{
			DAP_START_EVENT( ++m_Sequence, "breakpoint" );
			DAP_SET_TABLE( body, 2 );
				body.SetString( "reason", "removed" );
				json_table_t &bp = body.SetTable( "breakpoint", 2 );
				bp.SetInt( "id", dw.id );
				bp.SetBool( "verified", false );
			DAP_SEND();

			stringbuf_t< 256 > buf;

			if ( dw.container )
			{
				buf.Put('[');
				buf.PutHex( _rawval(dw.container->_obj) );
				buf.Put(' ');
				buf.Puts( GetType( dw.container->_obj ) );
				buf.Put(']');
				buf.Put('-');
				buf.Put('>');
				buf.Puts( dw.name );
			}
			else
			{
				buf.Put('`');
				buf.Puts( dw.name );
				buf.Put('`');
			}

			buf.Puts(" was removed");
			buf.Term();

			SQPrint( vm, _SC("(sqdbg) Data breakpoint hit: " FMT_CSTR "\n"), buf.ptr );

			Break( vm, { breakreason_t::DataBreakpoint, buf, dw.id } );

			FreeDataWatch( dw );
			m_DataWatches.remove(i);
		}
	}
}

void SQDebugServer::FreeDataWatch( datawatch_t &dw )
{
	if ( dw.container )
		__ObjRelease( dw.container );

	if ( dw.condtype != DBC_NONE )
	{
		if ( is_delegable(dw.condition) && _delegable(dw.condition)->_delegate )
		{
			sq_release( m_pRootVM, &dw.condition );
			dw.condition.Null();
		}
	}

	FreeString( &dw.name );
}

void SQDebugServer::RemoveDataBreakpoints()
{
	for ( int i = 0; i < m_DataWatches.size(); i++ )
		FreeDataWatch( m_DataWatches[i] );

	m_DataWatches.clear();
}

static inline bool HasEscapes( const SQChar *src, SQInteger len )
{
	const SQChar *end = src + len;

	for ( ; src < end; src++ )
	{
		switch ( *src )
		{
			case '\"': case '\\':
			case '\a': case '\b': case '\f':
			case '\n': case '\r': case '\t': case '\v':
			case '\0':
				return true;

			default:
#ifdef SQUNICODE
				if ( !IN_RANGE( *src, 0x20, 0x7E ) )
#else
				if ( !IN_RANGE_CHAR( *(unsigned char*)src, 0x20, 0x7E ) )
#endif
				{
#ifdef SQUNICODE
					if ( *src < 0x80 )
					{
						return true;
					}
					else
					{
						int ret = IsValidUnicode( src, end - src );
						if ( ret > 0 )
						{
							src += ret - 1;
						}
						else
						{
							return true;
						}
					}
#else
					if ( *(unsigned char*)src < 0x80 )
					{
						return true;
					}
					else
					{
						int ret = IsValidUTF8( (unsigned char*)src, end - src );
						if ( ret != 0 )
						{
							src += ret - 1;
						}
						else
						{
							return true;
						}
					}
#endif
				}
		}
	}

	return false;
}

#ifndef SQUNICODE
static inline int CountEscapes( const char *src, SQInteger len )
{
	const char *end = src + len;
	int count = 0;

	for ( ; src < end; src++ )
	{
		switch ( *src )
		{
			case '\"': case '\\':
			case '\a': case '\b': case '\f':
			case '\n': case '\r': case '\t': case '\v':
			case '\0':
				count++;
				break;

			default:
				if ( !IN_RANGE_CHAR( *(unsigned char*)src, 0x20, 0x7E ) )
				{
					int ret = IsValidUTF8( (unsigned char*)src, end - src );
					if ( ret != 0 )
					{
						src += ret - 1;
					}
					else
					{
						count += sizeof(char) * 2 + 1;
					}
				}
		}
	}

	return count;
}
#endif // !SQUNICODE

#define _memmove( dst, src, count, bound ) \
	Assert( (dst) + (count) <= bound ); \
	memmove( (dst), (src), (count) );

static void Escape( char *dst, int *len, int size )
{
	if ( size < *len )
		*len = size;

	char *strEnd = dst + *len;
	char *memEnd = dst + size;

	for ( ; dst < strEnd; dst++ )
	{
		switch ( *dst )
		{
			case '\"': case '\\':
			case '\a': case '\b': case '\f':
			case '\n': case '\r': case '\t': case '\v':
			case '\0':
			{
				if ( dst + 1 >= memEnd )
				{
					dst[0] = '?';
					break;
				}

				_memmove( dst + 1, dst, memEnd - ( dst + 1 ), memEnd );
				*dst++ = '\\';

				switch ( *dst )
				{
					case '\"':
					case '\\': break;
					case '\a': *dst = 'a'; break;
					case '\b': *dst = 'b'; break;
					case '\f': *dst = 'f'; break;
					case '\n': *dst = 'n'; break;
					case '\r': *dst = 'r'; break;
					case '\t': *dst = 't'; break;
					case '\v': *dst = 'v'; break;
					case '\0': *dst = '0'; break;
					default: UNREACHABLE();
				}

				if ( strEnd < memEnd )
				{
					strEnd++;
					(*len)++;
				}

				break;
			}
			default:
				if ( !IN_RANGE_CHAR( *(unsigned char*)dst, 0x20, 0x7E ) )
				{
					int ret = 0;
					if ( *(unsigned char*)dst < 0x80 ||
							( ret = IsValidUTF8( (unsigned char*)dst, strEnd - dst ) ) == 0 )
					{
						if ( dst + 1 + sizeof(SQChar) * 2 >= memEnd )
						{
							do
							{
								*dst++ = '?';
							}
							while ( dst < memEnd );

							break;
						}

						SQChar val = (SQChar)((unsigned char*)dst)[0];

						_memmove( dst + 2 + sizeof(SQChar) * 2,
							dst + 1,
							memEnd - ( dst + 2 + sizeof(SQChar) * 2 ),
							memEnd );

						dst[0] = '\\';
						dst[1] = 'x';

						int l = printhex< true, false >( dst + 2, memEnd - ( dst + 2 ), val );
						dst += 1 + l;
						strEnd += 1 + l;
						*len += 1 + l;

						if ( strEnd > memEnd )
						{
							*len -= (int)( strEnd - memEnd );
							strEnd = memEnd;
						}
					}
					else if ( ret )
					{
						dst += ret - 1;
					}
				}
		}
	}
}

#ifndef SQUNICODE
static void UndoEscape( char *dst, int *len )
{
	char *end = dst + *len;

	for ( ; dst < end; dst++ )
	{
		if ( *dst != '\\' )
			continue;

		switch ( dst[1] )
		{
			case '\\':
shift_one:
				memmove( dst, dst + 1, end - dst );
				end--;
				(*len)--;
				break;
			case '\"': goto shift_one;
			case 'a': dst[1] = '\a'; goto shift_one;
			case 'b': dst[1] = '\b'; goto shift_one;
			case 'f': dst[1] = '\f'; goto shift_one;
			case 'n': dst[1] = '\n'; goto shift_one;
			case 'r': dst[1] = '\r'; goto shift_one;
			case 't': dst[1] = '\t'; goto shift_one;
			case 'v': dst[1] = '\v'; goto shift_one;
			case '0': dst[1] = '\0'; goto shift_one;
			case 'x':
			{
				atox( { dst + 2, sizeof(SQChar) * 2 }, (SQChar*)dst );
				memmove( dst + 1, (dst + 2) + sizeof(SQChar) * 2, end - ( (dst + 2) + sizeof(SQChar) * 2 ) );
				end -= sizeof(SQChar) * 2 + 1;
				*len -= sizeof(SQChar) * 2 + 1;
				break;
			}
		}
	}
}
#endif // !SQUNICODE

#if 0
template < typename T >
static int StringifiedBytesLength( int len )
{
	return len * ( 2 + sizeof(T) * 2 );
}

template < typename T >
static int StringifyBytes( T *str, int strLen, char *dst, int size )
{
	T *strEnd = str + strLen;
	int len = 0;

	for ( ; str < strEnd && len <= size - (int)( 2 + sizeof(T) * 2 ); str++ )
	{
		*dst++ = '\\';
		*dst++ = 'x';
		len++;
		len++;

		int l = printhex< true, false >( dst, size - len, *str );
		dst += l;
		len += l;
	}

	return len;
}

static bool ReadStringifiedBytes( char *dst, int *len )
{
	char *end = dst + *len;

	do
	{
		if ( dst[0] == '\\' && dst[1] == 'x' )
		{
			if ( !atox( { dst + 2, sizeof(SQChar) * 2 }, (SQChar*)dst ) )
				return false;

			memmove( dst + sizeof(SQChar), dst + 2 + sizeof(SQChar) * 2, end - ( dst + 2 + sizeof(SQChar) * 2 ) );
			end -= 2 + sizeof(SQChar) * 2 - sizeof(SQChar);
			(*len) -= 2 + sizeof(SQChar) * 2 - sizeof(SQChar);
			dst += sizeof(SQChar) - 1;
		}
	}
	while ( ++dst < end );

	return true;
}
#endif

static inline string_t SpecialFloatValue( SQFloat val )
{
#ifdef SQUSEDOUBLE
	if ( val == DBL_MAX )
	{
		return "DBL_MAX";
	}
	if ( val == DBL_MIN )
	{
		return "DBL_MIN";
	}
	if ( val == DBL_EPSILON )
	{
		return "DBL_EPSILON";
	}
#endif
	if ( val == FLT_MAX )
	{
		return "FLT_MAX";
	}
	if ( val == FLT_MIN )
	{
		return "FLT_MIN";
	}
	if ( val == FLT_EPSILON )
	{
		return "FLT_EPSILON";
	}
	return { 0, 0 };
}

string_t SQDebugServer::GetValue( const SQObject &obj, int flags )
{
	switch ( sq_type(obj) )
	{
		case OT_STRING:
		{
			if ( !( flags & kFS_NoQuote ) )
			{
#ifdef SQUNICODE
				int size = 2 + UTF8Length< kUTFEscape >( _string(obj)->_val, _string(obj)->_len );
#else
				int escapes = CountEscapes( _string(obj)->_val, _string(obj)->_len );
				int size = 2 + _string(obj)->_len + escapes;
#endif

				char *buf = ScratchPad( size );
				int len = 0;
#if 0
				if ( rawBytes )
				{
					// Identify keys while sending regular strings for values
					if ( flags & kFS_KeyVal )
						buf[len++] = 'R';

					buf[len++] = '\"';
					len += StringifyBytes(
							_string(obj)->_val,
							min( ( size - len ) / (int)sizeof(SQChar), (int)_string(obj)->_len ),
							buf + len,
							size - len - 1 );
				}
#endif

				buf[0] = '\"';
#ifdef SQUNICODE
				len = SQUnicodeToUTF8< kUTFEscape >( buf + 1, size - 2, _string(obj)->_val, _string(obj)->_len );
#else
				len = scstombs( buf + 1, size - 2, _string(obj)->_val, _string(obj)->_len );
				Escape( buf + 1, &len, size - 2 );
#endif
				len++;

				buf[len++] = '\"';
				Assert( len == size );

				return { buf, len };
			}
			else
			{
#ifdef SQUNICODE
				const int size = UTF8Length( _string(obj)->_val, _string(obj)->_len );
				char *buf = ScratchPad( size );
				int len = SQUnicodeToUTF8( buf, size, _string(obj)->_val, _string(obj)->_len );
				return { buf, len };
#else
				return _string(obj);
#endif
			}
		}
		case OT_FLOAT:
		{
			if ( flags & kFS_Decimal )
			{
				const int size = FMT_INT_LEN;
				char *buf = ScratchPad( size );
				int len = printint( buf, size, _integer(obj) );
				return { buf, len };
			}
			else if ( flags & kFS_Binary )
			{
				int len = 2 + ( sizeof( SQFloat ) << 3 );
				char *buf = ScratchPad( len );

				char *c = buf;
				*c++ = '0';
				*c++ = 'b';

				if ( flags & kFS_NoPrefix )
				{
					len -= 2;
					c -= 2;
				}

				for ( int i = ( sizeof( SQFloat ) << 3 ); i--; )
					*c++ = '0' + ( ( _integer(obj) & ( (SQUnsignedInteger)1 << i ) ) != 0 );

				return { buf, len };
			}
			else
			{
getfloat:
				string_t val = SpecialFloatValue( _float(obj) );

				if ( !val.ptr || ( flags & ( kFS_Float | kFS_FloatE | kFS_FloatG ) ) )
				{
					const int size = FMT_FLT_LEN + 1;
					val.ptr = ScratchPad( size );

					if ( flags & kFS_FloatE )
					{
						val.len = snprintf( val.ptr, size, "%e", _float(obj) );
					}
					else if ( flags & kFS_FloatG )
					{
						val.len = snprintf( val.ptr, size, "%g", _float(obj) );
					}
					else
					{
						val.len = snprintf( val.ptr, size, "%f", _float(obj) );
					}
				}

				return val;
			}
		}
		case OT_INTEGER:
		{
			if ( flags & kFS_Binary )
			{
				int i;

				if ( !( flags & kFS_Padding ) )
				{
					// Print at 1, 2, 4 byte boundaries
					if ( (SQUnsignedInteger)_integer(obj) > 0xFFFFFFFF )
					{
						i = sizeof( SQInteger ) * 8;
					}
					else if ( (SQUnsignedInteger)_integer(obj) > 0x0000FFFF )
					{
						i = 4 * 8;
					}
					else if ( (SQUnsignedInteger)_integer(obj) > 0x000000FF )
					{
						i = 2 * 8;
					}
					else
					{
						i = 1 * 8;
					}
				}
				else
				{
					i = sizeof( SQInteger ) * 8;
				}

				int len = 2 + i;
				char *buf = ScratchPad( len );
				char *c = buf;
				*c++ = '0';
				*c++ = 'b';

				if ( flags & kFS_NoPrefix )
				{
					len -= 2;
					c -= 2;
				}

				while ( i-- )
					*c++ = '0' + ( ( _integer(obj) & ( (SQUnsignedInteger)1 << i ) ) != 0 );

				return { buf, len };
			}
			else if ( flags & kFS_Octal )
			{
				const int size = FMT_OCT_LEN;
				char *buf = ScratchPad( size );
				int len = printoct( buf, size, (SQUnsignedInteger)_integer(obj) );
				return { buf, len };
			}
			else if ( flags & kFS_Float )
			{
				goto getfloat;
			}
			else if ( flags & kFS_Character )
			{
				const int size = FMT_INT_LEN + FMT_PTR_LEN + 3;
				char *buf = ScratchPad( size );
				int len;

				if ( _integer(obj) > (SQInteger)( 1 << ( ( sizeof(SQChar) << 3 ) - 1 ) ) )
				{
					len = printint( buf, size, _integer(obj) );
					return { buf, len };
				}

				SQChar ch = (SQChar)_integer(obj);

				if ( !(flags & kFS_Hexadecimal) )
				{
#ifdef SQUNICODE
					len = printint( buf, size, ch );
#else
					len = printint( buf, size, (unsigned char)ch );
#endif
				}
				else
				{
					len = printhex< false >( buf, size, ch );
				}

				buf[len++] = ' ';
				buf[len++] = '\'';

				if ( IN_RANGE( ch, 0x20, 0x7E ) )
				{
					switch ( ch )
					{
						case '\'': buf[len++] = '\\'; break;
						case '\\': buf[len++] = '\\'; break;
					}

					buf[len++] = (char)ch;
				}
				else
				{
#ifdef SQUNICODE
					if ( ch <= (SQChar)0xFF )
					{
						buf[len++] = '\\';
						buf[len++] = 'x';
						len += printhex< true, false, false >( buf + len, size, (char)ch );
					}
					else if ( IsValidUnicode( &ch, 1 ) )
					{
						len += SQUnicodeToUTF8( buf + len, size - len - 1, &ch, 1 );
					}
					else
#endif
					{
						buf[len++] = '\\';
						buf[len++] = 'x';
						len += printhex< true, false, false >( buf + len, size, ch );
					}
				}

				buf[len++] = '\'';

				return { buf, len };
			}
			// Check hex last to make watch format specifiers overwrite "format.hex" client option
			else if ( ( flags & kFS_Hexadecimal ) && !( flags & kFS_Decimal ) )
			{
				const int size = FMT_PTR_LEN;
				char *buf = ScratchPad( size );
				int len;

				if ( !( flags & kFS_Uppercase ) )
				{
					if ( !( flags & kFS_NoPrefix ) )
					{
						if ( !( flags & kFS_Padding ) )
						{
							len = printhex< false, true, false >( buf, size, (SQUnsignedInteger)_integer(obj) );
						}
						else
						{
							len = printhex< true, true, false >( buf, size, (SQUnsignedInteger)_integer(obj) );
						}
					}
					else
					{
						if ( !( flags & kFS_Padding ) )
						{
							len = printhex< false, false, false >( buf, size, (SQUnsignedInteger)_integer(obj) );
						}
						else
						{
							len = printhex< true, false, false >( buf, size, (SQUnsignedInteger)_integer(obj) );
						}
					}
				}
				else
				{
					if ( !( flags & kFS_NoPrefix ) )
					{
						if ( !( flags & kFS_Padding ) )
						{
							len = printhex< false, true, true >( buf, size, (SQUnsignedInteger)_integer(obj) );
						}
						else
						{
							len = printhex< true, true, true >( buf, size, (SQUnsignedInteger)_integer(obj) );
						}
					}
					else
					{
						if ( !( flags & kFS_Padding ) )
						{
							len = printhex< false, false, true >( buf, size, (SQUnsignedInteger)_integer(obj) );
						}
						else
						{
							len = printhex< true, false, true >( buf, size, (SQUnsignedInteger)_integer(obj) );
						}
					}
				}

				return { buf, len };
			}
			else
			{
				const int size = FMT_INT_LEN;
				char *buf = ScratchPad( size );
				int len = printint( buf, size, _integer(obj) );
				return { buf, len };
			}
		}
		case OT_BOOL:
		{
			if ( _integer(obj) )
				return "true";

			return "false";
		}
		case OT_NULL:
		{
			return "null";
		}
		case OT_ARRAY:
		{
			const int size = STRLEN(" {size = }") + FMT_PTR_LEN + FMT_INT_LEN;

			stringbufext_t buf( ScratchPad( size ), size );

			if ( !( flags & kFS_NoAddr ) )
			{
				buf.PutHex( _rawval(obj) );
				buf.Put(' ');
			}

			buf.Puts("{size = ");

			if ( !( flags & kFS_Hexadecimal ) )
			{
				buf.PutInt( _array(obj)->_values.size() );
			}
			else
			{
				buf.PutHex( _array(obj)->_values.size(), false );
			}

			buf.Put('}');

			return buf;
		}
		case OT_TABLE:
		{
			const int size = STRLEN(" {size = }") + FMT_PTR_LEN + FMT_INT_LEN;

			stringbufext_t buf( ScratchPad( size ), size );

			if ( !( flags & kFS_NoAddr ) )
			{
				buf.PutHex( _rawval(obj) );
				buf.Put(' ');
			}

			buf.Puts("{size = ");

			if ( !( flags & kFS_Hexadecimal ) )
			{
				buf.PutInt( _table(obj)->CountUsed() );
			}
			else
			{
				buf.PutHex( _table(obj)->CountUsed(), false );
			}

			buf.Put('}');

			return buf;
		}
		case OT_INSTANCE:
		{
			SQClass *base = _instance(obj)->_class;
			Assert( base );
			const SQObjectPtr *def = GetClassDefValue( base );

			if ( def )
			{
				SQObjectPtr res;

				if ( RunClosure( *def, &obj, res ) && sq_type(res) == OT_STRING )
				{
					if ( !( flags & kFS_NoAddr ) )
					{
						const int size = 1024;

						stringbufext_t buf( ScratchPad( size ), size );
						buf.PutHex( _rawval(obj) );
						buf.Put(' ');
						buf.Put('{');
						buf.Puts( _string(res) );
						buf.Put('}');

						return buf;
					}
					else
					{
#ifdef SQUNICODE
						const int size = UTF8Length( _string(res)->_val, _string(res)->_len );
						char *buf = ScratchPad( size );
						int len = SQUnicodeToUTF8( buf, size, _string(res)->_val, _string(res)->_len );
						return { buf, len };
#else
						const int size = _string(res)->_len;
						char *buf = ScratchPad( size );
						memcpy( buf, _string(res)->_val, size );
						return { buf, size };
#endif
					}
				}
			}

			goto default_label;
		}
		case OT_CLASS:
		{
			const classdef_t *def = FindClassDef( _class(obj) );

			if ( def && def->name.ptr )
			{
				if ( !( flags & kFS_NoAddr ) )
				{
					return def->name;
				}
				else
				{
					Assert( def->name.len >= FMT_PTR_LEN + 1 );
					return { def->name.ptr + FMT_PTR_LEN + 1, def->name.len - FMT_PTR_LEN - 1 };
				}
			}

			goto default_label;
		}
		case OT_CLOSURE:
		case OT_NATIVECLOSURE:
		{
			const SQObjectPtr *name;

			if ( sq_type(obj) == OT_CLOSURE )
			{
				name = &_fp(_closure(obj)->_function)->_name;
			}
			else
			{
				name = &_nativeclosure(obj)->_name;
			}

			if ( sq_type(*name) == OT_STRING )
			{
#ifdef SQUNICODE
				const int size = FMT_PTR_LEN + 1 + UTF8Length( _string(*name)->_val, _string(*name)->_len );
#else
				const int size = FMT_PTR_LEN + 1 + _string(*name)->_len;
#endif
				char *buf = ScratchPad( size );
				int len = printhex( buf, size, _rawval(obj) );
				buf[len++] = ' ';
				len += scstombs( buf + len, size - FMT_PTR_LEN - 1, _string(*name)->_val, _string(*name)->_len );
				return { buf, len };
			}

			goto default_label;
		}
		default:
		default_label:
		{
			const int size = FMT_PTR_LEN;
			char *buf = ScratchPad( size );
			int len = printhex( buf, size, _rawval(obj) );
			return { buf, len };
		}
	}
}

// To make sure strings are not unnecessarily allocated and iterated through
// Escape and quote strings while writing to json
void SQDebugServer::JSONSetString( json_table_t &elem, const string_t &key, const SQObject &obj, int flags )
{
	switch ( sq_type(obj) )
	{
		case OT_NULL:
		{
			elem.SetString( key, "null" );
			break;
		}
		case OT_BOOL:
		{
			if ( _integer(obj) )
			{
				elem.SetString( key, "true" );
			}
			else
			{
				elem.SetString( key, "false" );
			}

			break;
		}
		case OT_STRING:
		{
			elem.SetString( key, _string(obj), !( flags & kFS_NoQuote ) );
			break;
		}
		default:
		{
			elem.SetString( key, GetValue( obj, flags ) );
		}
	}
}

bool SQDebugServer::IsJumpOp( const SQInstruction *instr )
{
	return ( instr->op == _OP_JMP ||
			instr->op == _OP_AND ||
			instr->op == _OP_OR ||
#if SQUIRREL_VERSION_NUMBER >= 300
			instr->op == _OP_JCMP ||
#else
			instr->op == _OP_JNZ ||
#endif
			instr->op == _OP_JZ ||
			instr->op == _OP_FOREACH ||
			instr->op == _OP_POSTFOREACH );
}

int SQDebugServer::GetJumpCount( const SQInstruction *instr )
{
	Assert( IsJumpOp( instr ) );

	if ( instr->op != _OP_POSTFOREACH )
		return instr->_arg1;

	return instr->_arg1 - 1;
}

// Line ops are ignored in disassembly, but jump ops account for them.
// Count out all line ops in the jump
// Doing this for setting instructions adds too much complexity that is not worth the effort.
int SQDebugServer::DeduceJumpCount( SQInstruction *instr )
{
	Assert( IsJumpOp( instr ) );

	int arg1 = GetJumpCount( instr );
	int sign = ( arg1 < 0 );
	if ( sign )
		arg1 = -arg1;

	for ( SQInstruction *ip = instr + GetJumpCount( instr );
			ip != instr;
			ip += sign ? 1 : -1 )
	{
		if ( ip->op == _OP_LINE )
			arg1--;
	}

	if ( sign )
		arg1 = -arg1;

	return arg1;
}

// to display the local variable name only in the target on local variable declarations
SQUnsignedInteger SQDebugServer::s_nTargetAssignment = 0;

SQString *SQDebugServer::GetLocalVarName( SQFunctionProto *func, SQInstruction *instr, unsigned int pos )
{
	SQUnsignedInteger ip = (SQUnsignedInteger)( instr - func->_instructions );

	for ( int i = 0; i < func->_nlocalvarinfos; i++ )
	{
		const SQLocalVarInfo &var = func->_localvarinfos[i];

		if ( (unsigned int)var._pos == pos &&
				var._start_op <= ip + s_nTargetAssignment && var._end_op >= ip - 1 )
		{
			return sq_type(var._name) == OT_STRING ? _string(var._name) : NULL;
		}
	}

	return NULL;
}

void SQDebugServer::PrintStackVar( SQFunctionProto *func, SQInstruction *instr, unsigned int pos,
		stringbufext_t &buf )
{
	buf.Put( '[' );

	SQString *var = GetLocalVarName( func, instr, pos );

	if ( !var )
	{
		buf.PutInt( (int)pos );
	}
	else
	{
		buf.Puts( var );
	}

	buf.Put( ']' );
}

void SQDebugServer::PrintOuter( SQFunctionProto *func, int pos, stringbufext_t &buf )
{
	SQString *val = _string(func->_outervalues[pos]._name);

	buf.Put( '[' );
	buf.Puts( val );
	buf.Put( ']' );
}

void SQDebugServer::PrintLiteral( SQFunctionProto *func, int pos, stringbufext_t &buf )
{
	string_t val = GetValue( func->_literals[pos] );

	if ( val.len > 64 )
		val.len = 64;

	buf.Puts( val );
}

void SQDebugServer::PrintStackTarget( SQFunctionProto *func, SQInstruction *instr,
		stringbufext_t &buf )
{
	if ( instr->_arg0 != 0xFF )
	{
		s_nTargetAssignment = 1;
		PrintStackVar( func, instr, instr->_arg0, buf );
		s_nTargetAssignment = 0;
		buf.Puts( " = " );
	}
}

void SQDebugServer::PrintStackTargetVar( SQFunctionProto *func, SQInstruction *instr,
		stringbufext_t &buf )
{
	if ( instr->_arg0 != 0xFF )
	{
		s_nTargetAssignment = 1;
		PrintStackVar( func, instr, instr->_arg0, buf );
		s_nTargetAssignment = 0;
	}
}

void SQDebugServer::PrintDeref( SQFunctionProto *func, SQInstruction *instr,
		unsigned int self, unsigned int key, stringbufext_t &buf )
{
	PrintStackVar( func, instr, self, buf );
	buf.Puts( "->" );
	PrintStackVar( func, instr, key, buf );
}

void SQDebugServer::DescribeInstruction( SQInstruction *instr, SQFunctionProto *func, stringbufext_t &buf )
{
#if SQUIRREL_VERSION_NUMBER < 212
	return;
#else
	buf.Puts( g_InstructionName[ instr->op ] );
	buf.Put( ' ' );

	switch ( instr->op )
	{
		case _OP_LOADNULLS:
		{
			PrintStackVar( func, instr, instr->_arg0, buf );
			buf.Put( ' ' );
			buf.PutInt( instr->_arg1 );
			break;
		}
		case _OP_LOADINT:
		{
			PrintStackTarget( func, instr, buf );
			buf.PutInt( instr->_arg1 );
			break;
		}
		case _OP_LOADFLOAT:
		{
			PrintStackTarget( func, instr, buf );
#if SQUIRREL_VERSION_NUMBER >= 300
LFLOAT:
#endif
			string_t val = SpecialFloatValue( *(SQFloat*)&instr->_arg1 );

			if ( !val.ptr )
			{
				int l = snprintf( buf.ptr + buf.len, buf.BytesLeft(), "%g", *(SQFloat*)&instr->_arg1 );
				if ( l < 0 || l > buf.BytesLeft() )
					l = buf.BytesLeft();

				buf.len += l;
			}
			else
			{
				buf.Puts( val );
			}

			break;
		}
		case _OP_LOADBOOL:
		{
			PrintStackTarget( func, instr, buf );
#if SQUIRREL_VERSION_NUMBER >= 300
LBOOL:
#endif
			if ( instr->_arg1 )
			{
				buf.Puts( "true" );
			}
			else
			{
				buf.Puts( "false" );
			}

			break;
		}
		case _OP_FOREACH:
		{
			PrintStackVar( func, instr, instr->_arg2, buf );
			buf.Puts( ", " );
			PrintStackVar( func, instr, instr->_arg2 + 1, buf );
			buf.Puts( " in " );
			PrintStackVar( func, instr, instr->_arg0, buf );
			buf.Puts( " jmp " );
			buf.PutInt( instr->_arg1 );
			break;
		}
		case _OP_POSTFOREACH:
		{
			PrintStackVar( func, instr, instr->_arg0, buf );
			buf.Puts( " jmp " );
			buf.PutInt( instr->_arg1 - 1 );
			break;
		}
		case _OP_PUSHTRAP:
		{
			PrintStackVar( func, instr, instr->_arg0, buf );
			buf.Puts( " jmp " );
			buf.PutInt( instr->_arg1 );
			break;
		}
		case _OP_TAILCALL:
		case _OP_CALL:
		{
			PrintStackTarget( func, instr, buf );
			PrintStackVar( func, instr, instr->_arg1, buf );
			buf.Put( ' ' );
			buf.PutInt( instr->_arg3 );
			break;
		}
		case _OP_RETURN:
		case _OP_YIELD:
		{
			if ( instr->_arg0 != 0xFF )
			{
				PrintStackVar( func, instr, instr->_arg1, buf );
			}
			else
			{
				buf.len--;
			}

			break;
		}
		case _OP_MOVE:
		case _OP_NEG:
		case _OP_NOT:
		case _OP_BWNOT:
		case _OP_TYPEOF:
		case _OP_RESUME:
		case _OP_CLONE:
		{
			PrintStackTarget( func, instr, buf );

			switch ( instr->op )
			{
				case _OP_MOVE:		break;
				case _OP_NEG:		buf.Put( '-' ); break;
				case _OP_NOT:		buf.Put( '!' ); break;
				case _OP_BWNOT:		buf.Put( '~' ); break;
				case _OP_TYPEOF:	buf.Puts( "typeof " ); break;
				case _OP_RESUME:	buf.Puts( "resume " ); break;
				case _OP_CLONE:		buf.Puts( "clone " ); break;
				default: UNREACHABLE();
			}

			PrintStackVar( func, instr, instr->_arg1, buf );
			break;
		}
#if SQUIRREL_VERSION_NUMBER >= 300
		case _OP_LOADROOT:
#else
		case _OP_LOADROOTTABLE:
#endif
		case _OP_THROW:
		{
			PrintStackVar( func, instr, instr->_arg0, buf );
			break;
		}
		case _OP_LOAD:
		{
			PrintStackTarget( func, instr, buf );
			PrintLiteral( func, instr->_arg1, buf );
			break;
		}
		case _OP_DLOAD:
		{
			PrintStackTarget( func, instr, buf );
			PrintLiteral( func, instr->_arg1, buf );
			buf.Puts( ", " );
			PrintStackVar( func, instr, instr->_arg2, buf );
			buf.Puts( " = " );
			PrintLiteral( func, instr->_arg3, buf );
			break;
		}
		case _OP_DMOVE:
		{
			PrintStackTarget( func, instr, buf );
			PrintStackVar( func, instr, instr->_arg1, buf );
			buf.Puts( ", " );
			PrintStackVar( func, instr, instr->_arg2, buf );
			buf.Puts( " = " );
			PrintStackVar( func, instr, instr->_arg3, buf );
			break;
		}
		case _OP_GETK:
		case _OP_PREPCALLK:
		{
			PrintStackTarget( func, instr, buf );
			PrintStackVar( func, instr, instr->_arg2, buf );
			buf.Puts( "->" );
			PrintLiteral( func, instr->_arg1, buf );
			break;
		}
		case _OP_PREPCALL:
		{
			PrintStackTarget( func, instr, buf );
			PrintDeref( func, instr, instr->_arg2, instr->_arg1, buf );
			break;
		}
		case _OP_DELETE:
		case _OP_GET:
		case _OP_SET:
		{
			PrintStackTarget( func, instr, buf );
			PrintDeref( func, instr, instr->_arg1, instr->_arg2, buf );

			if ( instr->op == _OP_SET )
			{
				buf.Puts( " = " );
				PrintStackVar( func, instr, instr->_arg3, buf );
			}

			break;
		}
		case _OP_NEWSLOT:
		{
			PrintStackTarget( func, instr, buf );
			PrintDeref( func, instr, instr->_arg1, instr->_arg2, buf );
			buf.Puts( " = " );
			PrintStackVar( func, instr, instr->_arg3, buf );
			break;
		}
		case _OP_NEWSLOTA:
		{
			if ( instr->_arg0 & NEW_SLOT_STATIC_FLAG )
				buf.Puts( "static " );

			PrintDeref( func, instr, instr->_arg1, instr->_arg2, buf );
			buf.Puts( " = " );
			PrintStackVar( func, instr, instr->_arg3, buf );

			if ( instr->_arg0 & NEW_SLOT_ATTRIBUTES_FLAG )
			{
				buf.Puts( ", " );
				PrintStackVar( func, instr, instr->_arg2 - 1, buf );
			}

			break;
		}
		case _OP_EXISTS:
		{
			PrintStackTarget( func, instr, buf );
			PrintStackVar( func, instr, instr->_arg2, buf );
			buf.Puts( " in " );
			PrintStackVar( func, instr, instr->_arg1, buf );
			break;
		}
		case _OP_INSTANCEOF:
		{
			PrintStackTarget( func, instr, buf );
			PrintStackVar( func, instr, instr->_arg2, buf );
			buf.Puts( " instanceof " );
			PrintStackVar( func, instr, instr->_arg1, buf );
			break;
		}
		case _OP_AND:
		{
			PrintStackTarget( func, instr, buf );
			PrintStackVar( func, instr, instr->_arg2, buf );
			buf.Puts( " jz " );
			buf.PutInt( DeduceJumpCount( instr ) );
			break;
		}
		case _OP_OR:
		{
			PrintStackTarget( func, instr, buf );
			PrintStackVar( func, instr, instr->_arg2, buf );
			buf.Puts( " jnz " );
			buf.PutInt( DeduceJumpCount( instr ) );
			break;
		}
		case _OP_JZ:
#if SQUIRREL_VERSION_NUMBER < 300
		case _OP_JNZ:
#endif
		{
			PrintStackVar( func, instr, instr->_arg0, buf );
			buf.Put( ' ' );
		case _OP_JMP:
			buf.PutInt( DeduceJumpCount( instr ) );
			break;
		}
		case _OP_EQ:
		case _OP_NE:
		{
			PrintStackTarget( func, instr, buf );
			PrintStackVar( func, instr, instr->_arg2, buf );

			if ( instr->op == _OP_EQ )
			{
				buf.Puts( " == " );
			}
			else
			{
				buf.Puts( " != " );
			}

			if ( instr->_arg3 == 0 )
			{
				PrintStackVar( func, instr, instr->_arg1, buf );
			}
			else
			{
				PrintLiteral( func, instr->_arg1, buf );
			}

			break;
		}
#if SQUIRREL_VERSION_NUMBER >= 300
		case _OP_JCMP:
		case _OP_CMP:
		{
			if ( instr->op == _OP_CMP )
#else
		case _OP_CMP:
		{
#endif
				PrintStackTarget( func, instr, buf );

			PrintStackVar( func, instr, instr->_arg2, buf );

			switch ( instr->_arg3 )
			{
				case CMP_G:		buf.Puts( " > " ); break;
				case CMP_GE:	buf.Puts( " >= " ); break;
				case CMP_L:		buf.Puts( " < " ); break;
				case CMP_LE:	buf.Puts( " <= " ); break;
#if SQUIRREL_VERSION_NUMBER >= 300
				case CMP_3W:	buf.Puts( " <=> " ); break;
#endif
				default: UNREACHABLE();
			}

#if SQUIRREL_VERSION_NUMBER >= 300
			if ( instr->op == _OP_JCMP )
			{
				PrintStackVar( func, instr, instr->_arg0, buf );
				buf.Puts( " jz " );
				buf.PutInt( DeduceJumpCount( instr ) );
			}
			else
#endif
			{
				PrintStackVar( func, instr, instr->_arg1, buf );
			}

			break;
		}
		case _OP_BITW:
		{
			unsigned char lhs = instr->_arg2;
			unsigned char rhs = (unsigned char)instr->_arg1;

			if ( instr->_arg0 != lhs )
				PrintStackTarget( func, instr, buf );

			PrintStackVar( func, instr, lhs, buf );
			buf.Put( ' ' );

			switch ( instr->_arg3 )
			{
				case BW_AND:		buf.Put( '&' ); break;
				case BW_OR:			buf.Put( '|' ); break;
				case BW_XOR:		buf.Put( '^' ); break;
				case BW_SHIFTL:		buf.Puts( "<<" ); break;
				case BW_SHIFTR:		buf.Puts( ">>" ); break;
				case BW_USHIFTR:	buf.Puts( ">>>" ); break;
				default: UNREACHABLE();
			}

			if ( instr->_arg0 == lhs )
				buf.Put( '=' );

			buf.Put( ' ' );
			PrintStackVar( func, instr, rhs, buf );
			break;
		}
#if SQUIRREL_VERSION_NUMBER >= 300
		case _OP_ADD:
		case _OP_SUB:
		case _OP_MUL:
		case _OP_DIV:
		case _OP_MOD:
		{
			unsigned char lhs = instr->_arg2;
			unsigned char rhs = (unsigned char)instr->_arg1;

			if ( instr->_arg0 != lhs )
				PrintStackTarget( func, instr, buf );

			PrintStackVar( func, instr, lhs, buf );
			buf.Put( ' ' );

			switch ( instr->op )
			{
				case _OP_ADD: buf.Put( '+' ); break;
				case _OP_SUB: buf.Put( '-' ); break;
				case _OP_MUL: buf.Put( '*' ); break;
				case _OP_DIV: buf.Put( '/' ); break;
				case _OP_MOD: buf.Put( '%' ); break;
				default: UNREACHABLE();
			}

			if ( instr->_arg0 == lhs )
				buf.Put( '=' );

			buf.Put( ' ' );
			PrintStackVar( func, instr, rhs, buf );
			break;
		}
#else
		case _OP_ARITH:
		{
			PrintStackTarget( func, instr, buf );
			PrintStackVar( func, instr, instr->_arg2, buf );
			buf.Put( ' ' );
			buf.Put( instr->_arg3 );
			buf.Put( ' ' );
			PrintStackVar( func, instr, instr->_arg1, buf );
			break;
		}
		case _OP_COMPARITHL:
		{
			PrintStackTarget( func, instr, buf );
			PrintStackVar( func, instr, instr->_arg1, buf );
			buf.Put( ' ' );
			buf.Put( instr->_arg3 );
			buf.Put( '=' );
			buf.Put( ' ' );
			PrintStackVar( func, instr, instr->_arg2, buf );
			break;
		}
#endif
		case _OP_COMPARITH:
		{
			PrintStackTarget( func, instr, buf );
			PrintDeref( func, instr,
					( (unsigned)instr->_arg1 & 0xFFFF0000 ) >> 16,
					instr->_arg2,
					buf );
			buf.Put( ' ' );
			buf.Put( instr->_arg3 );
			buf.Put( '=' );
			buf.Put( ' ' );
			PrintStackVar( func, instr, ( instr->_arg1 & 0x0000FFFF ), buf );
			break;
		}
		case _OP_INCL:
		case _OP_INC:
		{
			if ( instr->_arg0 != instr->_arg1 )
				PrintStackTarget( func, instr, buf );

			if ( instr->_arg3 == (unsigned char)-1 )
			{
				buf.Puts( "--" );
			}
			else
			{
				buf.Puts( "++" );
			}

			if ( instr->op == _OP_INCL )
			{
				PrintStackVar( func, instr, instr->_arg1, buf );
			}
			else
			{
				PrintDeref( func, instr, instr->_arg1, instr->_arg2, buf );
			}

			break;
		}
		case _OP_PINCL:
		case _OP_PINC:
		{
			PrintStackTarget( func, instr, buf );

			if ( instr->op == _OP_PINCL )
			{
				PrintStackVar( func, instr, instr->_arg1, buf );
			}
			else
			{
				PrintDeref( func, instr, instr->_arg1, instr->_arg2, buf );
			}

			if ( instr->_arg3 == (unsigned char)-1 )
			{
				buf.Puts( "--" );
			}
			else
			{
				buf.Puts( "++" );
			}

			break;
		}
#if SQUIRREL_VERSION_NUMBER >= 300
		case _OP_SETOUTER:
		case _OP_GETOUTER:
#else
		case _OP_LOADFREEVAR:
#endif
		{
			PrintStackTarget( func, instr, buf );
			PrintOuter( func, instr->_arg1, buf );

#if SQUIRREL_VERSION_NUMBER >= 300
			if ( instr->op == _OP_SETOUTER )
			{
				buf.Puts( " = " );
				PrintStackVar( func, instr, instr->_arg2, buf );
			}
#endif
			break;
		}
		case _OP_CLOSURE:
		{
			PrintStackTargetVar( func, instr, buf );

#if SQUIRREL_VERSION_NUMBER >= 300
			if ( instr->_arg2 != 0xFF )
			{
				buf.Put( ' ' );
				PrintStackVar( func, instr, instr->_arg2, buf );
			}
#endif
			break;
		}
#if SQUIRREL_VERSION_NUMBER >= 300
		case _OP_APPENDARRAY:
		{
			PrintStackVar( func, instr, instr->_arg0, buf );
			buf.Puts( " <- " );

			switch ( instr->_arg2 )
			{
				case AAT_STACK:		PrintStackVar( func, instr, instr->_arg1, buf ); break;
				case AAT_INT:		buf.PutInt( instr->_arg1 ); break;
				case AAT_FLOAT:		goto LFLOAT;
				case AAT_BOOL:		goto LBOOL;
				case AAT_LITERAL:	PrintLiteral( func, instr->_arg1, buf ); break;
				default: UNREACHABLE();
			}

			break;
		}
		case _OP_NEWOBJ:
		{
			PrintStackTarget( func, instr, buf );

			switch ( instr->_arg3 )
			{
				case NOT_TABLE:
				{
					buf.Puts( "TABLE " );
					buf.PutInt( instr->_arg1 );
					break;
				}
				case NOT_ARRAY:
				{
					buf.Puts( "ARRAY " );
					buf.PutInt( instr->_arg1 );
					break;
				}
				case NOT_CLASS:
				{
					buf.Puts( "CLASS" );

					if ( instr->_arg1 != -1 )
					{
						buf.Puts( " extends " );
						PrintStackVar( func, instr, instr->_arg1, buf );
					}

					break;
				}
				default: UNREACHABLE();
			}

			break;
		}
#else
		case _OP_APPENDARRAY:
		{
			PrintStackVar( func, instr, instr->_arg0, buf );
			buf.Puts( " <- " );

			if ( instr->_arg3 != 0 )
			{
				PrintLiteral( func, instr->_arg1, buf );
			}
			else
			{
				PrintStackVar( func, instr, instr->_arg1, buf );
			}

			break;
		}
		case _OP_CLASS:
		{
			PrintStackTargetVar( func, instr, buf );

			if ( instr->_arg1 != -1 )
			{
				buf.Puts( " extends " );
				PrintStackVar( func, instr, instr->_arg1, buf );
			}

			break;
		}
		case _OP_NEWTABLE:
		case _OP_NEWARRAY:
		{
			PrintStackTargetVar( func, instr, buf );
			buf.Put( ' ' );
			buf.PutInt( instr->_arg1 );
			break;
		}
		case _OP_DELEGATE:
		{
			PrintStackTarget( func, instr, buf );
			PrintStackVar( func, instr, instr->_arg1, buf );
			buf.Puts( " <- " );
			PrintStackVar( func, instr, instr->_arg2, buf );
			break;
		}
		case _OP_VARGC:
		{
			PrintStackVar( func, instr, instr->_arg0, buf );
			break;
		}
		case _OP_GETVARGV:
		{
			PrintStackTarget( func, instr, buf );
			buf.Puts( "vargv" );
			buf.Puts( "->" );
			PrintStackVar( func, instr, instr->_arg1, buf );
			break;
		}
#endif
		default:
		{
			buf.len--;
		}
	}
#endif
}

bool SQDebugServer::IsValidStackFrame( HSQUIRRELVM vm, int id )
{
	Assert( !!vm->_callsstacksize == !!vm->ci );
	return id >= 0 && id < vm->_callsstacksize && vm->_callsstacksize > 0;
}

int SQDebugServer::GetStackBase( HSQUIRRELVM vm, const SQVM::CallInfo *ci )
{
	Assert( ci >= vm->_callsstack && ci < vm->_callsstack + vm->_callsstacksize );

	int stackbase = 0;
	for ( ; ci >= vm->_callsstack; ci-- )
		stackbase += ci->_prevstkbase;
	return stackbase;
}

void SQDebugServer::InitEnv_GetVal( SQObjectPtr &env )
{
	Assert( sq_type(env) == OT_NULL ||
			( sq_type(env) == OT_TABLE && _table(env)->_delegate && !_table(env)->_delegate->_delegate ) );

	SQTable *mt;

	if ( sq_type(env) != OT_TABLE )
	{
		SQObjectPtr _null;

		mt = SQTable::Create( _ss(m_pRootVM), 6 );
		mt->NewSlot( m_sqstrCallFrame, _null );
		mt->NewSlot( m_sqstrDelegate, _null );

		Assert( sq_type(env) == OT_NULL );
		env = SQTable::Create( _ss(m_pRootVM), 0 );
		_table(env)->SetDelegate( mt );
	}
	else
	{
		mt = _table(env)->_delegate;
	}

#ifdef CLOSURE_ROOT
	mt->NewSlot( m_sqstrRoot, m_pRootVM->_roottable );
#endif
	mt->NewSlot( CreateSQString( m_pRootVM, _SC("_get") ), m_sqfnGet );
	mt->NewSlot( CreateSQString( m_pRootVM, _SC("_set") ), m_sqfnSet );
	mt->NewSlot( CreateSQString( m_pRootVM, _SC("_newslot") ), m_sqfnNewSlot );
}

void SQDebugServer::SetCallFrame( SQObjectPtr &env, HSQUIRRELVM vm, const SQVM::CallInfo *ci )
{
	Assert( sq_type(env) == OT_TABLE );
	Assert( _table(env)->_delegate );

	SQObjectPtr frame = (SQInteger)( ci - vm->_callsstack );
	Assert( _integer(frame) >= 0 && _integer(frame) < vm->_callsstacksize );
	Verify( _table(env)->_delegate->Set( m_sqstrCallFrame, frame ) );
}

void SQDebugServer::SetEnvDelegate( SQObjectPtr &env, const SQObject &delegate )
{
	Assert( sq_type(env) == OT_TABLE );
	Assert( _table(env)->_delegate );

	SQObjectPtr weakref;

	if ( ISREFCOUNTED( sq_type(delegate) ) )
		weakref = GetWeakRef( _refcounted(delegate), sq_type(delegate) );

	Verify( _table(env)->_delegate->Set( m_sqstrDelegate, weakref ) );
}

void SQDebugServer::ClearEnvDelegate( SQObjectPtr &env )
{
	Assert( sq_type(env) == OT_TABLE );
	Assert( _table(env)->_delegate );

	SQObjectPtr _null;
	_table(env)->_delegate->Set( m_sqstrDelegate, _null );
}

#ifdef CLOSURE_ROOT
void SQDebugServer::SetEnvRoot( SQObjectPtr &env, const SQObjectPtr &root )
{
	Assert( sq_type(env) == OT_TABLE );
	Assert( _table(env)->_delegate );
	Assert( sq_type(root) == OT_TABLE ||
			( sq_type(root) == OT_WEAKREF && sq_type(_weakref(root)->_obj) == OT_TABLE ) );

	Verify( _table(env)->_delegate->Set( m_sqstrRoot, root ) );
}
#endif

bool SQDebugServer::RunExpression( const string_t &expression, HSQUIRRELVM vm, const SQVM::CallInfo *ci,
		SQObjectPtr &out, bool multiline )
{
	Assert( !ci || ( ci >= vm->_callsstack && ci < vm->_callsstack + vm->_callsstacksize ) );

	// Fallback to root on native stack frame
	if ( !ci || sq_type(ci->_closure) != OT_CLOSURE )
#ifdef CLOSURE_ROOT
		return RunScript( vm, expression, NULL, NULL, out, multiline );
#else
		return RunScript( vm, expression, NULL, out, multiline );
#endif

#ifdef CLOSURE_ROOT
	bool bRoot = _closure(ci->_closure)->_root &&
		_table(_closure(ci->_closure)->_root->_obj) != _table(m_pRootVM->_roottable);

	if ( bRoot )
		SetEnvRoot( m_EnvGetVal, _closure(ci->_closure)->_root );
#endif

	SetCallFrame( m_EnvGetVal, vm, ci );
	SetEnvDelegate( m_EnvGetVal, vm->_stack._vals[ GetStackBase( vm, ci ) ] );

#ifdef CLOSURE_ROOT
	SQWeakRef *root = bRoot ? _closure(ci->_closure)->_root : NULL;
	bool ret = RunScript( vm, expression, root, &m_EnvGetVal, out, multiline );
#else
	bool ret = RunScript( vm, expression, &m_EnvGetVal, out, multiline );
#endif

#ifdef CLOSURE_ROOT
	if ( bRoot )
		SetEnvRoot( m_EnvGetVal, m_pRootVM->_roottable );
#endif

	return ret;
}

bool SQDebugServer::CompileScript( const string_t &script, SQObjectPtr &out )
{
	const bool multiline = false;
	int size;
	SQChar *buf;

	if ( !multiline )
	{
#ifdef SQUNICODE
		size = sq_rsl( STRLEN("return()") + SQUnicodeLength( script.ptr, script.len ) + 1 );
#else
		size = STRLEN("return()") + script.len + 1;
#endif
		buf = (SQChar*)ScratchPad( size );

		memcpy( buf, _SC("return("), sq_rsl( STRLEN("return(") ) ); // ))
		buf += STRLEN("return(");
	}
	else
	{
#ifdef SQUNICODE
		size = sq_rsl( SQUnicodeLength( script.ptr, script.len ) + 1 );
#else
		size = script.len + 1;
#endif
		buf = (SQChar*)ScratchPad( size );
	}

#ifdef SQUNICODE
	buf += UTF8ToSQUnicode( buf, size - ( (char*)buf - ScratchPad() ), script.ptr, script.len );
#else
	memcpy( buf, script.ptr, script.len );
	buf += script.len;
#endif

	if ( !multiline )
		*buf++ = _SC(')');

	*buf = 0;

	Assert( ( (char*)buf - ScratchPad() ) == ( size - (int)sq_rsl(1) ) );

	if ( SQ_SUCCEEDED( sq_compilebuffer( m_pCurVM, (SQChar*)ScratchPad(), size, _SC("sqdbg"), SQFalse ) ) )
	{
		// Don't create varargs on calls
		SQFunctionProto *fn = _fp(_closure(m_pCurVM->Top())->_function);
		if ( fn->_varparams )
		{
			fn->_varparams = false;
#if SQUIRREL_VERSION_NUMBER >= 300
			fn->_nparameters--;
#endif
		}

		out = m_pCurVM->Top();
		m_pCurVM->Pop();
		return true;
	}

	return false;
}

bool SQDebugServer::RunScript( HSQUIRRELVM vm, const string_t &script,
#ifdef CLOSURE_ROOT
		SQWeakRef *root,
#endif
		const SQObject *env, SQObjectPtr &out, bool multiline )
{
	int size;
	SQChar *buf;

	if ( !multiline )
	{
#ifdef SQUNICODE
		size = sq_rsl( STRLEN("return()") + SQUnicodeLength( script.ptr, script.len ) + 1 );
#else
		size = STRLEN("return()") + script.len + 1;
#endif
		buf = (SQChar*)ScratchPad( size );

		memcpy( buf, _SC("return("), sq_rsl( STRLEN("return(") ) ); // ))
		buf += STRLEN("return(");
	}
	else
	{
#ifdef SQUNICODE
		size = sq_rsl( SQUnicodeLength( script.ptr, script.len ) + 1 );
#else
		size = script.len + 1;
#endif
		buf = (SQChar*)ScratchPad( size );
	}

#ifdef SQUNICODE
	buf += UTF8ToSQUnicode( buf, size - ( (char*)buf - ScratchPad() ), script.ptr, script.len );
#else
	memcpy( buf, script.ptr, script.len );
	buf += script.len;
#endif

	if ( !multiline )
		*buf++ = _SC(')');

	*buf = 0;

	Assert( ( (char*)buf - ScratchPad() ) == ( size - (int)sq_rsl(1) ) );

	if ( SQ_SUCCEEDED( sq_compilebuffer( vm, (SQChar*)ScratchPad(), size, _SC("sqdbg"), SQFalse ) ) )
	{
		// Don't create varargs on calls
		SQFunctionProto *fn = _fp(_closure(vm->Top())->_function);
		if ( fn->_varparams )
		{
			fn->_varparams = false;
#if SQUIRREL_VERSION_NUMBER >= 300
			fn->_nparameters--;
#endif
		}

		CCallGuard cg( this, vm );

		// m_pCurVM will incorrectly change if a script is executed on a different thread.
		// save and restore
		HSQUIRRELVM curvm = m_pCurVM;

#ifdef CLOSURE_ROOT
		if ( root )
			_closure(vm->Top())->SetRoot( root );
#endif

		vm->Push( env ? *env : vm->_roottable );

		if ( SQ_SUCCEEDED( sq_call( vm, 1, SQTrue, SQFalse ) ) )
		{
			m_pCurVM = curvm;
			out = vm->Top();
			vm->Pop();
			vm->Pop();
			return true;
		}

		m_pCurVM = curvm;
		vm->Pop();
	}

	return false;
}

bool SQDebugServer::RunClosure( HSQUIRRELVM vm, const SQObjectPtr &closure, const SQObject *env,
		SQObjectPtr &ret )
{
	vm->Push( closure );
	vm->Push( env ? *env : vm->_roottable );

	if ( SQ_SUCCEEDED( sq_call( vm, 1, SQTrue, SQFalse ) ) )
	{
		ret = vm->Top();
		vm->Pop();
		vm->Pop();
		return true;
	}

	vm->Pop();
	return false;
}

bool SQDebugServer::RunClosure( HSQUIRRELVM vm, const SQObjectPtr &closure, const SQObject *env,
		const SQObjectPtr &p1, SQObjectPtr &ret )
{
	vm->Push( closure );
	vm->Push( env ? *env : vm->_roottable );
	vm->Push( p1 );

	if ( SQ_SUCCEEDED( sq_call( vm, 2, SQTrue, SQFalse ) ) )
	{
		ret = vm->Top();
		vm->Pop();
		vm->Pop();
		return true;
	}

	vm->Pop();
	return false;
}

bool SQDebugServer::RunClosure( HSQUIRRELVM vm, const SQObjectPtr &closure, const SQObject *env,
		const SQObjectPtr &p1, const SQObjectPtr &p2, SQObjectPtr &ret )
{
	vm->Push( closure );
	vm->Push( env ? *env : vm->_roottable );
	vm->Push( p1 );
	vm->Push( p2 );

	if ( SQ_SUCCEEDED( sq_call( vm, 3, SQTrue, SQFalse ) ) )
	{
		ret = vm->Top();
		vm->Pop();
		vm->Pop();
		return true;
	}

	vm->Pop();
	return false;
}

bool SQDebugServer::RunClosure( HSQUIRRELVM vm, const SQObjectPtr &closure, const SQObject *env,
		const SQObjectPtr &p1, const SQObjectPtr &p2, const SQObjectPtr &p3, SQObjectPtr &ret )
{
	vm->Push( closure );
	vm->Push( env ? *env : vm->_roottable );
	vm->Push( p1 );
	vm->Push( p2 );
	vm->Push( p3 );

	if ( SQ_SUCCEEDED( sq_call( vm, 4, SQTrue, SQFalse ) ) )
	{
		ret = vm->Top();
		vm->Pop();
		vm->Pop();
		return true;
	}

	vm->Pop();
	return false;
}

bool SQDebugServer::RunClosure( HSQUIRRELVM vm, const SQObjectPtr &closure, const SQObject *env,
		const SQObjectPtr &p1, const SQObjectPtr &p2, const SQObjectPtr &p3, const SQObjectPtr &p4,
		SQObjectPtr &ret )
{
	vm->Push( closure );
	vm->Push( env ? *env : vm->_roottable );
	vm->Push( p1 );
	vm->Push( p2 );
	vm->Push( p3 );
	vm->Push( p4 );

	if ( SQ_SUCCEEDED( sq_call( vm, 5, SQTrue, SQFalse ) ) )
	{
		ret = vm->Top();
		vm->Pop();
		vm->Pop();
		return true;
	}

	vm->Pop();
	return false;
}

bool SQDebugServer::RunClosure( HSQUIRRELVM vm, const SQObjectPtr &closure, const SQObject *env,
		const SQObjectPtr &p1, const SQObjectPtr &p2, const SQObjectPtr &p3, const SQObjectPtr &p4,
		const SQObjectPtr &p5, SQObjectPtr &ret )
{
	vm->Push( closure );
	vm->Push( env ? *env : vm->_roottable );
	vm->Push( p1 );
	vm->Push( p2 );
	vm->Push( p3 );
	vm->Push( p4 );
	vm->Push( p5 );

	if ( SQ_SUCCEEDED( sq_call( vm, 6, SQTrue, SQFalse ) ) )
	{
		ret = vm->Top();
		vm->Pop();
		vm->Pop();
		return true;
	}

	vm->Pop();
	return false;
}

SQInteger SQDebugServer::SQMM_Get( HSQUIRRELVM vm )
{
	SQObjectPtr value;
	HSQOBJECT mtenv, index;
	sq_getstackobj( vm, -2, &mtenv );
	sq_getstackobj( vm, -1, &index );

	Assert( sq_type(mtenv) == OT_TABLE && _table(mtenv)->_delegate );
	SQObjectPtr frame;
	Verify( SQTable_Get( _table(mtenv)->_delegate, _SC(KW_CALLFRAME), frame ) );
	Assert( sq_type(frame) == OT_INTEGER && _integer(frame) >= 0 && _integer(frame) < vm->_callsstacksize );

	if ( !( _integer(frame) >= 0 && _integer(frame) < vm->_callsstacksize ) )
	{
		vm->Raise_Error( _SC("invalid call frame") );
		return -1;
	}

	if ( GetVariable( vm, vm->_callsstack + _integer(frame), mtenv, index, value ) )
	{
		sq_pushobject( vm, value );
		return 1;
	}

	vm->Raise_IdxError( index );
	return -1;
}

SQInteger SQDebugServer::SQMM_Set( HSQUIRRELVM vm )
{
	HSQOBJECT mtenv, index, value;
	sq_getstackobj( vm, -3, &mtenv );
	sq_getstackobj( vm, -2, &index );
	sq_getstackobj( vm, -1, &value );

	Assert( sq_type(mtenv) == OT_TABLE && _table(mtenv)->_delegate );
	SQObjectPtr frame;
	Verify( SQTable_Get( _table(mtenv)->_delegate, _SC(KW_CALLFRAME), frame ) );
	Assert( sq_type(frame) == OT_INTEGER && _integer(frame) >= 0 && _integer(frame) < vm->_callsstacksize );

	if ( !( _integer(frame) >= 0 && _integer(frame) < vm->_callsstacksize ) )
	{
		vm->Raise_Error( _SC("invalid call frame") );
		return -1;
	}

	if ( SetVariable( vm, vm->_callsstack + _integer(frame), mtenv, index, value ) )
		return 0;

	vm->Raise_IdxError( index );
	return -1;
}

SQInteger SQDebugServer::SQMM_NewSlot( HSQUIRRELVM vm )
{
	HSQOBJECT mtenv, index, value;
	sq_getstackobj( vm, -3, &mtenv );
	sq_getstackobj( vm, -2, &index );
	sq_getstackobj( vm, -1, &value );

	if ( NewSlot( vm, mtenv, index, value ) )
		return 0;

	vm->Raise_Error( _SC("could not create new slot") );
	return -1;
}

bool SQDebugServer::GetVariable( HSQUIRRELVM vm, const SQVM::CallInfo *ci,
		const SQObject &mtenv, const SQObject &index, SQObjectPtr &value )
{
	Assert( !ci || ( ci >= vm->_callsstack && ci < vm->_callsstack + vm->_callsstacksize ) );

	// locals
	if ( ci && sq_type(ci->_closure) == OT_CLOSURE && sq_type(index) == OT_STRING )
	{
		SQClosure *pClosure = _closure(ci->_closure);
		SQFunctionProto *func = _fp(pClosure->_function);

		SQUnsignedInteger ip = (SQUnsignedInteger)( ci->_ip - func->_instructions - 1 );

		for ( int i = 0; i < func->_nlocalvarinfos; i++ )
		{
			const SQLocalVarInfo &var = func->_localvarinfos[i];
			if ( var._start_op <= ip + 1 && var._end_op >= ip &&
					_string(index) == _string(var._name) )
			{
				int stackbase = GetStackBase( vm, ci );
				value = vm->_stack._vals[ stackbase + var._pos ];
				return true;
			}
		}

		for ( int i = 0; i < func->_noutervalues; i++ )
		{
			const SQOuterVar &var = func->_outervalues[i];
			if ( _string(index) == _string(var._name) )
			{
				value = *_outervalptr( pClosure->_outervalues[i] );
				return true;
			}
		}

		// this/vargv keywords compile in the temp env, add custom keywords to redirect
		// Having locals named __this/__vargv will break this hack
		if ( _string(index)->_len == 6 || _string(index)->_len == 7 )
		{
			if ( sqstring_t(_SC(KW_THIS)).IsEqualTo( _string(index) ) )
			{
				int stackbase = GetStackBase( vm, ci );
				value = vm->_stack._vals[ stackbase ];
				return true;
			}
#if SQUIRREL_VERSION_NUMBER >= 300
			else if ( func->_varparams && func->_nlocalvarinfos >= 2 )
			{
				if ( sqstring_t(_SC(KW_VARGV)).IsEqualTo( _string(index) ) )
				{
					const SQLocalVarInfo &var = func->_localvarinfos[ func->_nlocalvarinfos - 2 ];
					if ( sqstring_t(_SC("vargv")).IsEqualTo( _string(var._name) ) )
					{
						int stackbase = GetStackBase( vm, ci );
						value = vm->_stack._vals[ stackbase + 1 ];
						return true;
					}
				}
				else if ( sqstring_t(_SC(KW_VARGC)).IsEqualTo( _string(index) ) )
				{
					const SQLocalVarInfo &var = func->_localvarinfos[ func->_nlocalvarinfos - 2 ];
					if ( sqstring_t(_SC("vargv")).IsEqualTo( _string(var._name) ) )
					{
						int stackbase = GetStackBase( vm, ci );
						value = vm->_stack._vals[ stackbase + 1 ];

						if ( sq_type(value) == OT_ARRAY )
						{
							value = _array(value)->Size();
							return true;
						}

						value.Null();
						return false;
					}
				}
			}
#else
			else if ( func->_varparams )
			{
				if ( sqstring_t(_SC(KW_VARGV)).IsEqualTo( _string(index) ) )
				{
					int size = ci->_vargs.size;
					if ( !size )
						return false;

					// This could alternatively register a userdata or table with _get that returns varg params
					SQArray *arr = SQArray::Create( _ss(vm), size );

					Assert( arr->Size() == size );

					for ( int i = 0; i < size; i++ )
					{
						const SQObjectPtr &val = vm->_vargsstack[ ci->_vargs.base + i ];
						arr->_values[i] = val;
					}

					value = arr;
					return true;
				}
				else if ( sqstring_t(_SC(KW_VARGC)).IsEqualTo( _string(index) ) )
				{
					value = (SQInteger)ci->_vargs.size;
					return true;
				}
			}
#endif
		}
	}

	Assert( sq_type(mtenv) == OT_TABLE && _table(mtenv)->_delegate );

	// env
	SQObjectPtr env;
	Verify( SQTable_Get( _table(mtenv)->_delegate, _SC(KW_DELEGATE), env ) );

	switch ( sq_type(env) )
	{
		case OT_TABLE:
		{
			SQTable *t = _table(env);
			do
			{
				if ( t->Get( index, value ) )
				{
					return true;
				}
			}
			while ( ( t = t->_delegate ) != NULL );

			break;
		}
		case OT_INSTANCE:
		{
			if ( _instance(env)->Get( index, value ) )
			{
				return true;
			}

			break;
		}
		case OT_CLASS:
		{
			if ( _class(env)->Get( index, value ) )
			{
				return true;
			}

			break;
		}
		default: break;
	}

	// metamethods
	if ( is_delegable(env) && _delegable(env)->_delegate )
	{
		SQObjectPtr mm;
		if ( _delegable(env)->GetMetaMethod( vm, MT_GET, mm ) )
		{
			if ( RunClosure( vm, mm, &env, index, value ) )
			{
				return true;
			}
		}
	}

	// root
#ifdef CLOSURE_ROOT
	SQObjectPtr root;
	Verify( SQTable_Get( _table(mtenv)->_delegate, _SC(KW_ROOT), root ) );

	if ( sq_type(root) == OT_TABLE ) // else the user invalidated it, reconnect the client to fix
#else
	const SQObjectPtr &root = vm->_roottable;
#endif
	{
		SQTable *t = _table(root);
		do
		{
			if ( t->Get( index, value ) )
			{
				return true;
			}
		}
		while ( ( t = t->_delegate ) != NULL );
	}

	return false;
}

bool SQDebugServer::SetVariable( HSQUIRRELVM vm, const SQVM::CallInfo *ci,
		const SQObject &mtenv, const SQObject &index, const SQObject &value )
{
	Assert( !ci || ( ci >= vm->_callsstack && ci < vm->_callsstack + vm->_callsstacksize ) );

	// locals
	if ( ci && sq_type(ci->_closure) == OT_CLOSURE && sq_type(index) == OT_STRING )
	{
		SQClosure *pClosure = _closure(ci->_closure);
		SQFunctionProto *func = _fp(pClosure->_function);

		SQUnsignedInteger ip = (SQUnsignedInteger)( ci->_ip - func->_instructions - 1 );

		for ( int i = 0; i < func->_nlocalvarinfos; i++ )
		{
			const SQLocalVarInfo &var = func->_localvarinfos[i];
			if ( var._start_op <= ip + 1 && var._end_op >= ip &&
					_string(index) == _string(var._name) )
			{
				int stackbase = GetStackBase( vm, ci );
				vm->_stack._vals[ stackbase + var._pos ] = value;
				return true;
			}
		}

		for ( int i = 0; i < func->_noutervalues; i++ )
		{
			const SQOuterVar &var = func->_outervalues[i];
			if ( _string(index) == _string(var._name) )
			{
				*_outervalptr( pClosure->_outervalues[i] ) = value;
				return true;
			}
		}
	}

	Assert( sq_type(mtenv) == OT_TABLE && _table(mtenv)->_delegate );

	// env
	SQObjectPtr env;
	Verify( SQTable_Get( _table(mtenv)->_delegate, _SC(KW_DELEGATE), env ) );

	switch ( sq_type(env) )
	{
		case OT_TABLE:
		{
			SQTable *t = _table(env);
			do
			{
				if ( t->Set( index, value ) )
				{
					return true;
				}
			}
			while ( ( t = t->_delegate ) != NULL );

			break;
		}
		case OT_INSTANCE:
		{
			if ( _instance(env)->Set( index, value ) )
			{
				return true;
			}

			break;
		}
		case OT_CLASS:
		{
			return _class(env)->NewSlot( _ss(vm), index, value, false );
		}
		default: break;
	}

	// metamethods
	if ( is_delegable(env) && _delegable(env)->_delegate )
	{
		SQObjectPtr mm;
		if ( _delegable(env)->GetMetaMethod( vm, MT_SET, mm ) )
		{
			SQObjectPtr dummy;
			if ( RunClosure( vm, mm, &env, index, value, dummy ) )
			{
				return true;
			}
		}
	}

	// root
#ifdef CLOSURE_ROOT
	SQObjectPtr root;
	Verify( SQTable_Get( _table(mtenv)->_delegate, _SC(KW_ROOT), root ) );

	if ( sq_type(root) == OT_TABLE ) // else the user invalidated it, reconnect the client to fix
#else
	const SQObjectPtr &root = vm->_roottable;
#endif
	{
		SQTable *t = _table(root);
		do
		{
			if ( t->Set( index, value ) )
			{
				return true;
			}
		}
		while ( ( t = t->_delegate ) != NULL );
	}

	return false;
}

bool SQDebugServer::NewSlot( HSQUIRRELVM vm, const SQObject &mtenv, const SQObject &index, const SQObject &value )
{
	Assert( sq_type(mtenv) == OT_TABLE && _table(mtenv)->_delegate );

	// env
	SQObjectPtr env;
	Verify( SQTable_Get( _table(mtenv)->_delegate, _SC(KW_DELEGATE), env ) );

	Assert( sq_type(env) != OT_ARRAY );

	if ( is_delegable(env) && _delegable(env)->_delegate )
	{
		SQObjectPtr mm;
		if ( _delegable(env)->GetMetaMethod( vm, MT_NEWSLOT, mm ) )
		{
			SQObjectPtr dummy;
			if ( RunClosure( vm, mm, &env, index, value, dummy ) )
			{
				return true;
			}
		}
	}

	switch ( sq_type(env) )
	{
		case OT_TABLE:
			_table(env)->NewSlot( index, value );
			return true;
		case OT_CLASS:
			return _class(env)->NewSlot( _ss(vm), index, value, false );
		default:
			return false;
	}
}

bool SQDebugServer::Get( const objref_t &obj, SQObjectPtr &value )
{
	if ( obj.type & objref_t::PTR )
	{
		value = *obj.ptr;
		return true;
	}

	Assert( !( obj.type & objref_t::READONLY ) );

	switch ( obj.type )
	{
		case objref_t::TABLE:
		{
			return sq_type(obj.src) == OT_TABLE &&
				_table(obj.src)->Get( obj.key, value );
		}
		case objref_t::INSTANCE:
		{
			return sq_type(obj.src) == OT_INSTANCE &&
				_instance(obj.src)->Get( obj.key, value );
		}
		case objref_t::CLASS:
		{
			return sq_type(obj.src) == OT_CLASS &&
				_class(obj.src)->Get( obj.key, value );
		}
		case objref_t::DELEGABLE_META:
		{
			if ( is_delegable(obj.src) && _delegable(obj.src)->_delegate )
			{
				SQObjectPtr mm;
				if ( _delegable(obj.src)->GetMetaMethod( m_pRootVM, MT_GET, mm ) )
				{
					return RunClosure( mm, &obj.src, obj.key, value );
				}
			}

			return false;
		}
		case objref_t::ARRAY:
		{
			Assert( sq_type(obj.key) == OT_INTEGER );

			if ( sq_type(obj.src) == OT_ARRAY &&
					_integer(obj.key) >= 0 &&
					_integer(obj.key) < _array(obj.src)->Size() )
			{
				value = _array(obj.src)->_values[ _integer(obj.key) ];
				return true;
			}

			return false;
		}
		case objref_t::CUSTOMMEMBER:
		{
			if ( sq_type(obj.src) == OT_INSTANCE )
			{
				const SQObjectPtr *def = GetClassDefCustomMembers( _instance(obj.src)->_class );

				if ( def )
				{
					SQObjectPtr custommembers = *def;

					if ( sq_type(custommembers) == OT_CLOSURE )
						RunClosure( custommembers, &obj.src, custommembers );

					if ( sq_type(custommembers) == OT_ARRAY )
					{
						objref_t tmp;
						SQObjectPtr strName = CreateSQString( m_pRootVM, _SC("name") );
						SQObjectPtr strGet = CreateSQString( m_pRootVM, _SC("get") );
						SQObjectPtr name, get;

						for ( unsigned int i = 0; i < _array(custommembers)->_values.size(); i++ )
						{
							const SQObjectPtr &memdef = _array(custommembers)->_values[i];

							if ( GetObj_Var( strName, memdef, tmp, name ) &&
									sq_type(obj.key) == sq_type(name) && _rawval(obj.key) == _rawval(name) )
							{
								return GetObj_Var( strGet, memdef, tmp, get ) &&
									CallCustomMembersGetFunc( get, &obj.src, obj.key, value );
							}
						}
					}
				}
			}

			return false;
		}
		case objref_t::INT:
		{
			value = (SQInteger)obj.val;
			return true;
		}
		default: UNREACHABLE();
	}
}

bool SQDebugServer::Set( const objref_t &obj, const SQObjectPtr &value )
{
	if ( obj.type & objref_t::READONLY )
		return false;

	if ( obj.type & objref_t::PTR )
	{
		*(obj.ptr) = value;
		return true;
	}

	switch ( obj.type )
	{
		case objref_t::TABLE:
		{
			return sq_type(obj.src) == OT_TABLE &&
				_table(obj.src)->Set( obj.key, value );
		}
		case objref_t::INSTANCE:
		{
			return sq_type(obj.src) == OT_INSTANCE &&
				_instance(obj.src)->Set( obj.key, value );
		}
		case objref_t::CLASS:
		{
			SQObjectPtr idx;

			if ( sq_type(obj.src) == OT_CLASS &&
					_class(obj.src)->_members->Get( obj.key, idx ) )
			{
				if ( _isfield(idx) )
				{
					_class(obj.src)->_defaultvalues[ _member_idx(idx) ].val = value;
				}
				else
				{
					_class(obj.src)->_methods[ _member_idx(idx) ].val = value;
				}

				return true;
			}

			return false;
		}
		case objref_t::DELEGABLE_META:
		{
			if ( is_delegable(obj.src) && _delegable(obj.src)->_delegate )
			{
				SQObjectPtr mm;
				if ( _delegable(obj.src)->GetMetaMethod( m_pRootVM, MT_SET, mm ) )
				{
					SQObjectPtr dummy;
					return RunClosure( mm, &obj.src, obj.key, value, dummy );
				}
			}

			return false;
		}
		case objref_t::ARRAY:
		{
			Assert( sq_type(obj.key) == OT_INTEGER );

			if ( sq_type(obj.src) == OT_ARRAY &&
					_integer(obj.key) >= 0 &&
					_integer(obj.key) < _array(obj.src)->Size() )
			{
				_array(obj.src)->_values[ _integer(obj.key) ] = value;
				return true;
			}

			return false;
		}
		case objref_t::CUSTOMMEMBER:
		{
			if ( sq_type(obj.src) == OT_INSTANCE )
			{
				const SQObjectPtr *def = GetClassDefCustomMembers( _instance(obj.src)->_class );

				if ( def )
				{
					SQObjectPtr custommembers = *def;

					if ( sq_type(custommembers) == OT_CLOSURE )
						RunClosure( custommembers, &obj.src, custommembers );

					if ( sq_type(custommembers) == OT_ARRAY )
					{
						objref_t tmp;
						SQObjectPtr strName = CreateSQString( m_pRootVM, _SC("name") );
						SQObjectPtr strSet = CreateSQString( m_pRootVM, _SC("set") );
						SQObjectPtr name, set;

						for ( unsigned int i = 0; i < _array(custommembers)->_values.size(); i++ )
						{
							const SQObjectPtr &memdef = _array(custommembers)->_values[i];

							if ( GetObj_Var( strName, memdef, tmp, name ) &&
									sq_type(obj.key) == sq_type(name) && _rawval(obj.key) == _rawval(name) )
							{
								return GetObj_Var( strSet, memdef, tmp, set ) &&
									CallCustomMembersSetFunc( set, &obj.src, obj.key, value, set );
							}
						}
					}
				}
			}

			return false;
		}
		case objref_t::INT:
		{
			return false;
		}
		default: UNREACHABLE();
	}
}

#ifndef SQDBG_DISABLE_COMPILER
bool SQDebugServer::NewSlot( const objref_t &obj, const SQObjectPtr &value )
{
	if ( obj.type & objref_t::READONLY )
		return false;

	if ( is_delegable(obj.src) && _delegable(obj.src)->_delegate )
	{
		SQObjectPtr mm;
		if ( _delegable(obj.src)->GetMetaMethod( m_pRootVM, MT_NEWSLOT, mm ) )
		{
			SQObjectPtr dummy;
			if ( RunClosure( mm, &obj.src, obj.key, value, dummy ) )
				return CompileReturnCode_Success;
		}
	}

	switch ( obj.type )
	{
		case objref_t::TABLE:
		{
			if ( sq_type(obj.src) == OT_TABLE )
			{
				_table(obj.src)->NewSlot( obj.key, value );
				return true;
			}

			return false;
		}
		case objref_t::CLASS:
		{
			return sq_type(obj.src) == OT_CLASS &&
				_class(obj.src)->NewSlot( _ss(m_pRootVM), obj.key, value, false );
		}
		default: return false;
	}
}

bool SQDebugServer::Delete( const objref_t &obj, SQObjectPtr &value )
{
	if ( obj.type & objref_t::READONLY )
		return false;

	if ( is_delegable(obj.src) && _delegable(obj.src)->_delegate )
	{
		SQObjectPtr mm;
		if ( _delegable(obj.src)->GetMetaMethod( m_pRootVM, MT_DELSLOT, mm ) )
		{
			return RunClosure( mm, &obj.src, obj.key, value );
		}
	}

	switch ( obj.type )
	{
		case objref_t::TABLE:
		{
			if ( sq_type(obj.src) == OT_TABLE &&
					_table(obj.src)->Get( obj.key, value ) )
			{
				_table(obj.src)->Remove( obj.key );
				return true;
			}
		}
		default: return false;
	}
}

bool SQDebugServer::Increment( const objref_t &obj, int amt )
{
#define _check(var) \
	switch ( sq_type(var) ) \
	{ \
		case OT_INTEGER: \
			_integer(var) += (SQInteger)amt; \
			break; \
		case OT_FLOAT: \
			_float(var) += (SQFloat)amt; \
			break; \
		default: \
			return false; \
	}

	if ( obj.type & objref_t::READONLY )
		return false;

	if ( obj.type & objref_t::PTR )
	{
		_check( *(obj.ptr) );
		return true;
	}

	switch ( obj.type )
	{
		case objref_t::TABLE:
		{
			if ( sq_type(obj.src) == OT_TABLE )
			{
				SQObjectPtr value;
				if ( _table(obj.src)->Get( obj.key, value ) )
				{
					_check( value );
					return _table(obj.src)->Set( obj.key, value );
				}
			}

			return false;
		}
		case objref_t::INSTANCE:
		{
			if ( sq_type(obj.src) == OT_INSTANCE )
			{
				SQObjectPtr value;
				if ( _instance(obj.src)->Get( obj.key, value ) )
				{
					_check( value );
					return _instance(obj.src)->Set( obj.key, value );
				}
			}

			return false;
		}
		case objref_t::ARRAY:
		{
			Assert( sq_type(obj.key) == OT_INTEGER );

			if ( sq_type(obj.src) == OT_ARRAY &&
					_integer(obj.key) >= 0 &&
					_integer(obj.key) < _array(obj.src)->Size() )
			{
				SQObjectPtr value = _array(obj.src)->_values[ _integer(obj.key) ];
				_check( value );
				_array(obj.src)->_values[ _integer(obj.key) ] = value;
				return true;
			}

			return false;
		}
		default:
			return false;
	}

#undef _check
}

//
// A very basic compiler that parses strings, characters, numbers (dec, hex, oct, bin, flt), identifiers,
// keywords (this, null, true, false),
// unary operators (-, ~, !, typeof, delete, clone),
// two argument operators (+, -, *, /, %, <<, >>, >>>, &, |, ^, <, >, <=, >=, <=>, ==, !=, &&, ||, in, instanceof),
// prefix/postfix increment/decrement operators,
// newslot and (compound) assignment operators,
// root (::) and identifier access (a.b, a[b]), function calls and grouping parantheses.
//
// Variable evaluation is done in given stack frame as they are parsed, state is not kept
//
class SQDebugServer::CCompiler
{
public:
	enum
	{
		Err_InvalidToken = -50,
		Err_UnfinishedString,
		Err_UnfinishedChar,
		Err_InvalidEscape,
		Err_InvalidXEscape,
		Err_InvalidU16Escape,
		Err_InvalidU32Escape,
		Err_InvalidDecimal,
		Err_InvalidOctal,
		Err_InvalidHexadecimal,
		Err_InvalidBinary,
		Err_InvalidFloat,

		Token_End = 1,
		Token_Ref,
		Token_Identifier,
		Token_String,
		Token_Integer,
		Token_Float,
		Token_Null,
		Token_False,
		Token_True,
		Token_This,
		Token_DoubleColon,
		Token_Delete,
#if SQUIRREL_VERSION_NUMBER < 300
		Token_Parent,
		Token_Vargv,
		Token_Vargc,
#endif

		Token_NewSlot,
		Token_PendingKey,
		Token_Operator,
		Token_Value,

		_op = 0xff00,

		// upper 4 bits : precedence
		// lower 4 bits : unique id
		Token_Not			= 0x00 | _op,
		Token_BwNot			= 0x01 | _op,
		Token_Typeof		= 0x02 | _op,
		Token_Clone			= 0x03 | _op,

		Token_Increment		= 0x10 | _op,
		Token_Decrement		= 0x11 | _op,

		Token_Mul			= 0x20 | _op,
		Token_Div			= 0x21 | _op,
		Token_Mod			= 0x22 | _op,

		Token_Add			= 0x30 | _op,
		Token_Sub			= 0x31 | _op,

		Token_LShift		= 0x40 | _op,
		Token_RShift		= 0x41 | _op,
		Token_URShift		= 0x42 | _op,

		Token_Cmp3W			= 0x50 | _op,

		Token_Less			= 0x60 | _op,
		Token_LessEq		= 0x61 | _op,
		Token_Greater		= 0x62 | _op,
		Token_GreaterEq		= 0x63 | _op,

		Token_Eq			= 0x70 | _op,
		Token_NotEq			= 0x71 | _op,

		Token_BwAnd			= 0x80 | _op,
		Token_BwXor			= 0x90 | _op,
		Token_BwOr			= 0xA0 | _op,

		Token_In			= 0xB0 | _op,
		Token_InstanceOf	= 0xB1 | _op,

		Token_LogicalAnd	= 0xC0 | _op,
		Token_LogicalOr		= 0xD0 | _op,

		_assign = 0xff0000,

		Token_Assign,
		Token_AssignAdd		= _assign | Token_Add,
		Token_AssignSub		= _assign | Token_Sub,
		Token_AssignMul		= _assign | Token_Mul,
		Token_AssignDiv		= _assign | Token_Div,
		Token_AssignMod		= _assign | Token_Mod,
		Token_AssignAnd		= _assign | Token_BwAnd,
		Token_AssignXor		= _assign | Token_BwXor,
		Token_AssignOr		= _assign | Token_BwOr,
		Token_AssignLS		= _assign | Token_LShift,
		Token_AssignRS		= _assign | Token_RShift,
		Token_AssignURS		= _assign | Token_URShift,
	};

	struct token_t
	{
		token_t() : type(0) {}

		int type;
		union
		{
			string_t _string;
			SQInteger _integer;
			SQFloat _float;
		};
	};

#ifndef SQDBG_COMPILER_MAX_PARAMETER_COUNT
#define SQDBG_COMPILER_MAX_PARAMETER_COUNT 6
#endif
#ifndef SQDBG_COMPILER_MAX_UNARY_STACK
#define SQDBG_COMPILER_MAX_UNARY_STACK 4
#endif

private:
	struct callparams_t
	{
		SQObjectPtr params[ SQDBG_COMPILER_MAX_PARAMETER_COUNT ];
		SQObjectPtr func;
		int paramCount;
	};

	string_t &m_expr;
	char *m_cur;
	char *m_end;
	int m_prevToken;

public:
	// To get partial matches for completions request
	token_t m_lastToken;

	// To add expression watchpoints
	// Only returns the last checked ref regardless of any other operations that come after
	// This is fine because adding illogical watchpoints doesn't have significant side effects -
	// prevention is at the user's discretion,
	// and the ability to add on expressions is useful
	objref_t m_lastRef;

public:
	CCompiler( string_t &expression ) :
		m_expr(expression),
		m_cur(expression.ptr),
		m_end(expression.ptr + expression.len),
		m_prevToken(0),
		m_lastToken(),
		m_lastRef()
	{
	}

	CCompiler( const CCompiler & );
	CCompiler &operator=( const CCompiler & );

	ECompileReturnCode Evaluate( SQDebugServer *dbg, HSQUIRRELVM vm, const SQVM::CallInfo *ci, SQObjectPtr &val,
			int closer = 0 )
	{
		token_t token;
		int prevtoken = 0;

		int opbufidx = -1;
		int unaryidx = -1;

		// Recursively parsing unary operators adds the complexity
		// of keeping track of and reverting to the previous token
		// Using an operator stack is simpler but limits the amount
		unsigned char unarybuf[ SQDBG_COMPILER_MAX_UNARY_STACK ];
		unsigned char opbuf[2];
		char deleteop = 0;
		char incrop = 0;

		SQObjectPtr callenv;
		SQObjectPtr valbuf[2];

		objref_t obj;
		obj.type = objref_t::INVALID;

		for ( ;; m_prevToken = token.type )
		{
			token = Lex();

			switch ( token.type )
			{
				case Token_Identifier:
				{
					if ( !ExpectsIdentifier( prevtoken ) )
						return CompileReturnCode_Unsupported;

					m_lastToken = token;

					if ( !dbg->GetObj_Frame( token._string, vm, ci, obj, val ) )
					{
						// allow non-existent key
						if ( Next() != Token_NewSlot )
							return CompileReturnCode_DoesNotExist;

						// implicit this.
						val = ci ? vm->_stack._vals[ GetStackBase( vm, ci ) ] : vm->_roottable;

						switch ( sq_type(val) )
						{
							case OT_TABLE:
								obj.type = objref_t::TABLE;
								break;
							case OT_CLASS:
								obj.type = objref_t::CLASS;
								break;
							default:
								if ( is_delegable(val) && _delegable(val)->_delegate )
								{
									obj.type = objref_t::DELEGABLE_META;
									break;
								}

								return CompileReturnCode_DoesNotExist;
						}

						obj.src = val;
						obj.key = CreateSQString( vm, token._string );
						prevtoken = Token_PendingKey;
					}
					else
					{
						ConvertPtr( obj );
						m_lastRef = obj;
						prevtoken = Token_Ref;
					}

					break;
				}
				case Token_String:
				{
					if ( !ExpectsValue( prevtoken ) )
						return CompileReturnCode_Unsupported;

					val = CreateSQString( vm, token._string );

					prevtoken = Token_Value;
					break;
				}
				case Token_Integer:
				{
					if ( !ExpectsValue( prevtoken ) )
						return CompileReturnCode_Unsupported;

					val.Null();
					val._type = OT_INTEGER;
					val._unVal.nInteger = token._integer;

					prevtoken = Token_Integer;
					break;
				}
				case Token_Float:
				{
					if ( !ExpectsValue( prevtoken ) )
						return CompileReturnCode_Unsupported;

					val.Null();
					val._type = OT_FLOAT;
					val._unVal.fFloat = token._float;

					prevtoken = Token_Float;
					break;
				}
				case Token_Null:
				{
					if ( !ExpectsValue( prevtoken ) )
						return CompileReturnCode_Unsupported;

					val.Null();

					prevtoken = Token_Value;
					break;
				}
				case Token_False:
				case Token_True:
				{
					if ( !ExpectsValue( prevtoken ) )
						return CompileReturnCode_Unsupported;

					val.Null();
					val._type = OT_BOOL;
					val._unVal.nInteger = token._integer;

					prevtoken = Token_Value;
					break;
				}
				case Token_This:
				{
					if ( !ExpectsIdentifier( prevtoken ) )
						return CompileReturnCode_Unsupported;

					val = ci ? vm->_stack._vals[ GetStackBase( vm, ci ) ] : vm->_roottable;

					prevtoken = Token_Value;
					break;
				}
				case Token_BwNot:
				case Token_Not:
				case Token_Typeof:
				case Token_Clone:
				{
unary:
					// identifier boundary
					if ( prevtoken == Token_Ref )
					{
						if ( deleteop )
						{
							if ( !dbg->Delete( obj, val ) )
								return CompileReturnCode_Unsupported;

							deleteop = 0;
							prevtoken = Token_Value;
						}
						else if ( incrop )
						{
							if ( !dbg->Increment( obj, ( incrop & 0x1 ) ? 1 : -1 ) )
								return CompileReturnCode_Unsupported;

							if ( incrop & 0x8 ) // prefix
								dbg->Get( obj, val );

							incrop = 0;
							prevtoken = Token_Value;
						}
					}

					if ( IsValue( prevtoken ) )
					{
						while ( unaryidx != -1 )
						{
							if ( !UnaryOp( dbg, unarybuf[unaryidx] | _op, val ) )
								return CompileReturnCode_Unsupported;

							unaryidx--;
						}
					}

					if ( !ExpectsValue( prevtoken ) )
						return CompileReturnCode_Unsupported;

					if ( unaryidx + 1 >= (int)sizeof(unarybuf) )
						return CompileReturnCode_OpBufferFull;

					unaryidx++;
					unarybuf[unaryidx] = (unsigned char)( token.type & ~_op );

					prevtoken = Token_Operator;
					break;
				}
				case Token_Sub:
				{
					if ( ExpectsValue( prevtoken ) )
						goto unary;
				}
				case Token_In:
				case Token_InstanceOf:
				case Token_Add:
				case Token_Mul:
				case Token_Div:
				case Token_Mod:
				case Token_LShift:
				case Token_RShift:
				case Token_URShift:
				case Token_BwAnd:
				case Token_BwXor:
				case Token_BwOr:
				case Token_LogicalAnd:
				case Token_LogicalOr:
				case Token_Greater:
				case Token_GreaterEq:
				case Token_Less:
				case Token_LessEq:
				case Token_Cmp3W:
				case Token_Eq:
				case Token_NotEq:
				{
					// identifier boundary
					if ( !IsValue( prevtoken ) )
						return CompileReturnCode_Unsupported;

					if ( prevtoken == Token_Ref )
					{
						if ( deleteop )
						{
							if ( !dbg->Delete( obj, val ) )
								return CompileReturnCode_Unsupported;

							deleteop = 0;
							prevtoken = Token_Value;
						}
						else if ( incrop )
						{
							if ( !dbg->Increment( obj, ( incrop & 0x1 ) ? 1 : -1 ) )
								return CompileReturnCode_Unsupported;

							if ( incrop & 0x8 ) // prefix
								dbg->Get( obj, val );

							incrop = 0;
							prevtoken = Token_Value;
						}
					}

					while ( unaryidx != -1 )
					{
						if ( !UnaryOp( dbg, unarybuf[unaryidx] | _op, val ) )
							return CompileReturnCode_Unsupported;

						unaryidx--;
					}

					while ( opbufidx != -1 )
					{
						int prev = opbuf[opbufidx] | _op;

						// Higher or equal precedence
						if ( ( prev & 0xf0 ) <= ( token.type & 0xf0 ) )
						{
							SQObjectPtr &lhs = valbuf[opbufidx];

							if ( !TwoArgOp( dbg, prev, lhs, val, val ) )
								return CompileReturnCode_Unsupported;

							lhs.Null();
							opbufidx--;
						}
						else
						{
							break;
						}
					}

					// Don't evaluate both sides
					// In both shortcuts, lhs is returned
					switch ( token.type )
					{
						case Token_LogicalAnd: if ( IsFalse( val ) ) return LexAll( closer ); break;
						case Token_LogicalOr: if ( !IsFalse( val ) ) return LexAll( closer ); break;
					}

					Assert( opbufidx + 1 < 2 );

					if ( opbufidx + 1 >= 2 )
						return CompileReturnCode_Unsupported;

					opbufidx++;
					opbuf[opbufidx] = (unsigned char)( token.type & ~_op );
					valbuf[opbufidx] = val;

					prevtoken = Token_Operator;
					break;
				}
				case Token_Increment:
				case Token_Decrement:
				{
					// decrement x000
					// increment x001
					// prefix    100x
					// postfix   000x
					if ( prevtoken == Token_Ref )
					{
						incrop = ( token.type == Token_Increment ? 0x11 : 0x10 );
					}
					else if ( ExpectsIdentifier( prevtoken ) )
					{
						incrop = ( token.type == Token_Increment ? 0x19 : 0x18 );
					}
					else
					{
						return CompileReturnCode_Unsupported;
					}

					break;
				}
				case Token_DoubleColon:
				{
					if ( !ExpectsIdentifier( prevtoken ) )
						return CompileReturnCode_Unsupported;

					token_t next = Lex();

					if ( next.type != Token_Identifier )
					{
						m_lastToken.type = 0;
						val = vm->_roottable;
						return CompileReturnCode_Unsupported;
					}

					m_lastToken = next;

					if ( !dbg->GetObj_Var( next._string, true, vm->_roottable, obj, val ) )
					{
						// allow non-existent key
						if ( Next() != Token_NewSlot )
							return CompileReturnCode_DoesNotExist;

						Assert( sq_type(vm->_roottable) == OT_TABLE );
						obj.type = objref_t::TABLE;
						obj.src = vm->_roottable;
						obj.key = CreateSQString( vm, next._string );
						prevtoken = Token_PendingKey;
					}
					else
					{
						ConvertPtr( obj );
						m_lastRef = obj;
						prevtoken = Token_Ref;
					}

					break;
				}
				case '.':
				{
					if ( !IsValue( prevtoken ) || prevtoken == Token_Integer || prevtoken == Token_Float )
						return CompileReturnCode_Unsupported;

					token_t next = Lex();

					if ( next.type != Token_Identifier )
					{
						m_lastToken = token;
						return CompileReturnCode_Unsupported;
					}

					m_lastToken = next;

					int nexttoken = Next();

					// cur.next() - save 'cur' as the call env
					if ( nexttoken =='(' )
						callenv = val;

					SQObjectPtr tmp;

					if ( !dbg->GetObj_Var( next._string, true, val, obj, tmp ) )
					{
						SQTable *del = GetDefaultDelegate( vm, sq_type(val) );
						if ( !del || !SQTable_Get( del, next._string, tmp ) )
						{
							// allow non-existent key
							if ( nexttoken != Token_NewSlot )
								return CompileReturnCode_DoesNotExist;

							switch ( sq_type(val) )
							{
								case OT_TABLE:
									obj.type = objref_t::TABLE;
									break;
								case OT_CLASS:
									obj.type = objref_t::CLASS;
									break;
								default:
									if ( is_delegable(val) && _delegable(val)->_delegate )
									{
										obj.type = objref_t::DELEGABLE_META;
										break;
									}

									return CompileReturnCode_DoesNotExist;
							}

							obj.src = val;
							obj.key = CreateSQString( vm, next._string );
							prevtoken = Token_PendingKey;
						}
						else
						{
							callenv = val;
							val = tmp;
							prevtoken = Token_Value;
						}
					}
					else
					{
						ConvertPtr( obj );
						m_lastRef = obj;
						val = tmp;
						prevtoken = Token_Ref;
					}

					break;
				}
				case '[':
				{
					if ( !IsValue( prevtoken ) )
						return CompileReturnCode_Unsupported;

					SQObjectPtr inner, tmp;

					ECompileReturnCode res = Evaluate( dbg, vm, ci, inner, ']' );
					if ( res != CompileReturnCode_Success )
						return res;

					if ( m_prevToken != ']' )
						return CompileReturnCode_Unsupported;

					if ( !dbg->GetObj_Var( inner, val, obj, tmp ) )
					{
						SQTable *del = GetDefaultDelegate( vm, sq_type(val) );
						if ( !del || !del->Get( inner, inner ) )
						{
							// allow non-existent key
							if ( Next() != Token_NewSlot )
								return CompileReturnCode_DoesNotExist;

							switch ( sq_type(val) )
							{
								case OT_TABLE:
									obj.type = objref_t::TABLE;
									break;
								case OT_CLASS:
									obj.type = objref_t::CLASS;
									break;
								default:
									if ( is_delegable(val) && _delegable(val)->_delegate )
									{
										obj.type = objref_t::DELEGABLE_META;
										break;
									}

									return CompileReturnCode_DoesNotExist;
							}

							obj.src = val;
							obj.key = inner;
							prevtoken = Token_PendingKey;
						}
						else
						{
							callenv = val;
							val = inner;
							prevtoken = Token_Value;
						}
					}
					else
					{
						ConvertPtr( obj );
						m_lastRef = obj;
						val = tmp;
						prevtoken = Token_Ref;
					}

					break;
				}
				case '(':
				{
					if ( !IsValue( prevtoken ) )
					{
						ECompileReturnCode res = Evaluate( dbg, vm, ci, val, ')' );
						if ( res != CompileReturnCode_Success )
							return res;

						if ( m_prevToken != ')' )
							return CompileReturnCode_Unsupported;

						prevtoken = Token_Value;
						break;
					}

					callparams_t cp{};

					switch ( sq_type(val) )
					{
						case OT_CLOSURE:
						case OT_NATIVECLOSURE:
						case OT_CLASS:
						{
							cp.func = val;
							break;
						}
						default:
							if ( is_delegable(val) )
							{
								if ( !_delegable(val)->_delegate ||
										!_delegable(val)->GetMetaMethod( vm, MT_CALL, cp.func ) )
									return CompileReturnCode_DoesNotExist;

								// in MT_CALL the object itself is the env,
								// and the env is passed as an extra parameter
								cp.params[ cp.paramCount++ ] = val;
								break;
							}

							return CompileReturnCode_Unsupported;
					}

					if ( !_rawval(callenv) )
					{
						const SQObjectPtr &env = ci ?
							vm->_stack._vals[ GetStackBase( vm, ci ) ] :
							vm->_roottable;
						cp.params[ cp.paramCount++ ] = env;
					}
					else
					{
						cp.params[ cp.paramCount++ ] = callenv;
						callenv.Null();
					}

					// call parameters
					for (;;)
					{
						ECompileReturnCode res = Evaluate( dbg, vm, ci, val, ')' );

						if ( res == CompileReturnCode_Success )
						{
							if ( cp.paramCount >= (int)_ArraySize(cp.params) )
								return CompileReturnCode_CallBufferFull;

							cp.params[ cp.paramCount++ ] = val;
						}
						else if ( res != CompileReturnCode_NoValue )
						{
							return res;
						}

						if ( m_prevToken == ',' )
							continue;

						if ( m_prevToken == ')' )
							break;

						return CompileReturnCode_Unsupported;
					}

					switch ( cp.paramCount )
					{
						case 1:
						{
							if ( !dbg->RunClosure( cp.func, &cp.params[0], val ) )
								return CompileReturnCode_DoesNotExist;
							break;
						}
						case 2:
						{
							if ( !dbg->RunClosure( cp.func,
										&cp.params[0],
										cp.params[1],
										val ) )
								return CompileReturnCode_DoesNotExist;
							break;
						}
						case 3:
						{
							if ( !dbg->RunClosure( cp.func,
										&cp.params[0],
										cp.params[1],
										cp.params[2],
										val ) )
								return CompileReturnCode_DoesNotExist;
							break;
						}
						case 4:
						{
							if ( !dbg->RunClosure( cp.func,
										&cp.params[0],
										cp.params[1],
										cp.params[2],
										cp.params[3],
										val ) )
								return CompileReturnCode_DoesNotExist;
							break;
						}
						case 5:
						{
							if ( !dbg->RunClosure( cp.func,
										&cp.params[0],
										cp.params[1],
										cp.params[2],
										cp.params[3],
										cp.params[4],
										val ) )
								return CompileReturnCode_DoesNotExist;
							break;
						}
						case 6:
						{
							if ( !dbg->RunClosure( cp.func,
										&cp.params[0],
										cp.params[1],
										cp.params[2],
										cp.params[3],
										cp.params[4],
										cp.params[5],
										val ) )
								return CompileReturnCode_DoesNotExist;
							break;
						}
						default: UNREACHABLE();
					}

					prevtoken = Token_Value;
					break;
				}
				case Token_Delete:
				{
					if ( !ExpectsValue( prevtoken ) )
						return CompileReturnCode_Unsupported;

					deleteop = 1;
					prevtoken = Token_Delete;
					break;
				}
#if SQUIRREL_VERSION_NUMBER < 300
				case Token_Parent:
				{
					if ( !IsValue( prevtoken ) )
					{
						if ( !ExpectsValue( prevtoken ) )
							return CompileReturnCode_Unsupported;

						// implicit this.
						val = ci ? vm->_stack._vals[ GetStackBase( vm, ci ) ] : vm->_roottable;
					}

					switch ( sq_type(val) )
					{
						case OT_TABLE:
						{
							if ( _table(val)->_delegate )
							{
								val = _table(val)->_delegate;
							}
							else
							{
								val.Null();
							}

							break;
						}
						case OT_CLASS:
						{
							if ( _class(val)->_base )
							{
								val = _class(val)->_base;
							}
							else
							{
								val.Null();
							}

							break;
						}
						default:
							return CompileReturnCode_DoesNotExist;
					}

					prevtoken = Token_Value;
					break;
				}
				case Token_Vargv:
				{
					if ( !ExpectsValue( prevtoken ) )
						return CompileReturnCode_Unsupported;

					if ( !ci )
						return CompileReturnCode_DoesNotExist;

					token_t next = Lex();

					if ( next.type != '[' )
						return CompileReturnCode_DoesNotExist;

					ECompileReturnCode res = Evaluate( dbg, vm, ci, val, ']' );
					if ( res != CompileReturnCode_Success )
						return res;

					if ( m_prevToken != ']' )
						return CompileReturnCode_Unsupported;

					if ( sq_type(val) != OT_INTEGER )
						return CompileReturnCode_DoesNotExist;

					if ( _integer(val) < 0 || _integer(val) >= ci->_vargs.size )
						return CompileReturnCode_DoesNotExist;

					val = vm->_vargsstack[ ci->_vargs.base + _integer(val) ];

					prevtoken = Token_Value;
					break;
				}
				case Token_Vargc:
				{
					if ( !ExpectsValue( prevtoken ) )
						return CompileReturnCode_Unsupported;

					if ( !ci )
						return CompileReturnCode_DoesNotExist;

					val.Null();
					val._type = OT_INTEGER;
					val._unVal.nInteger = ci->_vargs.size;

					prevtoken = Token_Value;
					break;
				}
#endif
				case Token_Assign:
				case Token_AssignAdd:
				case Token_AssignSub:
				case Token_AssignMul:
				case Token_AssignDiv:
				case Token_AssignMod:
				case Token_AssignAnd:
				case Token_AssignXor:
				case Token_AssignOr:
				case Token_AssignLS:
				case Token_AssignRS:
				case Token_AssignURS:
				case Token_NewSlot:
				{
					if ( prevtoken != Token_Ref &&
							!( token.type == Token_NewSlot && prevtoken == Token_PendingKey ) )
						return CompileReturnCode_Unsupported;

					if ( unaryidx != -1 || opbufidx != -1 )
						return CompileReturnCode_Unsupported;

					SQObjectPtr target = val;

					ECompileReturnCode res = Evaluate( dbg, vm, ci, val, closer );
					if ( res != CompileReturnCode_Success )
						return res;

					switch ( token.type )
					{
						case Token_AssignAdd:
						case Token_AssignSub:
						case Token_AssignMul:
						case Token_AssignDiv:
						case Token_AssignMod:
						case Token_AssignAnd:
						case Token_AssignXor:
						case Token_AssignOr:
						case Token_AssignLS:
						case Token_AssignRS:
						case Token_AssignURS:
						{
							if ( !TwoArgOp( dbg, token.type & ~_assign, target, val, val ) )
								return CompileReturnCode_Unsupported;
						}
						case Token_Assign:
						{
							if ( !dbg->Set( obj, val ) )
								return CompileReturnCode_Unsupported;

							return CompileReturnCode_Success;
						}
						case Token_NewSlot:
						{
							if ( !dbg->NewSlot( obj, val ) )
								return CompileReturnCode_Unsupported;

							return CompileReturnCode_Success;
						}
						default: UNREACHABLE();
					}
				}
				default:
				{
					// identifier boundary
					if ( token.type > 0 )
					{
						m_prevToken = token.type;

						if ( ExpectsValue( prevtoken ) )
							return CompileReturnCode_NoValue;

						if ( prevtoken == Token_PendingKey )
							return CompileReturnCode_Unsupported;

						if ( prevtoken == Token_Ref )
						{
							if ( deleteop )
							{
								if ( !dbg->Delete( obj, val ) )
									return CompileReturnCode_Unsupported;

								deleteop = 0;
								prevtoken = Token_Value;
							}
							else if ( incrop )
							{
								if ( !dbg->Increment( obj, ( incrop & 0x1 ) ? 1 : -1 ) )
									return CompileReturnCode_Unsupported;

								if ( incrop & 0x8 ) // prefix
									dbg->Get( obj, val );

								incrop = 0;
								prevtoken = Token_Value;
							}
						}

						while ( unaryidx != -1 )
						{
							if ( !UnaryOp( dbg, unarybuf[unaryidx] | _op, val ) )
								return CompileReturnCode_Unsupported;

							unaryidx--;
						}

						while ( opbufidx != -1 )
						{
							if ( !TwoArgOp( dbg, opbuf[opbufidx] | _op, valbuf[opbufidx], val, val ) )
								return CompileReturnCode_Unsupported;

							opbufidx--;
						}

						switch ( token.type )
						{
							case ']':
							{
								if ( closer != ']' )
									return CompileReturnCode_Unsupported;
								break;
							}
							case ')':
							{
								if ( closer != ')' )
									return CompileReturnCode_Unsupported;
								break;
							}
							case ',':
							{
								if ( closer != ')' )
									return CompileReturnCode_Unsupported;
								break;
							}
						}

						return CompileReturnCode_Success;
					}
					else if ( token.type == Err_InvalidToken )
					{
						return CompileReturnCode_Unsupported;
					}
					else
					{
						return CompileReturnCode_SyntaxError;
					}
				}
			}
		}
	}

private:
	ECompileReturnCode LexAll( int closer )
	{
		// fast shortcut - ignore everything
		if ( closer == 0 )
			return CompileReturnCode_Success;

		m_prevToken = closer;

		int depth = 0;
		int opener;

		switch ( closer )
		{
			case ')': opener = '('; break;
			case ']': opener = '['; break;
			default: UNREACHABLE();
		}

		for (;;)
		{
			token_t token = Lex();

			if ( token.type > 0 )
			{
				if ( token.type == opener )
				{
					depth++;
				}
				else if ( token.type == closer )
				{
					if ( depth )
					{
						depth--;
					}
					else
					{
						return CompileReturnCode_Success;
					}
				}
			}
			else if ( token.type == Err_InvalidToken )
			{
				return CompileReturnCode_Unsupported;
			}
			else
			{
				return CompileReturnCode_SyntaxError;
			}
		}
	}

private:
	static bool IsValue( int token )
	{
		switch ( token )
		{
			case Token_Ref:
			case Token_Value:
			case Token_Integer:
			case Token_Float:
				return true;
			default:
				return false;
		}
	}

	static bool ExpectsValue( int token )
	{
		switch ( token )
		{
			case 0:
			case '(':
			case ',':
			case Token_Operator:
				return true;
			default:
				return false;
		}
	}

	static bool ExpectsIdentifier( int token )
	{
		switch ( token )
		{
			case 0:
			case '(':
			case ',':
			case Token_Operator:
			case Token_Delete:
				return true;
			default:
				return false;
		}
	}

	static bool TwoArgOp( SQDebugServer *dbg,
			int op, const SQObjectPtr &lhs, const SQObjectPtr &rhs, SQObjectPtr &out )
	{
		Assert( ( op & _op ) == _op );

		switch ( op )
		{
			case Token_Add: return dbg->ArithOp( '+', lhs, rhs, out );
			case Token_Sub: return dbg->ArithOp( '-', lhs, rhs, out );
			case Token_Mul: return dbg->ArithOp( '*', lhs, rhs, out );
			case Token_Div: return dbg->ArithOp( '/', lhs, rhs, out );
			case Token_Mod: return dbg->ArithOp( '%', lhs, rhs, out );
			case Token_In:
			{
				switch ( sq_type(rhs) )
				{
					case OT_TABLE:
						SetBool( out, _table(rhs)->Get( lhs, out ) );
						break;
					case OT_CLASS:
						SetBool( out, _class(rhs)->Get( lhs, out ) );
						break;
					case OT_INSTANCE:
						Assert( _instance(rhs)->_class );
						SetBool( out, _instance(rhs)->_class->Get( lhs, out ) );
						break;
					case OT_ARRAY:
						SetBool( out, sq_type(lhs) == OT_INTEGER &&
								_integer(lhs) >= 0 && _integer(lhs) < _array(rhs)->Size() );
						break;
					case OT_STRING:
						SetBool( out, sq_type(lhs) == OT_INTEGER &&
								_integer(lhs) >= 0 && _integer(lhs) < _string(rhs)->_len );
						break;
					default:
						SetBool( out, false );
				}

				return true;
			}
			case Token_InstanceOf:
			{
				if ( sq_type(rhs) == OT_CLASS )
				{
					Assert( sq_type(lhs) != OT_INSTANCE || _instance(lhs)->_class );
					SetBool( out, sq_type(lhs) == OT_INSTANCE && _instance(lhs)->_class == _class(rhs) );
					return true;
				}

				return false;
			}
			case Token_LShift:
			{
				if ( ( sq_type(lhs) | sq_type(rhs) ) == OT_INTEGER )
				{
					out = (SQInteger)( _integer(lhs) << _integer(rhs) );
					return true;
				}

				return false;
			}
			case Token_RShift:
			{
				if ( ( sq_type(lhs) | sq_type(rhs) ) == OT_INTEGER )
				{
					out = (SQInteger)( _integer(lhs) >> _integer(rhs) );
					return true;
				}

				return false;
			}
			case Token_URShift:
			{
				if ( ( sq_type(lhs) | sq_type(rhs) ) == OT_INTEGER )
				{
					out = (SQInteger)( *(SQUnsignedInteger*)&_integer(lhs) >> _integer(rhs) );
					return true;
				}

				return false;
			}
			case Token_BwAnd:
			{
				if ( ( sq_type(lhs) | sq_type(rhs) ) == OT_INTEGER )
				{
					out = (SQInteger)( _integer(lhs) & _integer(rhs) );
					return true;
				}

				return false;
			}
			case Token_BwXor:
			{
				if ( ( sq_type(lhs) | sq_type(rhs) ) == OT_INTEGER )
				{
					out = (SQInteger)( _integer(lhs) ^ _integer(rhs) );
					return true;
				}

				return false;
			}
			case Token_BwOr:
			{
				if ( ( sq_type(lhs) | sq_type(rhs) ) == OT_INTEGER )
				{
					out = (SQInteger)( _integer(lhs) | _integer(rhs) );
					return true;
				}

				return false;
			}
			case Token_LogicalAnd:
			{
				out = IsFalse( lhs ) ? lhs : rhs;
				return true;
			}
			case Token_LogicalOr:
			{
				out = !IsFalse( lhs ) ? lhs : rhs;
				return true;
			}
			case Token_Greater:
			{
				int res = dbg->CompareObj( lhs, rhs );
				if ( res != DBC_NONE )
				{
					SetBool( out, ( res & DBC_G ) != 0 );
					return true;
				}

				return false;
			}
			case Token_GreaterEq:
			{
				int res = dbg->CompareObj( lhs, rhs );
				if ( res != DBC_NONE )
				{
					SetBool( out, ( res & DBC_GE ) != 0 );
					return true;
				}

				return false;
			}
			case Token_Less:
			{
				int res = dbg->CompareObj( lhs, rhs );
				if ( res != DBC_NONE )
				{
					SetBool( out, ( res & DBC_L ) != 0 );
					return true;
				}

				return false;
			}
			case Token_LessEq:
			{
				int res = dbg->CompareObj( lhs, rhs );
				if ( res != DBC_NONE )
				{
					SetBool( out, ( res & DBC_LE ) != 0 );
					return true;
				}

				return false;
			}
			case Token_Cmp3W:
			{
				int res = dbg->CompareObj( lhs, rhs );
				if ( res != DBC_NONE )
				{
					switch ( res )
					{
						case DBC_EQ: res = 0; break;
						case DBC_G:
						case DBC_GE: res = 1; break;
						case DBC_L:
						case DBC_LE: res = -1; break;
						default: UNREACHABLE();
					}
					out = (SQInteger)res;
					return true;
				}

				return false;
			}
			case Token_Eq:
			{
				bool res;
#if SQUIRREL_VERSION_NUMBER >= 300
				dbg->m_pRootVM->IsEqual( lhs, rhs, res );
#else
				dbg->m_pRootVM->IsEqual( (SQObjectPtr&)lhs, (SQObjectPtr&)rhs, res );
#endif
				SetBool( out, res );
				return true;
			}
			case Token_NotEq:
			{
				bool res;
#if SQUIRREL_VERSION_NUMBER >= 300
				dbg->m_pRootVM->IsEqual( lhs, rhs, res );
#else
				dbg->m_pRootVM->IsEqual( (SQObjectPtr&)lhs, (SQObjectPtr&)rhs, res );
#endif
				SetBool( out, !res );
				return true;
			}
			default: UNREACHABLE();
		}
	}

	static bool UnaryOp( SQDebugServer *dbg, int op, SQObjectPtr &val )
	{
		Assert( ( op & _op ) == _op );

		switch ( op )
		{
			case Token_Sub:
			{
				if ( sq_type(val) == OT_INTEGER )
				{
					_integer(val) = -_integer(val);
					return true;
				}
				else if ( sq_type(val) == OT_FLOAT )
				{
					_float(val) = -_float(val);
					return true;
				}
				else if ( is_delegable(val) && _delegable(val)->_delegate )
				{
					SQObjectPtr mm;
					if ( _delegable(val)->GetMetaMethod( dbg->m_pRootVM, MT_UNM, mm ) )
					{
						return dbg->RunClosure( mm, &val, val );
					}
				}

				return false;
			}
			case Token_BwNot:
			{
				if ( sq_type(val) == OT_INTEGER )
				{
					_integer(val) = ~_integer(val);
					return true;
				}

				return false;
			}
			case Token_Not:
			{
				SetBool( val, IsFalse( val ) );
				return true;
			}
			case Token_Typeof:
			{
				if ( is_delegable(val) && _delegable(val)->_delegate )
				{
					SQObjectPtr mm;
					if ( _delegable(val)->GetMetaMethod( dbg->m_pRootVM, MT_TYPEOF, mm ) )
					{
						return dbg->RunClosure( mm, &val, val );
					}
				}

				extern const SQChar *GetTypeName( const SQObjectPtr & );
				const SQChar *tname = GetTypeName(val);

				if ( tname )
				{
					val = SQString::Create( _ss(dbg->m_pRootVM), tname, scstrlen(tname) );
					return true;
				}

				return false;
			}
			case Token_Clone:
			{
				switch ( sq_type(val) )
				{
					case OT_TABLE:
					case OT_INSTANCE:
					case OT_ARRAY:
						return dbg->m_pRootVM->Clone( val, val );
					default:
						return false;
				}
			}
			default: UNREACHABLE();
		}
	}

private:
	int Next()
	{
		while ( m_cur < m_end )
		{
			switch ( *m_cur )
			{
				case ' ': case '\t': case '\r': case '\n':
				{
					m_cur++;
					continue;
				}
				case '\"':
				{
					return *m_cur;
				}
				case '0': case '1': case '2': case '3': case '4':
				case '5': case '6': case '7': case '8': case '9':
				{
					return *m_cur;
				}
				case '.': case ',':
				case '[': case ']':
				case '(': case ')':
				{
					return *m_cur;
				}
				case '+':
				{
					if ( m_cur + 1 < m_end && m_cur[1] == '=' )
					{
						return Token_AssignAdd;
					}
					else if ( m_cur + 1 < m_end && m_cur[1] == '+' )
					{
						return Token_Increment;
					}
					else
					{
						return Token_Add;
					}
				}
				case '-':
				{
					if ( m_cur + 1 < m_end && m_cur[1] == '=' )
					{
						return Token_AssignSub;
					}
					else if ( m_cur + 1 < m_end && m_cur[1] == '-' )
					{
						return Token_Decrement;
					}
					else
					{
						return Token_Sub;
					}
				}
				case '*':
				{
					if ( m_cur + 1 < m_end && m_cur[1] == '=' )
					{
						return Token_AssignMul;
					}
					else
					{
						return Token_Mul;
					}
				}
				case '/':
				{
					if ( m_cur + 1 < m_end && m_cur[1] == '=' )
					{
						return Token_AssignDiv;
					}
					else
					{
						return Token_Div;
					}
				}
				case '%':
				{
					if ( m_cur + 1 < m_end && m_cur[1] == '=' )
					{
						return Token_AssignMod;
					}
					else
					{
						return Token_Mod;
					}
				}
				case '~':
				{
					return Token_BwNot;
				}
				case '^':
				{
					if ( m_cur + 1 < m_end && m_cur[1] == '=' )
					{
						return Token_AssignXor;
					}
					else
					{
						return Token_BwXor;
					}
				}
				case '=':
				{
					if ( m_cur + 1 < m_end && m_cur[1] == '=' )
					{
						return Token_Eq;
					}
					else
					{
						return Token_Assign;
					}
				}
				case '!':
				{
					if ( m_cur + 1 < m_end && m_cur[1] == '=' )
					{
						return Token_NotEq;
					}
					else
					{
						return Token_Not;
					}
				}
				case '<':
				{
					if ( m_cur + 1 < m_end && m_cur[1] == '<' )
					{
						if ( m_cur + 2 < m_end && m_cur[2] == '=' )
						{
							return Token_AssignLS;
						}
						else
						{
							return Token_LShift;
						}
					}
					else if ( m_cur + 1 < m_end && m_cur[1] == '=' )
					{
						if ( m_cur + 2 < m_end && m_cur[2] == '>' )
						{
							return Token_Cmp3W;
						}
						else
						{
							return Token_LessEq;
						}
					}
					else if ( m_cur + 1 < m_end && m_cur[1] == '-' )
					{
						return Token_NewSlot;
					}
					else
					{
						return Token_Less;
					}
				}
				case '>':
				{
					if ( m_cur + 1 < m_end && m_cur[1] == '>' )
					{
						if ( m_cur + 2 < m_end && m_cur[2] == '>' )
						{
							if ( m_cur + 3 < m_end && m_cur[3] == '=' )
							{
								return Token_AssignURS;
							}
							else
							{
								return Token_URShift;
							}
						}
						else if ( m_cur + 2 < m_end && m_cur[2] == '=' )
						{
							return Token_AssignRS;
						}
						else
						{
							return Token_RShift;
						}
					}
					else if ( m_cur + 1 < m_end && m_cur[1] == '=' )
					{
						return Token_GreaterEq;
					}
					else
					{
						return Token_Greater;
					}
				}
				case '&':
				{
					if ( m_cur + 1 < m_end && m_cur[1] == '&' )
					{
						return Token_LogicalAnd;
					}
					else if ( m_cur + 1 < m_end && m_cur[1] == '=' )
					{
						return Token_AssignAnd;
					}
					else
					{
						return Token_BwAnd;
					}
				}
				case '|':
				{
					if ( m_cur + 1 < m_end && m_cur[1] == '|' )
					{
						return Token_LogicalOr;
					}
					else if ( m_cur + 1 < m_end && m_cur[1] == '=' )
					{
						return Token_AssignOr;
					}
					else
					{
						return Token_BwOr;
					}
				}
				case ':':
				{
					if ( m_cur + 1 < m_end && m_cur[1] == ':' )
					{
						return Token_DoubleColon;
					}
					else
					{
						return Err_InvalidToken;
					}
				}
				case '\'':
				{
					return *m_cur;
				}
				default:
				{
					if ( _isalpha( *(unsigned char*)m_cur ) || *m_cur == '_' )
					{
						return *m_cur;
					}
					else
					{
						return Err_InvalidToken;
					}
				}
			}
		}

		return Token_End;
	}

	token_t Lex()
	{
		token_t token;

		while ( m_cur < m_end )
		{
			switch ( *m_cur )
			{
				case ' ': case '\t': case '\r': case '\n':
				{
					m_cur++;
					continue;
				}
				case '\"':
				{
					ParseString( token );
					return token;
				}
				case '0': case '1': case '2': case '3': case '4':
				case '5': case '6': case '7': case '8': case '9':
				{
					ParseNumber( token, false );
					return token;
				}
				case '.': case ',':
				case '[': case ']':
				case '(': case ')':
				{
					token.type = *m_cur++;
					return token;
				}
				case '+':
				{
					m_cur++;

					if ( m_cur < m_end && *m_cur == '=' )
					{
						m_cur++;
						token.type = Token_AssignAdd;
					}
					else if ( m_cur < m_end && *m_cur == '+' )
					{
						m_cur++;
						token.type = Token_Increment;
					}
					else
					{
						token.type = Token_Add;
					}

					return token;
				}
				case '-':
				{
					m_cur++;

					if ( m_cur < m_end && *m_cur == '=' )
					{
						m_cur++;
						token.type = Token_AssignSub;
					}
					else if ( m_cur < m_end && *m_cur == '-' )
					{
						m_cur++;
						token.type = Token_Decrement;
					}
					else
					{
						token.type = Token_Sub;
					}

					return token;
				}
				case '*':
				{
					m_cur++;

					if ( m_cur < m_end && *m_cur == '=' )
					{
						m_cur++;
						token.type = Token_AssignMul;
					}
					else
					{
						token.type = Token_Mul;
					}

					return token;
				}
				case '/':
				{
					m_cur++;

					if ( m_cur < m_end && *m_cur == '=' )
					{
						m_cur++;
						token.type = Token_AssignDiv;
					}
					else
					{
						token.type = Token_Div;
					}

					return token;
				}
				case '%':
				{
					m_cur++;

					if ( m_cur < m_end && *m_cur == '=' )
					{
						m_cur++;
						token.type = Token_AssignMod;
					}
					else
					{
						token.type = Token_Mod;
					}

					return token;
				}
				case '~':
				{
					m_cur++;
					token.type = Token_BwNot;
					return token;
				}
				case '^':
				{
					m_cur++;

					if ( m_cur < m_end && *m_cur == '=' )
					{
						m_cur++;
						token.type = Token_AssignXor;
					}
					else
					{
						token.type = Token_BwXor;
					}

					return token;
				}
				case '=':
				{
					m_cur++;

					if ( m_cur < m_end && *m_cur == '=' )
					{
						m_cur++;
						token.type = Token_Eq;
					}
					else
					{
						token.type = Token_Assign;
					}

					return token;
				}
				case '!':
				{
					m_cur++;

					if ( m_cur < m_end && *m_cur == '=' )
					{
						m_cur++;
						token.type = Token_NotEq;
					}
					else
					{
						token.type = Token_Not;
					}

					return token;
				}
				case '<':
				{
					m_cur++;

					if ( m_cur < m_end && *m_cur == '<' )
					{
						m_cur++;

						if ( m_cur < m_end && *m_cur == '=' )
						{
							m_cur++;
							token.type = Token_AssignLS;
						}
						else
						{
							token.type = Token_LShift;
						}
					}
					else if ( m_cur < m_end && *m_cur == '=' )
					{
						m_cur++;

						if ( m_cur < m_end && *m_cur == '>' )
						{
							m_cur++;
							token.type = Token_Cmp3W;
						}
						else
						{
							token.type = Token_LessEq;
						}
					}
					else if ( m_cur < m_end && *m_cur == '-' )
					{
						m_cur++;
						token.type = Token_NewSlot;
					}
					else
					{
						token.type = Token_Less;
					}

					return token;
				}
				case '>':
				{
					m_cur++;

					if ( m_cur < m_end && *m_cur == '>' )
					{
						m_cur++;

						if ( m_cur < m_end && *m_cur == '>' )
						{
							m_cur++;

							if ( m_cur < m_end && *m_cur == '=' )
							{
								m_cur++;
								token.type = Token_AssignURS;
							}
							else
							{
								token.type = Token_URShift;
							}
						}
						else if ( m_cur < m_end && *m_cur == '=' )
						{
							m_cur++;
							token.type = Token_AssignRS;
						}
						else
						{
							token.type = Token_RShift;
						}
					}
					else if ( m_cur < m_end && *m_cur == '=' )
					{
						m_cur++;
						token.type = Token_GreaterEq;
					}
					else
					{
						token.type = Token_Greater;
					}

					return token;
				}
				case '&':
				{
					m_cur++;

					if ( m_cur < m_end && *m_cur == '&' )
					{
						m_cur++;
						token.type = Token_LogicalAnd;
					}
					else if ( m_cur < m_end && *m_cur == '=' )
					{
						m_cur++;
						token.type = Token_AssignAnd;
					}
					else
					{
						token.type = Token_BwAnd;
					}

					return token;
				}
				case '|':
				{
					m_cur++;

					if ( m_cur < m_end && *m_cur == '|' )
					{
						m_cur++;
						token.type = Token_LogicalOr;
					}
					else if ( m_cur < m_end && *m_cur == '=' )
					{
						m_cur++;
						token.type = Token_AssignOr;
					}
					else
					{
						token.type = Token_BwOr;
					}

					return token;
				}
				case ':':
				{
					m_cur++;

					if ( m_cur < m_end && *m_cur == ':' )
					{
						m_cur++;
						token.type = Token_DoubleColon;
					}
					else
					{
						token.type = Err_InvalidToken;
					}

					return token;
				}
				case '\'':
				{
					ParseChar( token );
					return token;
				}
				default:
				{
					if ( _isalpha( *(unsigned char*)m_cur ) || *m_cur == '_' )
					{
						char *pStart = m_cur++;
						while ( m_cur < m_end && ( _isalnum( *(unsigned char*)m_cur ) || *m_cur == '_' ) )
							m_cur++;

						token.type = Token_Identifier;
						token._string.Assign( pStart, m_cur - pStart );

						if ( token._string.len == 2 ||
								token._string.len == 4 || token._string.len == 5 || token._string.len == 6 )
						{
							if ( token._string.IsEqualTo("in") )
							{
								token.type = Token_In;
							}
							else if ( token._string.IsEqualTo("this") )
							{
								token.type = Token_This;
							}
							else if ( token._string.IsEqualTo("null") )
							{
								token.type = Token_Null;
							}
							else if ( token._string.IsEqualTo("true") )
							{
								token.type = Token_True;
								token._integer = 1;
							}
							else if ( token._string.IsEqualTo("false") )
							{
								token.type = Token_False;
								token._integer = 0;
							}
							else if ( token._string.IsEqualTo("typeof") )
							{
								token.type = Token_Typeof;
							}
							else if ( token._string.IsEqualTo("clone") )
							{
								token.type = Token_Clone;
							}
							else if ( token._string.IsEqualTo("delete") )
							{
								token.type = Token_Delete;
							}
#if SQUIRREL_VERSION_NUMBER < 300
							else if ( token._string.IsEqualTo("parent") )
							{
								token.type = Token_Parent;
							}
							else if ( token._string.IsEqualTo("vargv") )
							{
								token.type = Token_Vargv;
							}
							else if ( token._string.IsEqualTo("vargc") )
							{
								token.type = Token_Vargc;
							}
#endif
						}
					}
					else
					{
						token.type = Err_InvalidToken;
					}

					return token;
				}
			}
		}

		token.type = Token_End;
		return token;
	}

	void ParseString( token_t &token )
	{
		char *pStart = ++m_cur;

		for (;;)
		{
			if ( m_cur >= m_end )
			{
				token.type = Err_UnfinishedString;
				return;
			}

			// end
			if ( *m_cur == '\"' )
			{
				token.type = Token_String;
				token._string.Assign( pStart, m_cur - pStart );
				m_cur++;
				return;
			}

			if ( *m_cur != '\\' )
			{
				m_cur++;
				continue;
			}

			if ( m_cur + 1 >= m_end )
			{
				token.type = Err_UnfinishedString;
				return;
			}

#define _shift( bytesWritten, bytesRead ) \
	Assert( (bytesWritten) < (bytesRead) ); \
	memmove( m_cur + (bytesWritten), m_cur + (bytesRead), m_end - ( m_cur + (bytesRead) ) ); \
	m_cur += (bytesWritten); \
	m_end -= (bytesRead) - (bytesWritten); \
	m_expr.len -= (bytesRead) - (bytesWritten);

			switch ( m_cur[1] )
			{
				case '\\':
shift_one:
					_shift( 0, 1 );
					m_cur++;
					break;
				case '\"': goto shift_one;
				case '\'': goto shift_one;
				case 'a': m_cur[1] = '\a'; goto shift_one;
				case 'b': m_cur[1] = '\b'; goto shift_one;
				case 'f': m_cur[1] = '\f'; goto shift_one;
				case 'n': m_cur[1] = '\n'; goto shift_one;
				case 'r': m_cur[1] = '\r'; goto shift_one;
				case 't': m_cur[1] = '\t'; goto shift_one;
				case 'v': m_cur[1] = '\v'; goto shift_one;
				case '0': m_cur[1] = '\0'; goto shift_one;
				case 'x':
#ifndef SQUNICODE
				{
					if ( m_cur + 2 >= m_end )
					{
						token.type = Err_InvalidXEscape;
						return;
					}

					int len = 0;
					for ( char *x = min( m_cur + 2, m_end ), *end = min( x + 2, m_end );
							x < end && _isxdigit( *(unsigned char*)x );
							x++ )
					{
						len++;
					}

					if ( len == 0 )
					{
						token.type = Err_InvalidXEscape;
						return;
					}

					unsigned int val;
					if ( !atox( { m_cur + 2, len }, &val ) )
					{
						token.type = Err_InvalidXEscape;
						return;
					}

					m_cur[0] = (char)val;

					_shift( 1, len + 2 );
					break;
				}
#endif
				case 'u':
				{
					if ( m_cur + 2 >= m_end )
					{
						token.type = Err_InvalidU16Escape;
						return;
					}

					int len = 0;
					for ( char *x = min( m_cur + 2, m_end ), *end = min( x + 4, m_end );
							x < end && _isxdigit( *(unsigned char*)x );
							x++ )
					{
						len++;
					}

					if ( len == 0 )
					{
						token.type = Err_InvalidU16Escape;
						return;
					}

					unsigned int val;
					if ( !atox( { m_cur + 2, len }, &val ) )
					{
						token.type = Err_InvalidU16Escape;
						return;
					}

					if ( val <= 0xFF )
					{
						m_cur[0] = (char)val;

						_shift( 1, len + 2 );
						break;
					}
					else if ( val <= 0x7FF )
					{
						UTF8_2_FROM_UTF32( (unsigned char*)m_cur, val );

						_shift( 2, len + 2 );
						break;
					}
					else if ( UTF_SURROGATE(val) )
					{
						Assert( len == 4 );

						if ( UTF_SURROGATE_LEAD(val) )
						{
							if ( m_cur + 11 < m_end &&
									m_cur[6] == '\\' && m_cur[7] == m_cur[1] &&
									_isxdigit( m_cur[8] ) && _isxdigit( m_cur[9] ) &&
									_isxdigit( m_cur[10] ) && _isxdigit( m_cur[11] ) )
							{
								unsigned int low;
								atox( { m_cur + 8, 4 }, &low );

								if ( UTF_SURROGATE_TRAIL( low ) )
								{
									val = UTF32_FROM_UTF16_SURROGATE( val, low );
									UTF8_4_FROM_UTF32( (unsigned char*)m_cur, val );

									_shift( 4, len + 2 + 6 );
									break;
								}
							}
						}
					}

					UTF8_3_FROM_UTF32( (unsigned char*)m_cur, val );

					_shift( 3, len + 2 );
					break;
				}
				case 'U':
				{
					if ( m_cur + 2 >= m_end )
					{
						token.type = Err_InvalidU32Escape;
						return;
					}

					int len = 0;
					for ( char *x = min( m_cur + 2, m_end ), *end = min( x + 8, m_end );
							x < end && _isxdigit( *(unsigned char*)x );
							x++ )
					{
						len++;
					}

					if ( len == 0 )
					{
						token.type = Err_InvalidU32Escape;
						return;
					}

					unsigned int val;
					if ( !atox( { m_cur + 2, len }, &val ) )
					{
						token.type = Err_InvalidU32Escape;
						return;
					}

					if ( val <= 0xFF )
					{
						m_cur[0] = (char)val;

						_shift( 1, len + 2 );
						break;
					}
					else if ( val <= 0x7FF )
					{
						UTF8_2_FROM_UTF32( (unsigned char*)m_cur, val );

						_shift( 2, len + 2 );
						break;
					}
					else if ( UTF_SURROGATE(val) )
					{
						// next could be \u or \U with differing lengths,
						// just ignore surrogates
						token.type = Err_InvalidToken;
						return;
					}
					else if ( val <= 0xFFFF )
					{
						UTF8_3_FROM_UTF32( (unsigned char*)m_cur, val );

						_shift( 3, len + 2 );
						break;
					}
					else
					{
						UTF8_4_FROM_UTF32( (unsigned char*)m_cur, val );

						_shift( 4, len + 2 );
						break;
					}
				}
				default:
					token.type = Err_InvalidEscape;
					return;
			}

#undef _shift
		}
	}

	void ParseChar( token_t &token )
	{
		m_cur++;

		if ( m_cur + 1 >= m_end )
		{
			token.type = Err_UnfinishedChar;
			return;
		}

		unsigned char c = *m_cur;

		if ( c == '\\' )
		{
			m_cur++;

			if ( m_cur + 1 >= m_end )
			{
				token.type = Err_UnfinishedChar;
				return;
			}

			switch ( *m_cur )
			{
				case '\\': break;
				case '\"': c = '\"'; break;
				case '\'': c = '\''; break;
				case 'a': c = '\a'; break;
				case 'b': c = '\b'; break;
				case 'f': c = '\f'; break;
				case 'n': c = '\n'; break;
				case 'r': c = '\r'; break;
				case 't': c = '\t'; break;
				case 'v': c = '\v'; break;
				case '0': c = '\0'; break;
				case 'x':
				{
					char *pStart = ++m_cur;
					while ( m_cur < m_end && _isxdigit( *m_cur ) )
						m_cur++;

					if ( m_cur == pStart ||
							(int)( m_cur - pStart ) > (int)sizeof(SQChar) * 2 ||
							m_cur >= m_end ||
							*m_cur != '\'' )
					{
						token.type = Err_UnfinishedChar;
						return;
					}

					unsigned int val;
					if ( !atox( { pStart, (int)( m_cur - pStart ) }, &val ) )
					{
						token.type = Err_UnfinishedChar;
						return;
					}

					token.type = Token_Integer;
					token._integer = val;
					m_cur++;
					return;
				}
				default:
					token.type = Err_UnfinishedChar;
					return;
			}
		}

		m_cur++;

		if ( *m_cur == '\'' )
		{
			token.type = Token_Integer;
			token._integer = c;
			m_cur++;
		}
		else
		{
			token.type = Err_UnfinishedChar;
		}
	}

	void ParseNumber( token_t &token, bool neg )
	{
		const char *pStart = m_cur;

		if ( *m_cur == '0' )
		{
			m_cur++;

			switch ( *m_cur )
			{
				case 'x':
				{
					m_cur++;

					while ( m_cur < m_end && _isxdigit( *(unsigned char*)m_cur ) )
						m_cur++;

					string_t str;
					str.Assign( pStart, m_cur - pStart );

					if ( str.len < 3 || str.len > (int)sizeof(SQInteger) * 2 ||
							!atox( str, &token._integer ) )
					{
						token.type = Err_InvalidHexadecimal;
						return;
					}

					token.type = Token_Integer;

					if ( neg )
						token._integer = -token._integer;

					return;
				}
				case 'b':
				{
					m_cur++;

					while ( m_cur < m_end && IN_RANGE_CHAR( *(unsigned char*)m_cur, '0', '1' ) )
						m_cur++;

					string_t str;
					str.Assign( pStart, m_cur - pStart );

					if ( str.len < 3 || str.len > (int)sizeof(SQInteger) * 8 )
					{
						token.type = Err_InvalidBinary;
						return;
					}

					token.type = Token_Integer;
					token._integer = 0;

					int inputbitlen = str.len - 2;

					for ( int i = str.len - 1; i >= 2; i-- )
					{
						switch ( str.ptr[i] )
						{
							case '1':
								token._integer |= ( (SQUnsignedInteger)1 << ( ( inputbitlen - 1 ) - ( i - 2 ) ) );
							case '0':
								continue;
							default: UNREACHABLE();
						}
					}

					if ( neg )
						token._integer = -token._integer;

					return;
				}
				default:
				{
					while ( m_cur < m_end && IN_RANGE_CHAR( *(unsigned char*)m_cur, '0', '7' ) )
						m_cur++;

					string_t str;
					str.Assign( pStart, m_cur - pStart );

					if ( !atoo( str, &token._integer ) )
					{
						token.type = Err_InvalidOctal;
						return;
					}

					token.type = Token_Integer;

					if ( neg )
						token._integer = -token._integer;

					break;
				}
			}
		}
		else
		{
			Assert( IN_RANGE_CHAR( *(unsigned char*)m_cur, '1', '9' ) );

			do
			{
				m_cur++;
			}
			while ( IN_RANGE_CHAR( *(unsigned char*)m_cur, '0', '9' ) );

			string_t str;
			str.Assign( pStart, m_cur - pStart );

			if ( str.len > (int)FMT_INT_LEN - 1 || !atoi( str, &token._integer ) )
			{
				token.type = Err_InvalidDecimal;
				return;
			}

			token.type = Token_Integer;

			if ( neg )
				token._integer = -token._integer;
		}

		bool isFloat = 0;

		if ( *m_cur == '.' )
		{
			isFloat = 1;
			m_cur++;

			while ( m_cur < m_end && IN_RANGE_CHAR( *(unsigned char*)m_cur, '0', '9' ) )
				m_cur++;
		}

		if ( m_cur < m_end && ( *m_cur == 'e' || *m_cur == 'E' ) )
		{
			isFloat = 1;
			m_cur++;

			if ( m_cur >= m_end )
			{
				token.type = Err_InvalidFloat;
				return;
			}

			if ( *m_cur == '-' || *m_cur == '+' )
			{
				m_cur++;

				if ( m_cur >= m_end )
				{
					token.type = Err_InvalidFloat;
					return;
				}
			}

			while ( m_cur < m_end && IN_RANGE_CHAR( *(unsigned char*)m_cur, '0', '9' ) )
				m_cur++;
		}

		if ( isFloat )
		{
			string_t str;
			str.Assign( pStart, m_cur - pStart );

			char c = str.ptr[str.len];
			str.ptr[str.len] = 0;
			token.type = Token_Float;
			token._float = (SQFloat)strtod( str.ptr, NULL );
			str.ptr[str.len] = c;

			if ( neg )
				token._float = -token._float;
		}
	}
};

SQDebugServer::ECompileReturnCode SQDebugServer::Evaluate( string_t &expression,
		HSQUIRRELVM vm, const SQVM::CallInfo *ci, SQObjectPtr &ret )
{
	CCompiler c( expression );
	ECompileReturnCode r = c.Evaluate( this, vm, ci, ret );
	return r;
}

SQDebugServer::ECompileReturnCode SQDebugServer::Evaluate( string_t &expression,
		HSQUIRRELVM vm, const SQVM::CallInfo *ci, SQObjectPtr &ret, objref_t &obj )
{
	CCompiler c( expression );
	ECompileReturnCode r = c.Evaluate( this, vm, ci, ret );
	obj = c.m_lastRef;
	return r;
}

SQTable *SQDebugServer::GetDefaultDelegate( HSQUIRRELVM vm, SQObjectType type )
{
	SQTable *del;

	switch ( type )
	{
		case OT_TABLE: del = _table(_ss(vm)->_table_default_delegate); break;
		case OT_ARRAY: del = _table(_ss(vm)->_array_default_delegate); break;
		case OT_STRING: del = _table(_ss(vm)->_string_default_delegate); break;
		case OT_INTEGER:
		case OT_FLOAT:
		case OT_BOOL: del = _table(_ss(vm)->_number_default_delegate); break;
		case OT_GENERATOR: del = _table(_ss(vm)->_generator_default_delegate); break;
		case OT_NATIVECLOSURE:
		case OT_CLOSURE: del = _table(_ss(vm)->_closure_default_delegate); break;
		case OT_THREAD: del = _table(_ss(vm)->_thread_default_delegate); break;
		case OT_CLASS: del = _table(_ss(vm)->_class_default_delegate); break;
		case OT_INSTANCE: del = _table(_ss(vm)->_instance_default_delegate); break;
		case OT_WEAKREF: del = _table(_ss(vm)->_weakref_default_delegate); break;
		default: del = NULL;
	}

	return del;
}

bool SQDebugServer::ArithOp( char op, const SQObjectPtr &lhs, const SQObjectPtr &rhs, SQObjectPtr &out )
{
	SQObjectType tl = sq_type(lhs);
	SQObjectType tr = sq_type(rhs);

	if ( ( tl | tr ) == OT_INTEGER )
	{
		switch ( op )
		{
			case '+':
				out = _integer(lhs) + _integer(rhs);
				return true;
			case '-':
				out = _integer(lhs) - _integer(rhs);
				return true;
			case '*':
				out = _integer(lhs) * _integer(rhs);
				return true;
			case '/':
				if ( _integer(rhs) == 0 || ( _integer(rhs) == -1 && _integer(lhs) == INT_MIN ) )
					return false;
				out = _integer(lhs) / _integer(rhs);
				return true;
			case '%':
				if ( _integer(rhs) == 0 || ( _integer(rhs) == -1 && _integer(lhs) == INT_MIN ) )
					return false;
				out = _integer(lhs) % _integer(rhs);
				return true;
			default: UNREACHABLE();
		}
	}

	if ( ( tl | tr ) == OT_FLOAT )
	{
		switch ( op )
		{
			case '+':
				out = _float(lhs) + _float(rhs);
				return true;
			case '-':
				out = _float(lhs) - _float(rhs);
				return true;
			case '*':
				out = _float(lhs) * _float(rhs);
				return true;
			case '/':
				out = _float(lhs) / _float(rhs);
				return true;
			case '%':
				out = (SQFloat)fmod( _float(lhs), _float(rhs) );
				return true;
			default: UNREACHABLE();
		}
	}

	if ( ( tl | tr ) == ( OT_INTEGER | OT_FLOAT ) )
	{
		SQFloat fl, fr;

		if ( tl == OT_INTEGER )
		{
			fl = (SQFloat)_integer(lhs);
			fr = _float(rhs);
		}
		else if ( tr == OT_INTEGER )
		{
			fl = _float(lhs);
			fr = (SQFloat)_integer(rhs);
		}
		else UNREACHABLE();

		switch ( op )
		{
			case '+':
				out = fl + fr;
				return true;
			case '-':
				out = fl - fr;
				return true;
			case '*':
				out = fl * fr;
				return true;
			case '/':
				out = fl / fr;
				return true;
			case '%':
				out = (SQFloat)fmod( fl, fr );
				return true;
			default: UNREACHABLE();
		}
	}

	if ( ( ( tl | tr ) & OT_STRING ) == OT_STRING && op == '+' )
	{
		// NOTE: calls MT_TOSTRING
		return m_pRootVM->StringCat( lhs, rhs, out );
	}

	if ( is_delegable(lhs) && _delegable(lhs)->_delegate )
	{
		SQMetaMethod emm;
		switch ( op )
		{
			case '+': emm = MT_ADD; break;
			case '-': emm = MT_SUB; break;
			case '*': emm = MT_MUL; break;
			case '/': emm = MT_DIV; break;
			case '%': emm = MT_MODULO; break;
			default: UNREACHABLE();
		}

		SQObjectPtr mm;
		if ( _delegable(lhs)->GetMetaMethod( m_pRootVM, emm, mm ) )
		{
			return RunClosure( mm, &lhs, rhs, out );
		}
	}

	return false;
}
#endif

void SQDebugServer::ConvertPtr( objref_t &obj )
{
	// doesn't convert stack/outer vars
	if ( obj.type & ~objref_t::PTR )
		obj.type = (objref_t::EOBJREF)( obj.type & ~objref_t::PTR );
}

bool SQDebugServer::GetObj_Var( const SQObjectPtr &key, const SQObjectPtr &var, objref_t &out, SQObjectPtr &value )
{
	Assert( &key != &value );

	switch ( sq_type(var) )
	{
		case OT_TABLE:
		{
			SQTable *t = _table(var);
			do
			{
				if ( t->Get( key, value ) )
				{
					out.type = objref_t::TABLE;
					out.src = t;
					out.key = key;
					return true;
				}
			}
			while ( ( t = t->_delegate ) != NULL );

			break;
		}
		case OT_INSTANCE:
		{
			if ( _instance(var)->Get( key, value ) )
			{
				out.type = objref_t::INSTANCE;
				out.src = var;
				out.key = key;
				return true;
			}

			break;
		}
		case OT_CLASS:
		{
			if ( _class(var)->_members->Get( key, value ) )
			{
				out.type = (objref_t::EOBJREF)( objref_t::PTR | objref_t::CLASS );

				if ( _isfield(value) )
				{
					out.ptr = &_class(var)->_defaultvalues[ _member_idx(value) ].val;
				}
				else
				{
					out.ptr = &_class(var)->_methods[ _member_idx(value) ].val;
				}

				out.src = var;
				out.key = key;

				value = *out.ptr;
				return true;
			}

			return false;
		}
		case OT_ARRAY:
		{
			if ( sq_type(key) == OT_INTEGER &&
					_integer(key) >= 0 && _integer(key) < _array(var)->Size() )
			{
				out.type = (objref_t::EOBJREF)( objref_t::PTR | objref_t::ARRAY );
				out.ptr = &_array(var)->_values[ _integer(key) ];

				out.src = var;
				out.key = _integer(key);

				value = *out.ptr;
				return true;
			}

			return false;
		}
		case OT_STRING:
		{
			if ( sq_type(key) == OT_INTEGER &&
					_integer(key) >= 0 && _integer(key) < _string(var)->_len )
			{
				out.type = objref_t::INT;
#ifdef SQUNICODE
				out.val = (int)_string(var)->_val[ _integer(key) ];
#else
				out.val = (int)(unsigned char)_string(var)->_val[ _integer(key) ];
#endif
				value = (SQInteger)out.val;
				return true;
			}

			return false;
		}
		case OT_USERDATA:
			break;
		default:
			return false;
	}

	// metamethods
	switch ( sq_type(var) )
	{
		case OT_INSTANCE:
		case OT_TABLE:
		case OT_USERDATA:
		{
			SQObjectPtr mm;
			if ( _delegable(var)->GetMetaMethod( m_pRootVM, MT_GET, mm ) )
			{
				if ( RunClosure( mm, &var, key, value ) )
				{
					out.type = objref_t::DELEGABLE_META;
					out.src = var;
					out.key = key;
					return true;
				}
			}

			break;
		}
		default: UNREACHABLE();
	}

	// user defined
	if ( sq_type(var) == OT_INSTANCE )
	{
		const SQObjectPtr *def = GetClassDefCustomMembers( _instance(var)->_class );

		if ( def )
		{
			SQObjectPtr custommembers = *def;

			if ( sq_type(custommembers) == OT_CLOSURE )
				RunClosure( custommembers, &var, custommembers );

			if ( sq_type(custommembers) == OT_ARRAY )
			{
				objref_t tmp;
				SQObjectPtr strName = CreateSQString( m_pRootVM, _SC("name") );
				SQObjectPtr strGet = CreateSQString( m_pRootVM, _SC("get") );
				SQObjectPtr name;

				for ( unsigned int i = 0; i < _array(custommembers)->_values.size(); i++ )
				{
					const SQObjectPtr &memdef = _array(custommembers)->_values[i];

					if ( GetObj_Var( strName, memdef, tmp, name ) &&
							sq_type(key) == sq_type(name) && _rawval(key) == _rawval(name) )
					{
						if ( GetObj_Var( strGet, memdef, tmp, value ) &&
								CallCustomMembersGetFunc( value, &var, key, value ) )
						{
							out.type = objref_t::CUSTOMMEMBER;
							out.src = var;
							out.key = key;
							return true;
						}

						return false;
					}
				}
			}
		}
	}

	// could be a default delegate func
	return false;
}

bool SQDebugServer::GetObj_Var( string_t &expression, bool identifierIsString, const SQObjectPtr &var,
		objref_t &out, SQObjectPtr &value )
{
	switch ( sq_type(var) )
	{
		case OT_ARRAY:
		{
			if ( expression.len <= 2 || !( expression.ptr[0] == '[' && expression.ptr[expression.len-1] == ']' ) )
				return false;

			expression.ptr++;
			expression.len -= 2;

			SQInteger idx;
			if ( atoi( expression, &idx ) &&
					idx >= 0 && idx < _array(var)->Size() )
			{
				out.type = (objref_t::EOBJREF)( objref_t::PTR | objref_t::ARRAY );
				out.ptr = &_array(var)->_values[idx];

				out.src = var;
				out.key = idx;

				value = *out.ptr;
				return true;
			}

			return false;
		}
		// Used in local indexing
		case OT_STRING:
		{
			if ( expression.len <= 2 || !( expression.ptr[0] == '[' && expression.ptr[expression.len-1] == ']' ) )
				return false;

			expression.ptr++;
			expression.len -= 2;

			SQInteger idx;
			if ( atoi( expression, &idx ) &&
					idx >= 0 && idx < _string(var)->_len )
			{
				out.type = objref_t::INT;
#ifdef SQUNICODE
				out.val = (int)_string(var)->_val[idx];
#else
				out.val = (int)(unsigned char)_string(var)->_val[idx];
#endif
				value = (SQInteger)out.val;
				return true;
			}

			return false;
		}
		case OT_TABLE:
		case OT_INSTANCE:
		case OT_CLASS:
		case OT_USERDATA:
			break;
		default:
			return false;
	}

	SQObjectPtr identifier;

	if ( identifierIsString )
	{
		identifier = CreateSQString( m_pRootVM, expression );
	}
	else
	{
		switch ( expression.ptr[0] )
		{
			// string
			case '\"':
			{
				Assert( expression.len > 2 );

				if ( expression.len <= 2 )
					return false;

				expression.ptr++;
				expression.len -= 2;

#ifdef SQUNICODE
				SQChar tmp[1024];
				int len = UTF8ToSQUnicode< true >( tmp, sizeof(tmp), expression.ptr, expression.len );
				identifier = CreateSQString( m_pRootVM, { tmp, len } );
#else
				UndoEscape( expression.ptr, &expression.len );
				identifier = CreateSQString( m_pRootVM, expression );
#endif
				break;
			}
			// integer
			case '[':
			{
				Assert( expression.len > 2 );

				if ( expression.len <= 2 )
					return false;

				expression.ptr++;
				expression.len -= 2;

				SQInteger val;
				if ( !atoi( expression, &val ) )
					return false;

				identifier = val;
				break;
			}
#if 0
			// raw bytes
			case 'R':
			{
				Assert( expression.len > 4 );

				if ( expression.len <= 4 )
					return false;

				Assert( expression.ptr[1] == '\"' );

				expression.ptr += 2;
				expression.len -= 3;

				if ( !ReadStringifiedBytes( expression.ptr, &expression.len ) )
					return false;

				sqstring_t sqstr( (SQChar*)expression.ptr, expression.len / sizeof(SQChar) );
				identifier = CreateSQString( m_pRootVM, sqstr );
				break;
			}
#endif
			default:
			{
				// object, check every member
				if ( expression.StartsWith("0x") )
				{
					Assert( expression.len >= FMT_PTR_LEN );

					if ( expression.len < FMT_PTR_LEN )
						return false;

					expression.len = FMT_PTR_LEN;

					SQUnsignedInteger pKey;
					if ( !atox( expression, &pKey ) )
						return false;

					SQObjectPtr obj = var;
					SQ_FOREACH_OP( obj, key, val )
					{
						if ( (SQUnsignedInteger)_rawval(key) == pKey )
						{
							switch ( sq_type(var) )
							{
								case OT_TABLE: out.type = objref_t::TABLE; break;
								case OT_INSTANCE: out.type = objref_t::INSTANCE; break;
								case OT_CLASS:
								{
									if ( _class(var)->_members->Get( key, val ) )
									{
										out.type = (objref_t::EOBJREF)( objref_t::PTR | objref_t::CLASS );

										if ( _isfield(val) )
										{
											out.ptr = &_class(var)->_defaultvalues[ _member_idx(val) ].val;
										}
										else
										{
											out.ptr = &_class(var)->_methods[ _member_idx(val) ].val;
										}

										out.src = var;
										out.key = key;

										value = *out.ptr;
										return true;
									}

									return false;
								}
								default: UNREACHABLE();
							}

							out.src = var;
							out.key = key;

							value = val;
							return true;
						}
					}
					SQ_FOREACH_END()

					return false;
				}
				// float
				// NOTE: float keys are broken in pre 2.2.5 if sizeof(SQInteger) != sizeof(SQFloat),
				// this is fixed in SQ 2.2.5 with SQ_OBJECT_RAWINIT in SQObjectPtr::operator=(SQFloat)
				else
				{
					identifier = (SQFloat)strtod( expression.ptr, NULL );
				}
			}
		}
	}

	return GetObj_Var( identifier, var, out, value );
}

bool SQDebugServer::GetObj_Frame( const string_t &expression, HSQUIRRELVM vm, const SQVM::CallInfo *ci,
		objref_t &out, SQObjectPtr &value )
{
	Assert( expression.len );
	Assert( !ci || ( ci >= vm->_callsstack && ci < vm->_callsstack + vm->_callsstacksize ) );

	if ( ci && sq_type(ci->_closure) == OT_CLOSURE )
	{
		SQClosure *pClosure = _closure(ci->_closure);
		SQFunctionProto *func = _fp(pClosure->_function);

		SQUnsignedInteger ip = (SQUnsignedInteger)( ci->_ip - func->_instructions - 1 );

		for ( int i = 0; i < func->_nlocalvarinfos; i++ )
		{
			const SQLocalVarInfo &var = func->_localvarinfos[i];
			if ( var._start_op <= ip + 1 && var._end_op >= ip &&
					expression.IsEqualTo( _string(var._name) ) )
			{
				int stackbase = GetStackBase( vm, ci );
				out.type = objref_t::PTR;
				out.ptr = &vm->_stack._vals[ stackbase + var._pos ];
				value = *out.ptr;
				return true;
			}
		}

		for ( int i = 0; i < func->_noutervalues; i++ )
		{
			const SQOuterVar &var = func->_outervalues[i];
			if ( expression.IsEqualTo( _string(var._name) ) )
			{
				out.type = objref_t::PTR;
				out.ptr = _outervalptr( pClosure->_outervalues[i] );
				value = *out.ptr;
				return true;
			}
		}

		if ( expression.len == 6 || expression.len == 7 )
		{
			if ( expression.IsEqualTo( KW_THIS ) )
			{
				int stackbase = GetStackBase( vm, ci );
				out.type = objref_t::PTR;
				out.ptr = &vm->_stack._vals[ stackbase ];
				value = *out.ptr;
				return true;
			}
#if SQUIRREL_VERSION_NUMBER >= 300
			else if ( func->_varparams && func->_nlocalvarinfos >= 2 )
			{
				if ( expression.IsEqualTo( KW_VARGV ) )
				{
					const SQLocalVarInfo &var = func->_localvarinfos[ func->_nlocalvarinfos - 2 ];
					if ( sqstring_t(_SC("vargv")).IsEqualTo( _string(var._name) ) )
					{
						int stackbase = GetStackBase( vm, ci );
						out.type = objref_t::PTR;
						out.ptr = &vm->_stack._vals[ stackbase + 1 ];
						value = *out.ptr;
						return true;
					}
				}
				else if ( expression.IsEqualTo( KW_VARGC ) )
				{
					const SQLocalVarInfo &var = func->_localvarinfos[ func->_nlocalvarinfos - 2 ];
					if ( sqstring_t(_SC("vargv")).IsEqualTo( _string(var._name) ) )
					{
						int stackbase = GetStackBase( vm, ci );
						const SQObjectPtr &val = vm->_stack._vals[ stackbase + 1 ];

						if ( sq_type(val) == OT_ARRAY )
						{
							out.type = objref_t::INT;
							out.val = (int)_array(val)->Size();
							value = _array(val)->Size();
							return true;
						}

						return false;
					}
				}
			}
#else
			else if ( func->_varparams )
			{
				// Returning a temporary vargv array here would be pointless in general,
				// and extra work for completions
				if ( expression.IsEqualTo( KW_VARGC ) )
				{
					out.type = objref_t::INT;
					out.val = (int)ci->_vargs.size;
					value = (SQInteger)ci->_vargs.size;
					return true;
				}
			}
#endif
		}
	}
	else
	{
		if ( expression.IsEqualTo( "this" ) )
		{
			out.type = (objref_t::EOBJREF)( objref_t::PTR | objref_t::READONLY );
			out.ptr = &vm->_roottable;
			value = *out.ptr;
			return true;
		}
	}

#if SQUIRREL_VERSION_NUMBER >= 220
	SQTable *pConstTable = _table(_ss(vm)->_consts);
	if ( SQTable_Get( pConstTable, expression, value ) )
	{
		out.type = (objref_t::EOBJREF)( objref_t::TABLE | objref_t::READONLY );
		out.src = pConstTable;
		out.key = CreateSQString( m_pRootVM, expression );
		return true;
	}
#endif

	if ( ci )
	{
		const SQObjectPtr &env = vm->_stack._vals[ GetStackBase( vm, ci ) ];
		if ( sq_type(env) == OT_TABLE )
		{
			SQTable *t = _table(env);
			do
			{
				if ( SQTable_Get( t, expression, value ) )
				{
					out.type = objref_t::TABLE;
					out.src = t;
					out.key = CreateSQString( m_pRootVM, expression );
					return true;
				}
			}
			while ( ( t = t->_delegate ) != NULL );
		}
		else if ( sq_type(env) == OT_INSTANCE )
		{
			SQString *pExpression = CreateSQString( m_pRootVM, expression );
			if ( _instance(env)->Get( pExpression, value ) )
			{
				out.type = objref_t::INSTANCE;
				out.src = env;
				out.key = pExpression;
				return true;
			}
		}
	}

	SQTable *root = _table(vm->_roottable);

#ifdef CLOSURE_ROOT
	if ( ci && sq_type(ci->_closure) == OT_CLOSURE &&
			_closure(ci->_closure)->_root &&
			_table(_closure(ci->_closure)->_root->_obj) != root )
	{
		Assert( sq_type(_closure(ci->_closure)->_root->_obj) == OT_TABLE );
		root = _table(_closure(ci->_closure)->_root->_obj);
	}
#endif

	do
	{
		if ( SQTable_Get( root, expression, value ) )
		{
			out.type = objref_t::TABLE;
			out.src = root;
			out.key = CreateSQString( m_pRootVM, expression );
			return true;
		}
	}
	while ( ( root = root->_delegate ) != NULL );

	return false;
}

bool SQDebugServer::GetObj_VarRef( string_t &expression, const varref_t *ref, objref_t &out, SQObjectPtr &value )
{
	switch ( ref->type )
	{
		case VARREF_OBJ:
		{
			return GetObj_Var( expression, !ref->obj.hasNonStringMembers, ref->GetVar(), out, value );
		}
		case VARREF_SCOPE_LOCALS:
		case VARREF_SCOPE_OUTERS:
		{
			return GetObj_Frame( expression, ref->GetThread(), ref->scope.frame, out, value );
		}
		case VARREF_OUTERS:
		{
			Assert( sq_type(ref->GetVar()) == OT_CLOSURE );

			if ( sq_type(ref->GetVar()) == OT_CLOSURE )
			{
				SQClosure *pClosure = _closure(ref->GetVar());
				SQFunctionProto *func = _fp(pClosure->_function);

				for ( int i = 0; i < func->_noutervalues; i++ )
				{
					const SQOuterVar &var = func->_outervalues[i];
					if ( expression.IsEqualTo( _string(var._name) ) )
					{
						out.type = objref_t::PTR;
						out.ptr = _outervalptr( pClosure->_outervalues[i] );
						value = *out.ptr;
						return true;
					}
				}
			}

			return false;
		}
		case VARREF_LITERALS:
		{
			Assert( sq_type(ref->GetVar()) == OT_CLOSURE );

			if ( sq_type(ref->GetVar()) == OT_CLOSURE )
			{
				int idx;
				if ( atoi( expression, &idx ) &&
						idx >= 0 && idx < (int)_fp(_closure(ref->GetVar())->_function)->_nliterals )
				{
					out.type = objref_t::PTR;
					out.ptr = &_fp(_closure(ref->GetVar())->_function)->_literals[idx];
					value = *out.ptr;
					return true;
				}
			}

			return false;
		}
		case VARREF_METAMETHODS:
		{
			int mm = -1;

			for ( int i = 0; i < MT_LAST; i++ )
			{
				if ( expression.IsEqualTo( g_MetaMethodName[i] ) )
				{
					mm = i;
					break;
				}
			}

			Assert( mm != -1 );

			if ( mm != -1 )
			{
				switch ( sq_type(ref->GetVar()) )
				{
					case OT_CLASS:
					{
						out.type = objref_t::PTR;
						out.ptr = &_class(ref->GetVar())->_metamethods[mm];
						value = *out.ptr;
						return true;
					}
					case OT_TABLE:
					{
						// metamethods are regular members of tables
						Assert( _table(ref->GetVar())->_delegate );

						out.type = objref_t::TABLE;
						out.src = _table(ref->GetVar())->_delegate;

						SQObjectPtr key = CreateSQString( m_pRootVM, expression );
						out.key = key;

						return _table(out.src)->Get( key, value );
					}
					default: Assert(0);
				}
			}

			return false;
		}
		case VARREF_STACK:
		{
			Assert( expression.len > 2 );

			if ( expression.len <= 2 )
				return false;

			while ( expression.len > 3 && expression.ptr[expression.len-1] != ']' )
				expression.len--;

			expression.ptr++;
			expression.len -= 2;

			int idx;
			if ( atoi( expression, &idx ) &&
					idx >= 0 && idx < (int)ref->GetThread()->_stack.size() )
			{
				out.type = objref_t::PTR;
				out.ptr = &ref->GetThread()->_stack._vals[idx];
				value = *out.ptr;
				return true;
			}

			return false;
		}
		case VARREF_INSTRUCTIONS:
		case VARREF_CALLSTACK:
		{
			return false;
		}
		default:
		{
			PrintError(_SC("(sqdbg) Invalid varref requested (%d)\n"), ref->type);
			AssertMsg1( 0, "Invalid varref requested (%d)", ref->type );
			return false;
		}
	}
}

static inline conststring_t GetPresentationHintKind( const SQObjectPtr &obj )
{
	switch ( sq_type(obj) )
	{
		case OT_CLOSURE:
		case OT_NATIVECLOSURE:
			return "method";
		case OT_CLASS:
			return "class";
		default:
			return "property";
	}
}

bool SQDebugServer::ShouldPageArray( const SQObject &obj, unsigned int limit )
{
	return ( sq_type(obj) == OT_ARRAY && _array(obj)->_values.size() > limit );
}

bool SQDebugServer::ShouldParseEvaluateName( const string_t &expression )
{
	return ( expression.len >= 4 && expression.ptr[0] == '@' && expression.ptr[2] == '@' );
}

bool SQDebugServer::ParseEvaluateName( const string_t &expression, HSQUIRRELVM vm, int frame,
		objref_t &out, SQObjectPtr &value )
{
	Assert( ShouldParseEvaluateName( expression ) );

	if ( expression.ptr[1] == 'L' )
	{
		int idx;
		if ( !atoi( { expression.ptr + 3, expression.len - 3 }, &idx ) )
			return false;

		if ( !IsValidStackFrame( vm, frame ) )
			return false;

		const SQVM::CallInfo &ci = vm->_callsstack[ frame ];

		if ( sq_type(ci._closure) != OT_CLOSURE )
			return false;

		SQClosure *pClosure = _closure(ci._closure);
		SQFunctionProto *func = _fp(pClosure->_function);
		SQUnsignedInteger ip = (SQUnsignedInteger)( ci._ip - func->_instructions - 1 );

		idx = func->_nlocalvarinfos - idx - 1;

		if ( idx < 0 || idx >= func->_nlocalvarinfos )
			return false;

		const SQLocalVarInfo &var = func->_localvarinfos[idx];
		if ( var._start_op <= ip + 1 && var._end_op >= ip )
		{
			int stackbase = GetStackBase( vm, &ci );
			out.type = objref_t::PTR;
			out.ptr = &vm->_stack._vals[ stackbase + var._pos ];
			value = *out.ptr;
			return true;
		}
	}

	return false;
}

bool SQDebugServer::ParseBinaryNumber( const string_t &value, SQObject &out )
{
#if defined(_SQ64) || defined(SQUSEDOUBLE)
	const int maxbitlen = 64;
#else
	const int maxbitlen = 32;
#endif

	// expect 0b prefix
	if ( value.len <= 2 || value.len > maxbitlen + 2 || value.ptr[0] != '0' || value.ptr[1] != 'b' )
		return false;

	out._type = OT_INTEGER;
	out._unVal.nInteger = 0;

	int inputbitlen = value.len - 2;
	Assert( inputbitlen > 0 && inputbitlen <= maxbitlen );

	for ( int i = value.len - 1; i >= 2; i-- )
	{
		switch ( value.ptr[i] )
		{
			case '1':
				out._unVal.nInteger |= ( (SQUnsignedInteger)1 << ( ( inputbitlen - 1 ) - ( i - 2 ) ) );
			case '0':
				continue;
			default:
				return false;
		}
	}

	return true;
}

int SQDebugServer::ParseFormatSpecifiers( string_t &expression, char **ppComma )
{
	if ( expression.len <= 2 )
		return 0;

	int flags = 0;

	// 4 flags at most ",*x0b\0"
	char *start = expression.ptr + expression.len - 6;
	char *end = expression.ptr + expression.len;
	char *c = end - 1;
	char *comma = NULL;

	if ( start < expression.ptr )
		start = expression.ptr;

	for ( ; c > start; c-- )
	{
		if ( *c == ',' )
		{
			comma = c;
			c++;
			break;
		}
	}

	// have flags
	if ( comma )
	{
		// has to be the first flag
		if ( *c == '*' )
		{
			flags |= kFS_Lock;
			c++;
		}

		if ( c < end )
		{
			switch ( *c++ )
			{
				case 'x': flags |= kFS_Hexadecimal; break;
				case 'X': flags |= kFS_Hexadecimal | kFS_Uppercase; break;
				case 'b': flags |= kFS_Binary; break;
				case 'd': flags |= kFS_Decimal; break;
				case 'o': flags |= kFS_Octal; break;
				case 'c': flags |= kFS_Character; break;
				case 'f': flags |= kFS_Float; break;
				case 'e': flags |= kFS_FloatE; break;
				case 'g': flags |= kFS_FloatG; break;
				case 'n':
					if ( c < end && *c++ == 'a' )
					{
						flags |= kFS_NoAddr;
						break;
					}
				default: return 0; // Invalid flag
			}

			// modifier
			if ( ( flags & ( kFS_Hexadecimal | kFS_Binary ) ) && c < end )
			{
				switch ( *c++ )
				{
					case '0': flags |= kFS_Padding; break;
					case 'b': flags |= kFS_NoPrefix; break;
					default: return 0;
				}

				if ( ( flags & kFS_Padding ) && c < end )
				{
					switch ( *c++ )
					{
						case 'b': flags |= kFS_NoPrefix; break;
						default: return 0;
					}
				}
			}

			// there should be no more flags
			if ( c < end )
				return 0;
		}

		if ( flags )
		{
			expression.len = comma - expression.ptr;

			// Terminate here, this expression might be passed to SQTable::Get through GetObj,
			// which compares strings disregarding length
			*comma = 0;

			if ( ppComma )
				*ppComma = comma;
		}
	}

	return flags;
}

void SQDebugServer::OnRequest_Evaluate( const json_table_t &arguments, int seq )
{
	HSQUIRRELVM vm;
	int frame;
	string_t context, expression;

	arguments.GetString( "context", &context );
	arguments.GetString( "expression", &expression );
	arguments.GetInt( "frameId", &frame, -1 );

	if ( expression.IsEmpty() )
	{
		DAP_ERROR_RESPONSE( seq, "evaluate" );
		DAP_ERROR_BODY( 0, "empty expression", 0 );
		DAP_SEND();
		return;
	}

	if ( !TranslateFrameID( frame, &vm, &frame ) )
	{
		vm = m_pCurVM;
		frame = -1;
	}

	int flags = 0;
	{
		json_table_t *format;
		if ( arguments.GetTable( "format", &format ) )
		{
			bool hex;
			format->GetBool( "hex", &hex );
			if ( hex )
				flags |= kFS_Hexadecimal;
		}
	}

	if ( IS_INTERNAL_TAG( expression.ptr ) )
	{
		if ( expression.IsEqualTo( INTERNAL_TAG("stack") ) )
		{
			stringbuf_t< 16 > res;
			res.Puts("[");
			res.PutInt( vm->_stack.size() );
			res.Put(']');

			DAP_START_RESPONSE( seq, "evaluate" );
			DAP_SET_TABLE( body, 3 );
				body.SetStringNoCopy( "result", res );
				body.SetInt( "variablesReference", ToVarRef( VARREF_STACK, vm, frame ) );
				json_table_t &hint = body.SetTable( "presentationHint", 1 );
				json_array_t &attributes = hint.SetArray( "attributes", 1 );
				attributes.Append( "readOnly" );
			DAP_SEND();

			return;
		}
		else if ( expression.IsEqualTo( INTERNAL_TAG("function") ) )
		{
			if ( IsValidStackFrame( vm, frame ) )
			{
				const SQObjectPtr &res = vm->_callsstack[ frame ]._closure;

				DAP_START_RESPONSE( seq, "evaluate" );
				DAP_SET_TABLE( body, 4 );
					body.SetStringNoCopy( "result", GetValue( res, flags ) );
					body.SetString( "type", GetType( res ) );
					body.SetInt( "variablesReference", ToVarRef( res, true ) );
					json_table_t &hint = body.SetTable( "presentationHint", 1 );
					json_array_t &attributes = hint.SetArray( "attributes", 1 );
					attributes.Append( "readOnly" );
				DAP_SEND();
			}
			else
			{
				DAP_ERROR_RESPONSE( seq, "evaluate" );
				DAP_ERROR_BODY( 0, "could not evaluate expression", 0 );
				DAP_SEND();
			}

			return;
		}
		else if ( expression.IsEqualTo( INTERNAL_TAG("caller") ) )
		{
			if ( IsValidStackFrame( vm, frame - 1 ) )
			{
				const SQObjectPtr &res = vm->_callsstack[ frame - 1 ]._closure;

				DAP_START_RESPONSE( seq, "evaluate" );
				DAP_SET_TABLE( body, 4 );
					body.SetStringNoCopy( "result", GetValue( res, flags ) );
					body.SetString( "type", GetType( res ) );
					body.SetInt( "variablesReference", ToVarRef( res, true ) );
					json_table_t &hint = body.SetTable( "presentationHint", 1 );
					json_array_t &attributes = hint.SetArray( "attributes", 1 );
					attributes.Append( "readOnly" );
				DAP_SEND();
			}
			else
			{
				DAP_ERROR_RESPONSE( seq, "evaluate" );
				DAP_ERROR_BODY( 0, "could not evaluate expression", 0 );
				DAP_SEND();
			}

			return;
		}
	}

	if ( context.IsEqualTo( "repl" ) )
	{
		// Don't hit breakpoints unless it's repl
		m_bInREPL = true;

		// Don't print quotes in repl
		flags |= kFS_NoQuote;
	}
	else
	{
		m_bDebugHookGuardAlways = true;
	}

	SQObjectPtr res;

	if ( context.IsEqualTo( "watch" ) || context.IsEqualTo( "clipboard" ) || context.IsEqualTo( "hover" ) )
	{
		flags |= ParseFormatSpecifiers( expression );

		objref_t obj;

		if ( ShouldParseEvaluateName( expression ) )
		{
			if ( ParseEvaluateName( expression, vm, frame, obj, res ) )
			{
				DAP_START_RESPONSE( seq, "evaluate" );
				DAP_SET_TABLE( body, 5 );
					JSONSetString( body, "result", res, flags );
					body.SetString( "type", GetType( res ) );
					body.SetInt( "variablesReference", ToVarRef( res, true ) );
					json_table_t &hint = body.SetTable( "presentationHint", 1 );
					hint.SetString( "kind", GetPresentationHintKind( res ) );

					if ( ShouldPageArray( res, 1024 ) )
					{
						body.SetInt( "indexedVariables", (int)_array(res)->_values.size() );
					}
				DAP_SEND();
			}
			else
			{
				DAP_ERROR_RESPONSE( seq, "evaluate" );
				DAP_ERROR_BODY( 0, "could not evaluate expression", 0 );
				DAP_SEND();
			}

			return;
		}

		if ( flags & kFS_Lock )
		{
			bool foundWatch = false;

			for ( int i = 0; i < m_LockedWatches.size(); i++ )
			{
				const watch_t &w = m_LockedWatches[i];
				if ( w.expression.IsEqualTo( expression ) )
				{
					vm = GetThread( w.thread );
					frame = w.frame;
					foundWatch = true;
					break;
				}
			}

			if ( !foundWatch )
			{
				watch_t &w = m_LockedWatches.append();
				CopyString( expression, &w.expression );
				w.thread = GetWeakRef( vm );
				w.frame = frame;
			}
		}

#ifndef SQDBG_DISABLE_COMPILER
		ECompileReturnCode cres = Evaluate( expression, vm, frame, res );
		if ( cres == CompileReturnCode_Success )
		{
			DAP_START_RESPONSE( seq, "evaluate" );
			DAP_SET_TABLE( body, 5 );
				JSONSetString( body, "result", res, flags );
				body.SetString( "type", GetType( res ) );
				body.SetInt( "variablesReference", ToVarRef( res, true ) );
				json_table_t &hint = body.SetTable( "presentationHint", 1 );
				hint.SetString( "kind", GetPresentationHintKind( res ) );

				if ( ShouldPageArray( res, 1024 ) )
				{
					body.SetInt( "indexedVariables", (int)_array(res)->_values.size() );
				}
			DAP_SEND();
		}
#else
		if ( GetObj_Frame( expression, vm, frame, obj, res ) )
		{
			DAP_START_RESPONSE( seq, "evaluate" );
			DAP_SET_TABLE( body, 5 );
				JSONSetString( body, "result", res, flags );
				body.SetString( "type", GetType( res ) );
				body.SetInt( "variablesReference", ToVarRef( res, true ) );
				json_table_t &hint = body.SetTable( "presentationHint", 1 );
				hint.SetString( "kind", GetPresentationHintKind( res ) );

				if ( ShouldPageArray( res, 1024 ) )
				{
					body.SetInt( "indexedVariables", (int)_array(res)->_values.size() );
				}
			DAP_SEND();
		}
#endif
#ifndef SQDBG_DISABLE_COMPILER
		else if ( cres > CompileReturnCode_Fallback &&
				RunExpression( expression, vm, frame, res ) )
#else
		else if ( RunExpression( expression, vm, frame, res ) ||
				ParseBinaryNumber( expression, res ) )
#endif
		{
			DAP_START_RESPONSE( seq, "evaluate" );
			DAP_SET_TABLE( body, 5 );
				JSONSetString( body, "result", res, flags );
				body.SetString( "type", GetType( res ) );
				body.SetInt( "variablesReference", ToVarRef( res, true ) );
				json_table_t &hint = body.SetTable( "presentationHint", 2 );
				hint.SetString( "kind", GetPresentationHintKind( res ) );
				json_array_t &attributes = hint.SetArray( "attributes", 1 );
				attributes.Append( "readOnly" );

				if ( ShouldPageArray( res, 1024 ) )
				{
					body.SetInt( "indexedVariables", (int)_array(res)->_values.size() );
				}
			DAP_SEND();
		}
		else
		{
			if ( flags & kFS_Lock )
			{
				for ( int i = 0; i < m_LockedWatches.size(); i++ )
				{
					watch_t &w = m_LockedWatches[i];
					if ( w.expression.IsEqualTo( expression ) )
					{
						FreeString( &w.expression );
						m_LockedWatches.remove( i );
						break;
					}
				}
			}

			DAP_ERROR_RESPONSE( seq, "evaluate" );
			DAP_ERROR_BODY( 0, "could not evaluate expression", 0 );
			DAP_SEND();
		}
	}
	else
	{
		Assert( context.IsEqualTo( "repl" ) || context.IsEqualTo( "variables" ) );

		if ( RunExpression( expression, vm, frame, res, expression.Contains('\n') ) ||
				ParseBinaryNumber( expression, res ) )
		{
			DAP_START_RESPONSE( seq, "evaluate" );
			DAP_SET_TABLE( body, 3 );
				JSONSetString( body, "result", res, flags );
				body.SetInt( "variablesReference", ToVarRef( res, context.IsEqualTo( "repl" ) ) );

				if ( ShouldPageArray( res, 1024 ) )
				{
					body.SetInt( "indexedVariables", (int)_array(res)->_values.size() );
				}
			DAP_SEND();
		}
		else
		{
			DAP_ERROR_RESPONSE( seq, "evaluate" );
			DAP_ERROR_BODY( 0, "{reason}", 1 );
				json_table_t &variables = error.SetTable( "variables", 1 );
				variables.SetStringNoCopy( "reason", GetValue( vm->_lasterror, kFS_NoQuote ) );
			DAP_SEND();
		}
	}

	if ( context.IsEqualTo( "repl" ) )
	{
		m_bInREPL = false;
	}
	else
	{
		m_bDebugHookGuardAlways = false;
	}
}

#ifndef SQDBG_DISABLE_COMPILER
// A very simple and incomplete completions implementation
// using the extra information from the sqdbg compiler.
// Easily breaks within parantheses and brackets, works best with simple expressions
void SQDebugServer::OnRequest_Completions( const json_table_t &arguments, int seq )
{
	HSQUIRRELVM vm;
	int frame;
	string_t text;
	int column;

	arguments.GetString( "text", &text );
	arguments.GetInt( "frameId", &frame, -1 );
	arguments.GetInt( "column", &column );

	column -= m_nClientColumnOffset;

	if ( column < 0 || column > text.len )
	{
		DAP_ERROR_RESPONSE( seq, "completions" );
		DAP_ERROR_BODY( 0, "invalid column", 0 );
		DAP_SEND();
		return;
	}

	if ( !TranslateFrameID( frame, &vm, &frame ) )
	{
		vm = m_pCurVM;
		frame = -1;
	}

	SQObjectPtr target;

	string_t expr( text.ptr, column );
	CCompiler c( expr );
	ECompileReturnCode r = c.Evaluate( this,
			vm,
			IsValidStackFrame( vm, frame ) ? &vm->_callsstack[frame] : NULL,
			target );

	CCompiler::token_t token = c.m_lastToken;

	if ( ( r != CompileReturnCode_Success &&
				r != CompileReturnCode_DoesNotExist &&
				r != CompileReturnCode_NoValue ) &&
			token.type != 0 &&
			token.type != CCompiler::Token_End &&
			token.type != '.' &&
			token.type != CCompiler::Token_Identifier )
	{
		DAP_ERROR_RESPONSE( seq, "completions" );
		DAP_ERROR_BODY( 0, "", 0 );
		DAP_SEND();
		return;
	}

	if ( token.type != '.' && token.type != CCompiler::Token_Identifier )
	{
		token.type = 0;
		target.Null();
	}

	int start = column;

	if ( token.type == CCompiler::Token_Identifier )
		start = token._string.ptr - text.ptr;

	int length = text.len - start;

	json_array_t targets( 32 );
	DAP_START_RESPONSE( seq, "completions" );
	DAP_SET_TABLE( body, 1 );
		body.SetArray( "targets", targets );
		FillCompletions( target,
				vm,
				IsValidStackFrame( vm, frame ) ? &vm->_callsstack[frame] : NULL,
				token.type,
				token._string,
				start == column ? -1 : start,
				length,
				targets );
	DAP_SEND();
}

void SQDebugServer::FillCompletions( const SQObjectPtr &target, HSQUIRRELVM vm, const SQVM::CallInfo *ci,
		int token, const string_t &partial, int start, int length, json_array_t &targets )
{
#define _check( key ) \
	( token == '.' || token == 0 || \
	  ( token == CCompiler::Token_Identifier && \
		sqstring_t( (key) ).StartsWith( partial ) ) )

#define _set( priority, label, val ) \
	json_table_t &elem = targets.AppendTable(5); \
	elem.SetString( "label", label ); \
	sortbuf.len = 0; \
	sortbuf.PutInt( priority ); \
	sortbuf.Puts( label ); \
	elem.SetString( "sortText", sortbuf ); \
	switch ( sq_type(val) ) \
	{ \
		case OT_CLOSURE: \
		case OT_NATIVECLOSURE: \
			elem.SetString( "type", "function" ); \
			elem.SetString( "detail", "function" ); \
			break; \
		case OT_CLASS: \
		{ \
			elem.SetString( "type", "class" ); \
			const classdef_t *def = FindClassDef( _class(val) ); \
			elem.SetString( "detail", def && def->name.ptr ? \
					string_t( def->name.ptr + FMT_PTR_LEN + 1, def->name.len - FMT_PTR_LEN - 1 ) : \
					string_t( "class" ) ); \
			break; \
		} \
		case OT_INSTANCE: \
			elem.SetString( "type", "field" ); \
			elem.SetString( "detail", "instance" ); \
			break; \
		default: \
			elem.SetString( "type", "variable" ); \
			elem.SetString( "detail", GetType( val ) ); \
	} \
	if ( start != -1 ) \
		elem.SetInt( "start", start ); \
	if ( length ) \
		elem.SetInt( "length", length );

	stringbuf_t< 64 > sortbuf;

	switch ( sq_type(target) )
	{
		case OT_TABLE:
		{
			SQTable *t = _table(target);
			do
			{
				SQObjectPtr key, val;
				FOREACH_SQTABLE( t, key, val )
				{
					if ( sq_type(key) == OT_STRING && _check( _string(key) ) )
					{
						_set( 0, sqstring_t( _string(key) ), val );
					}
				}
			}
			while ( ( t = t->_delegate ) != NULL );

			break;
		}
		case OT_INSTANCE:
		{
			SQClass *base = _instance(target)->_class;
			Assert( base );

			// metamembers
			SQObjectPtr mm;
			const classdef_t *def = FindClassDef( base );

			if ( def &&
					sq_type(def->metamembers) != OT_NULL &&
					_instance(target)->GetMetaMethod( vm, MT_GET, mm ) )
			{
				for ( unsigned int i = 0; i < _array(def->metamembers)->_values.size(); i++ )
				{
					SQObjectPtr val;
					const SQObjectPtr &key = _array(def->metamembers)->_values[i];

					if ( sq_type(key) == OT_STRING && _check( _string(key) ) )
					{
						RunClosure( mm, &target, key, val );
						_set( 0, sqstring_t( _string(key) ), val );
					}
				}
			}

			SQObjectPtr key, val;

			// values
			{
				FOREACH_SQTABLE( base->_members, key, val )
				{
					if ( _isfield(val) && sq_type(key) == OT_STRING && _check( _string(key) ) )
					{
						_instance(target)->Get( key, val );
						_set( 0, sqstring_t( _string(key) ), val );
					}
				}
			}

			// methods
			{
				FOREACH_SQTABLE( base->_members, key, val )
				{
					if ( !_isfield(val) && sq_type(key) == OT_STRING && _check( _string(key) ) )
					{
						_instance(target)->Get( key, val );
						_set( 1, sqstring_t( _string(key) ), val );
					}
				}
			}

			break;
		}
		case OT_CLASS:
		{
			SQObjectPtr key, val;

			// values
			{
				FOREACH_SQTABLE( _class(target)->_members, key, val )
				{
					if ( _isfield(val) && sq_type(key) == OT_STRING && _check( _string(key) ) )
					{
						_class(target)->Get( key, val );
						_set( 0, sqstring_t( _string(key) ), val );
					}
				}
			}

			// methods
			{
				FOREACH_SQTABLE( _class(target)->_members, key, val )
				{
					if ( !_isfield(val) && sq_type(key) == OT_STRING && _check( _string(key) ) )
					{
						_class(target)->Get( key, val );
						_set( 1, sqstring_t( _string(key) ), val );
					}
				}
			}

			break;
		}
		default: break;
	}

	SQTable *del = GetDefaultDelegate( vm, sq_type(target) );
	if ( del )
	{
		SQObjectPtr key, val;
		FOREACH_SQTABLE( del, key, val )
		{
			if ( sq_type(key) == OT_STRING && _check( _string(key) ) )
			{
				_set( 2, sqstring_t( _string(key) ), val );
			}
		}
	}

	SQTable *pEnvTable = NULL;

	if ( sq_type(target) == OT_NULL )
	{
		// locals
		if ( ci && sq_type(ci->_closure) == OT_CLOSURE )
		{
			SQClosure *pClosure = _closure(ci->_closure);
			SQFunctionProto *func = _fp(pClosure->_function);

			SQUnsignedInteger ip = (SQUnsignedInteger)( ci->_ip - func->_instructions - 1 );

			for ( int i = 0; i < func->_nlocalvarinfos; i++ )
			{
				const SQLocalVarInfo &var = func->_localvarinfos[i];
				if ( var._start_op <= ip + 1 && var._end_op >= ip &&
						_check( _string(var._name) ) )
				{
					_set( 0, sqstring_t( _string(var._name) ), vm->_stack._vals[ GetStackBase( vm, ci ) + var._pos ] );

					if ( sqstring_t( _string(var._name) ).IsEqualTo(_SC("this")) )
					{
						elem.SetString( "text", KW_THIS );
					}
#if SQUIRREL_VERSION_NUMBER >= 300
					else if ( sqstring_t( _string(var._name) ).IsEqualTo(_SC("vargv")) )
					{
						elem.SetString( "text", KW_VARGV );
					}
#endif
				}
			}

			for ( int i = 0; i < func->_noutervalues; i++ )
			{
				const SQOuterVar &var = func->_outervalues[i];
				if ( _check( _string(var._name) ) )
				{
					_set( 0, sqstring_t( _string(var._name) ), *_outervalptr( pClosure->_outervalues[i] ) );
				}
			}

#if SQUIRREL_VERSION_NUMBER < 300
			if ( func->_varparams )
			{
				if ( token == CCompiler::Token_Identifier && string_t("vargv").StartsWith( partial ) )
				{
					_set( 0, "vargv", SQObjectPtr() );
					elem.SetString( "text", KW_VARGV );
				}

				if ( token == CCompiler::Token_Identifier && string_t("vargc").StartsWith( partial ) )
				{
					_set( 0, "vargc", SQObjectPtr() );
					elem.SetString( "text", KW_VARGC );
				}
			}
#endif
		}

		// env
		if ( ci )
		{
			const SQObjectPtr &env = vm->_stack._vals[ GetStackBase( vm, ci ) ];
			if ( sq_type(env) == OT_TABLE )
			{
				pEnvTable = _table(env);
				SQTable *t = _table(env);
				do
				{
					SQObjectPtr key, val;
					FOREACH_SQTABLE( t, key, val )
					{
						if ( sq_type(key) == OT_STRING && _check( _string(key) ) )
						{
							_set( 1, sqstring_t( _string(key) ), val );
						}
					}
				}
				while ( ( t = t->_delegate ) != NULL );
			}
			else if ( sq_type(env) == OT_INSTANCE )
			{
				SQClass *base = _instance(env)->_class;
				Assert( base );

				// metamembers
				SQObjectPtr mm;
				const classdef_t *def = FindClassDef( base );

				if ( def &&
						sq_type(def->metamembers) != OT_NULL &&
						_instance(target)->GetMetaMethod( vm, MT_GET, mm ) )
				{
					for ( unsigned int i = 0; i < _array(def->metamembers)->_values.size(); i++ )
					{
						SQObjectPtr val;
						const SQObjectPtr &key = _array(def->metamembers)->_values[i];

						if ( sq_type(key) == OT_STRING && _check( _string(key) ) )
						{
							RunClosure( mm, &env, key, val );
							_set( 1, sqstring_t( _string(key) ), val );
						}
					}
				}

				SQObjectPtr key, val;

				// values
				{
					FOREACH_SQTABLE( base->_members, key, val )
					{
						if ( _isfield(val) && sq_type(key) == OT_STRING && _check( _string(key) ) )
						{
							_instance(env)->Get( key, val );
							_set( 1, sqstring_t( _string(key) ), val );
						}
					}
				}

				// methods
				{
					FOREACH_SQTABLE( base->_members, key, val )
					{
						if ( !_isfield(val) && sq_type(key) == OT_STRING && _check( _string(key) ) )
						{
							_instance(env)->Get( key, val );
							_set( 2, sqstring_t( _string(key) ), val );
						}
					}
				}
			}
		}

		SQTable *root = _table(vm->_roottable);

#ifdef CLOSURE_ROOT
		if ( ci && sq_type(ci->_closure) == OT_CLOSURE &&
				_closure(ci->_closure)->_root &&
				_table(_closure(ci->_closure)->_root->_obj) != root )
		{
			Assert( sq_type(_closure(ci->_closure)->_root->_obj) == OT_TABLE );
			root = _table(_closure(ci->_closure)->_root->_obj);
		}
#endif

		if ( root != pEnvTable )
		{
			do
			{
				SQObjectPtr key, val;
				FOREACH_SQTABLE( root, key, val )
				{
					if ( sq_type(key) == OT_STRING && _check( _string(key) ) )
					{
						_set( 3, sqstring_t( _string(key) ), val );
					}
				}
			}
			while ( ( root = root->_delegate ) != NULL );
		}
	}

#undef _set
#undef _check
}
#endif

void SQDebugServer::OnRequest_Scopes( const json_table_t &arguments, int seq )
{
	HSQUIRRELVM vm;
	int frame;
	arguments.GetInt( "frameId", &frame, -1 );

	if ( !TranslateFrameID( frame, &vm, &frame ) )
	{
		DAP_ERROR_RESPONSE( seq, "scopes" );
		DAP_ERROR_BODY( 0, "invalid stack frame {id}", 1 );
			json_table_t &variables = error.SetTable( "variables", 1 );
			variables.SetIntString( "id", frame );
		DAP_SEND();
		return;
	}

	const SQVM::CallInfo &ci = vm->_callsstack[ frame ];
	if ( sq_type(ci._closure) != OT_CLOSURE )
	{
		DAP_ERROR_RESPONSE( seq, "scopes" );
		DAP_ERROR_BODY( 0, "native call frame", 0 );
		DAP_SEND();
		return;
	}

	SQClosure *pClosure = _closure(ci._closure);
	SQFunctionProto *func = _fp(pClosure->_function);

	json_table_t locals(4), outers(4);
	json_array_t scopes(2);

	DAP_START_RESPONSE( seq, "scopes" );
	DAP_SET_TABLE( body, 1 );
		body.SetArray( "scopes", scopes );
		{
			scopes.Append( locals );
			locals.SetString( "name", "Locals" );
			locals.SetString( "presentationHint", "locals" );
			locals.SetBool( "expensive", false );
			locals.SetInt( "variablesReference", ToVarRef( VARREF_SCOPE_LOCALS, vm, frame ) );
		}
		if ( func->_noutervalues )
		{
			scopes.Append( outers );
			outers.SetString( "name", "Outers" );
			outers.SetString( "presentationHint", "locals" );
			outers.SetBool( "expensive", false );
			outers.SetInt( "variablesReference", ToVarRef( VARREF_SCOPE_OUTERS, vm, frame ) );
		}
	DAP_SEND();
}

int SQDebugServer::ThreadToID( HSQUIRRELVM vm )
{
	for ( int i = m_Threads.size(); i--; )
	{
		SQWeakRef *wr = m_Threads[i];

		if ( wr && sq_type(wr->_obj) == OT_THREAD )
		{
			if ( _thread(wr->_obj) == vm )
				return i;
		}
		else
		{
			__ObjRelease( wr );
			m_Threads.remove(i);
		}
	}

	SQWeakRef *wr = GetWeakRef( vm );
	__ObjAddRef( wr );

	int i = m_Threads.size();
	m_Threads.append( wr );
	return i;
}

HSQUIRRELVM SQDebugServer::ThreadFromID( int id )
{
	if ( id >= 0 && id < m_Threads.size() )
	{
		SQWeakRef *wr = m_Threads[id];

		if ( wr && sq_type(wr->_obj) == OT_THREAD )
			return _thread(wr->_obj);

		__ObjRelease( wr );
		m_Threads.remove(id);
	}

	return NULL;
}

void SQDebugServer::RemoveThreads()
{
	for ( int i = m_Threads.size(); i--; )
	{
		SQWeakRef *wr = m_Threads[i];
		__ObjRelease( wr );
	}

	m_Threads.purge();
}

void SQDebugServer::OnRequest_Threads( int seq )
{
	json_array_t threads( m_Threads.size() );

	DAP_START_RESPONSE( seq, "threads" );
	DAP_SET_TABLE( body, 1 );
		body.SetArray( "threads", threads );
		for ( int i = 0; i < m_Threads.size(); i++ )
		{
			SQWeakRef *wr = m_Threads[i];

			if ( wr && sq_type(wr->_obj) == OT_THREAD )
			{
				json_table_t &thread = threads.AppendTable(2);
				thread.SetInt( "id", i );

				if ( _thread(wr->_obj) == m_pRootVM )
				{
					thread.SetString( "name", "MainThread" );
				}
				else
				{
					stringbuf_t< STRLEN("Thread ") + FMT_PTR_LEN > name;
					name.Puts("Thread ");
					name.PutHex( (SQUnsignedInteger)_thread(wr->_obj) );
					thread.SetString( "name", name );
				}
			}
			else
			{
				__ObjRelease( wr );
				m_Threads.remove(i);
				i--;
			}
		}
	DAP_SEND();
}

bool SQDebugServer::ShouldIgnoreStackFrame( const SQVM::CallInfo &ci )
{
	// Ignore RunScript (first frame)
	if ( sq_type(ci._closure) == OT_CLOSURE &&
			sq_type(_fp(_closure(ci._closure)->_function)->_sourcename) == OT_STRING &&
			sqstring_t(_SC("sqdbg")).IsEqualTo( _string(_fp(_closure(ci._closure)->_function)->_sourcename) ) )
		return true;

	// Ignore error handler / debug hook (last frame)
	if ( sq_type(ci._closure) == OT_NATIVECLOSURE && (
				_nativeclosure(ci._closure)->_function == &SQDebugServer::SQErrorHandler
#ifndef NATIVE_DEBUG_HOOK
			|| _nativeclosure(ci._closure)->_function == &SQDebugServer::SQDebugHook
#endif
			) )
		return true;

	return false;
}

int SQDebugServer::ConvertToFrameID( int threadId, int stackFrame )
{
	for ( int i = 0; i < m_FrameIDs.size(); i++ )
	{
		frameid_t &v = m_FrameIDs[i];
		if ( v.threadId == threadId && v.frame == stackFrame )
			return i;
	}

	int i = m_FrameIDs.size();

	frameid_t &v = m_FrameIDs.append();
	v.threadId = threadId;
	v.frame = stackFrame;

	return i;
}

bool SQDebugServer::TranslateFrameID( int frameId, HSQUIRRELVM *thread, int *stackFrame )
{
	if ( frameId >= 0 && frameId < m_FrameIDs.size() )
	{
		frameid_t &v = m_FrameIDs[frameId];
		*thread = ThreadFromID( v.threadId );
		*stackFrame = v.frame;

		return thread && *thread && IsValidStackFrame( *thread, *stackFrame );
	}

	return false;
}

void SQDebugServer::OnRequest_StackTrace( const json_table_t &arguments, int seq )
{
	int threadId, startFrame, levels;
	json_table_t *format;
	bool parameters = true;

	arguments.GetInt( "threadId", &threadId, -1 );
	arguments.GetInt( "startFrame", &startFrame );
	arguments.GetInt( "levels", &levels );

	if ( arguments.GetTable( "format", &format ) )
		format->GetBool( "parameters", &parameters );

	HSQUIRRELVM vm = ThreadFromID( threadId );
	if ( !vm )
	{
		DAP_ERROR_RESPONSE( seq, "stackTrace" );
		DAP_ERROR_BODY( 0, "invalid thread {id}", 1 );
			json_table_t &variables = error.SetTable( "variables", 1 );
			variables.SetIntString( "id", threadId );
		DAP_SEND();
		return;
	}

#ifdef NATIVE_DEBUG_HOOK
	int lastFrame = vm->_callsstacksize - 1;
#else
	int lastFrame = vm->_callsstacksize - 1 - 1;
#endif

	if ( startFrame > lastFrame )
	{
		DAP_ERROR_RESPONSE( seq, "stackTrace" );
		DAP_ERROR_BODY( 0, "", 0 );
		DAP_SEND();
		return;
	}

	if ( startFrame < 0 )
		startFrame = 0;

	// reverse
	startFrame = lastFrame - startFrame;

	if ( levels <= 0 || levels > lastFrame )
		levels = lastFrame;

	int targetFrame = startFrame - levels;

	if ( targetFrame < 0 )
		targetFrame = 0;

	_DAP_START_RESPONSE( seq, "stackTrace", true, 2 );
	DAP_SET_TABLE( body, 1 );
		json_array_t &stackFrames = body.SetArray( "stackFrames", startFrame - targetFrame + 1 );
		for ( int i = startFrame; i >= targetFrame; i-- )
		{
			const SQVM::CallInfo &ci = vm->_callsstack[i];

			if ( ShouldIgnoreStackFrame(ci) )
				continue;

			if ( sq_type(ci._closure) == OT_CLOSURE )
			{
				json_table_t &frame = stackFrames.AppendTable(6);
				frame.SetInt( "id", ConvertToFrameID( threadId, i ) );

				SQFunctionProto *func = _fp(_closure(ci._closure)->_function);

				stringbuf_t< 256 > name;

				if ( sq_type(func->_name) == OT_STRING )
				{
					name.Puts( _string(func->_name) );
				}
				else
				{
					name.PutHex( (SQUnsignedInteger)func );
				}

				if ( parameters )
				{
					name.Put('(');

					Assert( func->_nparameters );

					int nparams = func->_nparameters;
#if SQUIRREL_VERSION_NUMBER >= 300
					if ( nparams > 1 )
#else
					if ( nparams > 1 || func->_varparams )
#endif
					{
#if SQUIRREL_VERSION_NUMBER >= 300
						if ( func->_varparams )
							nparams--;
#endif
						for ( int j = 1; j < nparams; j++ )
						{
							const SQObjectPtr &param = func->_parameters[j];
							if ( sq_type(param) == OT_STRING )
							{
								name.Puts( _string(param) );
							}

							name.Put(',');
							name.Put(' ');
						}

						if ( !func->_varparams )
						{
							name.len -= 2;
						}
						else
						{
							name.Put('.');
							name.Put('.');
							name.Put('.');
						}
					}

					name.Put(')');
				}

				frame.SetString( "name", name );

				if ( sq_type(func->_sourcename) == OT_STRING )
				{
					json_table_t &source = frame.SetTable( "source", 2 );
					SetSource( source, _string(func->_sourcename) );
				}

				frame.SetInt( "line", (int)func->GetLine( ci._ip ) );
				frame.SetInt( "column", 1 );

				stringbuf_t< FMT_PTR_LEN > instrref;
				instrref.PutHex( (SQUnsignedInteger)ci._ip );
				frame.SetString( "instructionPointerReference", instrref );
			}
			else if ( sq_type(ci._closure) == OT_NATIVECLOSURE )
			{
				json_table_t &frame = stackFrames.AppendTable(6);
				frame.SetInt( "id", ConvertToFrameID( threadId, i ) );

				SQNativeClosure *closure = _nativeclosure(ci._closure);

				json_table_t &source = frame.SetTable( "source", 1 );
				source.SetString( "name", "NATIVE" );

				stringbuf_t< 256 > name;

				if ( sq_type(closure->_name) == OT_STRING )
				{
					name.Puts( _string(closure->_name) );
				}
				else
				{
					name.PutHex( (SQUnsignedInteger)closure );
				}

				if ( parameters )
				{
					name.Put('(');
					name.Put(')');
				}

				frame.SetString( "name", name );

				frame.SetInt( "line", -1 );
				frame.SetInt( "column", 1 );
				frame.SetString( "presentationHint", "subtle" );
			}
			else UNREACHABLE();
		}
	DAP_SET( "totalFrames", lastFrame + 1 );
	DAP_SEND();
}

static bool HasMetaMethods( HSQUIRRELVM vm, const SQObjectPtr &obj )
{
	switch ( sq_type(obj) )
	{
		case OT_CLASS:
		{
			for ( unsigned int i = 0; i < MT_LAST; i++ )
			{
				if ( sq_type(_class(obj)->_metamethods[i]) != OT_NULL )
				{
					return true;
				}
			}

			return false;
		}
		default:
		{
			if ( is_delegable(obj) && _delegable(obj)->_delegate )
			{
				SQObjectPtr dummy;
				for ( unsigned int i = 0; i < MT_LAST; i++ )
				{
					if ( _delegable(obj)->GetMetaMethod( vm, (SQMetaMethod)i, dummy ) )
					{
						return true;
					}
				}
			}

			return false;
		}
	}
}

static inline void SetVirtualHint( json_table_t &elem )
{
	json_table_t &hint = elem.SetTable( "presentationHint", 2 );
	hint.SetString( "kind", "virtual" );
	json_array_t &attributes = hint.SetArray( "attributes", 1 );
	attributes.Append( "readOnly" );
}

static int _sortkeys( const SQObjectPtr *a, const SQObjectPtr *b )
{
	if ( sq_type(*a) == OT_STRING )
	{
		if ( sq_type(*b) == OT_STRING )
		{
			return scstricmp( _stringval(*a), _stringval(*b) );
		}
		else
		{
			return 1;
		}
	}
	else
	{
		if ( sq_type(*b) == OT_STRING )
		{
			return -1;
		}
		else
		{
			return ( _integer(*a) >= _integer(*b) );
		}
	}
}

#define _checkNonStringMembers(key) \
	( sq_type(key) != OT_STRING || HasEscapes( _string(key)->_val, _string(key)->_len ) )

static inline void SortKeys( SQTable *table, vector< SQObjectPtr > *values, bool *hasNonStringMembers )
{
	bool nsm = false;

	SQObjectPtr key, val;
	FOREACH_SQTABLE( table, key, val )
	{
		values->append( key );

		if ( !nsm )
			nsm = _checkNonStringMembers( key );
	}

	values->sort( _sortkeys );
	*hasNonStringMembers = nsm;
}

static inline void SortKeys( SQClass *pClass,
		int *nAttributes, vector< SQObjectPtr > *values, vector< SQObjectPtr > *methods, bool *hasNonStringMembers )
{
	bool nsm = false;
	*nAttributes = 0;

	SQObjectPtr key, idx;
	FOREACH_SQTABLE( pClass->_members, key, idx )
	{
		// Ignore inherited fields
		if ( pClass->_base )
		{
			SQObjectPtr baseval;
			if ( pClass->_base->Get( key, baseval ) )
			{
				const SQObjectPtr &val = _isfield(idx) ?
					pClass->_defaultvalues[ _member_idx(idx) ].val :
					pClass->_methods[ _member_idx(idx) ].val;

				if ( sq_type(val) == sq_type(baseval) && _rawval(val) == _rawval(baseval) )
					continue;
			}
		}

		if ( _isfield(idx) )
		{
			values->append( key );
		}
		else
		{
			methods->append( key );
		}

		const SQObjectPtr &attr = _isfield(idx) ?
			pClass->_defaultvalues[ _member_idx(idx) ].attrs :
			pClass->_methods[ _member_idx(idx) ].attrs;

		if ( sq_type(attr) != OT_NULL )
			(*nAttributes)++;

		if ( !nsm )
			nsm = _checkNonStringMembers( key );
	}

	if ( values->size() )
		values->sort( _sortkeys );

	if ( methods->size() )
		methods->sort( _sortkeys );

	*hasNonStringMembers = nsm;
}

static inline void SortKeys( SQClass *pClass, vector< SQObjectPtr > *values, bool *hasNonStringMembers )
{
	bool nsm = false;

	SQObjectPtr key, idx;
	FOREACH_SQTABLE( pClass->_members, key, idx )
	{
		if ( _isfield(idx) )
		{
			values->append( key );

			if ( !nsm )
				nsm = _checkNonStringMembers( key );
		}
	}

	if ( values->size() )
		values->sort( _sortkeys );

	*hasNonStringMembers = nsm;
}

#undef _checkNonStringMembers

void SQDebugServer::OnRequest_Variables( const json_table_t &arguments, int seq )
{
	int variablesReference;
	arguments.GetInt( "variablesReference", &variablesReference );

	varref_t *ref = FromVarRef( variablesReference );
	if ( !ref )
	{
		DAP_ERROR_RESPONSE( seq, "variables" );
		DAP_ERROR_BODY( 0, "failed to find variable", 0 );
		DAP_SEND();
		return;
	}

	int flags = 0;
	{
		json_table_t *format;
		if ( arguments.GetTable( "format", &format ) )
		{
			bool hex;
			format->GetBool( "hex", &hex );
			if ( hex )
				flags |= kFS_Hexadecimal;
		}
	}

	switch ( ref->type )
	{
		case VARREF_SCOPE_LOCALS:
		{
			HSQUIRRELVM vm = ref->GetThread();
			int frame = ref->scope.frame;

			if ( !IsValidStackFrame( vm, frame ) ||
					sq_type(vm->_callsstack[frame]._closure) != OT_CLOSURE )
			{
				DAP_ERROR_RESPONSE( seq, "variables" );
				DAP_ERROR_BODY( 0, "invalid stack frame {id}", 1 );
					json_table_t &variables = error.SetTable( "variables", 1 );
					variables.SetIntString( "id", frame );
				DAP_SEND();
				break;
			}

			const SQVM::CallInfo &ci = vm->_callsstack[ frame ];
			int stackbase = GetStackBase( vm, &ci );
			SQClosure *pClosure = _closure(ci._closure);
			SQFunctionProto *func = _fp(pClosure->_function);

			SQUnsignedInteger ip = (SQUnsignedInteger)( ci._ip - func->_instructions - 1 );

			int count = m_ReturnValues.size();

			for ( int i = func->_nlocalvarinfos; i--; )
			{
				const SQLocalVarInfo &var = func->_localvarinfos[i];
				if ( var._start_op <= ip + 1 && var._end_op >= ip )
					count++;
			}

			DAP_START_RESPONSE( seq, "variables" );
			DAP_SET_TABLE( body, 1 );
				json_array_t &variables = body.SetArray( "variables", count );

				for ( int i = 0; i < m_ReturnValues.size(); i++ )
				{
					const returnvalue_t &rv = m_ReturnValues[i];
					json_table_t &elem = variables.AppendTable(6);

					string_t str;

					if ( !( m_iYieldValues & (1<<(i+1)) ) )
					{
						str.Assign( "return@" );
					}
					else
					{
						str.Assign( "yield@" );
					}

					if ( rv.funcname )
					{
						string_t buf;
						buf.len = str.len + rv.funcname->_len;
						buf.ptr = (char*)sqdbg_malloc( buf.len + 1 );

						memcpy( buf.ptr, str.ptr, str.len );
						int l = scstombs( buf.ptr + str.len, buf.len + 1 - str.len,
								rv.funcname->_val, rv.funcname->_len );
						buf.len = str.len + l;

						elem.SetStringExternal( "name", buf );
					}
					else
					{
						elem.SetStringNoCopy( "name", str );
					}

					JSONSetString( elem, "value", rv.value, flags );
					elem.SetString( "type", GetType( rv.value ) );
					elem.SetInt( "variablesReference", ToVarRef( rv.value ) );
					SetVirtualHint( elem );

					if ( ShouldPageArray( rv.value, 32 * 1024 ) )
					{
						elem.SetInt( "indexedVariables", (int)_array(rv.value)->_values.size() );
					}
				}

				for ( int i = func->_nlocalvarinfos; i--; )
				{
					const SQLocalVarInfo &var = func->_localvarinfos[i];
					Assert( sq_type(var._name) == OT_STRING );

					if ( var._start_op <= ip + 1 && var._end_op >= ip )
					{
						const SQObjectPtr &val = vm->_stack._vals[ stackbase + var._pos ];
						json_table_t &elem = variables.AppendTable(6);
						elem.SetString( "name", _string(var._name) );
						JSONSetString( elem, "value", val, flags );
						elem.SetString( "type", GetType( val ) );
						stringbuf_t< 8 > buf;
						buf.Put('@');
						buf.Put('L');
						buf.Put('@');
						buf.PutInt( (int)func->_nlocalvarinfos - i - 1 );
						elem.SetString( "evaluateName", buf );
						elem.SetInt( "variablesReference", ToVarRef( val ) );

						if ( ShouldPageArray( val, 32 * 1024 ) )
						{
							elem.SetInt( "indexedVariables", (int)_array(val)->_values.size() );
						}
					}
				}
			DAP_SEND();

			break;
		}
		case VARREF_SCOPE_OUTERS:
		{
			HSQUIRRELVM vm = ref->GetThread();
			int frame = ref->scope.frame;

			if ( !IsValidStackFrame( vm, frame ) ||
					sq_type(vm->_callsstack[frame]._closure) != OT_CLOSURE )
			{
				DAP_ERROR_RESPONSE( seq, "variables" );
				DAP_ERROR_BODY( 0, "invalid stack frame {id}", 1 );
					json_table_t &variables = error.SetTable( "variables", 1 );
					variables.SetIntString( "id", frame );
				DAP_SEND();
				break;
			}

			const SQVM::CallInfo &ci = vm->_callsstack[ frame ];
			SQClosure *pClosure = _closure(ci._closure);
			SQFunctionProto *func = _fp(pClosure->_function);

			DAP_START_RESPONSE( seq, "variables" );
			DAP_SET_TABLE( body, 1 );
				json_array_t &variables = body.SetArray( "variables", func->_noutervalues );
				for ( int i = 0; i < func->_noutervalues; i++ )
				{
					const SQOuterVar &var = func->_outervalues[i];
					Assert( sq_type(var._name) == OT_STRING );

					const SQObjectPtr &val = *_outervalptr( pClosure->_outervalues[i] );
					json_table_t &elem = variables.AppendTable(5);
					elem.SetString( "name", _string(var._name) );
					JSONSetString( elem, "value", val, flags );
					elem.SetString( "type", GetType( val ) );
					elem.SetInt( "variablesReference", ToVarRef( val ) );

					if ( ShouldPageArray( val, 32 * 1024 ) )
					{
						elem.SetInt( "indexedVariables", (int)_array(val)->_values.size() );
					}
				}
			DAP_SEND();

			break;
		}
		case VARREF_OBJ:
		{
			SQObject target = ref->GetVar();

			if ( !ISREFCOUNTED( sq_type(target) ) )
			{
				DAP_ERROR_RESPONSE( seq, "variables" );
				DAP_ERROR_BODY( 0, "invalid object", 0 );
				DAP_SEND();
				return;
			}

			DAP_START_RESPONSE( seq, "variables" );
			DAP_SET_TABLE( body, 1 );
				json_array_t &variables = body.SetArray( "variables", 8 );

				{
					json_table_t &elem = variables.AppendTable(4);
					elem.SetString( "name", INTERNAL_TAG("refs") );
					elem.SetInt( "variablesReference", -1 );
					SetVirtualHint( elem );

					if ( sq_type(target) != OT_WEAKREF )
					{
						elem.SetString( "value", GetValue( _refcounted(target)->_uiRef, flags ) );
					}
					else
					{
						stringbuf_t< 256 > buf;
						buf.PutInt( _refcounted(target)->_uiRef );

						do
						{
							target = _weakref(target)->_obj;
							buf.Put(' ');
							buf.Put('>');
							buf.Put( ( sq_type(target) != OT_WEAKREF ) ? '*' : ' ' );
							buf.PutInt( _refcounted(target)->_uiRef );

							if ( buf.len >= (int)sizeof(buf.ptr) - 4 )
								break;
						}
						while ( sq_type(target) == OT_WEAKREF );

						buf.Term();

						elem.SetString( "value", buf );
					}
				}

				if ( sq_type(target) == OT_ARRAY )
				{
					SQObjectPtrVec &vals = _array(target)->_values;

					Assert( vals.size() <= INT_MAX );

					json_table_t &elem = variables.AppendTable(4);
					elem.SetString( "name", INTERNAL_TAG("allocated") );
					elem.SetString( "value", GetValue( vals.capacity(), flags ) );
					elem.SetInt( "variablesReference", -1 );
					SetVirtualHint( elem );

					int idx, end;

					string_t filter;
					if ( arguments.GetString( "filter", &filter ) && filter.IsEqualTo("indexed") )
					{
						arguments.GetInt( "start", &idx );
						arguments.GetInt( "count", &end );

						if ( idx < 0 )
							idx = 0;

						if ( end <= 0 )
						{
							end = vals.size();
						}
						else
						{
							end += idx;
							if ( end > (int)vals.size() )
								end = vals.size();
						}
					}
					else
					{
						idx = 0;
						end = vals.size();
					}

					if ( variables.size() + ( end - idx ) > variables.capacity() )
						variables.reserve( variables.size() + ( end - idx ) );

					for ( ; idx < end; idx++ )
					{
						const SQObjectPtr &val = vals[idx];

						stringbuf_t< 32 > name;
						name.Put('[');
						name.PutInt( idx );
						name.Put(']');

						json_table_t &elem = variables.AppendTable(5);
						elem.SetString( "name", name );
						JSONSetString( elem, "value", val, flags );
						elem.SetString( "type", GetType( val ) );
						elem.SetInt( "variablesReference", ToVarRef( val ) );
						json_table_t &hint = elem.SetTable( "presentationHint", 1 );
						hint.SetString( "kind", GetPresentationHintKind( val ) );
					}

					// done with arrays
				}

				// delegates
				switch ( sq_type(target) )
				{
					case OT_INSTANCE:
					{
						if ( _instance(target)->_class )
						{
							json_table_t &elem = variables.AppendTable(4);
							elem.SetString( "name", INTERNAL_TAG("class") );
							elem.SetString( "value", GetValue( ToSQObject( _instance(target)->_class ) ) );
							elem.SetInt( "variablesReference", ToVarRef( ToSQObject( _instance(target)->_class ) ) );
							SetVirtualHint( elem );
						}

						break;
					}
					case OT_CLASS:
					{
						if ( _class(target)->_base )
						{
							json_table_t &elem = variables.AppendTable(4);
							elem.SetString( "name", INTERNAL_TAG("base") );
							elem.SetString( "value", GetValue( ToSQObject( _class(target)->_base ) ) );
							elem.SetInt( "variablesReference", ToVarRef( ToSQObject( _class(target)->_base ) ) );
							SetVirtualHint( elem );
						}

						break;
					}
					case OT_TABLE:
					{
						if ( _table(target)->_delegate )
						{
							json_table_t &elem = variables.AppendTable(4);
							elem.SetString( "name", INTERNAL_TAG("delegate") );
							elem.SetString( "value", GetValue( ToSQObject( _table(target)->_delegate ) ) );
							elem.SetInt( "variablesReference", ToVarRef( ToSQObject( _table(target)->_delegate ) ) );
							SetVirtualHint( elem );
						}

						break;
					}
					default: break;
				}

				// metamethods
				if ( sq_type(target) != OT_INSTANCE && HasMetaMethods( m_pRootVM, target ) )
				{
					json_table_t &elem = variables.AppendTable(4);
					elem.SetString( "name", INTERNAL_TAG("metamethods") );
					elem.SetString( "value", "{...}" );
					elem.SetInt( "variablesReference", ToVarRef( VARREF_METAMETHODS, target ) );
					SetVirtualHint( elem );
				}

				bool shouldQuoteKeys;

				// members
				switch ( sq_type(target) )
				{
					case OT_TABLE:
					{
						int keyflags = flags | kFS_NoQuote | kFS_KeyVal;

						Assert( _table(target)->CountUsed() <= INT_MAX );

						vector< SQObjectPtr > keys( _table(target)->CountUsed() );
						SortKeys( _table(target), &keys, &shouldQuoteKeys );

						if ( variables.size() + keys.size() > variables.capacity() )
							variables.reserve( variables.size() + keys.size() );

						ref->obj.hasNonStringMembers = shouldQuoteKeys;

						if ( shouldQuoteKeys )
							keyflags &= ~kFS_NoQuote;

						for ( int i = 0; i < keys.size(); i++ )
						{
							const SQObjectPtr &key = keys[i];
							SQObjectPtr val;
							_table(target)->Get( key, val );

							json_table_t &elem = variables.AppendTable(5);

							if ( shouldQuoteKeys && sq_type(key) == OT_INTEGER )
							{
								stringbuf_t< 32 > name;
								name.Put('[');
								name.PutInt( _integer(key) );
								name.Put(']');
								elem.SetString( "name", name );
							}
							else
							{
								JSONSetString( elem, "name", key, keyflags );
							}

							JSONSetString( elem, "value", val, flags );
							elem.SetString( "type", GetType( val ) );
							elem.SetInt( "variablesReference", ToVarRef( val ) );
							json_table_t &hint = elem.SetTable( "presentationHint", 1 );
							hint.SetString( "kind", GetPresentationHintKind( val ) );
						}

						break;
					}
					case OT_CLASS:
					{
						int keyflags = flags | kFS_NoQuote | kFS_KeyVal;
						int nAttributes;

						Assert( _class(target)->_members->CountUsed() <= INT_MAX );

						vector< SQObjectPtr > values, methods;
						SortKeys( _class(target), &nAttributes, &values, &methods, &shouldQuoteKeys );

						if ( variables.size() + values.size() + methods.size() + 1 > variables.capacity() )
							variables.reserve( variables.size() + values.size() + methods.size() + 1 );

						ref->obj.hasNonStringMembers = shouldQuoteKeys;

						if ( shouldQuoteKeys )
							keyflags &= ~kFS_NoQuote;

						if ( sq_type(_class(target)->_attributes) != OT_NULL )
						{
							json_table_t &elem = variables.AppendTable(4);
							elem.SetString( "name", INTERNAL_TAG("attributes") );
							elem.SetString( "value", GetValue( _class(target)->_attributes, flags ) );
							elem.SetInt( "variablesReference", ToVarRef( _class(target)->_attributes ) );
							SetVirtualHint( elem );
						}
						else if ( nAttributes )
						{
							json_table_t &elem = variables.AppendTable(4);
							elem.SetString( "name", INTERNAL_TAG("attributes") );
							elem.SetString( "value", "{...}" );
							elem.SetInt( "variablesReference", ToVarRef( VARREF_ATTRIBUTES, target ) );
							SetVirtualHint( elem );
						}

						for ( int i = 0; i < values.size(); i++ )
						{
							const SQObjectPtr &key = values[i];
							SQObjectPtr val;
							_class(target)->Get( key, val );

							json_table_t &elem = variables.AppendTable(5);

							if ( shouldQuoteKeys && sq_type(key) == OT_INTEGER )
							{
								stringbuf_t< 32 > name;
								name.Put('[');
								name.PutInt( _integer(key) );
								name.Put(']');
								elem.SetString( "name", name );
							}
							else
							{
								JSONSetString( elem, "name", key, keyflags );
							}

							JSONSetString( elem, "value", val, flags );
							elem.SetString( "type", GetType( val ) );
							elem.SetInt( "variablesReference", ToVarRef( val ) );
							json_table_t &hint = elem.SetTable( "presentationHint", 1 );
							hint.SetString( "kind", GetPresentationHintKind( val ) );
						}

						for ( int i = 0; i < methods.size(); i++ )
						{
							const SQObjectPtr &key = methods[i];
							SQObjectPtr val;
							_class(target)->Get( key, val );

							json_table_t &elem = variables.AppendTable(5);

							if ( shouldQuoteKeys && sq_type(key) == OT_INTEGER )
							{
								stringbuf_t< 32 > name;
								name.Put('[');
								name.PutInt( _integer(key) );
								name.Put(']');
								elem.SetString( "name", name );
							}
							else
							{
								JSONSetString( elem, "name", key, keyflags );
							}

							JSONSetString( elem, "value", val, flags );
							elem.SetString( "type", GetType( val ) );
							elem.SetInt( "variablesReference", ToVarRef( val ) );
							json_table_t &hint = elem.SetTable( "presentationHint", 1 );
							hint.SetString( "kind", GetPresentationHintKind( val ) );
						}

						break;
					}
					case OT_INSTANCE:
					{
						int keyflags = flags | kFS_NoQuote | kFS_KeyVal;
						SQClass *base = _instance(target)->_class;

						Assert( base );
						Assert( base->_members->CountUsed() <= INT_MAX );

						vector< SQObjectPtr > values;
						SortKeys( base, &values, &shouldQuoteKeys );

						if ( variables.size() + values.size() > variables.capacity() )
							variables.reserve( variables.size() + values.size() );

						ref->obj.hasNonStringMembers = shouldQuoteKeys;

						if ( shouldQuoteKeys )
							keyflags &= ~kFS_NoQuote;

						// metamembers
						SQObjectPtr mm;
						const SQObjectPtr *def = GetClassDefMetaMembers( base );

						if ( def && _instance(target)->GetMetaMethod( m_pRootVM, MT_GET, mm ) )
						{
							for ( unsigned int i = 0; i < _array(*def)->_values.size(); i++ )
							{
								SQObjectPtr val;
								const SQObjectPtr &key = _array(*def)->_values[i];

								if ( RunClosure( mm, &target, key, val ) )
								{
									json_table_t &elem = variables.AppendTable(5);
									JSONSetString( elem, "name", key, keyflags );
									// NOTE: val can be temporary, copy it and keep weakref
									elem.SetString( "value", GetValue( val, flags ) );
									elem.SetString( "type", GetType( val ) );
									elem.SetInt( "variablesReference", ToVarRef( val, true ) );
									json_table_t &hint = elem.SetTable( "presentationHint", 1 );
									hint.SetString( "kind", GetPresentationHintKind( val ) );
								}
							}
						}

						// user defined
						def = GetClassDefCustomMembers( base );

						if ( def )
						{
							SQObjectPtr custommembers = *def;

							if ( sq_type(custommembers) == OT_CLOSURE )
								RunClosure( custommembers, &target, custommembers );

							if ( sq_type(custommembers) == OT_ARRAY )
							{
								objref_t tmp;
								SQObjectPtr strName = CreateSQString( m_pRootVM, _SC("name") );
								SQObjectPtr strGet = CreateSQString( m_pRootVM, _SC("get") );
								SQObjectPtr strSet = CreateSQString( m_pRootVM, _SC("set") );
								SQObjectPtr name, val;

								for ( unsigned int i = 0; i < _array(custommembers)->_values.size(); i++ )
								{
									const SQObjectPtr &memdef = _array(custommembers)->_values[i];

									if ( GetObj_Var( strName, memdef, tmp, name ) &&
											GetObj_Var( strGet, memdef, tmp, val ) &&
											CallCustomMembersGetFunc( val, &target, name, val ) )
									{
										json_table_t &elem = variables.AppendTable(5);

										if ( shouldQuoteKeys && sq_type(name) == OT_INTEGER )
										{
											stringbuf_t< 32 > buf;
											buf.Put('[');
											buf.PutInt( _integer(name) );
											buf.Put(']');
											elem.SetString( "name", buf );
										}
										else
										{
											// NOTE: name can be temporary, copy it
											elem.SetString( "name", GetValue( name, keyflags ) );
										}

										// NOTE: val can be temporary, keep strong ref for inspection
										JSONSetString( elem, "value", val, flags );
										elem.SetString( "type", GetType( val ) );
										elem.SetInt( "variablesReference", ToVarRef( val, false, true ) );
										json_table_t &hint = elem.SetTable( "presentationHint", 2 );
										hint.SetString( "kind", GetPresentationHintKind( val ) );

										if ( !GetObj_Var( strSet, memdef, tmp, val ) ||
												( sq_type(val) != OT_CLOSURE && sq_type(val) != OT_NATIVECLOSURE ) )
										{
											json_array_t &attributes = hint.SetArray( "attributes", 1 );
											attributes.Append( "readOnly" );
										}
									}
								}
							}
						}

						for ( int i = 0; i < values.size(); i++ )
						{
							const SQObjectPtr &key = values[i];
							SQObjectPtr val;
							_instance(target)->Get( key, val );

							json_table_t &elem = variables.AppendTable(5);

							if ( shouldQuoteKeys && sq_type(key) == OT_INTEGER )
							{
								stringbuf_t< 32 > name;
								name.Put('[');
								name.PutInt( _integer(key) );
								name.Put(']');
								elem.SetString( "name", name );
							}
							else
							{
								JSONSetString( elem, "name", key, keyflags );
							}

							JSONSetString( elem, "value", val, flags );
							elem.SetString( "type", GetType( val ) );
							elem.SetInt( "variablesReference", ToVarRef( val ) );
							json_table_t &hint = elem.SetTable( "presentationHint", 1 );
							hint.SetString( "kind", GetPresentationHintKind( val ) );
						}

						break;
					}
					case OT_CLOSURE:
					{
						SQFunctionProto *func = _fp(_closure(target)->_function);

						Assert( func->_ninstructions <= INT_MAX );
						Assert( func->GetLine( &func->_instructions[ func->_ninstructions - 1 ] ) <= INT_MAX );
						Assert( func->_nliterals <= INT_MAX );
						Assert( func->_noutervalues <= INT_MAX );
						Assert( func->_nlocalvarinfos <= INT_MAX );
						Assert( func->_nlineinfos <= INT_MAX );

						if ( sq_type(func->_name) == OT_STRING )
						{
							json_table_t &elem = variables.AppendTable(4);
							elem.SetString( "name", INTERNAL_TAG("name") );
							elem.SetString( "value", _string(func->_name) );
							elem.SetInt( "variablesReference", -1 );
							SetVirtualHint( elem );
						}

						if ( sq_type(func->_sourcename) == OT_STRING )
						{
							json_table_t &elem = variables.AppendTable(4);
							elem.SetString( "name", INTERNAL_TAG("source") );

							int decline = GetFunctionDeclarationLine( func );
							if ( decline )
							{
								stringbuf_t< 256 > buf;
								buf.Puts( _string(func->_sourcename) );
								buf.Put(':');
								buf.PutInt( decline );
								elem.SetString( "value", buf );
							}
							else
							{
								elem.SetString( "value", _string(func->_sourcename) );
							}

							elem.SetInt( "variablesReference", -1 );
							SetVirtualHint( elem );
						}

						if ( func->_bgenerator )
						{
							json_table_t &elem = variables.AppendTable(4);
							elem.SetString( "name", INTERNAL_TAG("generator") );
							elem.SetString( "value", "1" );
							elem.SetInt( "variablesReference", -1 );
							SetVirtualHint( elem );
						}

						int nparams = func->_nparameters;
#if SQUIRREL_VERSION_NUMBER >= 300
						if ( nparams > 1 )
#else
						if ( nparams > 1 || func->_varparams )
#endif
						{
							json_table_t &elem = variables.AppendTable(4);
							elem.SetString( "name", INTERNAL_TAG("parameters") );

							if ( !func->_varparams )
							{
								elem.SetString( "value", GetValue( nparams, flags ) );
							}
							else
							{
#if SQUIRREL_VERSION_NUMBER >= 300
								nparams--;
#endif
								stringbuf_t< 16 > buf;

								if ( !( flags & kFS_Hexadecimal ) )
								{
									buf.PutInt( nparams );
								}
								else
								{
									buf.PutHex( nparams );
								}

								buf.Puts("...");
								elem.SetString( "value", buf );
							}

							elem.SetInt( "variablesReference", -1 );
							SetVirtualHint( elem );
						}

						{
							json_table_t &elem = variables.AppendTable(4);
							elem.SetString( "name", INTERNAL_TAG("stacksize") );
							elem.SetString( "value", GetValue( func->_stacksize, flags ) );
							elem.SetInt( "variablesReference", -1 );
							SetVirtualHint( elem );
						}

						{
							json_table_t &elem = variables.AppendTable(4);
							elem.SetString( "name", INTERNAL_TAG("instructions") );

							// ignore line ops
							int c = func->_ninstructions;
							for ( int i = c; i--; )
								if ( func->_instructions[i].op == _OP_LINE )
									c--;

							elem.SetString( "value", GetValue( c, flags ) );
							elem.SetInt( "variablesReference", ToVarRef( VARREF_INSTRUCTIONS, target ) );
							SetVirtualHint( elem );
						}

						if ( func->_nliterals )
						{
							json_table_t &elem = variables.AppendTable(4);
							elem.SetString( "name", INTERNAL_TAG("literals") );
							elem.SetString( "value", GetValue( func->_nliterals, flags ) );
							elem.SetInt( "variablesReference", ToVarRef( VARREF_LITERALS, target ) );
							SetVirtualHint( elem );
						}

						if ( func->_noutervalues )
						{
							json_table_t &elem = variables.AppendTable(4);
							elem.SetString( "name", INTERNAL_TAG("outervalues") );
							elem.SetString( "value", GetValue( func->_noutervalues, flags ) );
							elem.SetInt( "variablesReference", ToVarRef( VARREF_OUTERS, target ) );
							SetVirtualHint( elem );
						}
#ifdef CLOSURE_ENV_ISVALID
						if ( CLOSURE_ENV_ISVALID( _closure(target)->_env ) )
						{
							json_table_t &elem = variables.AppendTable(4);
							elem.SetString( "name", INTERNAL_TAG("env") );
							elem.SetString( "value", GetValue(
										CLOSURE_ENV_OBJ( _closure(target)->_env ) ) );
							elem.SetInt( "variablesReference", ToVarRef(
										CLOSURE_ENV_OBJ( _closure(target)->_env ) ) );
							SetVirtualHint( elem );
						}
#endif
#ifdef CLOSURE_ROOT
						if ( _closure(target)->_root &&
								_table(_closure(target)->_root->_obj) != _table(m_pRootVM->_roottable) )
						{
							json_table_t &elem = variables.AppendTable(4);
							elem.SetString( "name", INTERNAL_TAG("root") );
							elem.SetString( "value", GetValue( _closure(target)->_root->_obj ) );
							elem.SetInt( "variablesReference", ToVarRef( _closure(target)->_root->_obj ) );
							SetVirtualHint( elem );
						}
#endif
						break;
					}
					case OT_NATIVECLOSURE:
					{
						if ( sq_type(_nativeclosure(target)->_name) == OT_STRING )
						{
							json_table_t &elem = variables.AppendTable(4);
							elem.SetString( "name", INTERNAL_TAG("name") );
							elem.SetString( "value", _string(_nativeclosure(target)->_name) );
							elem.SetInt( "variablesReference", -1 );
							SetVirtualHint( elem );
						}

						if ( _nativeclosure(target)->_nparamscheck != 1 &&
								_nativeclosure(target)->_nparamscheck != 0 )
						{
							json_table_t &elem = variables.AppendTable(4);
							elem.SetString( "name", INTERNAL_TAG("parameters") );

							if ( _nativeclosure(target)->_nparamscheck > 0 )
							{
								elem.SetString( "value", GetValue( _nativeclosure(target)->_nparamscheck, flags ) );
							}
							else
							{
								stringbuf_t< 16 > buf;

								Assert( -_nativeclosure(target)->_nparamscheck <= INT_MAX );

								if ( !( flags & kFS_Hexadecimal ) )
								{
									buf.PutInt( (int)-_nativeclosure(target)->_nparamscheck );
								}
								else
								{
									buf.PutHex( (int)-_nativeclosure(target)->_nparamscheck );
								}

								buf.Puts("...");
								elem.SetString( "value", buf );
							}

							elem.SetInt( "variablesReference", -1 );
							SetVirtualHint( elem );
						}

						if ( _nativenoutervalues(_nativeclosure(target)) )
						{
							json_table_t &elem = variables.AppendTable(4);
							elem.SetString( "name", INTERNAL_TAG("outervalues") );
							elem.SetString( "value", GetValue(
										_nativenoutervalues(_nativeclosure(target)), flags ) );
							elem.SetInt( "variablesReference", -1 );
							SetVirtualHint( elem );
						}
#ifdef CLOSURE_ENV_ISVALID
						if ( CLOSURE_ENV_ISVALID( _nativeclosure(target)->_env ) )
						{
							json_table_t &elem = variables.AppendTable(4);
							elem.SetString( "name", INTERNAL_TAG("env") );
							elem.SetString( "value", GetValue(
										CLOSURE_ENV_OBJ( _nativeclosure(target)->_env ) ) );
							elem.SetInt( "variablesReference", ToVarRef(
										CLOSURE_ENV_OBJ( _nativeclosure(target)->_env ) ) );
							SetVirtualHint( elem );
						}
#endif
						break;
					}
					case OT_THREAD:
					{
						Assert( _thread(_ss(_thread(target))->_root_vm) == m_pRootVM );

						{
							json_table_t &elem = variables.AppendTable(4);
							elem.SetString( "name", INTERNAL_TAG("state") );
							switch ( sq_getvmstate( _thread(target) ) )
							{
								case SQ_VMSTATE_IDLE:		elem.SetString( "value", "idle" ); break;
								case SQ_VMSTATE_RUNNING:	elem.SetString( "value", "running" ); break;
								case SQ_VMSTATE_SUSPENDED:	elem.SetString( "value", "suspended" ); break;
								default: UNREACHABLE();
							}
							elem.SetInt( "variablesReference", -1 );
							SetVirtualHint( elem );
						}

						if ( _table(_thread(target)->_roottable) != _table(m_pRootVM->_roottable) )
						{
							json_table_t &elem = variables.AppendTable(4);
							elem.SetString( "name", INTERNAL_TAG("root") );
							elem.SetString( "value", GetValue( _thread(target)->_roottable ) );
							elem.SetInt( "variablesReference", -1 );
							SetVirtualHint( elem );
						}

						if ( _thread(target) != m_pRootVM )
						{
							const SQObjectPtr &val = _thread(target)->_stack._vals[0];
							Assert( sq_type(val) == OT_CLOSURE || sq_type(val) == OT_NATIVECLOSURE );

							json_table_t &elem = variables.AppendTable(4);
							elem.SetString( "name", INTERNAL_TAG("function") );
							elem.SetString( "value", GetValue( val ) );
							elem.SetInt( "variablesReference", ToVarRef( val ) );
							SetVirtualHint( elem );
						}

						if ( _thread(target)->_callsstacksize != 0 )
						{
							json_table_t &elem = variables.AppendTable(4);
							elem.SetString( "name", INTERNAL_TAG("callstack") );
							elem.SetString( "value", GetValue( _thread(target)->_callsstacksize ) );
							elem.SetInt( "variablesReference", ToVarRef( VARREF_CALLSTACK, target ) );
							SetVirtualHint( elem );
						}

						break;
					}
					case OT_GENERATOR:
					{
						{
							json_table_t &elem = variables.AppendTable(4);
							elem.SetString( "name", INTERNAL_TAG("state") );
							switch ( _generator(target)->_state )
							{
								case SQGenerator::eSuspended:	elem.SetString( "value", "suspended" ); break;
								case SQGenerator::eRunning:		elem.SetString( "value", "running" ); break;
								case SQGenerator::eDead:		elem.SetString( "value", "dead" ); break;
								default: UNREACHABLE();
							}
							elem.SetInt( "variablesReference", -1 );
							SetVirtualHint( elem );
						}

						if ( _generator(target)->_state != SQGenerator::eDead )
						{
							const SQVM::CallInfo &ci = _generator(target)->_ci;
							Assert( sq_type(ci._closure) == OT_CLOSURE );

							SQFunctionProto *func = _fp(_closure(ci._closure)->_function);

							sqstring_t source = sq_type(func->_sourcename) == OT_STRING ?
								sqstring_t(_string(func->_sourcename)) :
								_SC("??");

							stringbuf_t< 256 > buf;
							buf.Puts( source );
							buf.Put(':');
							buf.PutInt( (int)func->GetLine( ci._ip ) );

							json_table_t &elem = variables.AppendTable(4);
							elem.SetString( "name", INTERNAL_TAG("frame") );
							elem.SetString( "value", buf );
							elem.SetInt( "variablesReference", -1 );
							SetVirtualHint( elem );
						}

						if ( sq_type(_generator(target)->_closure) != OT_NULL )
						{
							const SQObjectPtr &val = _generator(target)->_closure;
							Assert( sq_type(val) == OT_CLOSURE || sq_type(val) == OT_NATIVECLOSURE );

							json_table_t &elem = variables.AppendTable(4);
							elem.SetString( "name", INTERNAL_TAG("function") );
							elem.SetString( "value", GetValue( val ) );
							elem.SetInt( "variablesReference", ToVarRef( val ) );
							SetVirtualHint( elem );
						}

						break;
					}
					case OT_STRING:
					case OT_ARRAY:
					case OT_WEAKREF:
					case OT_USERDATA:
						break;
					default:
						Assert(!"unknown type");
				}
			DAP_SEND();
			break;
		}
		case VARREF_INSTRUCTIONS:
		{
			SQObject target = ref->GetVar();

			if ( sq_type(target) != OT_CLOSURE )
			{
				DAP_ERROR_RESPONSE( seq, "variables" );
				DAP_ERROR_BODY( 0, "invalid object", 0 );
				DAP_SEND();
				return;
			}

			SQFunctionProto *func = _fp(_closure(target)->_function);
			int ninstructions = func->_ninstructions;

			DAP_START_RESPONSE( seq, "variables" );
			DAP_SET_TABLE( body, 1 );
				json_array_t &variables = body.SetArray( "variables", ninstructions );

				// ignore line ops
				int lines = 0;

				for ( int i = 0; i < ninstructions; i++ )
				{
					SQInstruction *instr = func->_instructions + i;

					if ( instr->op == _OP_LINE )
					{
						lines++;
						continue;
					}

					json_table_t &elem = variables.AppendTable(4);
					{
						stringbuf_t< 32 > instrBytes; // "0xFF -2147483648 255 255 255"
						instrBytes.PutHex( instr->op ); instrBytes.Put(' ');
						instrBytes.PutInt( instr->_arg0 ); instrBytes.Put(' ');
						instrBytes.PutInt( instr->_arg1 ); instrBytes.Put(' ');
						instrBytes.PutInt( instr->_arg2 ); instrBytes.Put(' ');
						instrBytes.PutInt( instr->_arg3 );
						elem.SetString( "value", instrBytes );
					}
					{
						stringbuf_t< 64 > name; // index:line
						name.PutInt( i - lines );
						name.Put(':');
						name.PutInt( (int)func->GetLine( instr ) );
						elem.SetString( "name", name );
					}
					elem.SetInt( "variablesReference", -1 );
#ifndef SQDBG_SUPPORTS_SET_INSTRUCTION
					elem.SetTable( "presentationHint", 1 ).SetArray( "attributes", 1 ).Append( "readOnly" );
#endif
				}
			DAP_SEND();

			break;
		}
		case VARREF_OUTERS:
		{
			SQObject target = ref->GetVar();

			if ( sq_type(target) != OT_CLOSURE )
			{
				DAP_ERROR_RESPONSE( seq, "variables" );
				DAP_ERROR_BODY( 0, "invalid object", 0 );
				DAP_SEND();
				return;
			}

			SQClosure *pClosure = _closure(target);
			SQFunctionProto *func = _fp(pClosure->_function);

			DAP_START_RESPONSE( seq, "variables" );
			DAP_SET_TABLE( body, 1 );
				json_array_t &variables = body.SetArray( "variables", func->_noutervalues );
				for ( int i = 0; i < func->_noutervalues; i++ )
				{
					const SQOuterVar &var = func->_outervalues[i];
					Assert( sq_type(var._name) == OT_STRING );

					const SQObjectPtr &val = *_outervalptr( pClosure->_outervalues[i] );
					json_table_t &elem = variables.AppendTable(4);
					elem.SetString( "name", _string(var._name) );
					JSONSetString( elem, "value", val, flags );
					elem.SetString( "type", GetType( val ) );
					elem.SetInt( "variablesReference", ToVarRef( val ) );
				}
			DAP_SEND();

			break;
		}
		case VARREF_LITERALS:
		{
			SQObject target = ref->GetVar();

			if ( sq_type(target) != OT_CLOSURE )
			{
				DAP_ERROR_RESPONSE( seq, "variables" );
				DAP_ERROR_BODY( 0, "invalid object", 0 );
				DAP_SEND();
				return;
			}

			SQClosure *pClosure = _closure(target);
			SQFunctionProto *func = _fp(pClosure->_function);

			DAP_START_RESPONSE( seq, "variables" );
			DAP_SET_TABLE( body, 1 );
				json_array_t &variables = body.SetArray( "variables", func->_nliterals );
				for ( int i = 0; i < func->_nliterals; i++ )
				{
					const SQObjectPtr &val = func->_literals[i];

					json_table_t &elem = variables.AppendTable(4);
					elem.SetIntString( "name", i );
					JSONSetString( elem, "value", val, flags );
					elem.SetString( "type", GetType( val ) );
					elem.SetInt( "variablesReference", ToVarRef( val ) );
				}
			DAP_SEND();

			break;
		}
		case VARREF_METAMETHODS:
		{
			SQObject target = ref->GetVar();

			if ( !ISREFCOUNTED( sq_type(target) ) )
			{
				DAP_ERROR_RESPONSE( seq, "variables" );
				DAP_ERROR_BODY( 0, "invalid object", 0 );
				DAP_SEND();
				return;
			}

			DAP_START_RESPONSE( seq, "variables" );
			DAP_SET_TABLE( body, 1 );
				json_array_t &variables = body.SetArray( "variables" );
				switch ( sq_type(target) )
				{
					case OT_INSTANCE: _class(target) = _instance(target)->_class;
					case OT_CLASS:
					{
						for ( unsigned int i = 0; i < MT_LAST; i++ )
						{
							const SQObjectPtr &val = _class(target)->_metamethods[i];
							if ( sq_type(val) != OT_NULL )
							{
								json_table_t &elem = variables.AppendTable(4);
								elem.SetString( "name", g_MetaMethodName[i] );
								elem.SetString( "value", GetValue( val ) );
								elem.SetString( "type", GetType( val ) );
								elem.SetInt( "variablesReference", ToVarRef( val ) );
							}
						}

						break;
					}
					default:
					{
						Assert( is_delegable(target) && _delegable(target)->_delegate );

						if ( is_delegable(target) && _delegable(target)->_delegate )
						{
							SQObjectPtr val;
							for ( unsigned int i = 0; i < MT_LAST; i++ )
							{
								if ( _delegable(target)->GetMetaMethod( m_pRootVM, (SQMetaMethod)i, val ) )
								{
									json_table_t &elem = variables.AppendTable(4);
									elem.SetString( "name", g_MetaMethodName[i] );
									elem.SetString( "value", GetValue( val ) );
									elem.SetString( "type", GetType( val ) );
									elem.SetInt( "variablesReference", ToVarRef( val ) );
								}
							}
						}

						break;
					}
				}
			DAP_SEND();

			break;
		}
		case VARREF_ATTRIBUTES:
		{
			SQObject target = ref->GetVar();
			bool shouldQuoteKeys = ref->obj.hasNonStringMembers;

			if ( sq_type(target) != OT_CLASS )
			{
				DAP_ERROR_RESPONSE( seq, "variables" );
				DAP_ERROR_BODY( 0, "invalid object", 0 );
				DAP_SEND();
				return;
			}

			int keyflags = flags | kFS_NoQuote | kFS_KeyVal;
			if ( shouldQuoteKeys )
				keyflags &= ~kFS_NoQuote;

			DAP_START_RESPONSE( seq, "variables" );
			DAP_SET_TABLE( body, 1 );
				json_array_t &variables = body.SetArray( "variables" );
				SQObjectPtr key, idx;
				FOREACH_SQTABLE( _class(target)->_members, key, idx )
				{
					const SQObjectPtr &val = _isfield(idx) ?
						_class(target)->_defaultvalues[ _member_idx(idx) ].attrs :
						_class(target)->_methods[ _member_idx(idx) ].attrs;

					if ( sq_type(val) != OT_NULL )
					{
						json_table_t &elem = variables.AppendTable(5);

						if ( shouldQuoteKeys && sq_type(key) == OT_INTEGER )
						{
							stringbuf_t< 32 > name;
							name.Put('[');
							name.PutInt( _integer(key) );
							name.Put(']');
							elem.SetString( "name", name );
						}
						else
						{
							JSONSetString( elem, "name", key, keyflags );
						}

						JSONSetString( elem, "value", val, flags );
						elem.SetString( "type", GetType( val ) );
						elem.SetInt( "variablesReference", ToVarRef( val ) );
						json_table_t &hint = elem.SetTable( "presentationHint", 1 );
						hint.SetString( "kind", GetPresentationHintKind( val ) );
					}
				}
			DAP_SEND();

			break;
		}
		case VARREF_CALLSTACK:
		{
			SQObject target = ref->GetVar();

			if ( sq_type(target) != OT_THREAD )
			{
				DAP_ERROR_RESPONSE( seq, "variables" );
				DAP_ERROR_BODY( 0, "invalid object", 0 );
				DAP_SEND();
				return;
			}

			DAP_START_RESPONSE( seq, "variables" );
			DAP_SET_TABLE( body, 1 );
				int i = _thread(target)->_callsstacksize;
				json_array_t &variables = body.SetArray( "variables", i );
				while ( i-- )
				{
					const SQVM::CallInfo &ci = _thread(target)->_callsstack[i];

					if ( ShouldIgnoreStackFrame(ci) )
						continue;

					stringbuf_t< 256 > buf;

					if ( sq_type(ci._closure) == OT_CLOSURE )
					{
						SQFunctionProto *func = _fp(_closure(ci._closure)->_function);

						if ( sq_type(func->_sourcename) == OT_STRING )
						{
							buf.Puts( _string(func->_sourcename) );
						}
						else
						{
							buf.Puts("??");
						}

						buf.Put(':');
						buf.PutInt( (int)func->GetLine( ci._ip ) );
					}
					else if ( sq_type(ci._closure) == OT_NATIVECLOSURE )
					{
						buf.Puts("NATIVE");
					}
					else UNREACHABLE();

					json_table_t &elem = variables.AppendTable(3);
					elem.SetIntString( "name", i );
					elem.SetString( "value", buf );
					elem.SetInt( "variablesReference", ToVarRef( ci._closure ) );
				}
			DAP_SEND();

			break;
		}
		case VARREF_STACK:
		{
			HSQUIRRELVM vm = ref->GetThread();
			int frame = ref->scope.frame;

			if ( !IsValidStackFrame( vm, frame ) )
				frame = -1;

			DAP_START_RESPONSE( seq, "variables" );
			DAP_SET_TABLE( body, 1 );
				json_array_t &variables = body.SetArray( "variables", vm->_top + 1 );

				int stackbase = -1;
				int callframe = 0;

				if ( vm->_callsstacksize )
					stackbase = vm->_callsstack[callframe]._prevstkbase;

				for ( int i = 0; i < (int)vm->_stack.size(); i++ )
				{
					const SQObjectPtr &val = vm->_stack._vals[i];

					if ( i > vm->_top && sq_type(val) == OT_NULL )
						continue;

					stringbuf_t< 32 > name;
					name.Put('[');
					name.PutInt( i );
					name.Put(']');

					if ( stackbase == i )
					{
						name.Put('*');

						if ( callframe == frame )
						{
							name.Put('~');
						}
#ifndef NATIVE_DEBUG_HOOK
						else if ( m_State == ThreadState_Suspended &&
								callframe == vm->_callsstacksize - 1 )
						{
							if ( sq_type(vm->_callsstack[callframe]._closure) == OT_NATIVECLOSURE &&
									_nativeclosure(vm->_callsstack[callframe]._closure)->_function ==
										&SQDebugServer::SQDebugHook )
							{
								name.Put('d');
							}
						}
#endif

						if ( ++callframe < vm->_callsstacksize )
							stackbase += vm->_callsstack[callframe]._prevstkbase;
					}
					else if ( vm->_top == i )
					{
						name.Put('_');
					}

					json_table_t &elem = variables.AppendTable(4);
					elem.SetString( "name", name );
					JSONSetString( elem, "value", val, flags );
					elem.SetString( "type", GetType( val ) );
					elem.SetInt( "variablesReference", ToVarRef( val ) );
				}
			DAP_SEND();

			break;
		}
		default: UNREACHABLE();
	}
}

//
// If the client supports SetExpression and the target variable has "evaluateName",
// it will send target "evaluateName" and stack frame to SetExpression.
// Client can choose to send variable "name" to SetExpression for watch variables.
// If the client doesn't support SetExpression or the target variable does not have "evaluateName",
// it will send target "name" and container "variableReference" to SetVariable.
//
// SetExpression needs to parse watch flags and "evaluateName",
// SetVariable only gets identifiers.
//
void SQDebugServer::OnRequest_SetVariable( const json_table_t &arguments, int seq )
{
	int variablesReference;
	string_t strName, strValue;

	arguments.GetInt( "variablesReference", &variablesReference );
	arguments.GetString( "name", &strName );
	arguments.GetString( "value", &strValue );

	bool hex = false;
	json_table_t *format;
	if ( arguments.GetTable( "format", &format ) )
		format->GetBool( "hex", &hex );

	varref_t *ref = FromVarRef( variablesReference );
	if ( !ref )
	{
		DAP_ERROR_RESPONSE( seq, "setVariable" );
		DAP_ERROR_BODY( 0, "failed to find variable", 0 );
		DAP_SEND();
		return;
	}

	HSQUIRRELVM vm;
	int frame;

	if ( IsScopeRef( ref->type ) )
	{
		vm = ref->GetThread();
		frame = ref->scope.frame;
	}
	else
	{
		vm = m_pCurVM;
		frame = -1;
	}

	if ( strName.IsEmpty() || strValue.IsEmpty() )
	{
		DAP_ERROR_RESPONSE( seq, "setVariable" );
		DAP_ERROR_BODY( 0, "empty expression", 0 );
		DAP_SEND();
		return;
	}

	switch ( ref->type )
	{
		// Requires special value parsing
		case VARREF_INSTRUCTIONS:
		{
#ifndef SQDBG_SUPPORTS_SET_INSTRUCTION
			DAP_ERROR_RESPONSE( seq, "setVariable" );
			DAP_ERROR_BODY( 0, "not supported", 0 );
			DAP_SEND();
#else
			if ( sq_type(ref->GetVar()) != OT_CLOSURE )
			{
				DAP_ERROR_RESPONSE( seq, "setVariable" );
				DAP_ERROR_BODY( 0, "invalid object", 0 );
				DAP_SEND();
				return;
			}

			int index, line;
			int op, arg0, arg1, arg2, arg3;

			int c1 = sscanf( strName.ptr, "%d:%d", &index, &line );
			int c2 = sscanf( strValue.ptr, "0x%02x %d %d %d %d", &op, &arg0, &arg1, &arg2, &arg3 );

			bool fail = ( c1 != 2 );

			if ( !fail && c2 != 5 )
			{
				// Check for floats
				if ( strchr( strValue.ptr, '.' ) )
				{
					float farg1;
					c2 = sscanf( strValue.ptr, "0x%02x %d %f %d %d", &op, &arg0, &farg1, &arg2, &arg3 );
					if ( c2 != 5 )
					{
						fail = true;
					}
					else
					{
						arg1 = *(int*)&farg1;

						if ( op != _OP_LOADFLOAT )
						{
							char buf[96];
							int len = snprintf( buf, sizeof(buf),
									"Warning: Setting float value (%.2f) to non-float instruction\n",
									farg1 );
							SendEvent_OutputStdOut( string_t( buf, min( len, (int)sizeof(buf) ) ), NULL );
						}
					}
				}
				else
				{
					fail = true;
				}
			}

			if ( fail )
			{
				DAP_ERROR_RESPONSE( seq, "setVariable" );
				DAP_ERROR_BODY( 0, "invalid amount of parameters in input", 1 );
					error.SetBool( "showUser", true );
				DAP_SEND();
				return;
			}

			SQFunctionProto *func = _fp(_closure(ref->GetVar())->_function);

			// line ops are ignored in the index
			for ( int c = 0; c < func->_ninstructions; c++ )
			{
				if ( func->_instructions[c].op == _OP_LINE )
					index++;

				if ( c == index )
					break;
			}

			// index will be wrong if user manualy set line ops
			if ( index >= func->_ninstructions )
			{
				DAP_ERROR_RESPONSE( seq, "setVariable" );
				DAP_ERROR_BODY( 0, "failed to set instruction", 1 );
					error.SetBool( "showUser", true );
				DAP_SEND();
				return;
			}

			SQInstruction *instr = func->_instructions + index;

			instr->op = op & 0xff;
			instr->_arg0 = arg0 & 0xff;
			instr->_arg1 = arg1;
			instr->_arg2 = arg2 & 0xff;
			instr->_arg3 = arg3 & 0xff;

			stringbuf_t< 32 > instrBytes;
			instrBytes.PutHex( instr->op ); instrBytes.Put(' ');
			instrBytes.PutInt( instr->_arg0 ); instrBytes.Put(' ');
			instrBytes.PutInt( instr->_arg1 ); instrBytes.Put(' ');
			instrBytes.PutInt( instr->_arg2 ); instrBytes.Put(' ');
			instrBytes.PutInt( instr->_arg3 );

			DAP_START_RESPONSE( seq, "setVariable" );
			DAP_SET_TABLE( body, 2 );
				body.SetStringNoCopy( "value", instrBytes );
				body.SetInt( "variablesReference", -1 );
			DAP_SEND();
#endif
			return;
		}
		default: break;
	}

	objref_t obj;
	SQObjectPtr value, dummy;

	if ( !GetObj_VarRef( strName, ref, obj, dummy ) )
	{
		DAP_ERROR_RESPONSE( seq, "setVariable" );
		DAP_ERROR_BODY( 0, "identifier '{name}' not found", 2 );
			json_table_t &variables = error.SetTable( "variables", 1 );

			// If the string is escapable, it was undone
			if ( ref->type == VARREF_OBJ && ref->obj.hasNonStringMembers && strName.ptr[-1] == '\"' )
			{
				// there is enough space to re-escape
				Escape( strName.ptr, &strName.len, strName.len * ( sizeof(SQChar) * 2 + 2 ) );
			}

			variables.SetStringNoCopy( "name", strName );
			error.SetBool( "showUser", true );
		DAP_SEND();
		return;
	}

#ifndef SQDBG_DISABLE_COMPILER
	ECompileReturnCode cres = Evaluate( strValue, vm, frame, value );
	if ( cres != CompileReturnCode_Success &&
			!( cres > CompileReturnCode_Fallback && RunExpression( strValue, vm, frame, value ) ) )
#else
	if ( !RunExpression( strValue, vm, frame, value ) )
#endif
	{
		DAP_ERROR_RESPONSE( seq, "setVariable" );
		DAP_ERROR_BODY( 0, "failed to evaluate value '{name}'\n\n[{reason}]", 2 );
			json_table_t &variables = error.SetTable( "variables", 2 );
			variables.SetStringNoCopy( "name", strValue );
			variables.SetStringNoCopy( "reason", GetValue( vm->_lasterror, kFS_NoQuote ) );
			error.SetBool( "showUser", true );
		DAP_SEND();
		return;
	}

	if ( !Set( obj, value ) )
	{
		DAP_ERROR_RESPONSE( seq, "setVariable" );
		DAP_ERROR_BODY( 0, "could not set '{name}'", 2 );
			json_table_t &variables = error.SetTable( "variables", 1 );
			variables.SetStringNoCopy( "name", GetValue( obj.key ) );
			error.SetBool( "showUser", true );
		DAP_SEND();
		return;
	}

	// Update data watch
	if ( IsObjectRef( ref->type ) && ref->obj.isWeak &&
			_refcounted(ref->GetVar())->_weakref )
	{
		for ( int i = m_DataWatches.size(); i--; )
		{
			datawatch_t &dw = m_DataWatches[i];
			if ( _refcounted(ref->GetVar())->_weakref == dw.container &&
					dw.name.IsEqualTo( strName ) )
			{
				dw.oldvalue = value;
				break;
			}
		}
	}

	DAP_START_RESPONSE( seq, "setVariable" );
	DAP_SET_TABLE( body, 3 );
		JSONSetString( body, "value", value, hex ? kFS_Hexadecimal : 0 );
		body.SetString( "type", GetType( value ) );
		body.SetInt( "variablesReference", ToVarRef( value ) );
	DAP_SEND();
}

void SQDebugServer::OnRequest_SetExpression( const json_table_t &arguments, int seq )
{
	HSQUIRRELVM vm;
	int frame;

	string_t expression, strValue;

	arguments.GetString( "expression", &expression );
	arguments.GetString( "value", &strValue );
	arguments.GetInt( "frameId", &frame, -1 );

	if ( expression.IsEmpty() || strValue.IsEmpty() )
	{
		DAP_ERROR_RESPONSE( seq, "evaluate" );
		DAP_ERROR_BODY( 0, "empty expression", 0 );
		DAP_SEND();
		return;
	}

	if ( !TranslateFrameID( frame, &vm, &frame ) )
	{
		vm = m_pCurVM;
		frame = -1;
	}

	int flags = ParseFormatSpecifiers( expression );
	{
		json_table_t *format;
		if ( arguments.GetTable( "format", &format ) )
		{
			bool hex;
			format->GetBool( "hex", &hex );
			if ( hex )
				flags |= kFS_Hexadecimal;
		}
	}

	SQObjectPtr value;

	// Evaluate value in current stack frame even if the expression has frame lock
#ifndef SQDBG_DISABLE_COMPILER
	ECompileReturnCode cres = Evaluate( strValue, vm, frame, value );
	if ( cres != CompileReturnCode_Success &&
			!( cres > CompileReturnCode_Fallback && RunExpression( strValue, vm, frame, value ) ) )
#else
	if ( !( ( flags & kFS_Binary ) && ParseBinaryNumber( strValue, value ) ) &&
			!RunExpression( strValue, vm, frame, value ) )
#endif
	{
		DAP_ERROR_RESPONSE( seq, "setExpression" );
		DAP_ERROR_BODY( 0, "failed to evaluate value '{name}'", 2 );
			json_table_t &variables = error.SetTable( "variables", 1 );
			variables.SetStringNoCopy( "name", strValue );
			error.SetBool( "showUser", true );
		DAP_SEND();
		return;
	}

	if ( flags & kFS_Lock )
	{
#ifdef _DEBUG
		bool foundWatch = false;
#endif
		for ( int i = 0; i < m_LockedWatches.size(); i++ )
		{
			const watch_t &w = m_LockedWatches[i];
			if ( w.expression.IsEqualTo( expression ) )
			{
				vm = GetThread( w.thread );
				frame = w.frame;
#ifdef _DEBUG
				foundWatch = true;
#endif
				break;
			}
		}

		Assert( foundWatch );
	}

	objref_t obj;
	obj.type = objref_t::INVALID;

	// Try to get identifier
	if ( ShouldParseEvaluateName( expression ) )
	{
		SQObjectPtr dummy;
		if ( !ParseEvaluateName( expression, vm, frame, obj, dummy ) )
		{
			DAP_ERROR_RESPONSE( seq, "setExpression" );
			DAP_ERROR_BODY( 0, "invalid variable reference '{name}'", 2 );
				json_table_t &variables = error.SetTable( "variables", 1 );
				variables.SetStringNoCopy( "name", expression );
				error.SetBool( "showUser", true );
			DAP_SEND();
			return;
		}
	}
	else
	{
		SQObjectPtr dummy;
		GetObj_Frame( expression, vm, frame, obj, dummy );
	}

	// Found identifier
	if ( obj.type != objref_t::INVALID )
	{
		if ( Set( obj, value ) )
		{
			DAP_START_RESPONSE( seq, "setExpression" );
			DAP_SET_TABLE( body, 3 );
				JSONSetString( body, "value", value, flags );
				body.SetString( "type", GetType( value ) );
				body.SetInt( "variablesReference", ToVarRef( value ) );
			DAP_SEND();
		}
		else
		{
			DAP_ERROR_RESPONSE( seq, "setExpression" );
			DAP_ERROR_BODY( 0, "could not set '{name}'", 2 );
				json_table_t &variables = error.SetTable( "variables", 1 );
				variables.SetStringNoCopy( "name", GetValue( obj.key ) );
				error.SetBool( "showUser", true );
			DAP_SEND();
			return;
		}
	}
	// No identifier, run the script ( exp = val )
	else
	{
		stringbuf_t< 256 > buf;
		int len = expression.len + 1;

#ifndef SQDBG_DISABLE_COMPILER
		len += strValue.len;
#else
		// Using sq compiler
		// If value was binary literal, put int
		if ( !( flags & kFS_Binary ) )
		{
			len += strValue.len;
		}
		else
		{
			len += 2 + countdigits<16>( (SQUnsignedInteger)_integer(value) );
		}
#endif

		if ( len < (int)sizeof(buf.ptr) )
		{
			buf.Puts( expression );
			buf.Put('=');

#ifndef SQDBG_DISABLE_COMPILER
			buf.Puts( strValue );
#else
			if ( !( flags & kFS_Binary ) )
			{
				buf.Puts( strValue );
			}
			else
			{
				buf.PutHex( (SQUnsignedInteger)_integer(value), false );
			}

			buf.Term();
#endif

#ifndef SQDBG_DISABLE_COMPILER
			string_t expr;
			expr.Assign( buf.ptr, buf.len );

			ECompileReturnCode cres = Evaluate( expr, vm, frame, value );
			if ( cres == CompileReturnCode_Success ||
					( cres > CompileReturnCode_Fallback && RunExpression( buf, vm, frame, value ) ) )
#else
			if ( RunExpression( buf, vm, frame, value ) )
#endif
			{
				DAP_START_RESPONSE( seq, "setExpression" );
				DAP_SET_TABLE( body, 3 );
					JSONSetString( body, "value", value, flags );
					body.SetString( "type", GetType( value ) );
					body.SetInt( "variablesReference", ToVarRef( value ) );
				DAP_SEND();
			}
			else
			{
				DAP_ERROR_RESPONSE( seq, "setExpression" );
				DAP_ERROR_BODY( 0, "failed to evaluate expression: {exp} = {val}\n\n[{reason}]", 2 );
					json_table_t &variables = error.SetTable( "variables", 3 );
					variables.SetStringNoCopy( "exp", expression );
					variables.SetStringNoCopy( "val", strValue );
					variables.SetStringNoCopy( "reason", GetValue( vm->_lasterror, kFS_NoQuote ) );
					error.SetBool( "showUser", true );
				DAP_SEND();
				return;
			}
		}
		else
		{
			DAP_ERROR_RESPONSE( seq, "setExpression" );
			DAP_ERROR_BODY( 0, "expression is too long to evaluate: {exp} = {val}", 2 );
				json_table_t &variables = error.SetTable( "variables", 2 );
				variables.SetStringNoCopy( "exp", expression );
				variables.SetStringNoCopy( "val", strValue );
				error.SetBool( "showUser", true );
			DAP_SEND();
			return;
		}
	}

	// Update data watch
	for ( int i = m_DataWatches.size(); i--; )
	{
		datawatch_t &dw = m_DataWatches[i];

		if ( sq_type(dw.container->_obj) == OT_NULL )
		{
			FreeDataWatch( dw );
			m_DataWatches.remove(i);
			continue;
		}

		if ( Get( dw.obj, value ) &&
				_rawval(dw.oldvalue) != _rawval(value) )
		{
			dw.oldvalue = value;
		}
	}
}

void SQDebugServer::OnRequest_Disassemble( const json_table_t &arguments, int seq )
{
	string_t memoryReference;
	int instructionOffset, instructionCount;

	arguments.GetString( "memoryReference", &memoryReference );
	arguments.GetInt( "instructionOffset", &instructionOffset );
	arguments.GetInt( "instructionCount", &instructionCount );

	SQFunctionProto *func = NULL;
	int instrIdx = -1;

	SQInstruction *ip;
	atox( memoryReference, (SQUnsignedInteger*)&ip );

	for ( int i = m_pCurVM->_callsstacksize; i--; )
	{
		const SQVM::CallInfo &ci = m_pCurVM->_callsstack[i];
		if ( sq_type(ci._closure) == OT_CLOSURE )
		{
			func = _fp(_closure(ci._closure)->_function);
			if ( ip >= func->_instructions &&
					ip < func->_instructions + func->_ninstructions )
			{
				instrIdx = ci._ip - func->_instructions;
				break;
			}
		}
	}

	if ( instrIdx == -1 )
	{
		DAP_ERROR_RESPONSE( seq, "disassemble" );
		DAP_ERROR_BODY( 0, "invalid instruction pointer", 0 );
		DAP_SEND();
		return;
	}

	RestoreCachedInstructions();

	int targetStart = instrIdx + instructionOffset;
	int targetEnd = targetStart + instructionCount;

	int validStart = max( 0, targetStart );
	int validEnd = min( func->_ninstructions - 1, targetEnd );

	json_array_t instructions( instructionCount );

	DAP_START_RESPONSE( seq, "disassemble" );
	DAP_SET_TABLE( body, 1 );
		body.SetArray( "instructions", instructions );

		for ( int index = targetStart; index < targetEnd; index++ )
		{
			json_table_t &elem = instructions.AppendTable(6);

			SQInstruction *instr = func->_instructions + index;

			stringbuf_t< FMT_PTR_LEN > addr;
			addr.PutHex( (SQUnsignedInteger)instr );
			elem.SetString( "address", addr );

			if ( index >= validStart && index <= validEnd )
			{
				if ( instr->op != _OP_LINE )
				{
					{
						stringbuf_t< 128 > instrStr;
						DescribeInstruction( instr, func, instrStr );
						elem.SetString( "instruction", instrStr );
					}
					{
						stringbuf_t< 32 > instrBytes;
						instrBytes.PutHex( instr->op ); instrBytes.Put(' ');
						instrBytes.PutInt( instr->_arg0 ); instrBytes.Put(' ');
						instrBytes.PutInt( instr->_arg1 ); instrBytes.Put(' ');
						instrBytes.PutInt( instr->_arg2 ); instrBytes.Put(' ');
						instrBytes.PutInt( instr->_arg3 );
						elem.SetString( "instructionBytes", instrBytes );
					}

					elem.SetInt( "line", (int)func->GetLine( instr ) );
				}
				else
				{
					elem.SetString( "instruction", "" );
				}

				elem.SetString( "presentationHint", "normal" );
			}
			else
			{
				elem.SetString( "instruction", "??" );
				elem.SetString( "presentationHint", "invalid" );
			}
		}

		if ( instructions.size() )
		{
			json_table_t &elem = instructions.Get(0)->AsTable();
			json_table_t &source = elem.SetTable( "location", 2 );
			SetSource( source, _string(func->_sourcename) );
		}

		Assert( instructions.size() == instructionCount );
	DAP_SEND();

	UndoRestoreCachedInstructions();
}

#ifdef SUPPORTS_RESTART_FRAME
void SQDebugServer::OnRequest_RestartFrame( const json_table_t &arguments, int seq )
{
	Assert( m_State == ThreadState_Suspended );

	HSQUIRRELVM vm;
	int frame;
	arguments.GetInt( "frameId", &frame, -1 );

	if ( !TranslateFrameID( frame, &vm, &frame ) || !vm->ci )
	{
		DAP_ERROR_RESPONSE( seq, "restartFrame" );
		DAP_ERROR_BODY( 0, "invalid stack frame {id}", 1 );
			json_table_t &variables = error.SetTable( "variables", 1 );
			variables.SetIntString( "id", frame );
		DAP_SEND();
		return;
	}

	if ( vm != m_pCurVM )
	{
		DAP_ERROR_RESPONSE( seq, "restartFrame" );
		DAP_ERROR_BODY( 0, "cannot restart frame on a different thread", 0 );
		DAP_SEND();
		return;
	}

	const SQVM::CallInfo *pCurrent = vm->ci;
	const SQVM::CallInfo *pTarget = vm->_callsstack + frame;

	for ( const SQVM::CallInfo *f = pCurrent; f >= pTarget; f-- )
	{
		if ( sq_type(f->_closure) != OT_CLOSURE )
		{
			DAP_ERROR_RESPONSE( seq, "restartFrame" );
			DAP_ERROR_BODY( 0, "cannot restart native call frame", 0 );
			DAP_SEND();
			return;
		}
	}

	while ( pCurrent > pTarget )
	{
		vm->LeaveFrame();
		pCurrent = vm->ci;
	}

	Assert( sq_type(vm->ci->_closure) == OT_CLOSURE );

	SQFunctionProto *func = _fp(_closure(vm->ci->_closure)->_function);

	vm->ci->_ip = func->_instructions;

	int top = vm->_top;
	int target = top - func->_stacksize + func->_nparameters;

	while ( top --> target )
	{
		vm->_stack._vals[top].Null();
	}

	DAP_START_RESPONSE( seq, "restartFrame" );
	DAP_SEND();

	Break( vm, breakreason_t::Restart );
}
#endif

static inline int GetOpAtLine( SQFunctionProto *func, int line )
{
	for ( int i = 0; i < (int)func->_nlineinfos; i++ )
	{
		const SQLineInfo &li = func->_lineinfos[i];
		if ( line <= (int)li._line )
			return li._op;
	}

	return -1;
}

static inline int GetOpAtNextLine( SQFunctionProto *func, int curop )
{
	Assert( func->_nlineinfos > 0 );

	int hi = func->_nlineinfos - 1;
	int lo = 0;
	int mid = 0;

	while ( lo <= hi )
	{
		mid = lo + ( ( hi - lo ) >> 1 );

		int op = func->_lineinfos[mid]._op;

		if ( curop > op )
		{
			lo = mid + 1;
		}
		else if ( curop < op )
		{
			hi = mid - 1;
		}
		else
		{
			break;
		}
	}

	while ( mid > 0 && func->_lineinfos[mid]._op > curop )
		mid--;

	while ( ++mid < (int)func->_nlineinfos )
	{
		int op = func->_lineinfos[mid]._op;
		if ( op != 0 && op != curop )
			return op;
	}

	return -1;
}

//
// Only allow goto if the requested source,line matches current executing function
//
void SQDebugServer::OnRequest_GotoTargets( const json_table_t &arguments, int seq )
{
	if ( m_State != ThreadState_Suspended )
	{
		DAP_ERROR_RESPONSE( seq, "gotoTargets" );
		DAP_ERROR_BODY( 0, "thread is not suspended", 0 );
		DAP_SEND();
		return;
	}

	json_table_t *source;
	string_t srcname;
	int line;
	arguments.GetInt( "line", &line );

	GET_OR_ERROR_RESPONSE( "gotoTargets", arguments, source );

	if ( ( !source->GetString( "name", &srcname ) || srcname.IsEmpty() ) &&
			source->GetString( "path", &srcname ) )
	{
		StripFileName( &srcname.ptr, &srcname.len );
	}

	if ( !m_pCurVM->ci )
	{
		DAP_ERROR_RESPONSE( seq, "gotoTargets" );
		DAP_ERROR_BODY( 0, "thread is not in execution", 0 );
		DAP_SEND();
		return;
	}

#ifdef NATIVE_DEBUG_HOOK
	const SQVM::CallInfo *ci = m_pCurVM->ci;
#else
	const SQVM::CallInfo *ci = m_pCurVM->ci - 1;
#endif

	if ( sq_type(ci->_closure) != OT_CLOSURE )
	{
		DAP_ERROR_RESPONSE( seq, "gotoTargets" );
		DAP_ERROR_BODY( 0, "goto on native call frame", 0 );
		DAP_SEND();
		return;
	}

	SQFunctionProto *func = _fp(_closure(ci->_closure)->_function);

	if ( func->_nlineinfos == 0 )
	{
		DAP_ERROR_RESPONSE( seq, "gotoTargets" );
		DAP_ERROR_BODY( 0, "no lineinfos", 0 );
		DAP_SEND();
		return;
	}

	if ( sq_type(func->_sourcename) == OT_STRING )
	{
		sqstring_t wfuncsrc( _string(func->_sourcename) );

#ifdef SQDBG_SOURCENAME_HAS_PATH
		StripFileName( &wfuncsrc.ptr, &wfuncsrc.len );
#endif

		stringbuf_t< 256 > funcsrc;
		funcsrc.Puts( wfuncsrc );

		if ( !srcname.IsEqualTo( funcsrc ) )
		{
			DAP_ERROR_RESPONSE( seq, "gotoTargets" );
			DAP_ERROR_BODY( 0, "requested source '{src1}' does not match executing function '{src2}'", 2 );
				json_table_t &variables = error.SetTable( "variables", 2 );
				variables.SetStringNoCopy( "src1", srcname );
				variables.SetStringNoCopy( "src2", funcsrc );
				error.SetBool( "showUser", true );
			DAP_SEND();
			return;
		}
	}

	if ( line < 0 || line > (int)func->_lineinfos[func->_nlineinfos-1]._line )
	{
		DAP_ERROR_RESPONSE( seq, "gotoTargets" );
		DAP_ERROR_BODY( 0, "line {line} is out of range", 2 );
			json_table_t &variables = error.SetTable( "variables", 1 );
			variables.SetIntString( "line", line );
			error.SetBool( "showUser", true );
		DAP_SEND();
		return;
	}

	int op = GetOpAtLine( func, line );

	if ( op == -1 )
	{
		DAP_ERROR_RESPONSE( seq, "gotoTargets" );
		DAP_ERROR_BODY( 0, "could not find line {line} in function", 2 );
			json_table_t &variables = error.SetTable( "variables", 1 );
			variables.SetIntString( "line", line );
			error.SetBool( "showUser", true );
		DAP_SEND();
		return;
	}

	DAP_START_RESPONSE( seq, "gotoTargets" );
	DAP_SET_TABLE( body, 1 );
		json_array_t &targets = body.SetArray( "targets", 1 );
			json_table_t &elem = targets.AppendTable(3);
			stringbuf_t< 1 + FMT_INT_LEN > buf;
			buf.Put('L');
			buf.PutInt( line );
			elem.SetStringNoCopy( "label", buf );
			elem.SetInt( "line", line );
			elem.SetInt( "id", line );
	DAP_SEND();
}

void SQDebugServer::OnRequest_Goto( const json_table_t &arguments, int seq )
{
	if ( m_State != ThreadState_Suspended )
	{
		DAP_ERROR_RESPONSE( seq, "goto" );
		DAP_ERROR_BODY( 0, "thread is not suspended", 0 );
		DAP_SEND();
		return;
	}

	int threadId, line;
	arguments.GetInt( "threadId", &threadId, -1 );
	arguments.GetInt( "targetId", &line );

	HSQUIRRELVM vm = ThreadFromID( threadId );

	if ( vm != m_pCurVM )
	{
		DAP_ERROR_RESPONSE( seq, "goto" );
		DAP_ERROR_BODY( 0, "cannot change execution on a different thread", 0 );
		DAP_SEND();
		return;
	}

	if ( !vm->ci )
	{
		DAP_ERROR_RESPONSE( seq, "goto" );
		DAP_ERROR_BODY( 0, "thread is not in execution", 0 );
		DAP_SEND();
		return;
	}

#ifdef NATIVE_DEBUG_HOOK
	SQVM::CallInfo *ci = vm->ci;
#else
	SQVM::CallInfo *ci = vm->ci - 1;
#endif

	if ( sq_type(ci->_closure) != OT_CLOSURE )
	{
		DAP_ERROR_RESPONSE( seq, "goto" );
		DAP_ERROR_BODY( 0, "goto on native call frame", 0 );
		DAP_SEND();
		return;
	}

	SQFunctionProto *func = _fp(_closure(ci->_closure)->_function);

	if ( func->_nlineinfos == 0 )
	{
		DAP_ERROR_RESPONSE( seq, "goto" );
		DAP_ERROR_BODY( 0, "no lineinfos", 0 );
		DAP_SEND();
		return;
	}

	if ( line < 0 || line > (int)func->_lineinfos[func->_nlineinfos-1]._line )
	{
		DAP_ERROR_RESPONSE( seq, "goto" );
		DAP_ERROR_BODY( 0, "line {line} is out of range", 2 );
			json_table_t &variables = error.SetTable( "variables", 1 );
			variables.SetIntString( "line", line );
			error.SetBool( "showUser", true );
		DAP_SEND();
		return;
	}

	int op = GetOpAtLine( func, line );

	if ( op == -1 )
	{
		DAP_ERROR_RESPONSE( seq, "goto" );
		DAP_ERROR_BODY( 0, "could not find line {line} in function", 2 );
			json_table_t &variables = error.SetTable( "variables", 1 );
			variables.SetIntString( "line", line );
			error.SetBool( "showUser", true );
		DAP_SEND();
		return;
	}

	ci->_ip = func->_instructions + op;

	while ( ci->_ip->op == _OP_LINE && ci->_ip + 1 < func->_instructions + func->_ninstructions )
		ci->_ip++;

	DAP_START_RESPONSE( seq, "goto" );
	DAP_SEND();

	Break( vm, breakreason_t::Goto );
}

void SQDebugServer::OnRequest_Next( const json_table_t &arguments, int seq )
{
	if ( m_State == ThreadState_Running )
	{
		DAP_START_RESPONSE( seq, "next" );
		DAP_SEND();
		return;
	}

	int threadId;
	arguments.GetInt( "threadId", &threadId, -1 );

	HSQUIRRELVM vm = ThreadFromID( threadId );

	if ( vm != m_pCurVM )
	{
		DAP_START_RESPONSE( seq, "next" );
		DAP_SEND();
		return;
	}

	if ( !vm->ci )
	{
		DAP_ERROR_RESPONSE( seq, "next" );
		DAP_ERROR_BODY( 0, "thread is not in execution", 0 );
		DAP_SEND();
		return;
	}

#ifdef NATIVE_DEBUG_HOOK
	SQVM::CallInfo *ci = vm->ci;
#else
	SQVM::CallInfo *ci = vm->ci - 1;
#endif

	string_t granularity;
	arguments.GetString( "granularity", &granularity );

	if ( ( granularity.IsEqualTo( "instruction" ) && InstructionStep( vm, ci ) )
			|| Step( vm, ci ) )
	{
		m_State = ThreadState_StepOverInstruction;
	}
	else
	{
		m_State = ThreadState_StepOver;
	}

	m_nStateCalls = m_nCalls;

	DAP_START_RESPONSE( seq, "next" );
	DAP_SEND();
}

void SQDebugServer::OnRequest_StepIn( const json_table_t &arguments, int seq )
{
	if ( m_State == ThreadState_Running )
	{
		DAP_START_RESPONSE( seq, "stepIn" );
		DAP_SEND();
		return;
	}

	int threadId;
	arguments.GetInt( "threadId", &threadId, -1 );

	HSQUIRRELVM vm = ThreadFromID( threadId );

	if ( vm != m_pCurVM )
	{
		DAP_START_RESPONSE( seq, "stepIn" );
		DAP_SEND();
		return;
	}

	if ( !vm->ci )
	{
		DAP_ERROR_RESPONSE( seq, "stepIn" );
		DAP_ERROR_BODY( 0, "thread is not in execution", 0 );
		DAP_SEND();
		return;
	}

#ifdef NATIVE_DEBUG_HOOK
	SQVM::CallInfo *ci = vm->ci;
#else
	SQVM::CallInfo *ci = vm->ci - 1;
#endif

	string_t granularity;
	arguments.GetString( "granularity", &granularity );

	if ( ( granularity.IsEqualTo( "instruction" ) && InstructionStep( vm, ci ) )
			|| Step( vm, ci ) )
	{
		m_State = ThreadState_StepInInstruction;
	}
	else
	{
		m_State = ThreadState_StepIn;
	}

	DAP_START_RESPONSE( seq, "stepIn" );
	DAP_SEND();
}

void SQDebugServer::OnRequest_StepOut( const json_table_t &arguments, int seq )
{
	if ( m_State == ThreadState_Running )
	{
		DAP_START_RESPONSE( seq, "stepOut" );
		DAP_SEND();
		return;
	}

	int threadId;
	arguments.GetInt( "threadId", &threadId, -1 );

	HSQUIRRELVM vm = ThreadFromID( threadId );

	if ( vm != m_pCurVM )
	{
		DAP_START_RESPONSE( seq, "stepOut" );
		DAP_SEND();
		return;
	}

	if ( !vm->ci )
	{
		DAP_ERROR_RESPONSE( seq, "stepOut" );
		DAP_ERROR_BODY( 0, "thread is not in execution", 0 );
		DAP_SEND();
		return;
	}

#ifdef NATIVE_DEBUG_HOOK
	SQVM::CallInfo *ci = vm->ci;
#else
	SQVM::CallInfo *ci = vm->ci - 1;
#endif

	string_t granularity;
	arguments.GetString( "granularity", &granularity );

	if ( ( granularity.IsEqualTo( "instruction" ) &&
				ci != vm->_callsstack && InstructionStep( vm, ci - 1, 1 ) )
			|| ( ci != vm->_callsstack && Step( vm, ci - 1 ) ) )
	{
		m_State = ThreadState_StepOutInstruction;
	}
	else
	{
		m_State = ThreadState_StepOut;
	}

	m_nStateCalls = m_nCalls;
	m_pStateVM = vm;

	DAP_START_RESPONSE( seq, "stepOut" );
	DAP_SEND();
}

bool SQDebugServer::InstructionStep( HSQUIRRELVM vm, SQVM::CallInfo *ci, int instroffset )
{
	Assert( m_State == ThreadState_Suspended ||
			m_State == ThreadState_StepOverInstruction ||
			m_State == ThreadState_StepInInstruction ||
			m_State == ThreadState_StepOutInstruction ||
			m_pPausedThread == vm );

	Assert( ci >= vm->_callsstack && ci < vm->_callsstack + vm->_callsstacksize );

	while ( sq_type(ci->_closure) != OT_CLOSURE && ci > vm->_callsstack )
		--ci;

	Assert( ci >= vm->_callsstack );

	if ( sq_type(ci->_closure) != OT_CLOSURE )
		return false;

	RestoreCachedInstructions();
	ClearCachedInstructions();

	SQFunctionProto *func = _fp(_closure(ci->_closure)->_function);
	SQInstruction *instrEnd = func->_instructions + func->_ninstructions;
	SQInstruction *ip = ci->_ip - instroffset;

	Assert( ip >= func->_instructions && ip < instrEnd );

	{
		for (;;)
		{
			++ip;

			if ( ip >= instrEnd )
			{
				// Step out
				if ( ci != vm->_callsstack )
					return InstructionStep( vm, ci - 1, 1 );

				return false;
			}

			if ( ip->op != _OP_LINE )
				break;
		}

		if ( ci->_etraps > 0 && ip->op == _OP_POPTRAP )
		{
			SQInstruction *trapIp = vm->_etraps.top()._ip;

			if ( trapIp > func->_instructions && trapIp < instrEnd )
			{
				for ( ; trapIp < instrEnd; trapIp++ )
				{
					if ( trapIp->op != _OP_LINE )
					{
						CacheInstruction( trapIp );
						break;
					}
				}
			}
		}

		CacheInstruction( ip );
	}

	ip = ci->_ip - instroffset;

	// Set break point at jump target as well
	// A simple alternative to evaluating the jump op
	if ( IsJumpOp( ip ) && GetJumpCount( ip ) != 0 )
	{
		if ( ip->op == _OP_FOREACH )
		{
			for ( SQInstruction *p = ip + 1; p < instrEnd; p++ )
			{
				if ( p->op != _OP_LINE )
				{
					CacheInstruction( p );
					break;
				}
			}
		}

		ip += GetJumpCount( ip );

		for (;;)
		{
			++ip;

			if ( ip >= instrEnd )
			{
				// Step out
				if ( ci != vm->_callsstack )
					return InstructionStep( vm, ci - 1, 1 );

				RestoreCachedInstructions();
				ClearCachedInstructions();
				return false;
			}

			if ( ip->op != _OP_LINE )
				break;
		}

		if ( ci->_etraps > 0 && ip->op == _OP_POPTRAP )
		{
			SQInstruction *trapIp = vm->_etraps.top()._ip;

			if ( trapIp > func->_instructions && trapIp < instrEnd )
			{
				for ( ; trapIp < instrEnd; trapIp++ )
				{
					if ( trapIp->op != _OP_LINE )
					{
						CacheInstruction( trapIp );
						break;
					}
				}
			}
		}

		CacheInstruction( ip );
	}

	return true;
}

bool SQDebugServer::Step( HSQUIRRELVM vm, SQVM::CallInfo *ci )
{
	Assert( m_State == ThreadState_Suspended ||
			m_State == ThreadState_StepOverInstruction ||
			m_State == ThreadState_StepInInstruction ||
			m_State == ThreadState_StepOutInstruction ||
			m_State == ThreadState_StepOver ||
			m_State == ThreadState_StepIn ||
			m_State == ThreadState_StepOut ||
			m_pPausedThread == vm );

	Assert( ci >= vm->_callsstack && ci < vm->_callsstack + vm->_callsstacksize );

	while ( sq_type(ci->_closure) != OT_CLOSURE && ci > vm->_callsstack )
		--ci;

	Assert( ci >= vm->_callsstack );

	if ( sq_type(ci->_closure) != OT_CLOSURE )
		return false;

	RestoreCachedInstructions();
	ClearCachedInstructions();

	SQFunctionProto *func = _fp(_closure(ci->_closure)->_function);

	// Check if this function was compiled with debug info
	// The first op is going to be a line op
	if ( func->_instructions->op == _OP_LINE )
	{
		Assert( func->_instructions->_arg1 != 0 );
		return false;
	}

	int op = GetOpAtNextLine( func, ci->_ip - func->_instructions );

	if ( op == -1 )
	{
		// Step out
		if ( ci != vm->_callsstack )
			return Step( vm, ci - 1 );

		return false;
	}

	Assert( op < func->_ninstructions );
	Assert( func->_instructions + op != ci->_ip );

	CacheInstruction( func->_instructions + op );

	// Set break point at every possible jump target
	for ( SQInstruction *ip = ci->_ip + 1; ip <= func->_instructions + op; ip++ )
	{
		if ( IsJumpOp( ip ) && GetJumpCount( ip ) != 0 )
		{
			if ( ip->op == _OP_FOREACH )
				CacheInstruction( ip + 1 );

			CacheInstruction( ip + GetJumpCount( ip ) + 1 );
		}
	}

	return true;
}

void SQDebugServer::CacheInstruction( SQInstruction *instr )
{
	// A way to keep a weak ref to this pointer would be keeping SQFunctionProto as SQWeakRef
	// This would only work in SQ3 because SQFunctionProto is ref counted since then
	cachedinstr_t &cached = m_CachedInstructions.append();
	cached.ip = instr;
	cached.instr = *cached.ip;
	memzero( cached.ip );
	Assert( cached.ip->op == _OP_LINE );
}

void SQDebugServer::ClearCachedInstructions()
{
	m_CachedInstructions.clear();
}

void SQDebugServer::RestoreCachedInstructions()
{
	for ( int i = m_CachedInstructions.size(); i--; )
	{
		cachedinstr_t &cached = m_CachedInstructions[i];
		if ( cached.ip )
			*cached.ip = cached.instr;
	}
}

void SQDebugServer::UndoRestoreCachedInstructions()
{
	for ( int i = m_CachedInstructions.size(); i--; )
	{
		cachedinstr_t &cached = m_CachedInstructions[i];
		if ( cached.ip )
			memzero( cached.ip );
	}
}

int SQDebugServer::ToVarRef( EVARREF type, HSQUIRRELVM vm, int frame )
{
	Assert( IsScopeRef( type ) );

	SQWeakRef *thread = GetWeakRef( vm );

	for ( int i = 0; i < m_Vars.size(); i++ )
	{
		varref_t &v = m_Vars[i];
		if ( v.type == type && v.scope.frame == frame && v.scope.thread == thread )
			return v.id;
	}

	varref_t &var = m_Vars.append();
	var.id = ++m_nVarRefIndex;
	var.type = type;
	var.scope.frame = frame;
	var.scope.thread = thread;

	return var.id;
}

void SQDebugServer::ConvertToWeakRef( varref_t &v )
{
	Assert( IsObjectRef( v.type ) );

	if ( !v.obj.isWeak )
	{
		v.obj.weakref = GetWeakRef( _refcounted(v.obj.obj), sq_type(v.obj.obj) );
		__ObjAddRef( v.obj.weakref );
		v.obj.isWeak = true;

		if ( v.obj.isStrong )
		{
			__ObjRelease( _refcounted(v.obj.obj) );
			v.obj.isStrong = false;
		}
	}
}

int SQDebugServer::ToVarRef( EVARREF type, const SQObject &obj, bool isWeak, bool isStrong )
{
	Assert( IsObjectRef( type ) );
	Assert( ( !isWeak && !isStrong ) || ( isWeak != isStrong ) );

	if ( !ISREFCOUNTED( sq_type(obj) ) )
		return INVALID_ID;

	if ( sq_type(obj) == OT_WEAKREF && isWeak )
	{
		if ( sq_type(_weakref(obj)->_obj) == OT_NULL )
			return INVALID_ID;
	}

	for ( int i = m_Vars.size(); i--; )
	{
		varref_t &v = m_Vars[i];
		if ( v.type == type && _rawval(v.GetVar()) == _rawval(obj) )
		{
			if ( isWeak )
				ConvertToWeakRef( v );

			return v.id;
		}
	}

	varref_t *var = &m_Vars.append();
	var->id = ++m_nVarRefIndex;
	var->type = type;
	var->obj.isWeak = isWeak;
	var->obj.isStrong = isStrong;

	if ( isWeak )
	{
		var->obj.weakref = GetWeakRef( _refcounted(obj), sq_type(obj) );
		__ObjAddRef( var->obj.weakref );

		Assert( sq_type(var->obj.weakref->_obj) != OT_NULL );
	}
	else
	{
		var->obj.obj = obj;

		if ( isStrong )
		{
			__ObjAddRef( _refcounted(var->obj.obj) );
		}
	}

	return var->id;
}

varref_t *SQDebugServer::FromVarRef( int id )
{
	int hi = m_Vars.size() - 1;
	int lo = 0;

	while ( lo <= hi )
	{
		int mid = lo + ( ( hi - lo ) >> 1 );

		varref_t *var = &m_Vars[mid];

		if ( id > var->id )
		{
			lo = mid + 1;
		}
		else if ( id < var->id )
		{
			hi = mid - 1;
		}
		else
		{
			Assert( var->type >= 0 && var->type < VARREF_MAX );

			if ( IsScopeRef( var->type ) ||
					( !var->obj.isWeak || sq_type(var->GetVar()) != OT_NULL ) )
				return var;

			Assert( var->obj.isWeak );
			Assert( var->obj.weakref );

			__ObjRelease( var->obj.weakref );
			m_Vars.remove(mid);

			return NULL;
		}
	}

	return NULL;
}

void SQDebugServer::RemoveVarRefs( bool all )
{
	if ( !all )
	{
		for ( int i = m_Vars.size(); i--; )
		{
			varref_t &v = m_Vars[i];

			// Keep living weakrefs, client might refer to them later
			if ( IsScopeRef( v.type ) )
			{
				m_Vars.remove(i);
			}
			else if ( IsObjectRef( v.type ) && !v.obj.isWeak )
			{
				if ( v.obj.isStrong )
				{
					Assert( ISREFCOUNTED( sq_type(v.obj.obj) ) );
					__ObjRelease( _refcounted(v.obj.obj) );
				}

				m_Vars.remove(i);
			}
			else
			{
				Assert( v.obj.isWeak );
				Assert( v.obj.weakref );

				if ( sq_type(v.obj.weakref->_obj) == OT_NULL )
				{
					__ObjRelease( v.obj.weakref );
					m_Vars.remove(i);
				}
			}
		}
	}
	else
	{
		for ( int i = m_Vars.size(); i--; )
		{
			varref_t &v = m_Vars[i];

			// Release all refs the debugger is holding
			if ( IsObjectRef( v.type ) )
			{
				Assert( ( !v.obj.isWeak && !v.obj.isStrong ) || ( v.obj.isWeak != v.obj.isStrong ) );

				if ( v.obj.isWeak )
				{
					Assert( v.obj.weakref );
					__ObjRelease( v.obj.weakref );
				}
				else if ( v.obj.isStrong )
				{
					Assert( ISREFCOUNTED( sq_type(v.obj.obj) ) );
					__ObjRelease( _refcounted(v.obj.obj) );
				}
			}
		}

		m_Vars.purge();
	}
}

void SQDebugServer::RemoveLockedWatches()
{
	for ( int i = 0; i < m_LockedWatches.size(); i++ )
		FreeString( &m_LockedWatches[i].expression );

	m_LockedWatches.purge();
}

int SQDebugServer::AddBreakpoint( int line, const string_t &src,
		const string_t &condition, int hitsTarget, const string_t &logMessage )
{
	Assert( line > 0 && !src.IsEmpty() );

#ifdef SQUNICODE
	Assert( SQUnicodeLength( src.ptr, src.len ) <= 256 );

	SQChar pSrc[256];
	sqstring_t wsrc( pSrc, UTF8ToSQUnicode( pSrc, sizeof(pSrc), src.ptr, src.len ) );

	breakpoint_t *bp = GetBreakpoint( line, wsrc );
#else
	breakpoint_t *bp = GetBreakpoint( line, src );
#endif

	if ( bp )
	{
		m_pCurVM->_lasterror = CreateSQString( m_pCurVM, _SC("duplicate breakpoint") );
		return INVALID_ID;
	}

	SQObjectPtr condFn;

	if ( !condition.IsEmpty() && !CompileScript( condition, condFn ) )
		return INVALID_ID;

	bp = &m_Breakpoints.append();
	memzero( bp );
	bp->conditionFn._type = OT_NULL;
	bp->conditionEnv._type = OT_NULL;

	bp->id = ++m_nBreakpointIndex;
	CopyString( src, &bp->src );

	bp->line = line;
	bp->hitsTarget = hitsTarget;

	if ( !logMessage.IsEmpty() )
		CopyString( logMessage, &bp->logMessage );

	if ( sq_type(condFn) != OT_NULL )
	{
		bp->conditionFn = condFn;
		InitEnv_GetVal( bp->conditionEnv );

		sq_addref( m_pRootVM, &bp->conditionFn );
		sq_addref( m_pRootVM, &bp->conditionEnv );
	}

	return bp->id;
}

int SQDebugServer::AddFunctionBreakpoint( const string_t &func, const string_t &funcsrc, int line,
		const string_t &condition, int hitsTarget, const string_t &logMessage )
{
	if ( line == -1 )
	{
		m_pCurVM->_lasterror = CreateSQString( m_pCurVM, _SC("invalid function line") );
		return INVALID_ID;
	}

#ifdef SQUNICODE
	Assert( SQUnicodeLength( func.ptr, func.len ) <= 256 );
	Assert( funcsrc.IsEmpty() || SQUnicodeLength( funcsrc.ptr, funcsrc.len ) <= 256 );

	SQChar pFunc[256], pSrc[256];
	sqstring_t wfunc( pFunc, UTF8ToSQUnicode( pFunc, sizeof(pFunc), func.ptr, func.len ) );
	sqstring_t wsrc( _SC("") );

	if ( !funcsrc.IsEmpty() )
		wsrc.Assign( pSrc, UTF8ToSQUnicode( pSrc, sizeof(pSrc), funcsrc.ptr, funcsrc.len ) );

	breakpoint_t *bp = GetFunctionBreakpoint( wfunc, wsrc, line );
#else
	breakpoint_t *bp = GetFunctionBreakpoint( func, funcsrc, line );
#endif

	if ( bp )
	{
		m_pCurVM->_lasterror = CreateSQString( m_pCurVM, _SC("duplicate breakpoint") );
		return INVALID_ID;
	}

	SQObjectPtr condFn;

	if ( !condition.IsEmpty() && !CompileScript( condition, condFn ) )
		return INVALID_ID;

	bp = &m_Breakpoints.append();
	memzero( bp );
	bp->conditionFn._type = OT_NULL;
	bp->conditionEnv._type = OT_NULL;

	bp->id = ++m_nBreakpointIndex;

	if ( !func.IsEmpty() )
	{
		CopyString( func, &bp->src );
	}
	else
	{
		bp->src.Assign( _SC("") );
	}

	if ( !funcsrc.IsEmpty() )
	{
		CopyString( funcsrc, &bp->funcsrc );
	}
	else
	{
		bp->funcsrc.Assign( _SC("") );
	}

	bp->funcline = line;
	bp->hitsTarget = hitsTarget;

	if ( !logMessage.IsEmpty() )
		CopyString( logMessage, &bp->logMessage );

	if ( sq_type(condFn) != OT_NULL )
	{
		bp->conditionFn = condFn;
		InitEnv_GetVal( bp->conditionEnv );

		sq_addref( m_pRootVM, &bp->conditionFn );
		sq_addref( m_pRootVM, &bp->conditionEnv );
	}

	return bp->id;
}

breakpoint_t *SQDebugServer::GetBreakpoint( int line, const sqstring_t &src )
{
	Assert( line && src.ptr );

	for ( int i = 0; i < m_Breakpoints.size(); i++ )
	{
		breakpoint_t &bp = m_Breakpoints[i];
		if ( bp.line == line && bp.src.IsEqualTo( src ) )
		{
			return &bp;
		}
	}

	return NULL;
}

breakpoint_t *SQDebugServer::GetFunctionBreakpoint( const sqstring_t &func, const sqstring_t &funcsrc, int line )
{
	Assert( func.ptr );

	for ( int i = 0; i < m_Breakpoints.size(); i++ )
	{
		breakpoint_t &bp = m_Breakpoints[i];
		if ( bp.line == 0 && bp.src.IsEqualTo( func ) &&
				( bp.funcsrc.IsEmpty() || bp.funcsrc.IsEqualTo( funcsrc ) ) &&
				( bp.funcline == 0 || bp.funcline == line ) )
		{
			return &bp;
		}
	}

	return NULL;
}

void SQDebugServer::FreeBreakpoint( breakpoint_t &bp )
{
	FreeString( &bp.src );
	FreeString( &bp.funcsrc );

	if ( sq_type(bp.conditionFn) != OT_NULL )
	{
		sq_release( m_pRootVM, &bp.conditionFn );
		bp.conditionFn.Null();
	}

	if ( sq_type(bp.conditionEnv) != OT_NULL )
	{
		ClearEnvDelegate( bp.conditionEnv );

		sq_release( m_pRootVM, &bp.conditionEnv );
		bp.conditionEnv.Null();
	}

	FreeString( &bp.logMessage );
	bp.hits = bp.hitsTarget = 0;
}

void SQDebugServer::RemoveAllBreakpoints()
{
	for ( int i = m_Breakpoints.size(); i--; )
		FreeBreakpoint( m_Breakpoints[i] );

	m_Breakpoints.clear();
}

void SQDebugServer::RemoveBreakpoints( const string_t &source )
{
#ifdef SQUNICODE
	Assert( SQUnicodeLength( source.ptr, source.len ) <= 256 );

	SQChar tmp[256];
	sqstring_t src( tmp, UTF8ToSQUnicode( tmp, sizeof(tmp), source.ptr, source.len ) );
#else
	sqstring_t src( source );
#endif

	for ( int i = m_Breakpoints.size(); i--; )
	{
		breakpoint_t &bp = m_Breakpoints[i];
		if ( bp.line != 0 && bp.src.IsEqualTo( src ) )
		{
			FreeBreakpoint( bp );
			m_Breakpoints.remove(i);
		}
	}
}

void SQDebugServer::RemoveFunctionBreakpoints()
{
	for ( int i = m_Breakpoints.size(); i--; )
	{
		breakpoint_t &bp = m_Breakpoints[i];
		if ( bp.line == 0 )
		{
			FreeBreakpoint( bp );
			m_Breakpoints.remove(i);
		}
	}
}

classdef_t *SQDebugServer::FindClassDef( SQClass *base )
{
	for ( int i = 0; i < m_ClassDefinitions.size(); i++ )
	{
		classdef_t &def = m_ClassDefinitions[i];
		if ( def.base == base )
			return &def;
	}

	return NULL;
}

const SQObjectPtr *SQDebugServer::GetClassDefValue( SQClass *base )
{
	const classdef_t *def = FindClassDef( base );

	if ( def )
	{
		if ( sq_type(def->value) == OT_NULL  )
		{
			// Check hierarchy
			while ( ( base = base->_base ) != NULL )
			{
				if ( ( def = FindClassDef( base ) ) != NULL && sq_type(def->value) != OT_NULL  )
					return &def->value;
			}

			return NULL;
		}

		return &def->value;
	}

	return NULL;
}

const SQObjectPtr *SQDebugServer::GetClassDefMetaMembers( SQClass *base )
{
	const classdef_t *def = FindClassDef( base );

	if ( def )
	{
		if ( sq_type(def->metamembers) == OT_NULL  )
		{
			// Check hierarchy
			while ( ( base = base->_base ) != NULL )
			{
				if ( ( def = FindClassDef( base ) ) != NULL && sq_type(def->metamembers) != OT_NULL  )
					return &def->metamembers;
			}

			return NULL;
		}

		return &def->metamembers;
	}

	return NULL;
}

const SQObjectPtr *SQDebugServer::GetClassDefCustomMembers( SQClass *base )
{
	const classdef_t *def = FindClassDef( base );

	if ( def )
	{
		if ( sq_type(def->custommembers) == OT_NULL  )
		{
			// Check hierarchy
			while ( ( base = base->_base ) != NULL )
			{
				if ( ( def = FindClassDef( base ) ) != NULL && sq_type(def->custommembers) != OT_NULL  )
					return &def->custommembers;
			}

			return NULL;
		}

		return &def->custommembers;
	}

	return NULL;
}

void SQDebugServer::DefineClass( SQClass *target, SQTable *params )
{
	classdef_t *def = FindClassDef( target );

	if ( !def )
	{
		def = &m_ClassDefinitions.append();
		memzero( def );
		def->base = target;
		def->value._type = OT_NULL;
		def->metamembers._type = OT_NULL;
		def->custommembers._type = OT_NULL;
	}

	SQObjectPtr name;

	if ( SQTable_Get( params, _SC("name"), name ) )
	{
		if ( sq_type(name) == OT_STRING && _string(name)->_len )
		{
			stringbuf_t< 1024 > buf;
			buf.PutHex( (SQUnsignedInteger)target );
			buf.Put(' ');
			buf.Puts( _string(name) );
			buf.Term();

			CopyString( (const string_t&)buf, &def->name );
		}
		else
		{
			FreeString( &def->name );
		}
	}

	SQObjectPtr value;

	if ( SQTable_Get( params, _SC("value"), value ) )
	{
		if ( sq_type(def->value) != OT_NULL )
		{
			sq_release( m_pRootVM, &def->value );
			def->value.Null();
		}

		if ( sq_type(value) == OT_CLOSURE || sq_type(value) == OT_NATIVECLOSURE )
		{
			def->value = value;
			sq_addref( m_pRootVM, &def->value );
		}
	}

	SQObjectPtr metamembers;

	if ( SQTable_Get( params, _SC("metamembers"), metamembers ) )
	{
		if ( sq_type(def->metamembers) != OT_NULL )
		{
			sq_release( m_pRootVM, &def->metamembers );
			def->metamembers.Null();
		}

		if ( sq_type(metamembers) == OT_ARRAY )
		{
			def->metamembers = metamembers;
			sq_addref( m_pRootVM, &def->metamembers );
		}
	}

	SQObjectPtr custommembers;

	if ( SQTable_Get( params, _SC("custommembers"), custommembers ) )
	{
		if ( sq_type(def->custommembers) != OT_NULL )
		{
			sq_release( m_pRootVM, &def->custommembers );
			def->custommembers.Null();
		}

		if ( sq_type(custommembers) == OT_ARRAY || sq_type(custommembers) == OT_CLOSURE )
		{
			def->custommembers = custommembers;
			sq_addref( m_pRootVM, &def->custommembers );
		}
	}
}

bool SQDebugServer::CallCustomMembersGetFunc( const SQObjectPtr &closure, const SQObject *env,
		const SQObjectPtr &key, SQObjectPtr &ret )
{
	int nparams;

	if ( sq_type(closure) == OT_CLOSURE )
	{
		nparams = _fp(_closure(closure)->_function)->_nparameters;
	}
	else if ( sq_type(closure) == OT_NATIVECLOSURE )
	{
		nparams = _nativeclosure(closure)->_nparamscheck;
	}
	else
	{
		return false;
	}

	if ( nparams == 1 )
	{
		return RunClosure( closure, env, ret );
	}
	else if ( nparams == 2 )
	{
		return RunClosure( closure, env, key, ret );
	}

	return false;
}

bool SQDebugServer::CallCustomMembersSetFunc( const SQObjectPtr &closure, const SQObject *env,
		const SQObjectPtr &key, const SQObjectPtr &val, SQObjectPtr &ret )
{
	int nparams;

	if ( sq_type(closure) == OT_CLOSURE )
	{
		nparams = _fp(_closure(closure)->_function)->_nparameters;
	}
	else if ( sq_type(closure) == OT_NATIVECLOSURE )
	{
		nparams = _nativeclosure(closure)->_nparamscheck;
	}
	else
	{
		return false;
	}

	if ( nparams == 2 )
	{
		return RunClosure( closure, env, val, ret );
	}
	else if ( nparams == 3 )
	{
		return RunClosure( closure, env, key, val, ret );
	}

	return false;
}

void SQDebugServer::RemoveClassDefs()
{
	for ( int i = 0; i < m_ClassDefinitions.size(); i++ )
	{
		classdef_t &def = m_ClassDefinitions[i];

		FreeString( &def.name );

		if ( sq_type(def.value) != OT_NULL )
		{
			sq_release( m_pRootVM, &def.value );
			def.value.Null();
		}

		if ( sq_type(def.metamembers) != OT_NULL )
		{
			sq_release( m_pRootVM, &def.metamembers );
			def.metamembers.Null();
		}

		if ( sq_type(def.custommembers) != OT_NULL )
		{
			sq_release( m_pRootVM, &def.custommembers );
			def.custommembers.Null();
		}
	}

	m_ClassDefinitions.purge();
}

int SQDebugServer::DisassemblyBufLen( SQClosure *target )
{
	SQFunctionProto *func = _fp(target->_function);
	const int maxParamNameLen = 64;
	const int buflen =
		STRLEN("stacksize     \n") + FMT_INT_LEN +
		STRLEN("instructions  \n") + FMT_INT_LEN +
		STRLEN("literals      \n") + FMT_INT_LEN +
		STRLEN("localvarinfos \n") + FMT_INT_LEN +
		6 + 1 +
		STRLEN("parameters    \n") + FMT_INT_LEN +
		func->_nparameters * ( maxParamNameLen + 2 ) - 2 + 1 +
#if SQUIRREL_VERSION_NUMBER > 212
		func->_ndefaultparams * ( maxParamNameLen + 3 ) +
#endif
		6 + 1 +
		func->_ninstructions * ( 6 + 30 + 128 + 1 ) - 1 +
		1;

	return buflen;
}

sqstring_t SQDebugServer::PrintDisassembly( SQClosure *target, SQChar *scratch, int bufsize )
{
	SQFunctionProto *func = _fp(target->_function);
	const int maxParamNameLen = 64;
	SQChar *buf = scratch;

#define _bs (bufsize - (int)((char*)buf - (char*)scratch))

	int instrCount = func->_ninstructions;
	for ( int i = instrCount; i--; )
		if ( func->_instructions[i].op == _OP_LINE )
			instrCount--;

#define putint( str, val ) \
	{ \
		int len = STRLEN(str); \
		memcpy( buf, _SC(str), sq_rsl(len) ); \
		buf += len; \
		buf += printint( buf, _bs, val ); \
		*buf++ = '\n'; \
	}

	putint( "stacksize     ", (int)func->_stacksize );
	putint( "instructions  ", instrCount );
	putint( "literals      ", (int)func->_nliterals );
	putint( "localvarinfos ", (int)func->_nlocalvarinfos );

	int nparams = func->_nparameters;

#if SQUIRREL_VERSION_NUMBER >= 300
	if ( func->_varparams )
		nparams--;
#endif

	putint( "parameters    ", nparams );

#undef putint

	for ( int i = 6; i--; )
		*buf++ = '-';

	*buf++ = '\n';

	for ( int i = 0; i < nparams; i++ )
	{
		const SQObjectPtr &param = func->_parameters[i];
		if ( sq_type(param) == OT_STRING )
		{
			int len = min( (int)_string(param)->_len, maxParamNameLen );
			memcpy( buf, _string(param)->_val, sq_rsl(len) );
			buf += len;

#if SQUIRREL_VERSION_NUMBER > 212
			int idx;
			if ( func->_ndefaultparams && ( idx = (int)func->_ndefaultparams - ( nparams - i ) ) >= 0 )
			{
				const SQObjectPtr &val = target->_defaultparams[idx];
				string_t str = GetValue( val, kFS_NoAddr );

				int len = STRLEN(" = ");
				memcpy( buf, _SC(" = "), sq_rsl(len) );
				buf += len;

				len = min( str.len, maxParamNameLen - 3 );
#ifdef SQUNICODE
				UTF8ToSQUnicode( buf, _bs, str.ptr, str.len );
#else
				memcpy( buf, str.ptr, sq_rsl(len) );
#endif
				buf += len;
			}
#endif
		}

		*buf++ = ',';
		*buf++ = ' ';
	}

	if ( !func->_varparams )
	{
		buf -= 2;
	}
	else
	{
		*buf++ = '.';
		*buf++ = '.';
		*buf++ = '.';
	}

	*buf++ = '\n';

	for ( int i = 6; i--; )
		*buf++ = '-';

	*buf++ = '\n';

	RestoreCachedInstructions();

	for ( int i = 0, index = 0; i < func->_ninstructions; i++ )
	{
		SQInstruction *instr = func->_instructions + i;
		if ( instr->op == _OP_LINE )
			continue;

		stringbuf_t< 128 > tmp;
		tmp.PutHex( instr->op ); tmp.Put(' ');
		tmp.PutInt( instr->_arg0 ); tmp.Put(' ');
		tmp.PutInt( instr->_arg1 ); tmp.Put(' ');
		tmp.PutInt( instr->_arg2 ); tmp.Put(' ');
		tmp.PutInt( instr->_arg3 );
		tmp.Term();

#ifdef SQUNICODE
		int l = scsprintf( buf, _bs / (int)sizeof(SQChar), _SC("%-6d %-29hs"), index++, tmp.ptr );

		if ( l < 0 || l > _bs / (int)sizeof(SQChar) )
			l = _bs / (int)sizeof(SQChar);

		buf += l;

		tmp.len = 0;
		DescribeInstruction( instr, func, tmp );

		buf += UTF8ToSQUnicode( buf, _bs, tmp.ptr, tmp.len );
#else
		int l = scsprintf( buf, _bs / (int)sizeof(SQChar), _SC("%-6d %-29s"), index++, tmp.ptr );

		if ( l < 0 || l > _bs / (int)sizeof(SQChar) )
			l = _bs / (int)sizeof(SQChar);

		buf += l;

		stringbufext_t sbuf( buf, _bs );
		DescribeInstruction( instr, func, sbuf );
		buf += sbuf.len;
#endif

		if ( _bs <= 0 )
		{
			buf--;
			break;
		}

		*buf++ = '\n';
	}

	UndoRestoreCachedInstructions();

	*buf-- = 0;

#undef _bs

	return { scratch, (int)( buf - scratch ) };
}

#ifndef SQDBG_DISABLE_PROFILER
CProfiler *SQDebugServer::GetProfiler( HSQUIRRELVM vm )
{
	for ( int i = 0; i < m_Profilers.size(); i++ )
	{
		threadprofiler_t &tp = m_Profilers[i];
		if ( tp.thread && sq_type(tp.thread->_obj) == OT_THREAD )
		{
			if ( _thread(tp.thread->_obj) == vm )
			{
				return &tp.prof;
			}
		}
	}

	return NULL;
}

CProfiler *SQDebugServer::GetProfilerFast( HSQUIRRELVM vm )
{
	if ( m_pCurVM == vm )
	{
		Assert( m_pProfiler == GetProfiler(vm) );
		return m_pProfiler;
	}

	return GetProfiler(vm);
}

void SQDebugServer::ProfSwitchThread( HSQUIRRELVM vm )
{
	Assert( IsProfilerEnabled() );

	if ( m_Profilers.size() == 0 )
		m_Profilers.reserve(1);

	for ( int i = 0; i < m_Profilers.size(); i++ )
	{
		threadprofiler_t &tp = m_Profilers[i];
		if ( tp.thread && sq_type(tp.thread->_obj) == OT_THREAD )
		{
			if ( _thread(tp.thread->_obj) == vm )
			{
				m_pProfiler = tp.prof.IsEnabled() ? &tp.prof : NULL;
				return;
			}
		}
		else
		{
			__ObjRelease( tp.thread );
			m_Profilers.remove(i);
			i--;
		}
	}

	threadprofiler_t &tp = m_Profilers.append();
	memzero( &tp );
	tp.thread = GetWeakRef( vm );
	__ObjAddRef( tp.thread );

	m_pProfiler = &tp.prof;
	m_pProfiler->Start( vm );
}

void SQDebugServer::ProfStart()
{
	if ( !IsClientConnected() )
		SetDebugHook( &SQProfHook );

	m_bProfilerEnabled = true;
	ProfSwitchThread( m_pCurVM );
}

void SQDebugServer::ProfStop()
{
	if ( !IsClientConnected() )
		SetDebugHook( NULL );

	for ( int i = 0; i < m_Profilers.size(); i++ )
	{
		threadprofiler_t &tp = m_Profilers[i];
		__ObjRelease( tp.thread );

		tp.prof.Stop();
	}

	m_Profilers.clear();
	m_pProfiler = NULL;
	m_bProfilerEnabled = false;
}

void SQDebugServer::ProfPause( HSQUIRRELVM vm )
{
	CProfiler *prof = GetProfilerFast( vm );
	if ( prof && prof->IsEnabled() )
	{
		prof->Pause();
	}
}

void SQDebugServer::ProfResume( HSQUIRRELVM vm )
{
	CProfiler *prof = GetProfilerFast( vm );
	if ( prof && prof->IsEnabled() )
	{
		prof->Resume();
	}
}

void SQDebugServer::ProfGroupBegin( HSQUIRRELVM vm, SQString *tag )
{
	CProfiler *prof = GetProfilerFast( vm );
	if ( prof && prof->IsActive() )
	{
		prof->GroupBegin( tag );
	}
}

void SQDebugServer::ProfGroupEnd( HSQUIRRELVM vm )
{
	CProfiler *prof = GetProfilerFast( vm );
	if ( prof && prof->IsActive() )
	{
		prof->GroupEnd();
	}
}

sqstring_t SQDebugServer::ProfGet( HSQUIRRELVM vm, SQString *tag, int type )
{
	Assert( IsProfilerEnabled() );

	CProfiler *pProfiler = NULL;

	for ( int i = 0; i < m_Profilers.size(); i++ )
	{
		threadprofiler_t &tp = m_Profilers[i];
		if ( tp.thread && sq_type(tp.thread->_obj) == OT_THREAD )
		{
			if ( _thread(tp.thread->_obj) == vm )
			{
				pProfiler = &tp.prof;
				break;
			}
		}
		else
		{
			__ObjRelease( tp.thread );
			m_Profilers.remove(i);
			i--;
		}
	}

	if ( !pProfiler )
		return { 0, 0 };

	const int size = pProfiler->GetMaxOutputLen( tag, type );
	SQChar *buf = _ss(m_pRootVM)->GetScratchPad( sq_rsl(size) );
	int len = pProfiler->Output( tag, type, buf, size );

	return { buf, len };
}

void SQDebugServer::ProfPrint( HSQUIRRELVM vm, SQString *tag, int type )
{
	sqstring_t str = ProfGet( vm, tag, type );
	if ( str.IsEmpty() )
		return;

	// Print each line
	for ( SQChar *start = str.ptr; ; )
	{
		SQChar *end = scstrchr( start, '\n' );
		if ( !end )
			break;

		int linelen = (int)( end + 1 - start );

		_OutputDebugStringFmt( _SC(FMT_VSTR), linelen, start );
		m_Print( vm, _SC(FMT_VSTR), linelen, start );
		SendEvent_OutputStdOut( sqstring_t( start, linelen ), NULL );

		if ( end + 1 >= str.ptr + str.len )
			break;

		start = end + 1;
	}
}
#endif

#ifndef SQDBG_CALL_DEFAULT_ERROR_HANDLER
void SQDebugServer::PrintVar( HSQUIRRELVM vm, const SQChar *name, const SQObjectPtr &obj )
{
	switch ( sq_type(obj) )
	{
		case OT_NULL:
			SQErrorAtFrame( vm, NULL, _SC("[%s] NULL\n"), name );
			break;
		case OT_INTEGER:
			SQErrorAtFrame( vm, NULL, _SC("[%s] " FMT_INT "\n"), name, _integer(obj) );
			break;
		case OT_FLOAT:
			SQErrorAtFrame( vm, NULL, _SC("[%s] %.14g\n"), name, _float(obj) );
			break;
		case OT_USERPOINTER:
			SQErrorAtFrame( vm, NULL, _SC("[%s] USERPOINTER\n"), name );
			break;
		case OT_STRING:
			SQErrorAtFrame( vm, NULL, _SC("[%s] \"%s\"\n"), name, _stringval(obj) );
			break;
		case OT_TABLE:
			SQErrorAtFrame( vm, NULL, _SC("[%s] TABLE\n"), name );
			break;
		case OT_ARRAY:
			SQErrorAtFrame( vm, NULL, _SC("[%s] ARRAY\n"), name );
			break;
		case OT_CLOSURE:
			SQErrorAtFrame( vm, NULL, _SC("[%s] CLOSURE\n"), name );
			break;
		case OT_NATIVECLOSURE:
			SQErrorAtFrame( vm, NULL, _SC("[%s] NATIVECLOSURE\n"), name );
			break;
		case OT_GENERATOR:
			SQErrorAtFrame( vm, NULL, _SC("[%s] GENERATOR\n"), name );
			break;
		case OT_USERDATA:
			SQErrorAtFrame( vm, NULL, _SC("[%s] USERDATA\n"), name );
			break;
		case OT_THREAD:
			SQErrorAtFrame( vm, NULL, _SC("[%s] THREAD\n"), name );
			break;
		case OT_CLASS:
			SQErrorAtFrame( vm, NULL, _SC("[%s] CLASS\n"), name );
			break;
		case OT_INSTANCE:
			SQErrorAtFrame( vm, NULL, _SC("[%s] INSTANCE\n"), name );
			break;
		case OT_WEAKREF:
			PrintVar( vm, name, _weakref(obj)->_obj );
			break;
		case OT_BOOL:
			SQErrorAtFrame( vm, NULL, _SC("[%s] %s\n"), name, _integer(obj) ? _SC("true") : _SC("false") );
			break;
		default: UNREACHABLE();
	}
}

void SQDebugServer::PrintStack( HSQUIRRELVM vm )
{
	SQErrorAtFrame( vm, NULL, _SC("\nCALLSTACK\n") );

	int i = vm->_callsstacksize;
	while ( i-- )
	{
		const SQVM::CallInfo &ci = vm->_callsstack[i];

		if ( ShouldIgnoreStackFrame(ci) )
			continue;

		if ( sq_type(ci._closure) == OT_CLOSURE )
		{
			SQFunctionProto *func = _fp(_closure(ci._closure)->_function);

			const SQChar *fn = _SC("??");
			const SQChar *src = _SC("??");
			int line = func->GetLine( ci._ip );

			if ( sq_type(func->_name) == OT_STRING )
				fn = _stringval(func->_name);

			if ( sq_type(func->_sourcename) == OT_STRING )
				src = _stringval(func->_sourcename);

			SQErrorAtFrame( vm, &ci, _SC("*FUNCTION [%s()] %s line [%d]\n"), fn, src, line );
		}
		else if ( sq_type(ci._closure) == OT_NATIVECLOSURE )
		{
			SQNativeClosure *closure = _nativeclosure(ci._closure);

			const SQChar *fn = _SC("??");
			const SQChar *src = _SC("NATIVE");
			int line = -1;

			if ( sq_type(closure->_name) == OT_STRING )
				fn = _stringval(closure->_name);

			SQErrorAtFrame( vm, NULL, _SC("*FUNCTION [%s()] %s line [%d]\n"), fn, src, line );
		}
		else UNREACHABLE();
	}

	SQErrorAtFrame( vm, NULL, _SC("\nLOCALS\n") );

	i = vm->_callsstacksize;
	if ( i > 10 )
		i = 10;

	while ( i-- )
	{
		const SQVM::CallInfo &ci = vm->_callsstack[i];

		if ( sq_type(ci._closure) != OT_CLOSURE )
			continue;

		if ( ShouldIgnoreStackFrame(ci) )
			continue;

		int stackbase = GetStackBase( vm, &ci );
		SQClosure *pClosure = _closure(ci._closure);
		SQFunctionProto *func = _fp(pClosure->_function);

		SQUnsignedInteger ip = (SQUnsignedInteger)( ci._ip - func->_instructions - 1 );

		for ( int i = 0; i < func->_nlocalvarinfos; i++ )
		{
			const SQLocalVarInfo &var = func->_localvarinfos[i];
			if ( var._start_op <= ip + 1 && var._end_op >= ip )
			{
				PrintVar( vm, _stringval(var._name), vm->_stack._vals[ stackbase + var._pos ] );
			}
		}

		for ( int i = 0; i < func->_noutervalues; i++ )
		{
			const SQOuterVar &var = func->_outervalues[i];
			PrintVar( vm, _stringval(var._name), *_outervalptr( pClosure->_outervalues[i] ) );
		}
	}
}
#endif

void SQDebugServer::ErrorHandler( HSQUIRRELVM vm )
{
	if ( m_bDebugHookGuard )
		return;

	string_t err;

	if ( sq_gettop( vm ) >= 1 )
	{
		HSQOBJECT o;
		sq_getstackobj( vm, 2, &o );
		err = GetValue( o, kFS_NoQuote );
	}
	else
	{
		err.Assign( "??" );
	}

	// An error handler is required to detect exceptions.
	// The downside of calling the default error handler instead of
	// replicating it in the debugger is the extra stack frame and redundant print locations.
	// Otherwise this would be preferrable for preserving custom error handlers.
#ifdef SQDBG_CALL_DEFAULT_ERROR_HANDLER
	SQObjectPtr dummy;
	vm->Call( m_ErrorHandler, 2, vm->_top-2, dummy, SQFalse );
#else
	SQErrorAtFrame( vm, NULL, _SC("\nAN ERROR HAS OCCURRED [" FMT_VCSTR "]\n"), STR_EXPAND(err) );
	PrintStack( vm );
#endif

	if ( m_bBreakOnExceptions )
	{
		Break( vm, { breakreason_t::Exception, err } );

#if SQUIRREL_VERSION_NUMBER < 300
		// SQ2 doesn't notify the debug hook of returns via errors,
		// have to suspend and finish profiler calls here
#ifndef SQDBG_DISABLE_PROFILER
		if ( IsProfilerEnabled() )
		{
			CProfiler *prof = GetProfiler( vm );
			if ( prof && prof->IsActive() )
			{
				prof->CallEndAll();
			}
		}
#endif
		// Use m_bExceptionPause to make stepping while suspended here continue the thread.
		if ( m_State == ThreadState_SuspendNow )
		{
			m_bExceptionPause = true;
			m_bInDebugHook = true;
			Suspend();
			m_bExceptionPause = false;
			m_bInDebugHook = false;
		}
#else
		// To fix instruction stepping in exception
		m_bExceptionPause = true;
#endif
	}
}

void SQDebugServer::Break( HSQUIRRELVM vm, breakreason_t reason )
{
	Assert( IsClientConnected() );
	Assert( reason.reason != breakreason_t::None );

	DAP_START_EVENT( ++m_Sequence, "stopped" );
	DAP_SET_TABLE( body, 5 );
		body.SetInt( "threadId", ThreadToID( vm ) );
		body.SetBool( "allThreadsStopped", true );

		switch ( reason.reason )
		{
			case breakreason_t::Step:
				body.SetString( "reason", "step" );
				break;
			case breakreason_t::Breakpoint:
				body.SetString( "reason", "breakpoint" );
				break;
			case breakreason_t::Exception:
				body.SetString( "reason", "exception" );
				break;
			case breakreason_t::Pause:
				body.SetString( "reason", "pause" );
				break;
			case breakreason_t::Restart:
				body.SetString( "reason", "restart" );
				break;
			case breakreason_t::Goto:
				body.SetString( "reason", "goto" );
				break;
			case breakreason_t::FunctionBreakpoint:
				body.SetString( "reason", "function breakpoint" );
				break;
			case breakreason_t::DataBreakpoint:
				body.SetString( "reason", "data breakpoint" );
				break;
			default: UNREACHABLE();
		}

		if ( !reason.text.IsEmpty() )
			body.SetStringNoCopy( "text", reason.text );

		if ( reason.reason == breakreason_t::Breakpoint ||
				reason.reason == breakreason_t::FunctionBreakpoint ||
				reason.reason == breakreason_t::DataBreakpoint )
		{
			body.SetArray( "hitBreakpointIds", 1 ).Append( reason.id );
		}
	DAP_SEND();

	if ( m_State != ThreadState_Suspended )
		m_State = ThreadState_SuspendNow;
}

void SQDebugServer::Suspend()
{
	Assert( m_State == ThreadState_SuspendNow );

	m_State = ThreadState_Suspended;

	do
	{
		if ( IsClientConnected() )
		{
			Frame();
		}
		else
		{
			Continue( NULL );
			break;
		}

		sqdbg_sleep( 5 );
	}
	while ( m_State == ThreadState_Suspended );
}

void SQDebugServer::Continue( HSQUIRRELVM vm )
{
	if ( m_State == ThreadState_SuspendNow )
		return;

	if ( IsClientConnected() && m_State != ThreadState_Running )
	{
		DAP_START_EVENT( ++m_Sequence, "continued" );
		DAP_SET_TABLE( body, 2 );
			body.SetInt( "threadId", ThreadToID( vm ) );
			body.SetBool( "allThreadsContinued", true );
		DAP_SEND();
	}

	m_State = ThreadState_Running;
	m_pPausedThread = NULL;
	m_ReturnValues.purge();
	m_iYieldValues = 0;
#if SQUIRREL_VERSION_NUMBER >= 300
	m_bExceptionPause = false;
#endif
	RestoreCachedInstructions();
	ClearCachedInstructions();
	RemoveVarRefs( false );
	RemoveLockedWatches();

	ClearEnvDelegate( m_EnvGetVal );

	if ( m_Scratch.Size() > 1024 )
		m_Scratch.Alloc( 1024 );
}

int SQDebugServer::EvalAndWriteExpr( HSQUIRRELVM vm, const SQVM::CallInfo *ci, string_t &expression,
		char *buf, int size )
{
	// Don't modify logMessage
	char *comma = NULL;

	// LOCK flag is ignored, it's ok
	int flags = ParseFormatSpecifiers( expression, &comma );

	// Write to buffer in here because
	// value could be holding the only ref to a SQString
	// which the result string_t will point to
	SQObjectPtr value;

#ifndef SQDBG_DISABLE_COMPILER
	bool res;

	if ( expression.len <= 256 )
	{
		// 'expression' is a substring of breakpoint_t::logMessage
		// don't modify its bytes in CCompiler::ParseString
		char cpy[256];
		string_t expr;
		expr.Assign( cpy, expression.len );
		memcpy( cpy, expression.ptr, expression.len );

		ECompileReturnCode cres = Evaluate( expr, vm, ci, value );
		res = ( cres == CompileReturnCode_Success ||
				( cres > CompileReturnCode_Fallback && RunExpression( expression, vm, ci, value ) ) );
	}
	else
	{
		res = RunExpression( expression, vm, ci, value );
	}

	if ( res )
#else
	objref_t obj;
	if ( GetObj_Frame( expression, vm, ci, obj, value ) || RunExpression( expression, vm, ci, value ) )
#endif
	{
		if ( comma )
			*comma = ',';

		string_t result = GetValue( value, flags );

		int writelen = min( result.len, size );
		memcpy( buf, result.ptr, writelen );
		return writelen;
	}

	if ( comma )
		*comma = ',';

	return 0;
}

//
// Expressions within `{}` are evaluated.
// Escape the opening bracket to print brackets `\{`
//
// Special keywords: $FUNCTION, $CALLER, $HITCOUNT
//
void SQDebugServer::TracePoint( breakpoint_t *bp, HSQUIRRELVM vm, int frame )
{
	char buf[512];
	int bufsize = sizeof(buf) - 2; // \n\0
	int readlen = min( bp->logMessage.len, bufsize );
	char *pWrite = buf;
	char *logMessage = bp->logMessage.ptr;

	// if logMessage is surrounded with \{ and }/,
	// evaluate the expression but don't print.
	// A simple way to inject expression evaluation without side effects
	// although still limited by print buffer size
	bool escapePrint = readlen > 4 &&
		logMessage[0] == '\\' &&
		logMessage[1] == '{' &&
		logMessage[readlen-2] == '}' &&
		logMessage[readlen-1] == '/';

	if ( escapePrint )
		logMessage[0] = 0;

	for ( int iRead = 0; iRead < readlen; iRead++ )
	{
		switch ( logMessage[iRead] )
		{
			case '{':
			{
				// '\' preceeds '{'
				if ( iRead && logMessage[iRead-1] == '\\' )
				{
					pWrite[-1] = '{';
					continue;
				}

				int depth = 1;
				for ( int j = iRead + 1; j < readlen; j++ )
				{
					switch ( logMessage[j] )
					{
						case '}':
						{
							depth--;

							// Found expression
							if ( depth == 0 )
							{
								const SQVM::CallInfo *ci = vm->_callsstack + frame;

								string_t expression;
								expression.Assign( logMessage + iRead + 1, j - iRead - 1 );

								if ( expression.len )
									pWrite += EvalAndWriteExpr( vm, ci, expression, pWrite, bufsize - j );

								iRead = j;
								goto exit;
							}

							break;
						}
						case '{':
						{
							depth++;
							break;
						}
					}
				}
			exit:;
				break;
			}
			case '$':
			{
				if ( iRead && logMessage[iRead-1] == '\\' )
				{
					pWrite[-1] = '$';
					continue;
				}

				#define STRCMP( s, StrLiteral ) \
					memcmp( (s), (StrLiteral), sizeof(StrLiteral)-1 )

				#define CHECK_KEYWORD(s) \
					( ( iRead + (int)STRLEN(s) < readlen ) && \
						!STRCMP( logMessage + iRead + 1, s ) )

				if ( CHECK_KEYWORD("FUNCTION") )
				{
					const SQVM::CallInfo *ci = vm->_callsstack + frame;
					const SQObjectPtr &funcname = _fp(_closure(ci->_closure)->_function)->_name;

					if ( sq_type(funcname) == OT_STRING )
					{
						SQString *name = _string(funcname);

						int writelen = scstombs( pWrite, bufsize - iRead, name->_val, name->_len );
						pWrite += writelen;
					}

					iRead += STRLEN("FUNCTION");
					break;
				}
				else if ( CHECK_KEYWORD("CALLER") )
				{
					const SQVM::CallInfo *ci = vm->_callsstack + frame;
					if ( ci > vm->_callsstack )
					{
						const SQVM::CallInfo *cii = ci - 1;

						if ( sq_type(cii->_closure) == OT_CLOSURE )
						{
							if ( sq_type(_fp(_closure(cii->_closure)->_function)->_name) == OT_STRING )
							{
								SQString *name = _string(_fp(_closure(cii->_closure)->_function)->_name);

								int writelen = scstombs( pWrite, bufsize - iRead, name->_val, name->_len );
								pWrite += writelen;
							}
						}
						else if ( sq_type(cii->_closure) == OT_NATIVECLOSURE )
						{
							if ( sq_type(_nativeclosure(cii->_closure)->_name) == OT_STRING )
							{
								SQString *name = _string(_nativeclosure(cii->_closure)->_name);

								int writelen = scstombs( pWrite, bufsize - iRead, name->_val, name->_len );
								pWrite += writelen;
							}
						}
						else UNREACHABLE();
					}

					iRead += STRLEN("CALLER");
					break;
				}
				else if ( CHECK_KEYWORD("HITCOUNT") )
				{
					// lazy hack, hit count was reset after hitting the target
					// if this count is to ignore hit target, keep trace hit count separately
					int hits = bp->hits ? bp->hits : bp->hitsTarget;
					pWrite += printint( pWrite, bufsize - iRead, hits );
					iRead += STRLEN("HITCOUNT");
					break;
				}
				// else fallthrough

				#undef STRCMP
				#undef CHECK_KEYWORD
			}
			default:
				*pWrite++ = logMessage[iRead];
		}
	}

	if ( escapePrint )
	{
		logMessage[0] = '\\';
		return;
	}

	*pWrite++ = '\n';
	*pWrite = 0;

	const SQVM::CallInfo *ci = vm->_callsstack + frame;

	_OutputDebugStringA( buf );
#ifdef SQUNICODE
	m_Print( vm, _SC(FMT_CSTR), buf );
#else
	m_Print( vm, buf );
#endif
	SendEvent_OutputStdOut( string_t( buf, (int)( pWrite - buf ) ), ci );
}

bool SQDebugServer::HasCondition( const breakpoint_t *bp )
{
	return ( sq_type(bp->conditionFn) != OT_NULL );
}

bool SQDebugServer::CheckBreakpointCondition( breakpoint_t *bp, HSQUIRRELVM vm, const SQVM::CallInfo *ci )
{
	Assert( HasCondition( bp ) );
	Assert( sq_type(bp->conditionFn) != OT_NULL &&
			sq_type(bp->conditionEnv) != OT_NULL );

	SetCallFrame( bp->conditionEnv, vm, ci );
	SetEnvDelegate( bp->conditionEnv, vm->_stack._vals[ GetStackBase( vm, ci ) ] );

	SQObjectPtr res;

	// Using sqdbg compiler here is faster for simple expressions,
	// but gets increasingly slower as the expression grows larger and more complex
	// while executing precompiled sq function is constant time
	if ( RunClosure( bp->conditionFn, &bp->conditionEnv, res ) )
	{
		return !IsFalse( res );
	}
	else
	{
		// Invalid condition, remove it
		ClearEnvDelegate( bp->conditionEnv );

		sq_release( m_pRootVM, &bp->conditionFn );
		sq_release( m_pRootVM, &bp->conditionEnv );

		bp->conditionFn.Null();
		bp->conditionEnv.Null();

		return false;
	}
}

#define SQ_HOOK_LINE 'l'
#define SQ_HOOK_CALL 'c'
#define SQ_HOOK_RETURN 'r'

void SQDebugServer::DebugHook( HSQUIRRELVM vm, SQInteger type,
		const SQChar *sourcename, SQInteger line, const SQChar *funcname )
{
	Assert( IsClientConnected() );

	if ( m_bDebugHookGuard )
		return;

#if SQUIRREL_VERSION_NUMBER < 300
	// Debug hook re-entry guard doesn't exist in SQ2
	if ( m_bInDebugHook )
		return;

	m_bInDebugHook = true;
#endif

#ifdef NATIVE_DEBUG_HOOK
	SQVM::CallInfo *ci = vm->ci;
#else
	SQVM::CallInfo *ci = vm->ci - 1;
#endif

	// The only way to detect thread change, not ideal
	if ( m_pCurVM != vm )
	{
		// HACKHACK: Detect thread.call, wakeup and suspend calls to make StepOver/StepOut make sense
		// This is not perfect as it is only detected on the first debug hook call
		// *after* the thread functions were called, which means it will miss instructions.
		if ( m_pCurVM->ci )
		{
			if ( m_pCurVM->_suspended )
			{
				SQInstruction *pip = m_pCurVM->ci->_ip - 1;

				if ( pip->op == _OP_CALL )
				{
					const SQObjectPtr &val = m_pCurVM->_stack._vals[ m_pCurVM->_stackbase + pip->_arg1 ];
					if ( sq_type(val) == OT_NATIVECLOSURE &&
							sq_type(_nativeclosure(val)->_name) == OT_STRING &&
							sqstring_t(_SC("suspend")).IsEqualTo( _string(_nativeclosure(val)->_name) ) )
					{
						m_nCalls -= (int)( m_pCurVM->ci - m_pCurVM->_callsstack ) + 1;

						switch ( m_State )
						{
							case ThreadState_StepOut:

								if ( m_pStateVM != m_pCurVM )
									break;

							case ThreadState_StepOver:

								m_State = ThreadState_StepIn;
								break;

							case ThreadState_StepOutInstruction:

								if ( m_pStateVM != m_pCurVM )
									break;

							case ThreadState_StepOverInstruction:
							case ThreadState_StepInInstruction:

								RestoreCachedInstructions();
								ClearCachedInstructions();

								if ( InstructionStep( vm, ci, 2 ) )
								{
									m_State = ThreadState_StepInInstruction;
								}
								else
								{
									m_State = ThreadState_NextStatement;
								}

							default:
								break;
						}
					}
				}
			}
			else
			{
				if ( sq_type(m_pCurVM->ci->_closure) == OT_NATIVECLOSURE &&
						sq_type(_nativeclosure(m_pCurVM->ci->_closure)->_name) == OT_STRING &&
						( sqstring_t(_SC("wakeup")).IsEqualTo(
							_string(_nativeclosure(m_pCurVM->ci->_closure)->_name) ) ||
						  sqstring_t(_SC("call")).IsEqualTo(
							_string(_nativeclosure(m_pCurVM->ci->_closure)->_name) ) ) )
				{
					m_nCalls += (int)( vm->ci - vm->_callsstack ) + 1;
				}
			}
		}

		ThreadToID( vm );
		m_pCurVM = vm;

#ifndef SQDBG_DISABLE_PROFILER
		if ( IsProfilerEnabled() &&
				// Ignore repl
				( !sourcename ||
				  !sqstring_t(_SC("sqdbg")).IsEqualTo( SQStringFromSQChar( sourcename ) ) ) )
		{
			ProfSwitchThread( vm );
		}
#endif
	}

#ifndef SQDBG_DISABLE_PROFILER
	Assert( !IsProfilerEnabled() ||
			!sourcename ||
			sqstring_t(_SC("sqdbg")).IsEqualTo( SQStringFromSQChar( sourcename ) ) ||
			m_pProfiler == GetProfiler(vm) );
#endif

	if ( m_pPausedThread == vm &&
			// Ignore repl
			( !sourcename ||
			  !sqstring_t(_SC("sqdbg")).IsEqualTo( SQStringFromSQChar( sourcename ) ) ))
	{
		m_pPausedThread = NULL;

		if ( type == SQ_HOOK_LINE && (ci->_ip-1)->_arg1 == 0 )
			ci->_ip--;

		RestoreCachedInstructions();
		ClearCachedInstructions();

#ifndef SQDBG_DISABLE_PROFILER
		if ( type == SQ_HOOK_CALL )
		{
			if ( IsProfilerEnabled() && m_pProfiler && m_pProfiler->IsActive() )
			{
				SQFunctionProto *func = _fp(_closure(ci->_closure)->_function);
				bool bGenerator = ( func->_bgenerator && ci->_ip == func->_instructions );

				if ( !bGenerator || ( ci != vm->_callsstack && ((ci-1)->_ip-1)->op == _OP_RESUME ) )
				{
					m_pProfiler->CallBegin( func );
				}
			}
		}
#endif

		Break( vm, breakreason_t::Pause );

		if ( m_State == ThreadState_SuspendNow )
			Suspend();

#if SQUIRREL_VERSION_NUMBER < 300
		m_bInDebugHook = false;
#endif
		return;
	}

	if ( m_DataWatches.size() )
	{
		// CMP metamethod function call can reallocate call stack
		int frame = ci - vm->_callsstack;
		CheckDataBreakpoints( vm );
		ci = vm->_callsstack + frame;
	}

	breakreason_t breakReason;

	switch ( m_State )
	{
		case ThreadState_Running:
			break;

		case ThreadState_StepOver:
		{
			if ( m_nCalls == m_nStateCalls )
			{
				switch ( type )
				{
					case SQ_HOOK_LINE:
					{
						breakReason.reason = breakreason_t::Step;
						break;
					}
					case SQ_HOOK_RETURN:
					{
						m_nStateCalls--;
						break;
					}
				}
			}

			break;
		}
		case ThreadState_StepIn:
		{
			if ( type == SQ_HOOK_LINE )
			{
				breakReason.reason = breakreason_t::Step;
			}

			break;
		}
		case ThreadState_StepOut:
		{
			if ( type == SQ_HOOK_RETURN &&
					m_nCalls == m_nStateCalls &&
					vm == m_pStateVM )
			{
				m_State = ThreadState_NextStatement;
			}

			break;
		}
		case ThreadState_NextStatement:
		{
			breakReason.reason = breakreason_t::Step;
			break;
		}
		case ThreadState_StepOverInstruction:
		{
			if ( m_nCalls == m_nStateCalls )
			{
				switch ( type )
				{
					case SQ_HOOK_LINE:
					{
						if ( (ci->_ip-1)->_arg1 != 0 )
						{
#if SQUIRREL_VERSION_NUMBER < 300
							// Exiting trap
							if ( ci->_ip - 3 > _fp(_closure(ci->_closure)->_function)->_instructions &&
									(ci->_ip-2)->op == _OP_JMP && (ci->_ip-3)->op == _OP_POPTRAP )
							{
								if ( InstructionStep( vm, ci, 1 ) )
								{
									m_State = ThreadState_StepInInstruction;
								}
								else
								{
									m_State = ThreadState_NextStatement;
								}
							}
#endif

							break;
						}

						breakReason.reason = breakreason_t::Step;

						RestoreCachedInstructions();
						ClearCachedInstructions();
						ci->_ip--;

						break;
					}
					case SQ_HOOK_CALL:
					{
						break;
					}
					case SQ_HOOK_RETURN:
					{
						RestoreCachedInstructions();
						ClearCachedInstructions();

						if ( ci != vm->_callsstack )
						{
							if ( InstructionStep( vm, ci - 1, 1 ) )
							{
								m_State = ThreadState_StepInInstruction;
							}
							else
							{
								m_State = ThreadState_NextStatement;
							}
						}

						break;
					}
					default: UNREACHABLE();
				}
			}

			break;
		}
		case ThreadState_StepInInstruction:
		{
			switch ( type )
			{
				case SQ_HOOK_LINE:
				{
					if ( (ci->_ip-1)->_arg1 != 0 )
					{
#if SQUIRREL_VERSION_NUMBER < 300
						// Exiting trap
						if ( ci->_ip - 3 > _fp(_closure(ci->_closure)->_function)->_instructions &&
								(ci->_ip-2)->op == _OP_JMP && (ci->_ip-3)->op == _OP_POPTRAP )
						{
							if ( InstructionStep( vm, ci, 1 ) )
							{
								m_State = ThreadState_StepInInstruction;
							}
							else
							{
								m_State = ThreadState_NextStatement;
							}
						}
#endif

						break;
					}

					breakReason.reason = breakreason_t::Step;

					RestoreCachedInstructions();
					ClearCachedInstructions();
					ci->_ip--;

					break;
				}
				case SQ_HOOK_CALL:
				{
					RestoreCachedInstructions();
					ClearCachedInstructions();

					if ( ci->_ip->op == _OP_LINE )
					{
						if ( InstructionStep( vm, ci ) )
						{
							m_State = ThreadState_StepInInstruction;
						}
						else
						{
							m_State = ThreadState_NextStatement;
						}
					}
					else
					{
						breakReason.reason = breakreason_t::Step;
					}

					break;
				}
				case SQ_HOOK_RETURN:
				{
					RestoreCachedInstructions();
					ClearCachedInstructions();

					if ( ci != vm->_callsstack )
					{
						if ( InstructionStep( vm, ci - 1, 1 ) )
						{
							m_State = ThreadState_StepInInstruction;
						}
						else
						{
							m_State = ThreadState_NextStatement;
						}
					}

					break;
				}
				default: UNREACHABLE();
			}

			break;
		}
		case ThreadState_StepOutInstruction:
		{
			if ( type == SQ_HOOK_LINE &&
					m_nCalls < m_nStateCalls &&
					vm == m_pStateVM )
			{
				if ( (ci->_ip-1)->_arg1 != 0 )
					break;

				breakReason.reason = breakreason_t::Step;

				RestoreCachedInstructions();
				ClearCachedInstructions();
				ci->_ip--;
			}

			break;
		}
		default: break;
	}

	switch ( type )
	{
		case SQ_HOOK_LINE:
		{
			if ( !sourcename )
				break;

			int srclen = SQStringFromSQChar( sourcename )->_len;
			Assert( (int)scstrlen(sourcename) == srclen );
#ifdef SQDBG_SOURCENAME_HAS_PATH
			StripFileName( &sourcename, &srclen );
#endif
			sqstring_t src;
			src.Assign( sourcename, srclen );

			breakpoint_t *bp = GetBreakpoint( line, src );

			if ( bp )
			{
				if ( HasCondition( bp ) )
				{
					// Breakpoint condition function call can reallocate call stack
					int frame = ci - vm->_callsstack;

					if ( !CheckBreakpointCondition( bp, vm, ci ) )
					{
						ci = vm->_callsstack + frame;
						break;
					}

					ci = vm->_callsstack + frame;
				}

				++bp->hits;

				if ( bp->hitsTarget )
				{
					if ( bp->hits < bp->hitsTarget )
						break;

					bp->hits = 0;
				}

				if ( bp->logMessage.IsEmpty() )
				{
					stringbuf_t< 64 > buf;
					buf.Puts( "(sqdbg) Breakpoint hit " );
					buf.Puts( src );
					buf.Put(':');
					buf.PutInt( line );
					buf.Put('\n');
					buf.Term();

					_OutputDebugStringA( buf.ptr );
					SendEvent_OutputStdOut( string_t( buf ), ci );
					breakReason.reason = breakreason_t::Breakpoint;
					breakReason.id = bp->id;
				}
				else
				{
					// Use frame index instead of ci, variable evaluation can reallocate call stack
					TracePoint( bp, vm, ci - vm->_callsstack );
				}
			}

			break;
		}
		case SQ_HOOK_CALL:
		{
			m_nCalls++;

			sqstring_t func, src;

			if ( funcname )
			{
				int funclen = SQStringFromSQChar( funcname )->_len;
				Assert( (int)scstrlen(funcname) == funclen );
				func.Assign( funcname, funclen );
			}
			else
			{
				func.Assign( _SC("") );
			}

			if ( sourcename )
			{
				int srclen = SQStringFromSQChar( sourcename )->_len;
				Assert( (int)scstrlen(sourcename) == srclen );
#ifdef SQDBG_SOURCENAME_HAS_PATH
				StripFileName( &sourcename, &srclen );
#endif
				src.Assign( sourcename, srclen );
			}
			else
			{
				src.Assign( _SC("") );
			}

#ifdef _DEBUG
			{
				bool bGenerator = ( _fp(_closure(ci->_closure)->_function)->_bgenerator &&
						ci->_ip != _fp(_closure(ci->_closure)->_function)->_instructions &&
						(ci->_ip-1)->op == _OP_YIELD );
				int decline = GetFunctionDeclarationLine( _fp(_closure(ci->_closure)->_function) );
				AssertMsg2( bGenerator || line == decline, "unexpected func line %d != %d", (int)line, decline );
			}
#endif

			breakpoint_t *bp = GetFunctionBreakpoint( func, src, line );

			if ( bp )
			{
				if ( HasCondition( bp ) )
				{
					// Breakpoint condition function call can reallocate call stack
					int frame = ci - vm->_callsstack;

					if ( !CheckBreakpointCondition( bp, vm, ci ) )
					{
						ci = vm->_callsstack + frame;
						break;
					}

					ci = vm->_callsstack + frame;
				}

				if ( bp->hitsTarget )
				{
					if ( ++bp->hits < bp->hitsTarget )
						break;

					bp->hits = 0;
				}

				stringbuf_t< 128 > buf;

				if ( funcname )
				{
					if ( !src.IsEmpty() )
					{
						buf.Puts( "(sqdbg) Breakpoint hit " );
						buf.Puts( func );
						buf.Puts( "() @ " );
						buf.Puts( src );

						if ( line )
						{
							buf.Put(':');
							buf.PutInt( line );
						}

						buf.Put('\n');
						buf.Term();
					}
					else
					{
						buf.Puts( "(sqdbg) Breakpoint hit " );
						buf.Puts( func );
						buf.Puts( "()\n" );
						buf.Term();
					}
				}
				else
				{
					if ( !src.IsEmpty() )
					{
						buf.Puts( "(sqdbg) Breakpoint hit 'anonymous function' @ " );
						buf.Puts( src );

						if ( line )
						{
							buf.Put(':');
							buf.PutInt( line );
						}

						buf.Put('\n');
						buf.Term();
					}
					else
					{
						buf.Puts( "(sqdbg) Breakpoint hit 'anonymous function'\n" );
						buf.Term();
					}
				}

				_OutputDebugStringA( buf.ptr );
				SendEvent_OutputStdOut( string_t( buf ), ci );
				breakReason.reason = breakreason_t::FunctionBreakpoint;
				breakReason.id = bp->id;
			}

#ifndef SQDBG_DISABLE_PROFILER
			if ( IsProfilerEnabled() && m_pProfiler && m_pProfiler->IsActive() &&
					// Ignore repl
					!src.IsEqualTo(_SC("sqdbg")) )
			{
				SQFunctionProto *func = _fp(_closure(ci->_closure)->_function);
				bool bGenerator = ( func->_bgenerator && ci->_ip == func->_instructions );

				if ( !bGenerator || ( ci != vm->_callsstack && ((ci-1)->_ip-1)->op == _OP_RESUME ) )
				{
					m_pProfiler->CallBegin( func );
				}
			}
#endif
			break;
		}
		case SQ_HOOK_RETURN:
		{
#ifndef SQDBG_DISABLE_PROFILER
			if ( IsProfilerEnabled() && m_pProfiler && m_pProfiler->IsActive() &&
					// Ignore repl
					( !sourcename ||
					  !sqstring_t(_SC("sqdbg")).IsEqualTo( SQStringFromSQChar( sourcename ) ) ) )
			{
				SQFunctionProto *func = _fp(_closure(ci->_closure)->_function);
				bool bGenerator = ( func->_bgenerator && ci->_ip == func->_instructions );

				if ( !bGenerator )
				{
					m_pProfiler->CallEnd();
				}
			}
#endif

			m_nCalls--;

			if ( m_State != ThreadState_Running &&
					ci == vm->_callsstack &&
					vm == m_pRootVM ) // if exiting thread, step into root
			{
				Continue( vm );
			}
			else if ( ( m_State == ThreadState_StepOver && m_nCalls <= m_nStateCalls ) ||
					// StepOut was changed to this
					( m_State == ThreadState_NextStatement && m_nCalls+1 <= m_nStateCalls ) ||
					( m_State == ThreadState_StepOverInstruction && m_nCalls <= m_nStateCalls ) ||
					( m_State == ThreadState_StepOutInstruction && m_nCalls+1 <= m_nStateCalls ) ||
					( m_State == ThreadState_StepIn || m_State == ThreadState_StepInInstruction ) )
			{
				bool bGenerator = ( _fp(_closure(ci->_closure)->_function)->_bgenerator &&
						ci->_ip == _fp(_closure(ci->_closure)->_function)->_instructions );

				if ( !bGenerator &&
						( (ci->_ip-1)->op == _OP_RETURN || (ci->_ip-1)->op == _OP_YIELD ) &&
						(ci->_ip-1)->_arg0 != 0xFF )
				{
#ifdef NATIVE_DEBUG_HOOK
					const SQObjectPtr &val = vm->_stack._vals[ vm->_stackbase + (ci->_ip-1)->_arg1 ];
#else
					const SQObjectPtr &val = vm->_stack._vals[
						vm->_stackbase - vm->ci->_prevstkbase + (ci->_ip-1)->_arg1 ];
#endif
					if ( !m_ReturnValues.size() ||
							_rawval(m_ReturnValues.top().value) != _rawval(val) ||
							sq_type(m_ReturnValues.top().value) != sq_type(val) )
					{
						returnvalue_t &rv = m_ReturnValues.append();
						rv.value = val;
						rv.funcname = funcname ? SQStringFromSQChar( funcname ) : NULL;

						if ( (ci->_ip-1)->op == _OP_YIELD &&
								// Keep track of yields up to 32 times at once
								m_ReturnValues.size() < (int)(sizeof(m_iYieldValues) << 3) )
						{
							m_iYieldValues |= 1 << m_ReturnValues.size();
						}
					}
				}
			}

			break;
		}
		default: UNREACHABLE();
	}

	if ( breakReason.reason )
		Break( vm, breakReason );

	if ( m_State == ThreadState_SuspendNow )
		Suspend();

#if SQUIRREL_VERSION_NUMBER < 300
	m_bInDebugHook = false;
#endif
}

#ifndef SQDBG_DISABLE_PROFILER
void SQDebugServer::ProfHook( HSQUIRRELVM vm, SQInteger type )
{
	Assert( !IsClientConnected() );

	if ( m_pCurVM != vm )
	{
		m_pCurVM = vm;
		ProfSwitchThread( vm );
	}

	if ( m_pProfiler && m_pProfiler->IsActive() )
	{
#ifdef NATIVE_DEBUG_HOOK
		const SQVM::CallInfo *ci = vm->ci;
#else
		const SQVM::CallInfo *ci = vm->ci - 1;
#endif

		SQFunctionProto *func = _fp(_closure(ci->_closure)->_function);
		bool bGenerator = ( func->_bgenerator && ci->_ip == func->_instructions );

		switch ( type )
		{
			case SQ_HOOK_CALL:
			{
				if ( !bGenerator || ( ci != vm->_callsstack && ((ci-1)->_ip-1)->op == _OP_RESUME ) )
				{
					m_pProfiler->CallBegin( func );
				}

				break;
			}
			case SQ_HOOK_RETURN:
			{
				if ( !bGenerator )
				{
					m_pProfiler->CallEnd();
				}

				break;
			}
			default: UNREACHABLE();
		}
	}
}
#endif

template < typename T >
void SQDebugServer::SendEvent_OutputStdOut( const T &strOutput, const SQVM::CallInfo *ci )
{
	Assert( !ci || sq_type(ci->_closure) == OT_CLOSURE );

	if ( IsClientConnected() )
	{
		DAP_START_EVENT( ++m_Sequence, "output" );
		DAP_SET_TABLE( body, 4 );
			body.SetString( "category", "stdout" );
			body.SetStringNoCopy( "output", strOutput );
			if ( ci )
			{
				SQFunctionProto *func = _fp(_closure(ci->_closure)->_function);
				if ( !sqstring_t(_SC("sqdbg")).IsEqualTo( _string(func->_sourcename) ) )
				{
					body.SetInt( "line", (int)func->GetLine( ci->_ip ) );
					json_table_t &source = body.SetTable( "source", 2 );
					SetSource( source, _string(func->_sourcename) );
				}
			}
		DAP_SEND();
	}
}

void SQDebugServer::OnSQPrint( HSQUIRRELVM vm, const SQChar *buf, int len )
{
	m_Print( vm, buf );

	const SQVM::CallInfo *ci = NULL;

	// Assume the latest script function is the output source
	for ( int i = vm->_callsstacksize; i-- > 0; )
	{
		if ( sq_type(vm->_callsstack[i]._closure) == OT_CLOSURE )
		{
			ci = vm->_callsstack + i;
			break;
		}
	}

	SendEvent_OutputStdOut( sqstring_t( buf, len ), ci );
}

void SQDebugServer::OnSQError( HSQUIRRELVM vm, const SQChar *buf, int len )
{
	m_PrintError( vm, buf );

	const SQVM::CallInfo *ci = NULL;

	// Assume the latest script function is the output source
	for ( int i = vm->_callsstacksize; i-- > 0; )
	{
		if ( sq_type(vm->_callsstack[i]._closure) == OT_CLOSURE )
		{
			ci = vm->_callsstack + i;
			break;
		}
	}

	SendEvent_OutputStdOut( sqstring_t( buf, len ), ci );
}


static inline HSQDEBUGSERVER sqdbg_get( HSQUIRRELVM vm );
static inline HSQDEBUGSERVER sqdbg_get_debugger( HSQUIRRELVM vm );
#ifdef NATIVE_DEBUG_HOOK
#ifdef DEBUG_HOOK_CACHED_SQDBG
static inline HSQDEBUGSERVER sqdbg_get_debugger_cached_debughook( HSQUIRRELVM vm );
#else
#define sqdbg_get_debugger_cached_debughook sqdbg_get_debugger
#endif
#endif

SQInteger SQDebugServer::SQDefineClass( HSQUIRRELVM vm )
{
	SQDebugServer *dbg = sqdbg_get( vm );
	if ( dbg )
	{
		HSQOBJECT target;
		HSQOBJECT params;

		sq_getstackobj( vm, -2, &target );
		sq_getstackobj( vm, -1, &params );

		Assert( sq_type(target) == OT_CLASS && sq_type(params) == OT_TABLE );

		dbg->DefineClass( _class(target), _table(params) );
	}

	return 0;
}

SQInteger SQDebugServer::SQPrintDisassembly( HSQUIRRELVM vm )
{
	SQDebugServer *dbg = sqdbg_get( vm );
	if ( dbg )
	{
		HSQOBJECT target;
		sq_getstackobj( vm, -1, &target );

		if ( sq_type(target) != OT_CLOSURE )
			return sq_throwerror( vm, _SC("expected closure") );

		int buflen = dbg->DisassemblyBufLen( _closure(target) );
		// NOTE: Both sqdbg and sq scratch pads are reused within this function call
		SQChar *scratch = (SQChar*)sqdbg_malloc( sq_rsl(buflen) );

		sqstring_t str = dbg->PrintDisassembly( _closure(target), scratch, sq_rsl(buflen) );
		sq_pushstring( vm, str.ptr, str.len );

		sqdbg_free( scratch, sq_rsl(buflen) );
		return 1;
	}

	sq_pushnull( vm );
	return 1;
}

#ifndef SQDBG_DISABLE_PROFILER
SQInteger SQDebugServer::SQProfStart( HSQUIRRELVM vm )
{
	SQDebugServer *dbg = sqdbg_get( vm );
	if ( dbg && !dbg->IsProfilerEnabled() )
	{
		dbg->ProfStart();
	}

	return 0;
}

SQInteger SQDebugServer::SQProfStop( HSQUIRRELVM vm )
{
	SQDebugServer *dbg = sqdbg_get( vm );
	if ( dbg && dbg->IsProfilerEnabled() )
	{
		dbg->ProfStop();
	}

	return 0;
}

SQInteger SQDebugServer::SQProfPause( HSQUIRRELVM vm )
{
	SQDebugServer *dbg = sqdbg_get( vm );
	if ( dbg && dbg->IsProfilerEnabled() )
	{
		dbg->ProfPause( vm );
	}

	return 0;
}

SQInteger SQDebugServer::SQProfResume( HSQUIRRELVM vm )
{
	SQDebugServer *dbg = sqdbg_get( vm );
	if ( dbg && dbg->IsProfilerEnabled() )
	{
		dbg->ProfResume( vm );
	}

	return 0;
}

SQInteger SQDebugServer::SQProfGroupBegin( HSQUIRRELVM vm )
{
	SQDebugServer *dbg = sqdbg_get( vm );
	if ( dbg && dbg->IsProfilerEnabled() )
	{
		HSQOBJECT tag;
		sq_getstackobj( vm, -1, &tag );
		Assert( sq_type(tag) == OT_STRING );
		dbg->ProfGroupBegin( vm, _string(tag) );
	}

	return 0;
}

SQInteger SQDebugServer::SQProfGroupEnd( HSQUIRRELVM vm )
{
	SQDebugServer *dbg = sqdbg_get( vm );
	if ( dbg && dbg->IsProfilerEnabled() )
	{
		dbg->ProfGroupEnd( vm );
	}

	return 0;
}

SQInteger SQDebugServer::SQProfGet( HSQUIRRELVM vm )
{
	SQDebugServer *dbg = sqdbg_get( vm );
	if ( dbg && dbg->IsProfilerEnabled() )
	{
		SQInteger top = sq_gettop(vm);
		HSQOBJECT arg1;
		sq_resetobject( &arg1 );
		HSQUIRRELVM thread = vm;

		if ( top > 3 )
		{
			return sq_throwerror( vm, _SC("wrong number of parameters") );
		}
		else if ( top > 2 )
		{
			sq_getstackobj( vm, -2, &arg1 );

			if ( sq_type(arg1) == OT_THREAD )
			{
				thread = _thread(arg1);
				sq_resetobject( &arg1 );
			}
		}

		sq_getstackobj( vm, -1, &arg1 );

		sqstring_t str;

		switch ( sq_type(arg1) )
		{
			case OT_STRING:
			{
				str = dbg->ProfGet( thread, _string(arg1), 0 );
				break;
			}
			case OT_INTEGER:
			{
				str = dbg->ProfGet( thread, NULL, _integer(arg1) );
				break;
			}
			default:
			{
				return sq_throwerror( vm, _SC("expected report type (integer) or group name (string)") );
			}
		}

		if ( str.len )
		{
			sq_pushstring( vm, str.ptr, str.len );
			return 1;
		}
	}

	sq_pushnull( vm );
	return 1;
}

SQInteger SQDebugServer::SQProfPrint( HSQUIRRELVM vm )
{
	SQDebugServer *dbg = sqdbg_get( vm );
	if ( dbg && dbg->IsProfilerEnabled() )
	{
		SQInteger top = sq_gettop(vm);
		HSQOBJECT arg1;
		sq_resetobject( &arg1 );
		HSQUIRRELVM thread = vm;

		if ( top > 3 )
		{
			return sq_throwerror( vm, _SC("wrong number of parameters") );
		}
		else if ( top > 2 )
		{
			sq_getstackobj( vm, -2, &arg1 );

			if ( sq_type(arg1) == OT_THREAD )
			{
				thread = _thread(arg1);
				sq_resetobject( &arg1 );
			}
		}

		sq_getstackobj( vm, -1, &arg1 );

		switch ( sq_type(arg1) )
		{
			case OT_STRING:
			{
				dbg->ProfPrint( thread, _string(arg1), 0 );
				break;
			}
			case OT_INTEGER:
			{
				dbg->ProfPrint( thread, NULL, _integer(arg1) );
				break;
			}
			default:
			{
				return sq_throwerror( vm, _SC("expected report type (integer) or group name (string)") );
			}
		}
	}

	return 0;
}
#endif

SQInteger SQDebugServer::SQBreak( HSQUIRRELVM vm )
{
	SQDebugServer *dbg = sqdbg_get( vm );
	if ( dbg && dbg->IsClientConnected() )
	{
		if ( dbg->m_State != ThreadState_Suspended &&
				( !dbg->m_bDebugHookGuard || !dbg->m_bInREPL ) )
		{
			dbg->m_pPausedThread = vm;
			dbg->InstructionStep( vm, vm->ci - 1, 1 );
		}
	}

	return 0;
}

#ifndef SQDBG_PRINTBUF_SIZE
#define SQDBG_PRINTBUF_SIZE 2048
#endif

void SQDebugServer::SQPrint( HSQUIRRELVM vm, const SQChar *fmt, ... )
{
	SQDebugServer *dbg = sqdbg_get_debugger( vm );
	Assert( dbg && dbg->IsClientConnected() );
	if ( dbg )
	{
		SQChar buf[ SQDBG_PRINTBUF_SIZE ];
		va_list va;
		va_start( va, fmt );
		int len = scvsprintf( buf, SQDBG_PRINTBUF_SIZE, fmt, va );
		va_end( va );

		if ( len < 0 || len > SQDBG_PRINTBUF_SIZE-1 )
			len = SQDBG_PRINTBUF_SIZE-1;

		_OutputDebugString( buf );
		dbg->OnSQPrint( vm, buf, len );
	}
}

void SQDebugServer::SQError( HSQUIRRELVM vm, const SQChar *fmt, ... )
{
	SQDebugServer *dbg = sqdbg_get_debugger( vm );
	Assert( dbg && dbg->IsClientConnected() );
	if ( dbg )
	{
		SQChar buf[ SQDBG_PRINTBUF_SIZE ];
		va_list va;
		va_start( va, fmt );
		int len = scvsprintf( buf, SQDBG_PRINTBUF_SIZE, fmt, va );
		va_end( va );

		if ( len < 0 || len > SQDBG_PRINTBUF_SIZE-1 )
			len = SQDBG_PRINTBUF_SIZE-1;

		_OutputDebugString( buf );
		dbg->OnSQError( vm, buf, len );
	}
}

#ifndef SQDBG_CALL_DEFAULT_ERROR_HANDLER
void SQDebugServer::SQErrorAtFrame( HSQUIRRELVM vm, const SQVM::CallInfo *ci, const SQChar *fmt, ... )
{
	SQDebugServer *dbg = sqdbg_get_debugger( vm );
	Assert( dbg && dbg->IsClientConnected() );
	if ( dbg )
	{
		SQChar buf[ SQDBG_PRINTBUF_SIZE ];
		va_list va;
		va_start( va, fmt );
		int len = scvsprintf( buf, SQDBG_PRINTBUF_SIZE, fmt, va );
		va_end( va );

		if ( len < 0 || len > SQDBG_PRINTBUF_SIZE-1 )
			len = SQDBG_PRINTBUF_SIZE-1;

		_OutputDebugString( buf );
		dbg->m_PrintError( vm, buf );
		dbg->SendEvent_OutputStdOut( sqstring_t( buf, len ), ci );
	}
}
#endif

SQInteger SQDebugServer::SQErrorHandler( HSQUIRRELVM vm )
{
	SQDebugServer *dbg = sqdbg_get_debugger( vm );
	if ( dbg && dbg->IsClientConnected() )
	{
		dbg->ErrorHandler( vm );
	}

	return 0;
}

#ifdef NATIVE_DEBUG_HOOK
void SQDebugServer::SQDebugHook( HSQUIRRELVM vm, SQInteger type,
		const SQChar *sourcename, SQInteger line, const SQChar *funcname )
{
	SQDebugServer *dbg = sqdbg_get_debugger_cached_debughook( vm );
	if ( dbg )
	{
		// NOTE: SetDebugHook does not set threads unknown to the debugger,
		// i.e. yet uncalled threads. This causes threads that were created while
		// a client was connected to call the debug hook after the client disconnects.
		// Check IsClientConnected here to catch those threads.
		if ( dbg->IsClientConnected() )
		{
			dbg->DebugHook( vm, type, sourcename, line, funcname );
		}
		else
		{
			dbg->DoSetDebugHook( vm, NULL );
		}
	}
}
#else
SQInteger SQDebugServer::SQDebugHook( HSQUIRRELVM vm )
{
	SQDebugServer *dbg = sqdbg_get( vm );
	if ( dbg )
	{
		if ( dbg->IsClientConnected() )
		{
			HSQOBJECT type;
			HSQOBJECT sourcename;
			HSQOBJECT line;
			HSQOBJECT funcname;

			sq_getstackobj( vm, -4, &type );
			sq_getstackobj( vm, -3, &sourcename );
			sq_getstackobj( vm, -2, &line );
			sq_getstackobj( vm, -1, &funcname );

			Assert( sq_type(type) == OT_INTEGER &&
					( sq_type(sourcename) == OT_STRING || sq_type(sourcename) == OT_NULL ) &&
					sq_type(line) == OT_INTEGER &&
					( sq_type(funcname) == OT_STRING || sq_type(funcname) == OT_NULL ) );

			const SQChar *src = sq_type(sourcename) == OT_STRING ? _stringval( sourcename ) : NULL;
			const SQChar *fun = sq_type(funcname) == OT_STRING ? _stringval( funcname ) : NULL;

			dbg->DebugHook( vm, _integer(type), src, _integer(line), fun );
		}
		else
		{
			dbg->DoSetDebugHook( vm, NULL );
		}
	}

	return 0;
}
#endif

#ifndef SQDBG_DISABLE_PROFILER
#ifdef NATIVE_DEBUG_HOOK
void SQDebugServer::SQProfHook( HSQUIRRELVM vm, SQInteger type,
		const SQChar *sourcename, SQInteger, const SQChar * )
{
	if ( type == SQ_HOOK_CALL || type == SQ_HOOK_RETURN )
	{
		SQDebugServer *dbg = sqdbg_get_debugger_cached_debughook( vm );
		Assert( dbg && !dbg->IsClientConnected() );
		if ( dbg && dbg->IsProfilerEnabled() )
		{
			// Rare case, client disconnected while waiting for repl response
			if ( !sourcename ||
					!sqstring_t(_SC("sqdbg")).IsEqualTo( SQStringFromSQChar( sourcename ) ) )
			{
				dbg->ProfHook( vm, type );
			}
		}
	}
}
#else
SQInteger SQDebugServer::SQProfHook( HSQUIRRELVM vm )
{
	HSQOBJECT type;
	sq_getstackobj( vm, -4 - 1, &type ); // -1 for debugger (sqdbg_get)
	Assert( sq_type(type) == OT_INTEGER );

	if ( _integer(type) == SQ_HOOK_CALL || _integer(type) == SQ_HOOK_RETURN )
	{
		SQDebugServer *dbg = sqdbg_get( vm );
		Assert( dbg && !dbg->IsClientConnected() );
		if ( dbg && dbg->IsProfilerEnabled() )
		{
			HSQOBJECT src;
			sq_getstackobj( vm, -3 - 1, &src );
			// Rare case, client disconnected while waiting for repl response
			if ( sq_type(src) != OT_STRING || !sqstring_t(_SC("sqdbg")).IsEqualTo( _string(src) ) )
			{
				dbg->ProfHook( vm, _integer(type) );
			}
		}
	}

	return 0;
}
#endif
#endif

#define SQDBG_SV_TAG "__sqdbg__"

class CDebuggerScriptRef
{
public:
	SQDebugServer *dbg;
};

static SQInteger OnSQVMShutdown( SQUserPointer ppRef, SQInteger )
{
	SQDebugServer *dbg = ((CDebuggerScriptRef*)_userdataval(*(SQObjectPtr*)ppRef))->dbg;

	if ( dbg )
	{
		dbg->Shutdown();
		dbg->~SQDebugServer();
		sqdbg_free( dbg, sizeof(SQDebugServer) );
	}

	((SQObjectPtr*)ppRef)->Null();

	return 0;
}

// For use within native calls from scripts
// Gets native closure outer variable
HSQDEBUGSERVER sqdbg_get( HSQUIRRELVM vm )
{
	HSQOBJECT ref;
	sq_getstackobj( vm, -1, &ref );
	Assert( sq_type(ref) == OT_USERDATA );
	sq_poptop( vm );
	return ((CDebuggerScriptRef*)_userdataval(ref))->dbg;
}

HSQDEBUGSERVER sqdbg_get_debugger( HSQUIRRELVM vm )
{
	// Use SQTable_Get to reduce hot path with stack operations
	SQObjectPtr ref;
	if ( SQTable_Get( _table(_ss(vm)->_registry), _SC(SQDBG_SV_TAG), ref ) )
	{
		Assert( sq_type(ref) == OT_USERDATA );
		return ((CDebuggerScriptRef*)_userdataval(ref))->dbg;
	}

	return NULL;
}

void sqdbg_get_debugger_ref( HSQUIRRELVM vm, SQObjectPtr &ref )
{
	SQTable_Get( _table(_ss(vm)->_registry), _SC(SQDBG_SV_TAG), ref );
	Assert( sq_type(ref) == OT_NULL || sq_type(ref) == OT_USERDATA );
}

#ifdef DEBUG_HOOK_CACHED_SQDBG
// Cache the debugger in an unused variable in the VM
// for at least 20% faster access on debug hook
// compared to registry table access
void sqdbg_set_debugger_cached_debughook( HSQUIRRELVM vm, bool state )
{
	if ( state )
	{
		SQObjectPtr ref;
		sqdbg_get_debugger_ref( vm, ref );

		vm->_debughook_closure = ref;
	}
	else Assert( sq_type(vm->_debughook_closure) == OT_NULL );
}

HSQDEBUGSERVER sqdbg_get_debugger_cached_debughook( HSQUIRRELVM vm )
{
	Assert( sq_type(vm->_debughook_closure) == OT_USERDATA );
	return ((CDebuggerScriptRef*)_userdataval(vm->_debughook_closure))->dbg;
}
#endif

HSQDEBUGSERVER sqdbg_attach_debugger( HSQUIRRELVM vm )
{
	CDebuggerScriptRef *ref = NULL;

	CStackCheck stackcheck( vm );

	sq_pushregistrytable( vm );
	sq_pushstring( vm, _SC(SQDBG_SV_TAG), -1 );

	if ( SQ_SUCCEEDED( sq_get( vm, -2 ) ) )
	{
		HSQOBJECT o;
		sq_getstackobj( vm, -1, &o );
		Assert( sq_type(o) == OT_USERDATA );
		sq_pop( vm, 2 );

		ref = (CDebuggerScriptRef*)_userdataval(o);

		if ( ref->dbg )
			return ref->dbg;
	}

	if ( !ref )
	{
		// Referenced by script functions and the registry
		sq_pushstring( vm, _SC(SQDBG_SV_TAG), -1 );
		ref = (CDebuggerScriptRef*)sq_newuserdata( vm, sizeof(CDebuggerScriptRef) );
		sq_newslot( vm, -3, SQFalse );

		// Only referenced by the registry
		sq_pushstring( vm, _SC(SQDBG_SV_TAG "*"), -1 );
		//
		// NOTE: ref can be freed while shutdown is in progress
		// through the release of references to the root table
		// in the debugger releasing the final references to the
		// debugger reference in script functions.
		// Using SQObjectPtr here to delay the release of the debugger ref,
		// otherwise this could be a pointer to CDebuggerScriptRef.
		// This issue practically only happens on SQ2
		//
		// The release hook isn't set on the debugger ref itself
		// because of its references on script functions causing
		// its release hook to be called after the VM was released,
		// which debugger shutdown requires for sq_release API calls
		// although it doesn't require the VM but the SS.
		// An alternative would be to guard sq api calls in debugger shutdown.
		//
		SQObjectPtr *ppRef = (SQObjectPtr*)sq_newuserdata( vm, sizeof(SQObjectPtr) );
		memzero( ppRef );
		sq_setreleasehook( vm, -1, &OnSQVMShutdown );
		sq_newslot( vm, -3, SQFalse );

		SQObjectPtr o;
		sqdbg_get_debugger_ref( vm, o );
		*ppRef = o;

		sq_poptop( vm );
	}

	ref->dbg = (SQDebugServer*)sqdbg_malloc( sizeof(SQDebugServer) );
	new ( ref->dbg ) SQDebugServer;

	ref->dbg->Attach( vm );

	return ref->dbg;
}

void sqdbg_destroy_debugger( HSQUIRRELVM vm )
{
	CStackCheck stackcheck( vm );

	sq_pushregistrytable( vm );
	sq_pushstring( vm, _SC(SQDBG_SV_TAG), -1 );

	if ( SQ_SUCCEEDED( sq_get( vm, -2 ) ) )
	{
		HSQOBJECT o;
		sq_getstackobj( vm, -1, &o );
		Assert( sq_type(o) == OT_USERDATA );

		CDebuggerScriptRef *ref = (CDebuggerScriptRef*)_userdataval(o);

		if ( ref->dbg )
		{
			ref->dbg->Shutdown();
			ref->dbg->~SQDebugServer();
			sqdbg_free( ref->dbg, sizeof(SQDebugServer) );
			ref->dbg = NULL;
		}

		sq_poptop( vm );
	}

	sq_poptop( vm );
}

int sqdbg_listen_socket( HSQDEBUGSERVER dbg, unsigned short port )
{
	return ( dbg->ListenSocket( port ) == false );
}

void sqdbg_frame( HSQDEBUGSERVER dbg )
{
	dbg->Frame();
}

void sqdbg_on_script_compile( HSQDEBUGSERVER dbg, const SQChar *script, SQInteger size,
		const SQChar *sourcename, SQInteger sourcenamelen )
{
	dbg->OnScriptCompile( script, size, sourcename, sourcenamelen );
}
