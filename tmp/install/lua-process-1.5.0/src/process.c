/*
 *  Copyright (C) 2014 Masatoshi Teruya
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a 
 *  copy of this software and associated documentation files (the "Software"), 
 *  to deal in the Software without restriction, including without limitation 
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 *  and/or sell copies of the Software, and to permit persons to whom the 
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL 
 *  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 *  DEALINGS IN THE SOFTWARE.
 *
 *  tmpl/process_tmpl.c
 *  lua-process
 *
 *  Created by Masatoshi Teruya on 14/03/27.
 */

#include "lprocess.h"


extern char **environ;

// MARK: environment
static int getenv_lua( lua_State *L )
{
    char **ptr = environ;
    char *val = NULL;
    
    lua_newtable( L );
    while( *ptr )
    {
        if( ( val = strchr( *ptr, '=' ) ) ){
            lua_pushlstring( L, *ptr, (ptrdiff_t)val - (ptrdiff_t)*ptr );
            lua_pushstring( L, val + 1 );
            lua_rawset( L, -3 );
        }
        ptr++;
    }
    
    return 1;
}


// MARK: process id
static int getpid_lua( lua_State *L )
{
    lua_pushinteger( L, getpid() );
    return 1;
}

static int getppid_lua( lua_State *L )
{
    lua_pushinteger( L, getppid() );
    return 1;
}


// MARK: user/group id
#define getuinfo(dest,getter,arg,t,field) do{ \
    t *info = NULL; \
    errno = 0; \
    if( ( info = getter( arg ) ) ){ \
        *dest = info->field; \
        return 0; \
    } \
    /* not found */ \
    else if( !errno ){ \
        errno = EINVAL; \
    } \
    return -1; \
}while(0)


static inline int uname2uid( uid_t *uid, const char *uname )
{
    getuinfo( uid, getpwnam, uname, struct passwd, pw_uid );
}


static inline int uid2uname( const char **uname, uid_t uid )
{
    getuinfo( uname, getpwuid, uid, struct passwd, pw_name );
}


static inline int gname2gid( gid_t *gid, const char *gname )
{
    getuinfo( gid, getgrnam, gname, struct group, gr_gid );
}


static inline int gid2gname( const char **gname, gid_t gid )
{
    getuinfo( gname, getgrgid, gid, struct group, gr_name );
}


#define getid_lua(L,t,name2id,getid) do { \
    size_t len = 0; \
    const char *name = luaL_optlstring( L, 1, NULL, &len ); \
    if( len ){ \
        t id = 0; \
        /* not found */ \
        if( name2id( &id, name ) != 0 ){ \
            lua_pushnil( L ); \
            lua_pushstring( L, strerror( errno ) ); \
            return 2; \
        } \
        /* push id */ \
        else { \
            lua_pushinteger( L, id ); \
        } \
    } \
    /* return id of current process */ \
    else { \
        lua_pushinteger( L, getid() ); \
    } \
    return 1; \
}while(0)


#define getname_lua(L,t,id2name,getid) do { \
    t id = (t)(lua_isnoneornil( L, 1 ) ? getid() : luaL_checkinteger( L, 1 )); \
    const char *name = NULL; \
    /* not found */ \
    if( id2name( &name, id ) != 0 ){ \
        lua_pushnil( L ); \
        lua_pushstring( L, strerror( errno ) ); \
        return 2; \
    } \
    /* push name */ \
    lua_pushstring( L, name ); \
    return 1; \
}while(0)


#define setid_lua(L,t,name2id,setid) do { \
    t id = 0; \
    if( lua_type( L, 1 ) == LUA_TSTRING ){ \
        const char *name = luaL_checkstring( L, 1 ); \
        /* set id by name */ \
        if( name2id( &id, name ) == 0 && setid( id ) == 0 ){ \
            return 0; \
        } \
    } \
    else { \
        id = (t)luaL_checkinteger( L, 1 ); \
        if( setid( id ) == 0 ){ \
            return 0; \
        } \
    } \
    /* got error */ \
    lua_pushstring( L, strerror( errno ) ); \
    return 1; \
}while(0)


#define setreid_lua(L,t,name2id,setid) do { \
    const char *name = NULL; \
    t rid = 0; \
    t eid = 0; \
    if( lua_type( L, 1 ) == LUA_TSTRING ){ \
        name = luaL_checkstring( L, 1 ); \
        if( name2id( &rid, name ) != 0 ){ \
            goto FAILURE; \
        } \
    } \
    else { \
        rid = (t)luaL_checkinteger( L, 1 ); \
    } \
    if( lua_type( L, 2 ) == LUA_TSTRING ){ \
        name = luaL_checkstring( L, 2 ); \
        if( name2id( &eid, name ) != 0 ){ \
            goto FAILURE; \
        } \
    } \
    else { \
        eid = (t)luaL_checkinteger( L, 2 ); \
    } \
    if( setid( rid, eid ) == 0 ){ \
        return 0; \
    } \
FAILURE: \
    /* got error */ \
    lua_pushstring( L, strerror( errno ) ); \
    return 1; \
}while(0);


static int getgid_lua( lua_State *L )
{
    getid_lua( L, gid_t, gname2gid, getgid );
}


static int getgname_lua( lua_State *L )
{
    getname_lua( L, gid_t, gid2gname, getgid );
}


static int setgid_lua( lua_State *L )
{
    setid_lua( L, gid_t, gname2gid, setgid );
}


static int getegid_lua( lua_State *L )
{
    lua_pushinteger( L, getegid() );
    return 1;
}


static int setegid_lua( lua_State *L )
{
    setid_lua( L, gid_t, gname2gid, setegid );
}


static int setregid_lua( lua_State *L )
{
    setreid_lua( L, gid_t, gname2gid, setregid );
}


static int getuid_lua( lua_State *L )
{
    getid_lua( L, uid_t, uname2uid, getuid );
}


static int getuname_lua( lua_State *L )
{
    getname_lua( L, uid_t, uid2uname, getuid );
}


static int setuid_lua( lua_State *L )
{
    setid_lua( L, uid_t, uname2uid, setuid );
}


static int geteuid_lua( lua_State *L )
{
    lua_pushinteger( L, geteuid() );
    return 1;
}


static int seteuid_lua( lua_State *L )
{
    setid_lua( L, uid_t, uname2uid, seteuid );
}


static int setreuid_lua( lua_State *L )
{
    setreid_lua( L, uid_t, uname2uid, setreuid );
}


// MARK: session id
static int getsid_lua( lua_State *L )
{
    pid_t pid = (pid_t)luaL_checkinteger( L, 1 );
    pid_t sid = getsid( pid );
    
    if( sid != -1 ){
        lua_pushinteger( L, sid );
        return 1;
    }
    
    // got error
    lua_pushnil( L );
    lua_pushstring( L, strerror( errno ) );
    
    return 2;
}


static int setsid_lua( lua_State *L )
{
    pid_t sid = setsid();
    
    if( sid != -1 ){
        lua_pushinteger( L, sid );
        return 1;
    }
    
    // got error
    lua_pushnil( L );
    lua_pushstring( L, strerror( errno ) );
    
    return 2;
}


// MARK: resource
static int getrusage_lua( lua_State *L )
{
    struct rusage usage;
    
    if( getrusage( RUSAGE_SELF, &usage ) == 0 ){
        lua_newtable( L );
        lstate_num2tbl( L, "maxrss", usage.ru_maxrss );
        lstate_num2tbl( L, "ixrss", usage.ru_ixrss );
        lstate_num2tbl( L, "idrss", usage.ru_idrss );
        lstate_num2tbl( L, "isrss", usage.ru_isrss );
        lstate_num2tbl( L, "minflt", usage.ru_minflt );
        lstate_num2tbl( L, "majflt", usage.ru_majflt );
        lstate_num2tbl( L, "nswap", usage.ru_nswap );
        lstate_num2tbl( L, "inblock", usage.ru_inblock );
        lstate_num2tbl( L, "oublock", usage.ru_oublock );
        lstate_num2tbl( L, "msgsnd", usage.ru_msgsnd );
        lstate_num2tbl( L, "msgrcv", usage.ru_msgrcv );
        lstate_num2tbl( L, "nsignals", usage.ru_nsignals );
        lstate_num2tbl( L, "nvcsw", usage.ru_nvcsw );
        lstate_num2tbl( L, "nivcsw", usage.ru_nivcsw );
        lua_pushstring( L, "utime" );
        lua_newtable( L );
        lstate_num2tbl( L, "sec", usage.ru_utime.tv_sec );
        lstate_num2tbl( L, "usec", usage.ru_utime.tv_usec );
        lua_rawset( L, -3 );
        lua_pushstring( L, "stime" );
        lua_newtable( L );
        lstate_num2tbl( L, "sec", usage.ru_stime.tv_sec );
        lstate_num2tbl( L, "usec", usage.ru_stime.tv_usec );
        lua_rawset( L, -3 );
        
        return 1;
    }
    
    // got error
    lua_pushnil( L );
    lua_pushstring( L, strerror( errno ) );
    
    return 2;
}


// MARK: current working directory
static int getcwd_lua( lua_State *L )
{
    char *cwd = getcwd( NULL, 0 );
    
    if( cwd ){
        lua_pushstring( L, cwd );
        free( cwd );
        return 1;
    }
    
    // got error
    lua_pushnil( L );
    lua_pushstring( L, strerror( errno ) );
    
    return 2;
}


static int chdir_lua( lua_State *L )
{
    const char *dir = luaL_checkstring( L, 1 );
    
    if( chdir( dir ) == 0 ){
        return 0;
    }
    
    // got error
    lua_pushstring( L, strerror( errno ) );
    
    return 1;
}


// MARK: child process
static int fork_lua( lua_State *L )
{
    pid_t pid = fork();
    
    if( pid != -1 ){
        lua_pushinteger( L, pid );
        return 1;
    }
    
    // got error
    lua_pushnil( L );
    lua_pushstring( L, strerror( errno ) );
    
    return 2;
}


static int waitpid_lua( lua_State *L )
{
    const int argc = lua_gettop( L );
    pid_t pid = luaL_checkinteger( L, 1 );
    pid_t rpid = 0;
    int rc = 0;
    int opts = 0;
    
    // check opts
    if( argc > 1 )
    {
        int i = 2;
        
        for(; i <= argc; i++ ){
            opts |= (int)luaL_optinteger( L, i, 0 );
        }
    }
    
    rpid = waitpid( pid, &rc, opts );
    // WNOHANG
    if( rpid == 0 ){
        lua_createtable( L, 0, 2 );
        lstate_num2tbl( L, "pid", pid );
        lstate_bool2tbl( L, "nohang", 1 );
        return 1;
    }
    else if( rpid != -1 )
    {
        lua_createtable( L, 0, 2 );
        lstate_num2tbl( L, "pid", rpid );
        // exit status
        if( WIFEXITED( rc ) ){
            lstate_num2tbl( L, "exit", WEXITSTATUS( rc ) );
        }
        // exit signal number
        else if( WIFSIGNALED( rc ) ){
            lstate_num2tbl( L, "termsig", WTERMSIG( rc ) );
        }
        // stop signal 
        else if( WIFSTOPPED( rc ) ){
            lstate_num2tbl( L, "stopsig", WSTOPSIG( rc ) );
        }
        // continue signal
        else if( WIFCONTINUED( rc ) ){
            lstate_bool2tbl( L, "continue", 1 );
        }
        return 1;
    }
    
    // got error
    lua_pushnil( L );
    lua_pushstring( L, strerror( errno ) );
    
    return 2;
}


static int exec_lua( lua_State *L )
{
    int argc = lua_gettop( L );
    const char *cmd = luaL_checkstring( L, 1 );
    pid_t pid = 0;
    int rc = 0;
    array_t argv = {
        .elts = NULL,
        .len = 0,
        .max = 0
    };
    array_t envs = {
        .elts = NULL,
        .len = 0,
        .max = 0
    };
    iopipe_t iop;
    
    // init arg containers
    if( arr_init( &argv, ARG_MAX ) == -1 ||
        arr_push( &argv, (char*)cmd ) == -1 || 
        arr_init( &envs, 0 ) == -1 || 
        iop_init( &iop ) == -1 ){
        arr_dispose( &argv );
        arr_dispose( &envs );
        lua_pushnil( L );
        lua_pushstring( L, strerror( errno ) );
        return 2;
    }
    // manipute arg length
    else if( argc > 4 ){
        argc = 4;
    }
    
    // check args
    switch( argc )
    {
        // cwd
        case 4:
            if( !lua_isnoneornil( L, 4 ) )
            {
                const char *dir = luaL_checkstring( L, 4 );
                
                if( chdir( dir ) != 0 ){
                    arr_dispose( &argv );
                    arr_dispose( &envs );
                    iop_dispose( &iop );
                    lua_pushnil( L );
                    lua_pushfstring( L, "chdir: %s", strerror( errno ) );
                    return 2;
                }
            }
        // envs
        case 3:
            if( !lua_isnoneornil( L, 3 ) )
            {
                rc = kvp2arr( L, 3, &envs, "%s=%s" );
                // got error
                if( rc != 0 )
                {
                    arr_dispose( &argv );
                    arr_dispose( &envs );
                    iop_dispose( &iop );
                    if( rc == EINVAL ){
                        lua_pushnil( L );
                        lua_pushliteral( L, "env must be pair table" );
                    }
                    else {
                        lua_pushnil( L );
                        lua_pushstring( L, strerror( errno ) );
                    }
                    return 2;
                }
                // push last NULL item
                else if( arr_push( &envs, NULL ) == -1 ){
                    arr_dispose( &argv );
                    arr_dispose( &envs );
                    iop_dispose( &iop );
                    lua_pushnil( L );
                    lua_pushstring( L, strerror( errno ) );
                    return 2;
                }
            }
        // argv
        case 2:
            if( !lua_isnoneornil( L, 2 ) )
            {
                rc = ivp2arr( L, 2, &argv );
                // got error
                if( rc != 0 )
                {
                    arr_dispose( &argv );
                    arr_dispose( &envs );
                    iop_dispose( &iop );
                    if( rc == E2BIG ){
                        lua_pushnil( L );
                        lua_pushfstring( L, "argv must be less than %d", ARG_MAX );
                    }
                    else if( rc == EINVAL ){
                        lua_pushnil( L );
                        lua_pushliteral( L, "argv must be ipair table" );
                    }
                    else {
                        lua_pushnil( L );
                        lua_pushstring( L, strerror( errno ) );
                    }
                    return 2;
                }
            }
        // add last NULL terminated item into arg array.
        default:
            if( arr_push( &argv, NULL ) == -1 ){
                arr_dispose( &argv );
                arr_dispose( &envs );
                iop_dispose( &iop );
                lua_pushnil( L );
                lua_pushstring( L, strerror( errno ) );
                return 2;
            }
    }
    
    pid = fork();
    // child
    if( pid == 0 )
    {
        // set std-in-out-err
        if( iop_set( &iop ) == 0 )
        {
            if( envs.len ){
                environ = envs.elts;
            }
            execvp( cmd, argv.elts );
        }
        arr_dispose( &argv );
        arr_dispose( &envs );
        fputs( strerror( errno ), stderr );
        _exit(0);
    }
    
    arr_dispose( &argv );
    arr_dispose( &envs );
    // got error
    if( pid == -1 ){
        lua_pushnil( L );
        lua_pushstring( L, strerror( errno ) );
        return 2;
    }
    
    // parent
    // close read-stdin, write-stdout
    iop_unset( &iop );
    if( newpchild( L, pid, iop.fds[IOP_IN_WRITE], iop.fds[IOP_OUT_READ], 
                   iop.fds[IOP_ERR_READ] ) != 0 )
    {
        int err = errno;
        
        // kill process safety
        if( kill( pid, SIGTERM ) == 0 )
        {
            sleep(1);
            rc = 0;
            if( waitpid( pid, &rc, WNOHANG ) == 0 ){
                kill( pid, SIGKILL );
            }
        }
        lua_pushnil( L );
        lua_pushstring( L, strerror( err ) );
        return 2;
    }
    
    return 1;
}


// MARK: suspend process
static int sleep_lua( lua_State *L )
{
    lua_Integer sec = luaL_checkinteger( L, 1 );
    
    lua_pushinteger( L, sleep( sec ) );
    return 1;
}


static int nsleep_lua( lua_State *L )
{
    lua_Integer nsec = luaL_checkinteger( L, 1 );
    struct timespec req = {
        .tv_sec = nsec / UINT64_C(1000000000),
        .tv_nsec = nsec % UINT64_C(1000000000)
    };
    
    lua_pushinteger( L, nanosleep( &req, NULL ) );
    
    return 1;
}


// MARK: errors
static int errno_lua( lua_State *L )
{
    lua_pushinteger( L, errno );
    return 1;
}


static int strerror_lua( lua_State *L )
{
    int err = errno;
    
    if( !lua_isnoneornil( L, 1 ) ){
        err = (int)luaL_checkinteger( L, 1 );
    }
    
    lua_pushstring( L, strerror( err ) );
    
    return 1;
}



// MARK: time
static int gettimeofday_lua( lua_State *L )
{
    struct timeval tv;
    
    if( gettimeofday( &tv, NULL ) == 0 ){
        lua_pushnumber( L, (lua_Number)tv.tv_sec +
                         (lua_Number)tv.tv_usec/1000000 );
        return 1;
    }

    // got error
    lua_pushnil( L );
    lua_pushstring( L, strerror( errno ) );

    return 2;
}



// MARK: descriptor
static int dup_lua( lua_State *L )
{
    lua_Integer oldfd = luaL_checkinteger( L, 1 );
    int newfd = dup( oldfd );

    if( newfd != -1 ){
        lua_pushinteger( L, newfd );
        return 1;
    }

    // got error
    lua_pushnil( L );
    lua_pushstring( L, strerror( errno ) );

    return 2;
}


static int dup2_lua( lua_State *L )
{
    lua_Integer oldfd = luaL_checkinteger( L, 1 );
    lua_Integer newfd = luaL_checkinteger( L, 2 );

    if( dup2( oldfd, newfd ) != -1 ){
        lua_pushinteger( L, newfd );
        return 1;
    }

    // got error
    lua_pushnil( L );
    lua_pushstring( L, strerror( errno ) );

    return 2;
}



LUALIB_API int luaopen_process( lua_State *L )
{
    struct luaL_Reg method[] = {
        // environment
        { "getenv", getenv_lua },
        // process id
        { "getpid", getpid_lua },
        { "getppid", getppid_lua },
        // group id
        { "getgid", getgid_lua },
        { "getgname", getgname_lua },
        { "setgid", setgid_lua },
        { "getegid", getegid_lua },
        { "setegid", setegid_lua },
        { "setregid", setregid_lua },
        // user id
        { "getuid", getuid_lua },
        { "getuname", getuname_lua },
        { "setuid", setuid_lua },
        { "geteuid", geteuid_lua },
        { "seteuid", seteuid_lua },
        { "setreuid", setreuid_lua },
        // session id
        { "getsid", getsid_lua },
        { "setsid", setsid_lua },
        // resources
        { "getrusage", getrusage_lua },
        // current working directory
        { "getcwd", getcwd_lua },
        { "chdir", chdir_lua },
        // child process
        { "fork", fork_lua },
        { "waitpid", waitpid_lua },
        { "exec", exec_lua },
        // suspend process
        { "sleep", sleep_lua },
        { "nsleep", nsleep_lua },
        // errors
        { "errno", errno_lua },
        { "strerror", strerror_lua },
        // time
        { "gettimeofday", gettimeofday_lua },
        // descriptor
        { "dup", dup_lua },
        { "dup2", dup2_lua },
        { NULL, NULL }
    };
    struct luaL_Reg *ptr = method;
    
    // define fd metatable
    luaopen_process_child( L );
    // create module table
    lua_newtable( L );
    // add methods
    do {
        lstate_fn2tbl( L, ptr->name, ptr->func );
        ptr++;
    } while( ptr->name );
    
    // set waitpid options
#ifdef WCONTINUED
    lstate_num2tbl( L, "WCONTINUED", WCONTINUED );
#endif

#ifdef WNOHANG
    lstate_num2tbl( L, "WNOHANG", WNOHANG );
#endif

#ifdef WNOWAIT
    lstate_num2tbl( L, "WNOWAIT", WNOWAIT );
#endif

#ifdef WUNTRACED
    lstate_num2tbl( L, "WUNTRACED", WUNTRACED );
#endif

    // set errno
#ifdef E2BIG
    lstate_num2tbl( L, "E2BIG", E2BIG );
#endif

#ifdef EACCES
    lstate_num2tbl( L, "EACCES", EACCES );
#endif

#ifdef EADDRINUSE
    lstate_num2tbl( L, "EADDRINUSE", EADDRINUSE );
#endif

#ifdef EADDRNOTAVAIL
    lstate_num2tbl( L, "EADDRNOTAVAIL", EADDRNOTAVAIL );
#endif

#ifdef EADV
    lstate_num2tbl( L, "EADV", EADV );
#endif

#ifdef EAFNOSUPPORT
    lstate_num2tbl( L, "EAFNOSUPPORT", EAFNOSUPPORT );
#endif

#ifdef EAGAIN
    lstate_num2tbl( L, "EAGAIN", EAGAIN );
#endif

#ifdef EALREADY
    lstate_num2tbl( L, "EALREADY", EALREADY );
#endif

#ifdef EAUTH
    lstate_num2tbl( L, "EAUTH", EAUTH );
#endif

#ifdef EBADARCH
    lstate_num2tbl( L, "EBADARCH", EBADARCH );
#endif

#ifdef EBADE
    lstate_num2tbl( L, "EBADE", EBADE );
#endif

#ifdef EBADEXEC
    lstate_num2tbl( L, "EBADEXEC", EBADEXEC );
#endif

#ifdef EBADF
    lstate_num2tbl( L, "EBADF", EBADF );
#endif

#ifdef EBADFD
    lstate_num2tbl( L, "EBADFD", EBADFD );
#endif

#ifdef EBADMACHO
    lstate_num2tbl( L, "EBADMACHO", EBADMACHO );
#endif

#ifdef EBADMSG
    lstate_num2tbl( L, "EBADMSG", EBADMSG );
#endif

#ifdef EBADR
    lstate_num2tbl( L, "EBADR", EBADR );
#endif

#ifdef EBADRPC
    lstate_num2tbl( L, "EBADRPC", EBADRPC );
#endif

#ifdef EBADRQC
    lstate_num2tbl( L, "EBADRQC", EBADRQC );
#endif

#ifdef EBADSLT
    lstate_num2tbl( L, "EBADSLT", EBADSLT );
#endif

#ifdef EBFONT
    lstate_num2tbl( L, "EBFONT", EBFONT );
#endif

#ifdef EBUSY
    lstate_num2tbl( L, "EBUSY", EBUSY );
#endif

#ifdef ECANCELED
    lstate_num2tbl( L, "ECANCELED", ECANCELED );
#endif

#ifdef ECHILD
    lstate_num2tbl( L, "ECHILD", ECHILD );
#endif

#ifdef ECHRNG
    lstate_num2tbl( L, "ECHRNG", ECHRNG );
#endif

#ifdef ECOMM
    lstate_num2tbl( L, "ECOMM", ECOMM );
#endif

#ifdef ECONNABORTED
    lstate_num2tbl( L, "ECONNABORTED", ECONNABORTED );
#endif

#ifdef ECONNREFUSED
    lstate_num2tbl( L, "ECONNREFUSED", ECONNREFUSED );
#endif

#ifdef ECONNRESET
    lstate_num2tbl( L, "ECONNRESET", ECONNRESET );
#endif

#ifdef EDEADLK
    lstate_num2tbl( L, "EDEADLK", EDEADLK );
#endif

#ifdef EDEADLOCK
    lstate_num2tbl( L, "EDEADLOCK", EDEADLOCK );
#endif

#ifdef EDESTADDRREQ
    lstate_num2tbl( L, "EDESTADDRREQ", EDESTADDRREQ );
#endif

#ifdef EDEVERR
    lstate_num2tbl( L, "EDEVERR", EDEVERR );
#endif

#ifdef EDOM
    lstate_num2tbl( L, "EDOM", EDOM );
#endif

#ifdef EDOTDOT
    lstate_num2tbl( L, "EDOTDOT", EDOTDOT );
#endif

#ifdef EDQUOT
    lstate_num2tbl( L, "EDQUOT", EDQUOT );
#endif

#ifdef EEXIST
    lstate_num2tbl( L, "EEXIST", EEXIST );
#endif

#ifdef EFAULT
    lstate_num2tbl( L, "EFAULT", EFAULT );
#endif

#ifdef EFBIG
    lstate_num2tbl( L, "EFBIG", EFBIG );
#endif

#ifdef EFTYPE
    lstate_num2tbl( L, "EFTYPE", EFTYPE );
#endif

#ifdef EHOSTDOWN
    lstate_num2tbl( L, "EHOSTDOWN", EHOSTDOWN );
#endif

#ifdef EHOSTUNREACH
    lstate_num2tbl( L, "EHOSTUNREACH", EHOSTUNREACH );
#endif

#ifdef EHWPOISON
    lstate_num2tbl( L, "EHWPOISON", EHWPOISON );
#endif

#ifdef EIDRM
    lstate_num2tbl( L, "EIDRM", EIDRM );
#endif

#ifdef EILSEQ
    lstate_num2tbl( L, "EILSEQ", EILSEQ );
#endif

#ifdef EINPROGRESS
    lstate_num2tbl( L, "EINPROGRESS", EINPROGRESS );
#endif

#ifdef EINTR
    lstate_num2tbl( L, "EINTR", EINTR );
#endif

#ifdef EINVAL
    lstate_num2tbl( L, "EINVAL", EINVAL );
#endif

#ifdef EIO
    lstate_num2tbl( L, "EIO", EIO );
#endif

#ifdef EISCONN
    lstate_num2tbl( L, "EISCONN", EISCONN );
#endif

#ifdef EISDIR
    lstate_num2tbl( L, "EISDIR", EISDIR );
#endif

#ifdef EISNAM
    lstate_num2tbl( L, "EISNAM", EISNAM );
#endif

#ifdef EKEYEXPIRED
    lstate_num2tbl( L, "EKEYEXPIRED", EKEYEXPIRED );
#endif

#ifdef EKEYREJECTED
    lstate_num2tbl( L, "EKEYREJECTED", EKEYREJECTED );
#endif

#ifdef EKEYREVOKED
    lstate_num2tbl( L, "EKEYREVOKED", EKEYREVOKED );
#endif

#ifdef EL2HLT
    lstate_num2tbl( L, "EL2HLT", EL2HLT );
#endif

#ifdef EL2NSYNC
    lstate_num2tbl( L, "EL2NSYNC", EL2NSYNC );
#endif

#ifdef EL3HLT
    lstate_num2tbl( L, "EL3HLT", EL3HLT );
#endif

#ifdef EL3RST
    lstate_num2tbl( L, "EL3RST", EL3RST );
#endif

#ifdef ELAST
    lstate_num2tbl( L, "ELAST", ELAST );
#endif

#ifdef ELIBACC
    lstate_num2tbl( L, "ELIBACC", ELIBACC );
#endif

#ifdef ELIBBAD
    lstate_num2tbl( L, "ELIBBAD", ELIBBAD );
#endif

#ifdef ELIBEXEC
    lstate_num2tbl( L, "ELIBEXEC", ELIBEXEC );
#endif

#ifdef ELIBMAX
    lstate_num2tbl( L, "ELIBMAX", ELIBMAX );
#endif

#ifdef ELIBSCN
    lstate_num2tbl( L, "ELIBSCN", ELIBSCN );
#endif

#ifdef ELNRNG
    lstate_num2tbl( L, "ELNRNG", ELNRNG );
#endif

#ifdef ELOOP
    lstate_num2tbl( L, "ELOOP", ELOOP );
#endif

#ifdef EMEDIUMTYPE
    lstate_num2tbl( L, "EMEDIUMTYPE", EMEDIUMTYPE );
#endif

#ifdef EMFILE
    lstate_num2tbl( L, "EMFILE", EMFILE );
#endif

#ifdef EMLINK
    lstate_num2tbl( L, "EMLINK", EMLINK );
#endif

#ifdef EMSGSIZE
    lstate_num2tbl( L, "EMSGSIZE", EMSGSIZE );
#endif

#ifdef EMULTIHOP
    lstate_num2tbl( L, "EMULTIHOP", EMULTIHOP );
#endif

#ifdef ENAMETOOLONG
    lstate_num2tbl( L, "ENAMETOOLONG", ENAMETOOLONG );
#endif

#ifdef ENAVAIL
    lstate_num2tbl( L, "ENAVAIL", ENAVAIL );
#endif

#ifdef ENEEDAUTH
    lstate_num2tbl( L, "ENEEDAUTH", ENEEDAUTH );
#endif

#ifdef ENETDOWN
    lstate_num2tbl( L, "ENETDOWN", ENETDOWN );
#endif

#ifdef ENETRESET
    lstate_num2tbl( L, "ENETRESET", ENETRESET );
#endif

#ifdef ENETUNREACH
    lstate_num2tbl( L, "ENETUNREACH", ENETUNREACH );
#endif

#ifdef ENFILE
    lstate_num2tbl( L, "ENFILE", ENFILE );
#endif

#ifdef ENOANO
    lstate_num2tbl( L, "ENOANO", ENOANO );
#endif

#ifdef ENOATTR
    lstate_num2tbl( L, "ENOATTR", ENOATTR );
#endif

#ifdef ENOBUFS
    lstate_num2tbl( L, "ENOBUFS", ENOBUFS );
#endif

#ifdef ENOCSI
    lstate_num2tbl( L, "ENOCSI", ENOCSI );
#endif

#ifdef ENODATA
    lstate_num2tbl( L, "ENODATA", ENODATA );
#endif

#ifdef ENODEV
    lstate_num2tbl( L, "ENODEV", ENODEV );
#endif

#ifdef ENOENT
    lstate_num2tbl( L, "ENOENT", ENOENT );
#endif

#ifdef ENOEXEC
    lstate_num2tbl( L, "ENOEXEC", ENOEXEC );
#endif

#ifdef ENOKEY
    lstate_num2tbl( L, "ENOKEY", ENOKEY );
#endif

#ifdef ENOLCK
    lstate_num2tbl( L, "ENOLCK", ENOLCK );
#endif

#ifdef ENOLINK
    lstate_num2tbl( L, "ENOLINK", ENOLINK );
#endif

#ifdef ENOMEDIUM
    lstate_num2tbl( L, "ENOMEDIUM", ENOMEDIUM );
#endif

#ifdef ENOMEM
    lstate_num2tbl( L, "ENOMEM", ENOMEM );
#endif

#ifdef ENOMSG
    lstate_num2tbl( L, "ENOMSG", ENOMSG );
#endif

#ifdef ENONET
    lstate_num2tbl( L, "ENONET", ENONET );
#endif

#ifdef ENOPKG
    lstate_num2tbl( L, "ENOPKG", ENOPKG );
#endif

#ifdef ENOPOLICY
    lstate_num2tbl( L, "ENOPOLICY", ENOPOLICY );
#endif

#ifdef ENOPROTOOPT
    lstate_num2tbl( L, "ENOPROTOOPT", ENOPROTOOPT );
#endif

#ifdef ENOSPC
    lstate_num2tbl( L, "ENOSPC", ENOSPC );
#endif

#ifdef ENOSR
    lstate_num2tbl( L, "ENOSR", ENOSR );
#endif

#ifdef ENOSTR
    lstate_num2tbl( L, "ENOSTR", ENOSTR );
#endif

#ifdef ENOSYS
    lstate_num2tbl( L, "ENOSYS", ENOSYS );
#endif

#ifdef ENOTBLK
    lstate_num2tbl( L, "ENOTBLK", ENOTBLK );
#endif

#ifdef ENOTCONN
    lstate_num2tbl( L, "ENOTCONN", ENOTCONN );
#endif

#ifdef ENOTDIR
    lstate_num2tbl( L, "ENOTDIR", ENOTDIR );
#endif

#ifdef ENOTEMPTY
    lstate_num2tbl( L, "ENOTEMPTY", ENOTEMPTY );
#endif

#ifdef ENOTNAM
    lstate_num2tbl( L, "ENOTNAM", ENOTNAM );
#endif

#ifdef ENOTRECOVERABLE
    lstate_num2tbl( L, "ENOTRECOVERABLE", ENOTRECOVERABLE );
#endif

#ifdef ENOTSOCK
    lstate_num2tbl( L, "ENOTSOCK", ENOTSOCK );
#endif

#ifdef ENOTSUP
    lstate_num2tbl( L, "ENOTSUP", ENOTSUP );
#endif

#ifdef ENOTTY
    lstate_num2tbl( L, "ENOTTY", ENOTTY );
#endif

#ifdef ENOTUNIQ
    lstate_num2tbl( L, "ENOTUNIQ", ENOTUNIQ );
#endif

#ifdef ENXIO
    lstate_num2tbl( L, "ENXIO", ENXIO );
#endif

#ifdef EOPNOTSUPP
    lstate_num2tbl( L, "EOPNOTSUPP", EOPNOTSUPP );
#endif

#ifdef EOVERFLOW
    lstate_num2tbl( L, "EOVERFLOW", EOVERFLOW );
#endif

#ifdef EOWNERDEAD
    lstate_num2tbl( L, "EOWNERDEAD", EOWNERDEAD );
#endif

#ifdef EPERM
    lstate_num2tbl( L, "EPERM", EPERM );
#endif

#ifdef EPFNOSUPPORT
    lstate_num2tbl( L, "EPFNOSUPPORT", EPFNOSUPPORT );
#endif

#ifdef EPIPE
    lstate_num2tbl( L, "EPIPE", EPIPE );
#endif

#ifdef EPROCLIM
    lstate_num2tbl( L, "EPROCLIM", EPROCLIM );
#endif

#ifdef EPROCUNAVAIL
    lstate_num2tbl( L, "EPROCUNAVAIL", EPROCUNAVAIL );
#endif

#ifdef EPROGMISMATCH
    lstate_num2tbl( L, "EPROGMISMATCH", EPROGMISMATCH );
#endif

#ifdef EPROGUNAVAIL
    lstate_num2tbl( L, "EPROGUNAVAIL", EPROGUNAVAIL );
#endif

#ifdef EPROTO
    lstate_num2tbl( L, "EPROTO", EPROTO );
#endif

#ifdef EPROTONOSUPPORT
    lstate_num2tbl( L, "EPROTONOSUPPORT", EPROTONOSUPPORT );
#endif

#ifdef EPROTOTYPE
    lstate_num2tbl( L, "EPROTOTYPE", EPROTOTYPE );
#endif

#ifdef EPWROFF
    lstate_num2tbl( L, "EPWROFF", EPWROFF );
#endif

#ifdef EQFULL
    lstate_num2tbl( L, "EQFULL", EQFULL );
#endif

#ifdef ERANGE
    lstate_num2tbl( L, "ERANGE", ERANGE );
#endif

#ifdef EREMCHG
    lstate_num2tbl( L, "EREMCHG", EREMCHG );
#endif

#ifdef EREMOTE
    lstate_num2tbl( L, "EREMOTE", EREMOTE );
#endif

#ifdef EREMOTEIO
    lstate_num2tbl( L, "EREMOTEIO", EREMOTEIO );
#endif

#ifdef ERESTART
    lstate_num2tbl( L, "ERESTART", ERESTART );
#endif

#ifdef ERFKILL
    lstate_num2tbl( L, "ERFKILL", ERFKILL );
#endif

#ifdef EROFS
    lstate_num2tbl( L, "EROFS", EROFS );
#endif

#ifdef ERPCMISMATCH
    lstate_num2tbl( L, "ERPCMISMATCH", ERPCMISMATCH );
#endif

#ifdef ESHLIBVERS
    lstate_num2tbl( L, "ESHLIBVERS", ESHLIBVERS );
#endif

#ifdef ESHUTDOWN
    lstate_num2tbl( L, "ESHUTDOWN", ESHUTDOWN );
#endif

#ifdef ESOCKTNOSUPPORT
    lstate_num2tbl( L, "ESOCKTNOSUPPORT", ESOCKTNOSUPPORT );
#endif

#ifdef ESPIPE
    lstate_num2tbl( L, "ESPIPE", ESPIPE );
#endif

#ifdef ESRCH
    lstate_num2tbl( L, "ESRCH", ESRCH );
#endif

#ifdef ESRMNT
    lstate_num2tbl( L, "ESRMNT", ESRMNT );
#endif

#ifdef ESTALE
    lstate_num2tbl( L, "ESTALE", ESTALE );
#endif

#ifdef ESTRPIPE
    lstate_num2tbl( L, "ESTRPIPE", ESTRPIPE );
#endif

#ifdef ETIME
    lstate_num2tbl( L, "ETIME", ETIME );
#endif

#ifdef ETIMEDOUT
    lstate_num2tbl( L, "ETIMEDOUT", ETIMEDOUT );
#endif

#ifdef ETOOMANYREFS
    lstate_num2tbl( L, "ETOOMANYREFS", ETOOMANYREFS );
#endif

#ifdef ETXTBSY
    lstate_num2tbl( L, "ETXTBSY", ETXTBSY );
#endif

#ifdef EUCLEAN
    lstate_num2tbl( L, "EUCLEAN", EUCLEAN );
#endif

#ifdef EUNATCH
    lstate_num2tbl( L, "EUNATCH", EUNATCH );
#endif

#ifdef EUSERS
    lstate_num2tbl( L, "EUSERS", EUSERS );
#endif

#ifdef EWOULDBLOCK
    lstate_num2tbl( L, "EWOULDBLOCK", EWOULDBLOCK );
#endif

#ifdef EXDEV
    lstate_num2tbl( L, "EXDEV", EXDEV );
#endif

#ifdef EXFULL
    lstate_num2tbl( L, "EXFULL", EXFULL );
#endif


    return 1;
}
