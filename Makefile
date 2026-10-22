# CC      = gcc
# CFLAGS  = -O2 -Wall -Wextra -Wno-unused-parameter
# LIBS    = -lole32 -luuid -luser32 -lgdi32 -ladvapi32

# tipdemo.dll: tipdemo.c tipdemo.def
# 	$(CC) -shared $(CFLAGS) -o $@ tipdemo.c tipdemo.def $(LIBS) -static -s

# clean:
# 	-del tipdemo.dll 2>nul
# 	-rm -f tipdemo.dll
# CC      = gcc
# CFLAGS  = -O2 -Wall -Wextra -Wno-unused-parameter
# LIBS    = -lole32 -luuid -luser32 -lgdi32 -ladvapi32

# tipdemo.dll: tipdemo.c tipdemo.def
# 	$(CC) -shared $(CFLAGS) -o $@ tipdemo.c tipdemo.def $(LIBS) -static -s

# clean:
# 	-del tipdemo.dll 2>nul
# 	-rm -f tipdemo.dll
# tipdemo.dll: tipdemo.c chime.o
# 	$(CC) $(CFLAGS) -shared -o $@ tipdemo.c chime.o -lole32 -loleaut32 -luuid -luser32 -lgdi32 -ladvapi32
# CC      = x86_64-w64-mingw32-gcc
# WINDRES = x86_64-w64-mingw32-windres
# CFLAGS  = -O2 -Wall -DUNICODE -D_UNICODE
# LDFLAGS = -shared -municode
# LIBS    = -lole32 -loleaut32 -luuid -luser32 -lgdi32 -ladvapi32
# # model.res.o: model.rc cifg_int8.bin
# # 	$(WINDRES) model.rc -O coff -o $@
# all: tipdemo.dll

# # Compile the .rc into a COFF object so it lands in the DLL
# model.res.o: model.rc cifg_int8.bin
# 	$(WINDRES) model.rc -O coff -o $@

# tipdemo.dll: tipdemo.c chime.o model.res.o
# 	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ tipdemo.c chime.o model.res.o $(LIBS)

# clean:
# 	rm -f tipdemo.dll *.o

# .PHONY: all clean

CC      ?= gcc
CFLAGS  ?= -O2 -Wall -municode -DUNICODE -D_UNICODE

# --- Chime integration ---
MODEL_C = cifg_int8_bin.c

tipdemo.dll: tipdemo.c chime.o $(MODEL_C)
	$(CC) $(CFLAGS) -shared -o $@ tipdemo.c $(MODEL_C) chime.o \
	    -lole32 -loleaut32 -luuid -luser32 -lgdi32 -ladvapi32

# Regenerate embedded blob if the model changes
cifg_int8_bin.c: C:/Users/oran/github/tinycfun/cifg_int8.bin
	xxd -i $< > $@