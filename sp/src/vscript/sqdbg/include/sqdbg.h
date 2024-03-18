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

struct SQDebugServer;
typedef SQDebugServer* HSQDEBUGSERVER;

#ifdef __cplusplus
extern "C" {
#endif

// Create and attach a new debugger
// Memory is owned by the VM, it is freed when the VM dies or
// the debugger is disconnected via sqdbg_destroy_debugger()
extern HSQDEBUGSERVER sqdbg_attach_debugger( HSQUIRRELVM vm );

// Detach and destroy the debugger attached to this VM
// Invalidates the handle returned from sqdbg_attach_debugger()
extern void sqdbg_destroy_debugger( HSQUIRRELVM vm );

// Open specified port and allow client connections
// If port is 0, the system will choose a unique available port
// Returns 0 on success
extern int sqdbg_listen_socket( HSQDEBUGSERVER dbg, unsigned short port );

// Process client connections and incoming messages
// Blocks on script breakpoints while a client is connected
extern void sqdbg_frame( HSQDEBUGSERVER dbg );

// Copies the script to be able to source it to debugger clients
extern void sqdbg_on_script_compile( HSQDEBUGSERVER dbg, const SQChar *script, SQInteger size,
		const SQChar *sourcename, SQInteger sourcenamelen );

#ifdef __cplusplus
}
#endif

#endif // SQDBG_H
