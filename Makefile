### configuration ###
ifndef ORACLE_HOME
ORACLE_HOME=/cygdrive/c/oraclexe/app/oracle/product/10.2.0/server
endif
HOME=/usr/local
#####################

all : oluacle.exe oluacle.dll

# mingw standalone executable
oluacle.exe : olua.o luaone.o
	$(CC) -mno-cygwin -o $@ $^ -llua -L$(HOME)/lib -loci -L$(ORACLE_HOME)/oci/lib/msvc

# mingw-dynamic library
oluacle.dll : olua.o oluacle.def
	$(CC) -shared -mno-cygwin -o $@ $^ -Wl,--exclude-libs,ALL -llua51 -L$(HOME)/lib -loci -L$(ORACLE_HOME)/oci/lib/msvc 

olua.o : olua.c
luaone.o : luaone.c 

.c.o :
	$(CC) -mno-cygwin -I$(HOME)/include -I$(ORACLE_HOME)/oci/include -Wall -c $<

install-cygbin:
	cp oluacle.exe $(HOME)/bin/.
install-cygdll:
	cp oluacle.dll $(HOME)/lib/lua/5.1/.

# install to Lua for Windows
install-windll:
	cp oluacle.dll "/cygdrive/c/Program Files/Lua/5.1/clibs/."
clean:
	rm *.o *.exe *.dll
package :
	tar jcvf olua-`date +%Y%m%d%H`.tar.bz2 *.exe *.dll test*

test1: test1.lua
	./oluacle test1.lua
test2: test2.lua
	./oluacle test2.lua
test3: test3.lua
	./oluacle test3.lua
test4: test4.lua
	./oluacle test4.lua

# vim:set noet ts=8 sw=8:
