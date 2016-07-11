CC = gcc
CFLAGS = -O2 -fPIC -Wall
LUADIR = /Users/eugeneye/Documents/gbc-core/gbc-core/bin/openresty/luajit/include/luajit-2.1 -L/Users/eugeneye/Documents/gbc-core/gbc-core/bin/openresty/luajit/lib -lluajit-5.1
TARGET = protobuf.so

.PHONY : all clean

all : $(TARGET)

$(TARGET) : pbc-lua.c
	$(CC) $(CFLAGS) -shared -o $@ -I../.. -I$(LUADIR) -L../../build $^ -lpbc

clean :
	rm -f $(TARGET)
