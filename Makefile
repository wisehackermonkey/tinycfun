CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -Wno-unused-parameter
LIBS    = -lole32 -luuid -luser32 -lgdi32 -ladvapi32

tipdemo.dll: tipdemo.c tipdemo.def
	$(CC) -shared $(CFLAGS) -o $@ tipdemo.c tipdemo.def $(LIBS) -static -s

clean:
	-del tipdemo.dll 2>nul
	-rm -f tipdemo.dll
