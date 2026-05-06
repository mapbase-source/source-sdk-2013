//-----------------------------------------------------------------------
//                       github.com/samisalreadytaken/sqdbg
//-----------------------------------------------------------------------
//

#ifndef SQDBG_DEBUG_H
#define SQDBG_DEBUG_H

#if 0

#ifdef _DEBUG
	#ifdef _WIN32
		#include <crtdbg.h>

		bool __IsDebuggerPresent();
		const char *GetModuleBaseName();

		#define DebuggerBreak() do { if ( __IsDebuggerPresent() ) __debugbreak(); } while(0)

		#define Assert( x ) \
			do { \
				__CAT( L, __LINE__ ): \
				if ( !(x) && (1 == _CrtDbgReport(_CRT_ASSERT, __FILE__, __LINE__, GetModuleBaseName(), #x)) ) \
				{ \
					if ( !__IsDebuggerPresent() ) \
						goto __CAT( L, __LINE__ ); \
					__debugbreak(); \
				} \
			} while(0)

		#define AssertMsg( x, msg ) \
			do { \
				__CAT( L, __LINE__ ): \
				if ( !(x) && (1 == _CrtDbgReport(_CRT_ASSERT, __FILE__, __LINE__, GetModuleBaseName(), msg)) ) \
				{ \
					if ( !__IsDebuggerPresent() ) \
						goto __CAT( L, __LINE__ ); \
					__debugbreak(); \
				} \
			} while(0)

		#define AssertMsg1( x, msg, a1 ) \
			do { \
				__CAT( L, __LINE__ ): \
				if ( !(x) && (1 == _CrtDbgReport(_CRT_ASSERT, __FILE__, __LINE__, GetModuleBaseName(), msg, a1)) ) \
				{ \
					if ( !__IsDebuggerPresent() ) \
						goto __CAT( L, __LINE__ ); \
					__debugbreak(); \
				} \
			} while(0)

		#define AssertMsg2( x, msg, a1, a2 ) \
			do { \
				__CAT( L, __LINE__ ): \
				if ( !(x) && (1 == _CrtDbgReport(_CRT_ASSERT, __FILE__, __LINE__, GetModuleBaseName(), msg, a1, a2)) ) \
				{ \
					if ( !__IsDebuggerPresent() ) \
						goto __CAT( L, __LINE__ ); \
					__debugbreak(); \
				} \
			} while(0)
	#else
		extern "C" int printf(const char *, ...);

		#define DebuggerBreak() asm("int3")

		#define Assert( x ) \
			do { \
				if ( !(x) ) \
				{ \
					::printf("Assertion failed %s:%d: %s\n", __FILE__, __LINE__, #x); \
					DebuggerBreak(); \
				} \
			} while(0)

		#define AssertMsg( x, msg ) \
			do { \
				if ( !(x) ) \
				{ \
					::printf("Assertion failed %s:%d: %s\n", __FILE__, __LINE__, msg); \
					DebuggerBreak(); \
				} \
			} while(0)

		#define AssertMsg1( x, msg, a1 ) \
			do { \
				if ( !(x) ) \
				{ \
					::printf("Assertion failed %s:%d: ", __FILE__, __LINE__); \
					::printf(msg, a1); \
					::printf("\n"); \
					DebuggerBreak(); \
				} \
			} while(0)

		#define AssertMsg2( x, msg, a1, a2 ) \
			do { \
				if ( !(x) ) \
				{ \
					::printf("Assertion failed %s:%d: ", __FILE__, __LINE__); \
					::printf(msg, a1, a2); \
					::printf("\n"); \
					DebuggerBreak(); \
				} \
			} while(0)
	#endif
	#define Verify( x ) Assert(x)
	#define STATIC_ASSERT( x ) static_assert( x, #x )
#else
	#define DebuggerBreak() ((void)0)
	#define Assert( x ) ((void)0)
	#define AssertMsg( x, msg ) ((void)0)
	#define AssertMsg1( x, msg, a1 ) ((void)0)
	#define AssertMsg2( x, msg, a1, a2 ) ((void)0)
	#define Verify( x ) x
	#define STATIC_ASSERT( x )
#endif // _DEBUG

#endif

#include <tier0/dbg.h>

#define STATIC_ASSERT COMPILE_TIME_ASSERT

// Misdefined for GCC in platform.h
#undef UNREACHABLE

#ifdef _WIN32
	#define UNREACHABLE() do { Assert(!"UNREACHABLE"); __assume(0); } while(0)
#else
	#define UNREACHABLE() do { Assert(!"UNREACHABLE"); __builtin_unreachable(); } while(0)
#endif

#endif // SQDBG_DEBUG_H
