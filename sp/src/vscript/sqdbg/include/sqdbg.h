//-----------------------------------------------------------------------
//                       github.com/samisalreadytaken/sqdbg
//-----------------------------------------------------------------------
//
// Squirrel Debugger
//

#ifndef SQDBG_H
#define SQDBG_H

#include <squirrel.h>

#define SQDBG_SV_API_VER 1

#ifndef SQDBG_API
#ifdef SQDBG_DLL
	#if defined(_WIN32)
		#ifdef SQDBG_DLL_EXPORT
			#define SQDBG_API __declspec(dllexport)
		#else
			#define SQDBG_API __declspec(dllimport)
		#endif
	#elif defined(__GNUC__)
		#ifdef SQDBG_DLL_EXPORT
			#define SQDBG_API __attribute__((visibility("default")))
		#else
			#define SQDBG_API extern
		#endif
	#else
		#define SQDBG_API extern
	#endif
#else
	#define SQDBG_API extern
#endif
#endif

struct SQDebugServer;
typedef SQDebugServer* HSQDEBUGSERVER;

#ifdef __cplusplus
extern "C" {
#endif

// Create and attach a new debugger
// Memory is owned by the VM, it is freed when the VM dies or
// the debugger is disconnected via sqdbg_destroy_debugger()
SQDBG_API HSQDEBUGSERVER sqdbg_attach_debugger( HSQUIRRELVM vm );

// Detach and destroy the debugger attached to this VM
// Invalidates the handle returned from sqdbg_attach_debugger()
SQDBG_API void sqdbg_destroy_debugger( HSQUIRRELVM vm );

// Open specified port and allow client connections
// If port is 0, the system will choose a unique available port
// Returns 0 on success
SQDBG_API int sqdbg_listen_socket( HSQDEBUGSERVER dbg, unsigned short port );

// Process client connections and incoming messages
// Blocks on script breakpoints while a client is connected
SQDBG_API void sqdbg_frame( HSQDEBUGSERVER dbg );

// Copies the script to be able to source it to debugger clients
SQDBG_API void sqdbg_on_script_compile( HSQDEBUGSERVER dbg,
		const SQChar *script, SQInteger scriptlen,
		const SQChar *sourcename, SQInteger sourcenamelen );

// Check if a client is connected to the debugger
// Returns 0 if there is no client connected
SQDBG_API int sqdbg_is_client_connected( HSQDEBUGSERVER dbg );

#ifdef __cplusplus
}
#endif

#endif // SQDBG_H
