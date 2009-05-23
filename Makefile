### configuration ###
ORACLE_HOME=/cygdrive/c/oraclexe/app/oracle/product/10.2.0/server
LUADIR=../lua-5.1.4
EXENAME=olua.exe
#####################

$(EXENAME) : olua.o luaone.o
	$(CC) -mno-cygwin -o $@ $^ -llua -L$(LUADIR)/src -loci \
		-L$(ORACLE_HOME)/oci/lib/msvc

olua.o : olua.c
luaone.o : luaone.c 

.c.o :
	$(CC) -mno-cygwin -I$(LUADIR)/src -I$(ORACLE_HOME)/oci/include -Wall -c $<

install:
	cp $(EXENAME) /cygdrive/c/usr/bin/.
clean:
	rm *.o *.exe
test1: test1.lua
	./$(EXENAME) test1.lua
test2: test2.lua
	./$(EXENAME) test2.lua
test3: test3.lua
	./$(EXENAME) test3.lua
test4: test4.lua
	./$(EXENAME) test4.lua

# vim:set noet ts=8 sw=8:
