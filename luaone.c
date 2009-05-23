#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include <sys/types.h>
#include <dirent.h>

int olua_connect( lua_State *lua );

int luaone_chdir(lua_State *lua)
{
    const char *newdir = lua_tostring(lua,1);
    if( newdir != NULL ){
        chdir( newdir );
    }
    return 0;
}

int luaone_bitand(lua_State *lua)
{
    int left=lua_tointeger(lua,1);
    int right=lua_tointeger(lua,2);
    if( !lua_isnumber(lua,1) || ! lua_isnumber(lua,1) ){
        lua_pushstring(lua,"not number");
        lua_error(lua);
    }
    lua_pushinteger(lua,left & right);
    return 1;
}
int luaone_bitor(lua_State *lua)
{
    int left=lua_tointeger(lua,1);
    int right=lua_tointeger(lua,2);
    if( !lua_isnumber(lua,1) || ! lua_isnumber(lua,1) ){
        lua_pushstring(lua,"not number");
        lua_error(lua);
    }
    lua_pushinteger(lua,left | right);
    return 1;
}
int luaone_bitxor(lua_State *lua)
{
    int left=lua_tointeger(lua,1);
    int right=lua_tointeger(lua,2);
    if( !lua_isnumber(lua,1) || ! lua_isnumber(lua,1) ){
        lua_pushstring(lua,"not number");
        lua_error(lua);
    }
    lua_pushinteger(lua,left ^ right);
    return 1;
}
int luaone_rshift(lua_State *lua)
{
    int left=lua_tointeger(lua,1);
    int right=lua_tointeger(lua,2);
    if( !lua_isnumber(lua,1) || ! lua_isnumber(lua,1) ){
        lua_pushstring(lua,"not number");
        lua_error(lua);
    }
    lua_pushinteger(lua,left >> right);
    return 1;
}
int luaone_lshift(lua_State *lua)
{
    int left=lua_tointeger(lua,1);
    int right=lua_tointeger(lua,2);
    if( !lua_isnumber(lua,1) || ! lua_isnumber(lua,1) ){
        lua_pushstring(lua,"not number");
        lua_error(lua);
    }
    lua_pushinteger(lua,left << right);
    return 1;
}

int luaone_readdir(lua_State *lua)
{
    void *userdata;
    DIR  *dirhandle;
    struct dirent *dirent;

    if( (userdata  = lua_touserdata(lua,1)) == NULL ||
        (dirhandle = *(DIR**)userdata)      == NULL ){
        return 0;
    }
    if( (dirent    = readdir(dirhandle)) == NULL ){
        closedir(dirhandle);
        *(DIR**)userdata = NULL;
        return 0;
    }
    lua_pop(lua,1);
    lua_pushstring(lua,dirent->d_name);
    return 1;
}

int luaone_closedir(lua_State *lua)
{
    void *userdata;
    DIR *dirhandle;

    if( (userdata=lua_touserdata(lua,1)) != NULL &&
        (dirhandle = *(DIR**)userdata)   != NULL )
    {
        closedir(dirhandle);
    }
    return 0;
}

int luaone_opendir(lua_State *lua)
{
    const char *dir = lua_tostring(lua,1);
    DIR *dirhandle;
    void *userdata=NULL;

    if( dir == NULL )
        return 0;

    dirhandle=opendir(dir);
    lua_pop(lua,1);

    lua_pushcfunction(lua,luaone_readdir); /* stack:1 */
    userdata = lua_newuserdata(lua,sizeof(dirhandle)); /* stack:2 */
    if( userdata == NULL )
        return 0;
    memcpy( userdata , &dirhandle , sizeof(dirhandle) );

    /* create metatable for closedir */
    lua_newtable(lua); /* stack:3 */
    lua_pushstring(lua,"__gc");
    lua_pushcfunction(lua,luaone_closedir);
    lua_settable(lua,3);
    lua_setmetatable(lua,2);

    return 2;
}

const char *cmdloader(lua_State *lua,void *data,size_t *size)
{
    static char buffer[ 1024 ];
    FILE *fp=data;

    if( feof(fp) || fgets(buffer,sizeof(buffer),fp)==NULL )
        return NULL;
    *size = strlen(buffer);
    return buffer;
}

static struct luaone_s {
    const char *name;
    int (*func)(lua_State *lua);
} luaone[] = {
    { "chdir" , luaone_chdir } ,
    { "dir"   , luaone_opendir },
    { "bitand", luaone_bitand },
    { "bitor" , luaone_bitor },
    { "bitxor", luaone_bitxor },
    { "rshift", luaone_rshift },
    { "lshift", luaone_lshift },
    { "new"   , olua_connect },
    { NULL    , NULL } ,
};

static FILE *find_luascript( const char *fname )
{
    char fullpath[ FILENAME_MAX ];
    FILE *fp=fopen(fname,"r");
    const char *path=getenv("PATH");
    char *path_dup=NULL;
    char *dirname;
    size_t fname_len;

    if( fp != NULL )
        return fp;

    fname_len = strlen(fname);
    if( fname_len + 5 > sizeof(fullpath) )
        return NULL;
    sprintf(fullpath,"%s.cmd",fname);
    if( (fp=fopen(fullpath,"r")) != NULL )
        return fp;

    if( path == NULL )
        return NULL;
    path_dup=strdup(path);
    if( path_dup == NULL )
        return NULL;
    for( dirname=strtok(path_dup,";:") ; dirname != NULL ; dirname=strtok(NULL,";:")){
        int dirname_len=strlen(dirname);
        if( dirname_len + fname_len + 10 > sizeof(fullpath) ){
            continue;
        }
        sprintf(fullpath,"%s\\%s",dirname,fname);
        if( (fp=fopen(fullpath,"r")) != NULL )
            break;
        sprintf(fullpath,"%s\\%s.cmd",dirname,fname);
        if( (fp=fopen(fullpath,"r")) != NULL )
            break;
    }
    free(path_dup);
    return fp;
}

int main(int argc, char **argv)
{
    int j,rv,c;
    lua_State *lua=NULL;
    struct luaone_s *p=NULL;
    FILE *fp=NULL;
    const char *emsg=NULL;
    int argp=1;

    if( argc < 2 ){
        fprintf(stderr,"%s: lua frontend program for Windows\n",argv[0]);
        return -1;
    }

    lua = luaL_newstate();
    luaL_openlibs(lua);

    lua_newtable(lua);
    for(p=luaone ; p->name != NULL ; p++ ){
        lua_pushstring(lua,p->name);
        lua_pushcfunction(lua,p->func);
        lua_settable(lua,1);
    }
    lua_setglobal(lua,"olua");

    for(argp=1 ; argp < argc ; argp++ ){
        if( argv[argp][0] == '-' ){
            if( argv[argp][1]=='e' ){
                if( argv[argp][2] != '\0' ){
                    if( luaL_loadstring(lua,&argv[argp][2]) )
                        goto errpt;
                    break;
                }else if( argp+1 < argc ){
                    if( luaL_loadstring(lua,argv[++argp]) )
                        goto errpt;
                    break;
                }
            }
        }else{
            fp=find_luascript(argv[argp]);
            if( fp == NULL ){
                fprintf(stderr,"%s: can not open '%s'.\n",argv[0],argv[argp]);
                return EXIT_FAILURE;
            }
            c=getc(fp);
            /* drop first '@' */
            if( c=='@' ){
                /* drop 1-line : "@luaone %0 & exit' */
                do{
                    c=getc(fp);
                }while( c != EOF && c != '\n' );
            }else{
                (ungetc)(c,fp);
            }
            rv = lua_load(lua,cmdloader,fp,argv[argp]);
            fclose(fp);
            if( rv != 0 )
                goto errpt;
            break;
        }
    }
    
    lua_newtable(lua);
    for(j=0;j<argc;j++){
        lua_pushinteger(lua,j-argp);
        lua_pushstring(lua,argv[j]);
        lua_settable(lua,2);
    }
    lua_setglobal(lua,"arg");

    lua_call(lua,0,0);
    
    lua_close(lua);
    return EXIT_SUCCESS;

errpt:
    emsg = lua_tostring(lua,1);
    fputs(emsg,stderr);
    lua_close(lua);
    return EXIT_FAILURE;
}
