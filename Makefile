CC = i686-w64-mingw32-gcc
OBJDUMP = i686-w64-mingw32-objdump
CFLAGS = -Os -march=i386 -mtune=i386 -Wall -Wextra -Werror -std=c89 -pedantic -fno-builtin -fno-stack-protector -DWINVER=0x0400 -D_WIN32_WINNT=0x0400 -D_WIN32_WINDOWS=0x0400
# No modern CRT startup: it would introduce APIs unavailable on Windows 95.
LDFLAGS = -mwindows -nostdlib -Wl,--entry,_welcome_entry,--subsystem,windows:4.0,--major-os-version,4,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat

.PHONY: all clean inspect
all: build/welcome95.exe

build/welcome95.exe: src/welcome95.c Makefile
	mkdir -p build
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -luser32 -lgdi32 -ladvapi32 -lkernel32

inspect: all
	$(OBJDUMP) -p build/welcome95.exe

clean:
	rm -rf build
