LUALIB=-I/Users/eugeneye/Documents/gbc-core/gbc-core/bin/openresty/luajit/include/luajit-2.1 -L/Users/eugeneye/Documents/gbc-core/gbc-core/bin/openresty/luajit/lib -lluajit-5.1
SOCKETLIB=-lws2_32

.PHONY: all win linux

all : 
	@echo Please do \'make PLATFORM\' where PLATFORM is one of these:
	@echo win linux

win: bson.dll

linux: bson.so

bson.dll : bson.c
	gcc --shared -Wall -O2 $^ -o$@ $(LUALIB) $(SOCKETLIB)

bson.so : bson.c
	gcc --shared -Wall -fPIC -O2 $^ -o$@ $(LUALIB)

clean:
	rm -f bson.dll bson.so
