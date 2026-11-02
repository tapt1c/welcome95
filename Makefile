CC = i686-w64-mingw32-gcc
OBJDUMP = i686-w64-mingw32-objdump
TEST_CC = x86_64-w64-mingw32-gcc
CFLAGS = -Os -march=i386 -mtune=i386 -Wall -Wextra -Werror -std=c89 -pedantic -fno-builtin -fno-stack-protector -DWINVER=0x0400 -D_WIN32_WINNT=0x0400 -D_WIN32_WINDOWS=0x0400
# No modern CRT startup: it would introduce APIs unavailable on Windows 95.
LDFLAGS = -mwindows -nostdlib -Wl,--subsystem,windows:4.0,--major-os-version,4,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat

.PHONY: all clean inspect test-helper test-helper64
all: build/welcome95.exe

build/welcome95.exe: src/welcome95.c src/native_logon.c src/native_logon.h Makefile
	mkdir -p build
	$(CC) $(CFLAGS) src/welcome95.c src/native_logon.c -o $@ $(LDFLAGS) -Wl,--entry,_welcome_entry -luser32 -lgdi32 -ladvapi32 -lkernel32

inspect: all
	$(OBJDUMP) -p build/welcome95.exe

test-helper: build/native_logon_test.exe

build/native_logon_test.exe: tests/native_logon_test.c src/native_logon.c src/native_logon.h Makefile
	mkdir -p build
	$(CC) $(CFLAGS) tests/native_logon_test.c src/native_logon.c -o $@ $(LDFLAGS) -Wl,--entry,_test_entry -luser32 -lgdi32 -lkernel32

# 64-bit test executables run under Wine64; production remains a Win95 x86 EXE.
test-helper64: build/native_logon_test64.exe build/topmost_test64.exe

build/native_logon_test64.exe: tests/native_logon_test.c src/native_logon.c src/native_logon.h Makefile
	mkdir -p build
	$(TEST_CC) $(filter-out -march=i386 -mtune=i386,$(CFLAGS)) tests/native_logon_test.c src/native_logon.c -o $@ -mwindows -nostdlib -Wl,--entry,test_entry -luser32 -lgdi32 -lkernel32

build/topmost_test64.exe: tests/topmost_test.c src/welcome95.c src/native_logon.c src/native_logon.h Makefile
	mkdir -p build
	$(TEST_CC) $(filter-out -march=i386 -mtune=i386,$(CFLAGS)) tests/topmost_test.c src/native_logon.c -o $@ -mwindows -nostdlib -Wl,--entry,topmost_test_entry -luser32 -lgdi32 -ladvapi32 -lkernel32

clean:
	rm -rf build
