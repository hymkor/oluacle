#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "oci.h"

#if 0
#  define DEBUG(x) (x,fflush(stdout))
#else
#  define DEBUG(x) ;
#  define NDEBUG
#endif

static OCIEnv   *envhp=NULL;
static OCIError *errhp=NULL;

static sword checkerr( lua_State *lua , OCIError *errhp , sword status , const char *funcname )
{
    text errbuf[512];
    sb4 errcode;

    if( status == OCI_SUCCESS )
        return status;

    switch (status){
    case OCI_SUCCESS_WITH_INFO:
        if( errhp != NULL ){
            OCIErrorGet ((dvoid *) errhp, (ub4) 1, (text *) NULL, &errcode,
                    errbuf, (ub4) sizeof(errbuf), (ub4) OCI_HTYPE_ERROR);
            lua_pushfstring(lua, "%s: Error - OCI_SUCCESS_WITH_INFO: %s" , funcname , errbuf );
        }else{
            lua_pushfstring(lua, "%s: Error - OCI_SUCCESS_WITH_INFO: ?" , funcname );
        }
        lua_error(lua);
        abort();

    case OCI_NEED_DATA:
        lua_pushfstring(lua,"%s: Error - OCI_NEED_DATA",funcname);
        lua_error(lua);
        abort();

    case OCI_NO_DATA:
        DEBUG( printf("%s: Error - OCI_NO_DATA\n",funcname) );
        break;

    case OCI_ERROR:
        if( errhp != NULL ){
            OCIErrorGet ((dvoid *) errhp, (ub4) 1, (text *) NULL, &errcode,
                    errbuf, (ub4) sizeof(errbuf), (ub4) OCI_HTYPE_ERROR);
            lua_pushfstring(lua,"%s: Error - %s", funcname,errbuf);
        }else{
            lua_pushfstring(lua,"%s: Error(%d)", funcname,status);
        }
        lua_error(lua);
        abort();

    case OCI_INVALID_HANDLE:
        lua_pushfstring(lua,"%s: Error - OCI_INVALID_HANDLE",funcname);
        lua_error(lua);
        abort();
        break;

    case OCI_STILL_EXECUTING:
        lua_pushfstring(lua,"%s: Error - OCI_STILL_EXECUTING",funcname);
        lua_error(lua);
        abort();
        break;

    case OCI_CONTINUE:
        lua_pushfstring(lua,"%s: Error - OCI_CONTINUE",funcname);
        lua_error(lua);
        abort();
        break;

    default:
        lua_pushfstring(lua,"%s: Error - %d", funcname,status);
        lua_error(lua);
        abort();
        break;
    }
    return status;
}

struct olua_bind_buffer {
    struct olua_bind_buffer *next;
    OCIBind *bind;
    sb2 indicator;
    union{
        sword number;
        char  buffer[1];
    }u;
};

static struct olua_bind_buffer *olua_bind_buffer_new( size_t size )
{
    struct olua_bind_buffer *p = malloc( sizeof(struct olua_bind_buffer)+size );
    if( p == NULL )
        return NULL;

    p->next = NULL;
    p->bind = NULL;
    p->indicator = 0;
    p->u.buffer[0] = '\0';
    return p;
}

static void olua_bind_buffer_free( struct olua_bind_buffer *p )
{
    while( p != NULL ){
        struct olua_bind_buffer *q=p->next;
        free(p);
        p=q;
    }
}

struct olua_fetch_buffer {
    ub2 type;
    ub2 size;
    ub2 len;
    sb2 ind;
    char *name;
    union{
        dvoid *pointor;
        char *string;
        int  *integer;
        double *number;
    }u;
    struct olua_fetch_buffer *next;
};

struct olua_fetch_buffer *olua_fetch_buffer_new(struct olua_fetch_buffer *self)
{
    if( self == NULL && (self=malloc(sizeof(struct olua_fetch_buffer)))==NULL ){
        return NULL;
    }
    self->size = 0;
    self->len = 0;
    self->ind = 0;
    self->name = NULL;
    self->u.pointor = NULL;
    self->next = NULL;

    return self;
}

static void olua_fetch_buffer_free(struct olua_fetch_buffer *p)
{
    DEBUG( printf("ENTER: olua_fetch_buffer_free(%p)\n",p ));
    while( p != NULL ){
        struct olua_fetch_buffer *q=p->next;
        if( p->u.pointor != NULL )
            free(p->u.pointor);
        if( p->name != NULL )
            free(p->name);
        free(p);
        p=q;
    }
    DEBUG( puts("LEAVE: olua_fetch_buffer_free()"));
}

struct olua_handle {
    int type;
    union{
        dvoid   *pointor;
        OCIStmt *stmthp;
    }h;
    struct olua_bind_buffer  *bind_buffer;
    struct olua_fetch_buffer *fetch_buffer;
};

struct olua_handle *olua_handle_new(struct olua_handle *self)
{
    if( self == NULL && (self = malloc( sizeof(struct olua_handle) ))==NULL ){
        return NULL;
    }
    self->type = OCI_HTYPE_STMT;
    self->h.pointor    = NULL;
    self->bind_buffer  = NULL;
    self->fetch_buffer = NULL;
    return self;
}

/* lua-function: olua_handle_gc
 *  stack-in
 *    (+1) userdata-object for oci-handle
 *  return
 *    nothing
 */
static int olua_handle_gc(lua_State *lua)
{
    struct olua_handle *handle;
    
    if( lua_istable(lua,1) ){
        lua_getfield(lua,1,"handle");
        handle = lua_touserdata(lua,-1);
        lua_pop(lua,1);
    }else if( lua_isuserdata(lua,1) ){
        handle = lua_touserdata(lua,-1);
    }else{
        lua_pushstring(lua,"olua_handle_gc: invalid statement-handle in parameter");
        lua_error(lua);
        abort();
    }

    DEBUG( printf("ENTER: olua_handle_gc(%p)\n",handle) );
    if( handle->h.pointor != NULL ){
        DEBUG( printf("OCIHandleFree(%p)\n",handle->h.pointor) );
        OCIHandleFree( handle->h.pointor , handle->type );
        handle->h.pointor = NULL;
    }
    olua_bind_buffer_free( handle->bind_buffer );
    handle->bind_buffer = NULL;
    olua_fetch_buffer_free( handle->fetch_buffer );
    handle->fetch_buffer = NULL;
    DEBUG( puts("LEAVE: olua_handle_gc()") );
    return 0;
}

static void olua_atexit()
{
    if( errhp != NULL ){
        OCIHandleFree( errhp , OCI_HTYPE_ERROR );
        errhp = NULL;
    }
}

static int olua_disconnect(lua_State *lua)
{
    void *userdata;
    OCISvcCtx *svchp;
    sword status;

    DEBUG( puts("olua_disconnect()") );
    
    if( (userdata=lua_touserdata(lua,1)) == NULL ){
        lua_pushstring(lua,"olua_disconnect: invalid parameter");
        lua_error(lua);
        abort();
    }
    if( (svchp=*(OCISvcCtx**)userdata) != NULL ){
        status = OCITransRollback(svchp, errhp, OCI_DEFAULT);
        if( status != OCI_SUCCESS )
            checkerr(lua,errhp,status,"olua_disconnect(OCITransRollback)");
        status = OCILogoff( svchp , errhp );
        if( status != OCI_SUCCESS )
            checkerr(lua,errhp,status,"olua_disconnect(OCILogoff)");
    }
    
    *(OCISvcCtx**)userdata = NULL;
    return 0;
}

static int olua_rollback(lua_State *lua)
{
    OCISvcCtx **svchp=lua_touserdata(lua,1);
    sword status;

    DEBUG( puts("ENTER olua_rollback()") );
    assert( svchp != NULL );
    status = OCITransRollback(*svchp, errhp, OCI_DEFAULT);
    if( status != OCI_SUCCESS )
        checkerr(lua,errhp,status,"olua_rollback(OCITransRollback)");
    DEBUG( puts("LEAVE olua_rollback()") );
    return 0;
}

static int olua_commit(lua_State *lua)
{
    OCISvcCtx **svchp=lua_touserdata(lua,1);
    sword status;

    DEBUG( puts("ENTER olua_commit()") );
    assert( svchp != NULL );
    status = OCITransCommit(*svchp, errhp, OCI_DEFAULT);
    if( status != OCI_SUCCESS )
        checkerr(lua,errhp,status,"olua_commit(OCITransCommit)");
    DEBUG( puts("LEAVE olua_commit()") );
    return 0;
}

static int olua_exec(lua_State *lua);
static int olua_prepare( lua_State *lua );
static int olua_execute( lua_State *lua );
static int olua_bind( lua_State *lua );
static int olua_fetch( lua_State *lua );

int olua_connect( lua_State *lua )
{
    void *userdata;
    sword status;
    OCISvcCtx *svchp=NULL;
    const char *user   = lua_tostring(lua,1);
    const char *passwd = lua_tostring(lua,2);
    const char *dbname = lua_tostring(lua,3);

    if( user == NULL || passwd == NULL || dbname == NULL ){
        lua_pushstring(lua,"invalid parameter");
        lua_error(lua);
    }
    DEBUG( printf("olua_connect(\"%s\",\"%s\",\"%s\")\n" 
                , user , passwd , dbname ) );

    putenv("NLS_DATE_FORMAT=YYYY/MM/DD HH24:MI:SS");

    /* initialize handle for environment */
    if( envhp == NULL ){

        status = OCIEnvCreate(&envhp , OCI_DEFAULT , NULL , NULL , NULL , NULL , 0 , NULL );
        if( status != OCI_SUCCESS ){
            checkerr(lua,NULL,status,"olua_connect(OCIEnvCreate)");
            abort();
        }
    }

    /** error handle */
    if( errhp == NULL ){
        status = OCIHandleAlloc(envhp , (dvoid**)&errhp , OCI_HTYPE_ERROR , 0 , NULL );
        if( status != OCI_SUCCESS ){
            checkerr(lua,NULL,status,"olua_connect(OCIHandleAlloc)");
            abort();
        }
        atexit( olua_atexit );
    }

    /** login session */
    status = OCILogon( envhp , errhp , &svchp , 
              (CONST text *)user   , strlen(user) ,
              (CONST text *)passwd , strlen(passwd) ,
              (CONST text *)dbname , strlen(dbname) );

    if( status != OCI_SUCCESS ){
        checkerr(lua,errhp,status,"olua_connect");
        abort();
    }

    if( svchp == NULL ){
        return 0;
    }else if( (userdata=lua_newuserdata(lua,sizeof(OCISvcCtx*))) == NULL){
        OCILogoff( svchp , errhp );
        return 0;
    }else{
        memcpy( userdata , &svchp , sizeof(OCISvcCtx*) );
        DEBUG( puts("successfully return 1") );

        lua_newtable(lua);

        lua_pushstring(lua,"__gc");
        lua_pushcfunction(lua,olua_disconnect);
        lua_settable(lua,-3);

        lua_pushstring(lua,"__index");

        lua_newtable(lua);

        /* method: exec */
        lua_pushstring(lua,"exec");
        lua_pushcfunction(lua,olua_exec);
        lua_settable(lua,-3);

        /* method: prepare */
        lua_pushstring(lua,"prepare");
        lua_pushcfunction(lua,olua_prepare);
        lua_settable(lua,-3);

        /* method: commit */
        lua_pushstring(lua,"commit");
        lua_pushcfunction(lua,olua_commit);
        lua_settable(lua,-3);

        /* method: commit */
        lua_pushstring(lua,"rollback");
        lua_pushcfunction(lua,olua_rollback);
        lua_settable(lua,-3);

        lua_settable(lua,-3); /* as __index */
        lua_setmetatable(lua,-2);

        return 1;
    }
}


/** olua_prepare
 * stack-in:
 *   (+1) connection.
 *   (+2) sql string
 * stack-out (success)
 *   (+1) statement-handle
 */
static int olua_prepare( lua_State *lua )
{
    struct olua_handle *handle=NULL;
    sword status;
    ub4 prefetch = 0;

    void *userdata  = lua_touserdata(lua,1);
    const char *sql = lua_tostring(lua,2);

    assert( lua_isuserdata(lua,1) );
    assert( lua_isstring(lua,2) );

    if( userdata == NULL || sql == NULL ){
        lua_pushstring(lua,"too few paramter");
        lua_error(lua);
        abort();
    }

    DEBUG( puts("ENTER: olua_prepare()"));

    /*** new statement object ***/
    lua_newtable(lua);
    lua_pushstring(lua,"handle");
    handle = olua_handle_new( lua_newuserdata(lua,sizeof(struct olua_handle)));
    assert( handle != NULL );

    lua_newtable(lua); /* meta-table for user object */
    lua_pushstring(lua,"__gc");/* destructor's name */
    lua_pushcfunction(lua,olua_handle_gc); /* destructor's func */
    lua_settable(lua,-3); /* assign destructor to meta table */
    lua_setmetatable(lua,-2); /* assign metatable to user-object */
    lua_settable(lua,-3);  /* assign user-object to instance */

    DEBUG( puts("CALL: OCIHandleAlloc") );
    status = OCIHandleAlloc(
        envhp ,
        &handle->h.pointor ,
        handle->type ,
        0 ,
        NULL );
    DEBUG( puts("EXIT: OCIHandleAlloc") );

    if( status != OCI_SUCCESS ){
        checkerr(lua,errhp,status,"olua_handle_alloc(OCIHandleAlloc)");
        abort();
    }

    status = OCIAttrSet(handle->h.stmthp, OCI_HTYPE_STMT,
		(dvoid *)&prefetch, (ub4)0, (ub4)OCI_ATTR_PREFETCH_ROWS,
		errhp);

    if( status != OCI_SUCCESS ){
        checkerr(lua,errhp,status,"olua_prepare(OCIAttrSet)");
        abort();
    }

    DEBUG( printf("SQL=[%s]\n",sql) );
    status = OCIStmtPrepare(
        handle->h.stmthp , errhp, (CONST text*)sql , (ub4)strlen(sql) ,
        (ub4)OCI_NTV_SYNTAX , (ub4)OCI_DEFAULT );

    DEBUG( printf("OCIStmtPrepare()=%d\n",status));

    if( status != OCI_SUCCESS ){
        checkerr(lua,errhp,status,"olua_prepare(OCIStmtPrepare)");
        abort();
    }
    DEBUG( printf("Statement-handle=%p\n",handle->h.stmthp) );
    DEBUG( puts("LEAVE: olua_prepare(success)"));

    /* stack:
     *   1:connection 
     *   2:sql
     * ------
     *   3:object
     *   4:meta-table (DEL)
     *   5:key (DEL)
     *   6:value (DEL)
     */

    /* method: bind */
    lua_pushstring(lua,"bind");
    lua_pushcfunction(lua,olua_bind);
    lua_settable(lua,-3);

    /* method: execute */
    lua_pushstring(lua,"execute");
    lua_pushcfunction(lua,olua_execute);
    lua_settable(lua,-3);

    /* method: fetch */
    lua_pushstring(lua,"fetch");
    lua_pushcfunction(lua,olua_fetch);
    lua_settable(lua,-3);

    /* member: connection */
    lua_pushstring(lua,"connection");
    lua_pushvalue(lua,1);
    lua_settable(lua,-3);

    return 1;
}

/*
 * -nbinds-1   : statement-handle
 * -nbinds..-1 : bind-variables
 */
static int olua_bind_core( lua_State *lua , int nbinds )
{
    int i;
    struct olua_handle *handle;
    sword status;
    
    lua_getfield(lua,-nbinds-1,"handle");
    handle = lua_touserdata(lua,-1);
    lua_pop(lua,1);

    for(i=0;i<nbinds;i++){
        DEBUG( printf("try bind %d\n",i+1));
        int sp=-nbinds+i;
        DEBUG( printf("Statement-handle=%p\n",handle->h.stmthp) );
        struct olua_bind_buffer *b;

        if( lua_toboolean(lua,sp)==0 ){ /* nil or false => NULL */
            b=olua_bind_buffer_new(0);
            b->indicator = OCI_IND_NULL ;
            status = OCIBindByPos( handle->h.stmthp , 
                          &b->bind ,
                          errhp ,
                          i+1 , 
                          (dvoid*)b->u.buffer , /* valuep */
                          1 , /* value_sz */
                          SQLT_STR , /* dty */
                          &b->indicator ,
                          NULL ,
                          NULL ,
                          0 ,
                          NULL ,
                          OCI_DEFAULT 
                    );

        }else if( lua_isnumber(lua,sp) ){
            b=olua_bind_buffer_new(0);
            assert( b != NULL );

            b->u.number = lua_tointeger(lua,sp);

            DEBUG( printf("find %d(as number)\n",b->u.number) );
            status = OCIBindByPos( handle->h.stmthp , 
                        &b->bind ,
                        errhp ,
                        i+1 , /* position */
                        (dvoid *)&b->u.number , /* valuep */
                        (sword)sizeof(sword) ,  /* value_sz */
                        SQLT_INT , /* dty */
                        &b->indicator ,
                        (ub2*)NULL ,
                        (ub2*)NULL , 
                        (ub4)0 ,  /* maxarr_len */
                        (ub4*)NULL , /* curelep */
                        OCI_DEFAULT /* mode */
                    ); 
        }else{
            const char *string=lua_tostring(lua,sp);
            size_t len;
            
            assert( string != NULL );
            len=strlen(string);

            b=olua_bind_buffer_new(len);
            assert( b != NULL );
            strcpy( b->u.buffer , string );

            DEBUG( printf("find '%s' (as string)\n",string) );
            status = OCIBindByPos( handle->h.stmthp , 
                          &b->bind ,
                          errhp ,
                          i+1 , 
                          (dvoid*)b->u.buffer , /* valuep */
                          len+1 , /* value_sz */
                          SQLT_STR , /* dty */
                          &b->indicator ,
                          NULL ,
                          NULL ,
                          0 ,
                          NULL ,
                          OCI_DEFAULT 
                    );
        }
        if( status != OCI_SUCCESS ){
            free( b );
            checkerr(lua,errhp,status,"olua_bind(OCIBindByPos)");
            abort();
        }
        b->next = handle->bind_buffer ;
        handle->bind_buffer = b;
    }
    lua_pushboolean(lua,1);
    DEBUG( printf("LEAVE: olua_bind(successfully)\n") );
    return 1;
}

/** olua_bind
 *
 * stack-in:
 *  +1   = statement-object(table)
 *  +2.. = bind-variables
 * stack-out:
 *  +3   = true:succcess , nil:failure
 */
static int olua_bind( lua_State *lua )
{
    int i=lua_gettop(lua);
    return olua_bind_core(lua,i-1);
}

/* olua_fetch_buffer_alloc */
static struct olua_fetch_buffer *olua_fetch_buffer_alloc(
    lua_State *lua ,
    struct olua_handle *stmthp )
{
    sb4 parm_status;
    dvoid *mypard;
    ub4 counter=0;
    const char *typename;

    struct olua_fetch_buffer dummyfirst;
    struct olua_fetch_buffer *curr=&dummyfirst;

    DEBUG( puts("ENTER olua_fetch_buffer_alloc()") );

    olua_fetch_buffer_new( &dummyfirst );

    while( (parm_status=OCIParamGet(
                stmthp->h.stmthp ,
                stmthp->type ,
                errhp ,
                &mypard ,
                ++counter )) == OCI_SUCCESS )
    {
        sword status;

        if( (curr->next = olua_fetch_buffer_new(NULL)) == NULL ){
            olua_fetch_buffer_free( dummyfirst.next );
            lua_pushstring(lua,"memory allocation error(1)");
            lua_error(lua);
            abort();
        }
        curr = curr->next;

        /* データサイズ取得 */
        status = OCIAttrGet(
            mypard ,
            (ub4)OCI_DTYPE_PARAM ,
            (dvoid*)&curr->size ,
            (ub4)0 ,
            (ub4)OCI_ATTR_DATA_SIZE , 
            errhp
        );
        if( status != OCI_SUCCESS ){
            DEBUG( puts("can not get datasize") );
            olua_fetch_buffer_free(dummyfirst.next);
            checkerr(lua,errhp,status,"olua_fetch_buffer_alloc(OCIAttrGet)");
            abort();
        }
        DEBUG( printf("DATASIZE=%d\n",curr->size) );

        /* データ属性取得 */
        status = OCIAttrGet(
            (dvoid*)mypard , (ub4)OCI_DTYPE_PARAM ,
            (dvoid*)&curr->type , 
            (ub4)0 ,
            (ub4)OCI_ATTR_DATA_TYPE , 
            errhp
        );
        if( status != OCI_SUCCESS ){
            olua_fetch_buffer_free(dummyfirst.next);
            checkerr(lua,errhp,status,"olua_fetch_buffer_alloc(OCIAttrGet)");
            abort();
        }
        DEBUG( printf("DATATYPE=%d\n" , curr->type ) );

        if( curr->type == SQLT_NUM ){
            curr->type = SQLT_FLT;
            curr->size = sizeof(double);
        }else if( curr->type == SQLT_DAT ){
            curr->type = SQLT_STR;
            curr->size = 32;
        }
        /* 領域確保 */
        if( (curr->u.pointor = malloc(curr->size+1))==NULL ){
            olua_fetch_buffer_free( dummyfirst.next );
            lua_pushstring(lua,"olua_fetch_buffer_alloc(): memory allocation error");
            lua_error(lua);
            abort();
        }

        /* 列名取得 */
        status = OCIAttrGet(
            (dvoid*)mypard, (ub4)OCI_DTYPE_PARAM ,
            (dvoid*)&typename ,
            (ub4)0 ,
            (ub4)OCI_ATTR_NAME ,
            errhp 
        );
        if( status != OCI_SUCCESS ){
            olua_fetch_buffer_free(dummyfirst.next);
            checkerr(lua,errhp,status,"olua_fetch_buffer_alloc(OCIAttrGet)");
            abort();
        }
        curr->name = strdup( typename );
    }
    DEBUG(puts("ENTER: OCIDefineByPos"));

    counter=0;
    for( curr=dummyfirst.next ; curr != NULL ; curr=curr->next ){
        OCIDefine *dfp=NULL;
        sword status;

        status = OCIDefineByPos(
            stmthp->h.stmthp ,
            &dfp ,
            errhp ,
            ++counter ,
            curr->u.pointor ,
            curr->size ,
            curr->type ,
            &curr->ind  ,
            &curr->len ,
            (ub4)NULL,
            OCI_DEFAULT
        );
        if( status != OCI_SUCCESS ){
            olua_fetch_buffer_free( dummyfirst.next );
            checkerr( lua , errhp , status , "OCIDefineByPos");
            abort();
        }
    }
    if( stmthp->fetch_buffer != NULL ){
        olua_fetch_buffer_free( stmthp->fetch_buffer );
        stmthp->fetch_buffer = NULL;
    }
    
    DEBUG( printf("LEAVE olua_fetch_buffer_alloc(%p)\n",dummyfirst.next) );
    return stmthp->fetch_buffer = dummyfirst.next;
}

/** olua_execute
 * stack-in:
 *   (-1) statement-handle
 * stack-out:
 *   nothing
 */
static int olua_execute(lua_State *lua)
{
    struct olua_handle *stmthp;
    OCISvcCtx *conn;
    sword status;
    ub2 type;
    ub4 iters;

    DEBUG( puts("ENTER: olua_execute()") );

    /* get connection */
    lua_getfield(lua,-1,"connection");
    conn = *(OCISvcCtx **)lua_touserdata(lua,-1);
    lua_pop(lua,1);
    
    if( conn == NULL ){
        lua_pushstring(lua,"olua_execute: conn is nil.");
        lua_error(lua);
        abort();
    }

    /* get handle */
    lua_getfield(lua,-1,"handle");
    stmthp = lua_touserdata(lua,-1);
    lua_pop(lua,1);

    if( stmthp == NULL || stmthp->h.pointor == NULL){
        lua_pushstring(lua,"olua_execute: stmt handle is nil.");
        lua_error(lua);
        abort();
    }

    if( stmthp->type != OCI_HTYPE_STMT ){
        lua_pushstring(lua,"olua_execute: stmt handle is not OCI_HTYPE_STMT");
        lua_error(lua);
        abort();
    }
    DEBUG( printf("statement-handle=%p\n", stmthp->h.stmthp ) );

    /***/

    status = OCIAttrGet(stmthp->h.stmthp, (ub4) OCI_HTYPE_STMT,
		(dvoid *)&type, (ub4 *)0, (ub4)OCI_ATTR_STMT_TYPE, errhp);
    if( status != OCI_SUCCESS ){
        checkerr(lua,errhp,status,"olua_execute(OCIAttrGet)");
        abort();
    }
    if (type == OCI_STMT_SELECT)
        iters = 0;
    else
        iters = 1;

    DEBUG( puts("call OCIStmtExecute()") );
    status = OCIStmtExecute(conn,stmthp->h.stmthp,errhp,iters,0,NULL,NULL,OCI_DEFAULT);
    if( status != OCI_SUCCESS ){
        checkerr(lua,errhp,status,"olua_execute(OCIStmtExecute)");
        abort();
    }
    if( type == OCI_STMT_SELECT ){
        olua_fetch_buffer_alloc(lua,stmthp);
        lua_pushcfunction(lua,olua_fetch);
        lua_insert(lua,-2);
        DEBUG( puts("LEAVE: olua_execute(OCI_STMT_SELECT)") );
        return 2;
    }else{
        DEBUG( puts("LEAVE: olua_execute(! OCI_STMT_SELECT)") );
        return 0;
    }
}

/** olua_fetch 
 *
 * stack-in:
 *   (+1) statement-handle
 * stack-out:
 *   (+1) column table.
 */
static int olua_fetch(lua_State *lua)
{
    struct olua_handle *stmthp=NULL;
    struct olua_fetch_buffer *fetch_buffer=NULL;

    sword status;
    int counter=0;

    DEBUG( puts("ENTER: olua_fetch()") );

    /* statement-handle */
    lua_getfield(lua,1,"handle");
    stmthp=lua_touserdata(lua,-1);
    lua_pop(lua,1);

    if( stmthp == NULL ){
        lua_pushstring(lua,"error: invalid parameter(stmthp==NULL)");
        lua_error(lua);
        return 0;
    }

    fetch_buffer = stmthp->fetch_buffer ;

    status = OCIStmtFetch2(
            stmthp->h.stmthp ,
            errhp ,
            (ub4)1 ,
            OCI_FETCH_NEXT ,
            0 ,
            OCI_DEFAULT );

    if( status == OCI_NO_DATA){
        stmthp=lua_touserdata(lua,-1);
        olua_handle_gc(lua);
        lua_pushnil(lua);
        return 1;
    }
    if( status != OCI_SUCCESS ){
        checkerr(lua,errhp,status,"olua_fetch(OCIStmtFetch2)");
        abort();
    }
    DEBUG( puts("push fetch values") );
    DEBUG( fflush(stdout) );
    
    lua_newtable(lua);
    for( counter=1 ; fetch_buffer != NULL ; ++counter ){
        lua_pushstring(lua,fetch_buffer->name);
        lua_pushinteger(lua,counter);
        if( fetch_buffer->ind != 0 ){
            lua_getglobal(lua,"olua");
            lua_getfield(lua,-1,"null");
            lua_remove(lua,-2);
            DEBUG(puts("ind==0"));
        }else{
            switch( fetch_buffer->type ){
            case SQLT_STR:
            case SQLT_CHR:
            case SQLT_VCS:
            case SQLT_AFC:
                lua_pushlstring(lua,fetch_buffer->u.string,fetch_buffer->len);
                break;
            case SQLT_INT:
                lua_pushinteger(lua,*fetch_buffer->u.integer);
                break;
            case SQLT_BDOUBLE:
            case SQLT_BFLOAT:
            case SQLT_FLT:
                lua_pushnumber(lua,*fetch_buffer->u.number);
                break;
            case SQLT_ODT:
            case SQLT_DATE:
            case SQLT_TIMESTAMP:
            case SQLT_TIMESTAMP_TZ:
            case SQLT_TIMESTAMP_LTZ:
            default:
                lua_pushnil(lua);
                break;
            }
        }
        lua_pushvalue(lua,-1);
        lua_insert(lua,-3);
        /* 1:table
         * 2:key-string
         * 3:value
         * 4:counter
         * 5:value
         */
        lua_settable(lua,-5);
        lua_settable(lua,-3);
        fetch_buffer = fetch_buffer->next;
    }
    DEBUG( puts("LEAVE: olua_fetch(successfully)") );
    return 1;
}


/** olua_exec
 *
 * stack-in:
 *   (+1) connection.
 *   (+2) sql string
 *   (+3) bind values
 * stack-out
 *   (+1) iterator(fetch-function)
 *   (+2) statement-handle
 */
static int olua_exec(lua_State *lua)
{
    int bindvars=lua_gettop(lua)-2;

    /* +1 connection
     * +2 sql
     * +3... binds
     */
    DEBUG( printf("stack=%d (before prepare)\n",lua_gettop(lua) ) );

    olua_prepare(lua);

    /* +1 connection  => DEL
     * +2 sql string  => DEL
     *  : bind-values
     * -1 statement-object
     */
    assert( lua_gettop(lua) == bindvars + 3 );

    lua_remove(lua,+2); /* del: sql(original) */
    lua_remove(lua,+1); /* del: connection(original) */
    lua_insert(lua,+1); /* mov: statement-handle */

    DEBUG( printf("stack=%d (before bind)\n",lua_gettop(lua) ) );

    /* +1 statement-handle
     *  : binds
     */
    assert( lua_gettop(lua) == bindvars + 1 );
    olua_bind_core(lua,bindvars);
    if( lua_isnil(lua,-1) )
        return 1;
    lua_settop(lua,1);

    /* +1 statement-handle */
    assert( lua_gettop(lua) == 1 );

    return olua_execute(lua);
}
