/* see copyright notice in squirrel.h */
#include <squirrel.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <sqstdsystem.h>


static SQInteger _system_getenv(HSQUIRRELVM v)
{
    const char *s;
    if(SQ_SUCCEEDED(sq_getstring(v,2,&s))){
        sq_pushstring(v,getenv(s),-1);
        return 1;
    }
    return 0;
}

static SQInteger _system_system(HSQUIRRELVM v)
{
    const char *s;
    if(SQ_SUCCEEDED(sq_getstring(v,2,&s))){
        sq_pushinteger(v,system(s));
        return 1;
    }
    return sq_throwerror(v,"wrong param");
}

static SQInteger _system_setenv(HSQUIRRELVM v)
{
    const char *envname,*envval;
    sq_getstring(v,2,&envname);
    sq_getstring(v,3,&envval);
    #if defined(_MSC_VER)
    int res = _putenv_s(envname,envval);
    #else
    int res = setenv(envname,envval,1);
    #endif
    if (res != 0)
        return sq_throwerror(v,"setenv() failed");
    return 0;
}

static SQInteger _system_remove(HSQUIRRELVM v)
{
    const char *s;
    sq_getstring(v,2,&s);
    if(remove(s)==-1)
        return sq_throwerror(v,"remove() failed");
    return 0;
}

static SQInteger _system_rename(HSQUIRRELVM v)
{
    const char *oldn,*newn;
    sq_getstring(v,2,&oldn);
    sq_getstring(v,3,&newn);
    if(rename(oldn,newn)==-1)
        return sq_throwerror(v,"rename() failed");
    return 0;
}


static const SQRegFunctionFromStr systemlib_funcs[] = {
    { _system_getenv, "getenv(name: string): string|null",            "Returns the value of the environment variable or null" },
    { _system_setenv, "setenv(name: string, value: string)",          "Sets the environment variable to the given value" },
    { _system_system, "system(cmd: string): int",                     "Executes a shell command and returns its exit code" },
    { _system_remove, "remove(path: string)",                         "Deletes the file at the given path, throws error in case of fail" },
    { _system_rename, "rename(old: string, new: string)",             "Renames the file from old to new path, throws error in case of fail" },
    { NULL, NULL, NULL }
};
#undef _DECL_FUNC


SQRESULT sqstd_register_command_line_args(HSQUIRRELVM v, int argc, char ** argv)
{
    sq_pushroottable(v);
    sq_pushstring(v, "__argv", -1);
    sq_newarray(v, 0);
    for (int idx = 0; idx < argc; idx++)
    {
        sq_pushstring(v, argv[idx], -1);
        sq_arrayappend(v, -2);
    }
    sq_newslot(v, -3, SQFalse);
    sq_pop(v,1);

    return SQ_OK;
}


SQRESULT sqstd_register_systemlib(HSQUIRRELVM v)
{
    SQInteger i = 0;
    while (systemlib_funcs[i].f) {
        sq_new_closure_slot_from_decl_string(v, systemlib_funcs[i].f, 0, systemlib_funcs[i].declstring, systemlib_funcs[i].docstring);
        i++;
    }
    return SQ_OK;
}
