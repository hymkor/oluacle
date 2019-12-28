oluacle (Lua for Oracle)
========================

Lua ���� Oracle-DB �ɃA�N�Z�X���郉�C�u����/�c�[���ł��B

Lua �ɂ� LuaSQL �Ƃ����ėp�� DB ���C�u����������܂����A

  * �o�C���h�ϐ����g���Ȃ�
  * ���\�b�h���ׂ����� SQL���s����Ɏ���܂ł̃X�e�b�v���������Ȃ肪��

�Ƃ������_������܂��B

oluacle �́ASQL ���s�܂łɕK�v�ȃ��\�b�h�� new �� exec �̓�ɗ}���A��
��ɏ��Ȃ��X�e�b�v���Ńf�[�^�𑀍�ł��郉�C�u�����ł��B���܂ŁASQL*Plus
+ awk �Ȃǂōs�Ȃ��Ă����������A Lua �ő�ւ��邱�Ƃ�ڎw���Ă��܂��B

Windows�̏ꍇ�ALua�{�̂̃C���X�g�[���͕s�v�ŁALua �C���^�v���^�{�̂�
���������P�i�t�@�C��(oluacle.exe)���R�s�[���邾���Ŏg���܂��B



�T���v��
========

One-liner
---------

OracleXE �̃T���v�����[�U��DB�e�[�u���ꗗ��W���o�͂֕\�����܂��B

bash ������Ȃ�΁F

    $ lua -e 'for rs in require("oluacle").new("HR","HR"):exec("select * from tab") do print(rs.TNAME) end'

Windows �̃R�}���h�v�����v�g(CMD.EXE)����́F

    lua -e "for rs in require('oluacle').new('HR','HR'):exec('select * from tab') do print(rs.TNAME) end"

bash �� CMD.EXE �ł͈��p�����Ђ�����Ԃ��Ă��邾���ŁA���͓����ł��B

���̑��AWindows �����ɂ́Alua �̃C���^�v���^(lua51.dll)�� oluacle.dll ��
static-link �����P�Ǝ��s�t�@�C�����p�ӂ��Ă��܂��B������g���ƁAOracle 
Client �̓C���X�g�[������Ă��邪�A lua ���C���X�g�[������Ă��Ȃ�����
���Aoluacle ���g�����Ƃ��ł��܂��B���̏ꍇ�A

    oluacle.exe -e "for rs in oluacle.new('HR','HR'):exec('select * from tab') do print(rs.TNAME) end"

�Ƃ����Ăяo�����ɂȂ�܂��B


������ƃf�J��
--------------

�\�̈�̈ꗗ��\�����܂��B

    if not oluacle then
        oluacle=require 'oluacle'
    end
    conn=oluacle.new(arg[1],arg[2])

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

    $ lua dbfree.lua (DBA���[�U��) (�p�X���[�h)
    TABLESPACE     ALLSIZE[KB]    FREESIZE[KB]
    ------------------------------------------
    SYSTEM              696320            2496
    UNDO                460800           76928
    SYSAUX            10106880           21632
    USERS               921600          100416
    ------------------------------------------
    $

�C���X�g�[��
============

Windows Binaries
----------------

oluacle.dll �́ALua �̃o�C�i�����C�u�����f�B���N�g�� (��> C:\Program Files\Lua\
5.1\clibs\) �ɒu���Ďg���܂��B

oluacle.exe �́APATH ���ʂ��Ă���f�B���N�g���Ȃ�A�ǂ��ɒu���Ă��\���܂���B

Linux
-----

make linux ���A�������� oluacle.so �� /usr/local/lib/lua/5.1/ �� cp ���Ă�����
���B



�\��
====

  * �d�l�����i�K�Ȃ̂ŁA����A�\���Ȃ��ɕύX����ꍇ������܂��B
  * Lua �ł́A(�A�z)�z����u�e�[�u���v�ƌĂт܂��B Oracle �̃e�[�u���Ƃ�₱��
    ���̂ŁA DB�e�[�u���ELua�e�[�u���Ɠǂ݊����ċ�ʂ��邱�Ƃɂ��܂��B


require 'oluacle'
-----------------

oluacle.dll/so ��ǂݍ��݂܂��B�߂�l�̃e�[�u���ɁA�ڑ����\�b�h 'new' ���܂܂�
�Ă��܂��B

oluacle = require('oluacle')

�Ȃ��Aoluacle.exe �ł� require �͕s�v�ł��B (oluacle �Ƃ����V���{���ɑ�������
��)


oluacle.new
------------

Oracle �Ɛڑ����āA�ڑ��I�u�W�F�N�g��Ԃ��܂��B

    conn = oluacle.new('���[�U��','�p�X���[�h'[,'DB��'][,�I�v�V����])

DB���A�I�v�V�����͏ȗ��\�ł��B

�ڑ��Ɏ��s����ƁA�������ɃG���[�ɂȂ�̂ŁA pcall �ȂǂŃL���b�`���Ă��������B

�I�v�V������ { null="" } �Ƃ�����Lua�e�[�u���ł��B���݂� null �̑�֒l���w�肷
�邱�Ƃ����o���܂���B


conn:exec
---------

SQL �����s���܂��B SELECT ���̏ꍇ�͊e�s���擾����C�^���[�^�ƁA���n���h���I�u
�W�F�N�g��Ԃ��܂��B (conn �� oluacle.new �̖߂�l�ł�)

    iterator,buffer = conn:exec('SQL�\��',�o�C���h�ϐ��c)
      or
    iterator,buffer = conn:exec('SQL�\��',{�ϐ���=�l ,�ϐ���=�l�c} )

�C�^���[�^�͕��n���h���������Ɏ��s����Ɗe���R�[�h��Lua�e�[�u���`���ŕԂ��܂��B
�ʏ�͎��̂悤�� for �� exec ��g�ݍ��킹��̂ŁA�ӎ�����K�v�͂���܂���B

    for rs in conn:exec('SQL�\��',�o�C���h�ϐ��c) do
           :
    end

  * rs �͎��̌`���Ŋe�J�����̓��e���i�[����܂��B
      + rs[�C���f�b�N�X] (�C���f�b�N�X�� 1 �X�^�[�g�̐����l)
      + rs["�J������"] (�J�������͑啶��)
      + rs.�J������ (�J�������͑啶��)
  * NULL �l�� false �ƂȂ�܂����Aolua.new �̃I�v�V�����ŕύX�\�ł� (����ł�
    nil �ɂ͏o���܂���)
  * DATE �^�� YYYY/MM/DD HH24:MI:SS �`���̕�����ƂȂ�܂��B
  * INSERT,UPDATE,DELETE �����g�����ꍇ�́A�X�V�����s����Ԃ��܂��B

conn:commit , conn:rollback , conn:disconnect
---------------------------------------------

�R�~�b�g/���[���o�b�N/�ؒf���s���܂��B disconnect() �̓��[���o�b�N���Ă���ؒf
���s���܂��B�܂��A�ڑ��I�u�W�F�N�g���K�ׁ[�W�R���N�g���ꂽ�����A disconnect ��
�Ăяo����܂��B

�Q�l
====

  * Lua�֌W
      + Lua 5.1 ���t�@�����X�}�j���A��
  * Oracle Call Interface
      + Amazon.co.jp�F OracleC/C++���H���� (DBMagazine SELECTION): �}���K�j: �{
      + Oracle Call Interface�v���O���}�[�Y�E�K�C�h 10g�����[�X2�i10.2�j
  * ���� OCI �c�[��
      + LuaSQL: Database connectivity for the Lua programming language
          o ODBC, ADO, Oracle, MySQL, SQLite , PostgreSQL �ɑΉ������A Lua ��
            DB ���C�u�����ł��B���ʂ͂��������g���Ǝv���܂����A�o�C���h�ϐ����g
            ���āA���������ȈՂȃC���^�[�t�F�C�X���~�����������߁A Lua for
            Oracle �����܂����B
      + ruby-oci8
          o Oracle Database�p�� ruby �C���^�[�t�F�[�X�ł��B�g������̗ǂ��Ƀ��X
            �y�N�g���āA�d�l�̎�{�Ƃ����Ă��������܂����B


�ۑ�
====

  * �G���[�������������ɓ���̊֐����ĂԂ悤�ɂ���
      + ��Foluacle.new(�`,�`,{ on_error=�֐� })
  * BLOG�^�ȂǁA���̌^�ւ̑Ή�
  * ���O�w��ɂ��ϐ��o�C���h
  * #define DEBUG �̎��̃��b�Z�[�W�̐���
  * SQL ���M���O�@�\
  * ���t������̎w��
      + ���͊��ϐ��w��� YYYY/MM/DD HH24:MI:SS �Œ艻���Ă���̂ŁA alter
        session ���ŕύX����K�v������B


�ύX����
========

  * 20090524
      + ����
  * 20090525
      + exec �͍X�V���A�X�V����������Ԃ��悤�ɂ����B
  * 2009052522
      + �J�������̖����ɃS�~�����Ă��܂����ɑΉ�
  * 2009052602
      + olua �� oluacle �ƃ��l�[���B
      + lua.exe �p�� DLL ��p�ӂ����B
  * 2009052901
      + �G���[�����ɕ⏕���C�u����(luaL_***)�n���C�u�������g���悤�ɂ��āA���b
        �Z�[�W�̌n�� Lua ���ɉ��߂�B
  * 2009053020
      + oluacle.new �̑�O����(�f�[�^�x�[�X��)���ȗ��ł���悤�ɂ����B
      + ���n���h���E�G���[�n���h���Ȃǂ̃O���[�o���ϐ���p�~
  * 20090531
      + ���O�w��̃o�C���h���o����悤�ɂȂ����B
  * 20090608
      + Linux�� Makefile ��p��
      + ���y�[�W�̐����� StandAlone�Ŏ��s�t�@�C�����S����A DLL �t�@�C�����S��
        �ύX����B
  * 20090924
      + �p�b�P�[�W�� zip �ɂ����BLuaForge �ւ��o�^
        �� �������A���̌�A�T�[�o��Q�ŏ�����������͗l (;_;)
  * 20100111
      + oluacle �Ƃ����V���{���𒼐ڎg�킸�Arequire �̖߂�l�Ƀ��\�b�h��Ԃ���
        ���ɂ����B
  * 20100523
      + �ăp�b�P�[�W���O�E�h�L�������g����

�A����
======

oluacle (Lua for Oracle) �́A�t���[�\�t�g�E�F�A�ł��B

���[�U�́AMIT License �ɂ��ƂÂ��āA�{�\�t�g�E�F�A��
�g�p�E�����E���ρE�z�z�����邱�Ƃ��ł��܂��B��� �t�R �O�́A
�{�\�t�g�E�F�A�ɂ���Đ��������ہE��Q�Ɋւ��Ĉ�ؐӔC�𕉂��܂���B

�ŐV�ł̃o�C�i���E�\�[�X�́A���L�t�q�k����_�E�����[�h�\�ł��B

    http://forgot.uh-oh.jp/doc/index.cgi?p=oluacle

����E�o�O���|�[�g�E��Ă��������܂�����A�ȉ��܂ł��A�����������B

    iyahaya@nifty.com

���̃p�b�P�[�W�́A�ȉ��̊��œ�����m�F���Ă��܂��B

    WindowsXP SP3
    MinGW32
    Oracle 10g Express Edition

�s����͂���܂�����Aiyahaya@nifty.com �܂ł��A�����������B
