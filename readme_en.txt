Oluacle - Lua for Oracle
========================

'oluacle' is a library to access Oracle database.

Diffences from LuaSQL are:

* Bind-variable supported.
* Few Lines required to access database
* Supports Oracle-database only.

Oluacle are executable and library's set.

    Executable: (oluacle.exe[Windows])
        Include lua interpretor.
    Library: (oluacle.dll / oluacle.so)
        Standard Lua interpretor can include it with 'require' statement.

Sample
======

One-liner
---------

Print a list of tables which 'HR' -- Oracle Express Edition's sample user -- owns.

[Windows]
    - Call Excutable on command prompt:

        C:> oluacle.exe -e "for rs in oluacle.new('HR','HR'):exec('select * from tab') do print(rs.TNAME) end"

    - On standard lua intepretor, include library:

        C:> lua.exe -e "for rs in require('oluacle').new('HR','HR'):exec('select * from tab') do print(rs.TNAME) end"

[Linux]
    - include library:

        $ lua -e 'for rs in require('oluacle').new("HR","HR"):exec("select * from tab") do print(rs.TNAME) end'


Another sample
--------------

print a list of tablespaces

SOURCE:
    conn=(oluacle or require('oluacle')).new(arg[1],arg[2])

    print(("%-10s %15s %15s"):format("TABLESPACE","ALLSIZE[KB]","FREESIZE[KB]"))
    print("------------------------------------------")
    for rs in conn:exec([[
            SELECT  t.TABLESPACE_NAME   as TABLESPACE_NAME ,
                    TRUNC(NVL(SUM(d.BYTES),0)/1024) as KBYTES ,
                    TRUNC(NVL(SUM(f.BYTES),0)/1024) as KFREE
             FROM  sys.dba_tablespaces t,
                   sys.dba_data_files  d,
                   sys.dba_free_space  f
             WHERE  t.TABLESPACE_NAME = d.TABLESPACE_NAME 
               AND  t.TABLESPACE_NAME = f.TABLESPACE_NAME(+)
          GROUP BY t.rowid, t.TABLESPACE_NAME
          ORDER BY t.rowid
    ]]) do
        print(("%-10s %15d %15d"):format(
            rs.TABLESPACE_NAME , rs.KBYTES , rs.KFREE ))
    end
    print("------------------------------------------")

    conn:disconnect()

OUTPUT:
    $ lua dbfree.lua (DBA-username) (password)
    TABLESPACE     ALLSIZE[KB]    FREESIZE[KB]
    ------------------------------------------
    SYSTEM              696320            2496
    UNDO                460800           76928
    SYSAUX            10106880           21632
    USERS               921600          100416
    ------------------------------------------
    $


Install
=======

[Windows]
    - put oluacle.dll on Lua's library directory ,for example,
    C:\Program Files\Lua\5.1\clibs.

    - put oluacle.exe on the one directory of directories pointed %PATH%.

[Linux]
    make -f Makefile.lin
    (requires lua-5.1.4 and devel packages)


Syntax
======

require 'oluacle'
-------------------------

Load oluacle.dll or oluacle.so  and  return the table with the method 'new' 
to connect database.

    oluacle = require('oluacle')

On oluacle.exe, it has not to do 'require'. The symbol 'oluacle' is used.

oluacle.new
-----------

Connect a Oracle-database and return connection-object.

    conn = oluacle.new('USERNAME','PASSWORD'[,'DBNAME'][,OPTIONTABLE])

When connection failed, error occurs.

OPTIONTABLE :

    { null=VALUE }
        Value used as NULL. default value is false.
        You can not set nil.


CONN:exec
---------

Execute SQL. conn is the return-value of oluacle.new
When SQL is 'SELECT', it returns iterator and statement-handle.
When SQL is 'INSERT','UPDATE' or 'DELETE', it returns the number of records.

    ITERATOR,BUFFER = conn:exec(SQL-STRING,B1,B2...)
or
    ITERATOR,BUFFER = conn:exec(SQL-STRING,{V1=B1,V2=B2...} )

B1,B2 are values for bind-variables. V1,V2 are names of bind-variables.

You call this with generic-for.

    for rs in conn:exec(SQL-STRING,B1,B2...) do
       :
    end

The table-variable 'rs' has values which you can access with 3-styles:

    (1) rs[1], rs[2] ...
    (2) rs.COLUMNNAME1, rs.COLUMNNAME2 ...
    (3) rs["COLUMNNAME1"] , rs["COLUMNNAME2"]

- NULL value is represented with 'false' by default.
  'new' method can change it with { null=... } except for nil.

- DATE value is represented with string formated 'YYYY/MM/DD HH24:MI:SS'


CONN:commit , CONN:rollback , CONN:disconnect
---------------------------------------------

CONN:commit() does commit database.
CONN:disconnect() rolls back database and disconnect.
When CONN is collected as garbage, CONN:disconnect is called. 


TO DO
=====

* function to trap error.
* BLOB


Bug report
==========

This package is tested on these softwares.

    WindowsXP SP3
    Mingw
    Oracle 10g Express Edition.

    Fedora 10
    Oracle 11g Instant Client

When you find bugs or have a question, please mail to iyahaya@nifty.com.

You can copy and modify oluacle with MIT License.
