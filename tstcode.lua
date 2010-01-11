function exists_table(conn)
    for rs in conn:exec([[
        select count(*) as CNT from tab
         where tname = :t
    ]],"OLUACLE_TEST") do
        return rs.CNT
    end
    return nil
end

function create_table(conn)
    local rc = conn:exec([[
        create table oluacle_test ( 
            serial   number(4)    not null ,
            value    varchar2(32) not null ,
            stamp    date         not null ,
            primary key(serial)
        )
    ]])
    print("CREATE TABLE == "..rc )
end

function drop_table(conn)
    local rc = conn:exec([[
        drop table oluacle_test
    ]]);
    print("DROP TABLE OLUACLE_TEST == " .. rc )
end

function insert_table(conn,key)
    local rc = conn:exec([[
        insert into oluacle_test values (
            :n , :v , to_date(:d,'YYYY/MM/DD HH24:MI:SS')
        )
    ]], key , 'HOGE' , '2010/01/20 12:23:34');
    print("INSERT TABLE OLUACLE_TEST == " .. rc )
end

function select_table(conn)
    for rs in conn:exec('select * from oluacle_test') do
        print( table.concat(rs,",") )
    end
end

function update_table(conn)
    local rc = conn:exec([[
        update oluacle_test set serial = serial + 1
    ]])
    print("UPDATE TABLE OLUACLE_TEST == " .. rc )
end

if not oluacle then
    oluacle = require 'oluacle'
end
conn = oluacle.new( arg and arg[1] or 'HR' ,
                    arg and arg[2] or 'HR' ,
                    { null='<null>' } )

local rc=exists_table(conn)
if rc >= 1 then
    print "The table 'OLUACLE_TEST' already exists"
else
    print "The table 'OLUACLE_TEST' does not exists"
    create_table(conn)
end
print "ORIGINAL"
select_table(conn)
print "UPDATE KEY++"
update_table(conn)
select_table(conn)
print "INSERT 1"
insert_table(conn,1)
select_table(conn)
print "COMMIT"
conn:commit()
print "INSERT 999"
insert_table(conn,999)
select_table(conn)
print "DO ROLLBACK"
conn:rollback()
select_table(conn)
drop_table(conn)
conn:logoff()
