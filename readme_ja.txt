oluacle (Lua for Oracle)
========================

Lua を使って、Oracle-DB にアクセスするツール・ライブラリです。

Lua には LuaSQL という汎用の DB ライブラリがありますが、

  * バインド変数が使えない
  * メソッドが細かくて SQL発行するに至るまでのステップ数が多くなりがち

という欠点があります。

oluacle は、SQL 発行までに必要なメソッドを new と exec の二つに抑え、非
常に少ないステップ数でデータを操作できるライブラリです。今まで、SQL*Plus
+ awk などで行なっていた処理を、 Lua で代替することを目指しています。

oluacle は、実行ファイルとライブラリを提供しています。

    実行ファイル：(oluacle.exe / oluacle )
        Lua インタプリタ内蔵。1実行ファイルをコピーするだけで使える
    ライブラリ： (oluacle.dll / oluacle.so )
        Lua のライブラリパスに置けば、標準の Lua インタプリタから
        呼び出し(require)できる。

サンプル
========

One-liner
---------

OracleXE のサンプルユーザのDBテーブル一覧を標準出力へ表示します。

【Windows】
    ◇ 実行ファイルをコマンドプロンプトより：

        C:> oluacle.exe -e "for rs in oluacle.new('HR','HR',{null=''}):exec('select * from tab') do print(rs.TNAME) end"


    ◇ ライブラリを LuaBinaries の Lua より require する。

        C:> lua -e "for rs in require('oluacle').new('HR','HR',{null=''}):exec('select * from tab') do print(rs.TNAME) end"


【Linux】
    ◇ 実行ファイルを bash より：

        $ ./oluacle -e 'for rs in oluacle.new("HR","HR","192.168.0.1:1521",{ null=""}):exec("select * from tab") do print( table.concat(rs,",") ) end'

    ◇ ライブラリを rpm で導入した Lua より require する。

        $ lua -e 'for rs in require("oluacle").new("HR","HR","192.168.0.1:1521",{ null=""}):exec("select * from tab") do print( table.concat(rs,",") ) end'



ちょっとデカめ
--------------

表領域の一覧を表示します。

ソース(dbfree.lua)：
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

実行例：
    $ lua dbfree.lua (DBAユーザ名) (パスワード)
    TABLESPACE     ALLSIZE[KB]    FREESIZE[KB]
    ------------------------------------------
    SYSTEM              696320            2496
    UNDO                460800           76928
    SYSAUX            10106880           21632
    USERS               921600          100416
    ------------------------------------------
    $


インストール
============

Windows
-------

oluacle.dll、oluacle.exe を PATH が通っているディレクトリに配置してください。


Linux
-----

Lua 5.2 を導入後、
    make -f Makefile.lin してください。


構文
====

  * 仕様検討段階なので、今後、予告なしに変更する場合があります。
  * Lua では、(連想)配列を「テーブル」と呼びます。 Oracle のテーブルとややこし
    いので、 DBテーブル・Luaテーブルと読み換えて区別することにします。


require 'oluacle'
-----------------

oluacle.dll/so を読み込みます。戻り値のテーブルに、接続メソッド 'new' が含まれ
ています。

oluacle = require('oluacle')

なお、oluacle.exe では require は不要です。 (oluacle というシンボルに代入されま
す)


oluacle.new
------------

Oracle と接続して、接続オブジェクトを返します。

    conn = oluacle.new('ユーザ名','パスワード'[,'DB接続文字列'][,オプション])

DB接続文字列、オプションは省略可能です。

接続に失敗すると、ただちにエラーになるので、pcall などでキャッチしてください。

オプションは { null="" } といったLuaテーブルです。現在は null の代替値を指定す
ることしか出来ません。


conn:exec
---------

SQL を実行します。 SELECT 文の場合は各行を取得するイタレータと、文ハンドルオブ
ジェクトを返します。 (conn は oluacle.new の戻り値です)

    iterator,buffer = conn:exec('SQL構文',バインド変数…)
      or
    iterator,buffer = conn:exec('SQL構文',{変数名=値 ,変数名=値…} )

イタレータは文ハンドルを引数に実行すると各レコードをLuaテーブル形式で返します。
通常は次のように for と exec を組み合わせるので、意識する必要はありません。

    for rs in conn:exec('SQL構文',バインド変数…) do
           :
    end

  * rs は次の形式で各カラムの内容が格納されます。
      + rs[インデックス] (インデックスは 1 スタートの整数値)
      + rs["カラム名"] (カラム名は大文字)
      + rs.カラム名 (カラム名は大文字)
  * NULL 値は false となりますが、oluacle.new のオプションで変更可能です (それでも
    nil には出来ません)
  * DATE 型は YYYY/MM/DD HH24:MI:SS 形式の文字列となります。
  * INSERT,UPDATE,DELETE 文を使った場合は、更新した行数を返します。

conn:commit , conn:rollback , conn:disconnect
---------------------------------------------

コミット/ロールバック/切断を行います。 disconnect() はロールバックしてから切断
を行います。また、接続オブジェクトがガべージコレクトされた時も、 disconnect が
呼び出されます。

参考
====

  * Lua関係
      + Lua 5.1 リファレンスマニュアル
      + Lua 5.2 リファレンスマニュアル
  * Oracle Call Interface
      + Amazon.co.jp： OracleC/C++実践入門 (DBMagazine SELECTION): 笠原規男: 本
      + Oracle Call Interfaceプログラマーズ・ガイド 10gリリース2（10.2）
  * 他の OCI ツール
      + LuaSQL: Database connectivity for the Lua programming language
          o ODBC, ADO, Oracle, MySQL, SQLite , PostgreSQL に対応した、 Lua の
            DB ライブラリです。普通はこっちを使うと思いますが、バインド変数が使
            えて、もう少し簡易なインターフェイスが欲しかったため、 Lua for
            Oracle を作りました。
      + ruby-oci8
          o Oracle Database用の ruby インターフェースです。使い勝手の良さにリス
            ペクトして、仕様の手本とさせていただきました。


課題
====

  * エラーが発生した時に特定の関数を呼ぶようにする
      + 例：oluacle.new(～,～,{ on_error=関数 })
  * BLOG型など、他の型への対応
  * 名前指定による変数バインド
  * #define DEBUG の時のメッセージの整備
  * SQL ロギング機能
  * 日付文字列の指定
      + 今は環境変数指定で YYYY/MM/DD HH24:MI:SS 固定化しているので、 alter
        session 文で変更する必要がある。


変更履歴
========

  * 20090524
      + 初版
  * 20090525
      + exec は更新時、更新した件数を返すようにした。
  * 2009052522
      + カラム名の末尾にゴミがついてしまう問題に対応
  * 2009052602
      + olua → oluacle とリネーム。
      + lua.exe 用の DLL を用意した。
  * 2009052901
      + エラー処理に補助ライブラリ(luaL_***)系ライブラリを使うようにして、メッ
        セージ体系を Lua 風に改める。
  * 2009053020
      + oluacle.new の第三引数(データベース名)を省略できるようにした。
      + 環境ハンドル・エラーハンドルなどのグローバル変数を廃止
  * 20090531
      + 名前指定のバインドが出来るようになった。
  * 20090608
      + Linux版 Makefile を用意
      + 当ページの説明を StandAlone版実行ファイル中心から、 DLL ファイル中心へ
        変更する。
  * 20090924
      + パッケージを zip にした。LuaForge へも登録
        ⇒ したが、その後、サーバ障害で消えちゃった模様 (;_;)
  * 20100111
      + oluacle というシンボルを直接使わず、require の戻り値にメソッドを返すよ
        うにした。
  * 20100523
      + 再パッケージング・ドキュメント整備
  * 20120104
      + Lua 5.2 対応

連絡先
======

oluacle (Lua for Oracle) は、フリーソフトウェアです。

ユーザは、MIT License にもとづいて、本ソフトウェアを
使用・複製・改変・配布をすることができます。作者 葉山 薫は、
本ソフトウェアによって生じた現象・障害に関して一切責任を負いません。

最新版のバイナリ・ソースは、下記ＵＲＬからダウンロード可能です。

    https://github.com/hymkor/oluacle/releases

このパッケージは、以下の環境で動作を確認しています。

    WindowsXP SP3
    MinGW32
    Oracle 10g Express Edition

    Fedora 10
    Oracle 11g Instant Client
