//-----------------------------------------------------------------------
//                       github.com/samisalreadytaken/sqdbg
//-----------------------------------------------------------------------
//

#ifndef SQDBG_DEBUG_H
#define SQDBG_DEBUG_H

#if 0

#if defined(_WIN32) && !defined(__MINGW32__)
	#define DebuggerBreak() __debugbreak()
#else
	#define DebuggerBreak() asm("int3")
#endif

#ifdef _WIN32
	#define __IsDebuggerPresent() IsDebuggerPresent()
#else
	#define __IsDebuggerPresent() 0
#endif

#ifdef _DEBUG
	#if defined(_WIN32) && !defined(__MINGW32__)
		#include <crtdbg.h>

		const char *GetModuleBaseName();

		#define Assert( x ) \
			do { \
				__CAT( L, __LINE__ ): \
				if ( !(x) && \
						(1 == _CrtDbgReport(_CRT_ASSERT, __FILE__, __LINE__, GetModuleBaseName(), "%s", #x)) ) \
				{ \
					if ( !__IsDebuggerPresent() ) \
						goto __CAT( L, __LINE__ ); \
					__debugbreak(); \
				} \
			} while(0)

		#define AssertMsg( x, msg ) \
			do { \
				__CAT( L, __LINE__ ): \
				if ( !(x) && \
						(1 == _CrtDbgReport(_CRT_ASSERT, __FILE__, __LINE__, GetModuleBaseName(), msg)) ) \
				{ \
					if ( !__IsDebuggerPresent() ) \
						goto __CAT( L, __LINE__ ); \
					__debugbreak(); \
				} \
			} while(0)

		#define AssertMsg1( x, msg, a1 ) \
			do { \
				__CAT( L, __LINE__ ): \
				if ( !(x) && \
						(1 == _CrtDbgReport(_CRT_ASSERT, __FILE__, __LINE__, GetModuleBaseName(), msg, a1)) ) \
				{ \
					if ( !__IsDebuggerPresent() ) \
						goto __CAT( L, __LINE__ ); \
					__debugbreak(); \
				} \
			} while(0)

		#define AssertMsg2( x, msg, a1, a2 ) \
			do { \
				__CAT( L, __LINE__ ): \
				if ( !(x) && \
						(1 == _CrtDbgReport(_CRT_ASSERT, __FILE__, __LINE__, GetModuleBaseName(), msg, a1, a2)) ) \
				{ \
					if ( !__IsDebuggerPresent() ) \
						goto __CAT( L, __LINE__ ); \
					__debugbreak(); \
				} \
			} while(0)
	#else
		#ifndef __MINGW32__
		extern "C"
		#endif
		int printf(const char *, ...);

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
					::printf(msg "\n", a1); \
					DebuggerBreak(); \
				} \
			} while(0)

		#define AssertMsg2( x, msg, a1, a2 ) \
			do { \
				if ( !(x) ) \
				{ \
					::printf("Assertion failed %s:%d: ", __FILE__, __LINE__); \
					::printf(msg "\n", a1, a2); \
					DebuggerBreak(); \
				} \
			} while(0)
	#endif
	#define Verify( x ) Assert(x)
#else
	#define Assert( x ) ((void)0)
	#define AssertMsg( x, msg ) ((void)0)
	#define AssertMsg1( x, msg, a1 ) ((void)0)
	#define AssertMsg2( x, msg, a1, a2 ) ((void)0)
	#define Verify( x ) x
#endif // _DEBUG

#define STATIC_ASSERT( x ) static_assert( x, #x )

#endif

#include <tier0/dbg.h>

#define STATIC_ASSERT COMPILE_TIME_ASSERT

// Misdefined for GCC in platform.h
#undef UNREACHABLE

#ifdef _MSC_VER
	#define UNREACHABLE() do { Assert(!"UNREACHABLE"); __assume(0); } while(0)
#else
	#define UNREACHABLE() do { Assert(!"UNREACHABLE"); __builtin_unreachable(); } while(0)
#endif

#endif // SQDBG_DEBUG_H
