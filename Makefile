CC = i686-w64-mingw32-gcc
OBJDUMP = i686-w64-mingw32-objdump
WINDRES = i686-w64-mingw32-windres
TEST_CC = x86_64-w64-mingw32-gcc
CFLAGS = -Os -march=i386 -mtune=i386 -Wall -Wextra -Werror -std=c89 -pedantic -fno-builtin -fno-stack-protector -DWINVER=0x0400 -D_WIN32_WINNT=0x0400 -D_WIN32_WINDOWS=0x0400
# No modern CRT startup: it would introduce APIs unavailable on Windows 95.
LDFLAGS = -mwindows -nostdlib -Wl,--subsystem,windows:4.0,--major-os-version,4,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat

.PHONY: all clean inspect test-helper test-helper64
all: build/welcome95.exe

build/welcome95.exe: src/welcome95.c src/native_logon.c src/native_logon.h src/profiles.c src/profiles.h src/fonts.c src/fonts.h src/power_dialog.c src/power_dialog.h src/power_actions.c src/power_actions.h src/shutdown_dos.c src/shutdown_dos.h src/startup.c src/startup.h src/control_panel.c src/control_panel.h build/assets.o Makefile
	mkdir -p build
	$(CC) $(CFLAGS) src/welcome95.c src/native_logon.c src/profiles.c src/fonts.c src/power_dialog.c src/power_actions.c src/shutdown_dos.c src/startup.c src/control_panel.c build/assets.o -o $@ $(LDFLAGS) -Wl,--entry,_welcome_entry -luser32 -lgdi32 -ladvapi32 -lkernel32 -lshell32

inspect: all
	$(OBJDUMP) -p build/welcome95.exe

test-helper: build/native_logon_test.exe

build/native_logon_test.exe: tests/native_logon_test.c src/native_logon.c src/native_logon.h Makefile
	mkdir -p build
	$(CC) $(CFLAGS) tests/native_logon_test.c src/native_logon.c -o $@ $(LDFLAGS) -Wl,--entry,_test_entry -luser32 -lgdi32 -lkernel32

# 64-bit test executables run under Wine64; production remains a Win95 x86 EXE.
test-helper64: build/control_panel_test64.exe build/ui_test64.exe build/native_logon_test64.exe build/topmost_test64.exe build/fonts_test64.exe build/profiles_test64.exe build/render_test64.exe build/power_test64.exe build/dos_test64.exe build/layout_test64.exe build/startup_test64.exe build/session_test64.exe

build/native_logon_test64.exe: tests/native_logon_test.c src/native_logon.c src/native_logon.h Makefile
	mkdir -p build
	$(TEST_CC) $(filter-out -march=i386 -mtune=i386,$(CFLAGS)) tests/native_logon_test.c src/native_logon.c -o $@ -mwindows -nostdlib -Wl,--entry,test_entry -luser32 -lgdi32 -lkernel32

build/topmost_test64.exe: tests/topmost_test.c src/welcome95.c src/native_logon.c src/native_logon.h src/profiles.c src/profiles.h src/fonts.c src/fonts.h src/power_dialog.c src/power_dialog.h src/power_actions.c src/power_actions.h src/shutdown_dos.c src/shutdown_dos.h src/startup.c src/startup.h Makefile
	mkdir -p build
	$(TEST_CC) $(filter-out -march=i386 -mtune=i386,$(CFLAGS)) tests/topmost_test.c src/native_logon.c src/profiles.c src/fonts.c src/power_dialog.c src/power_actions.c src/shutdown_dos.c src/startup.c src/control_panel.c -o $@ -mwindows -nostdlib -Wl,--entry,topmost_test_entry -luser32 -lgdi32 -ladvapi32 -lkernel32 -lshell32

clean:
	rm -rf build

build/assets.o: build/w95boot.exe build/w95panel.cpl assets/welcome95.rc $(wildcard assets/*.bmp) $(wildcard assets/fonts/*.TTF)
	mkdir -p build
	$(WINDRES) --preprocessor="$(CC)" --preprocessor-arg=-E --preprocessor-arg=-xc --preprocessor-arg=-DRC_INVOKED -I assets assets/welcome95.rc -O coff -o $@

TEST_WINDRES = x86_64-w64-mingw32-windres
build/assets64.o: build/w95boot.exe build/w95panel.cpl assets/welcome95.rc $(wildcard assets/*.bmp) $(wildcard assets/fonts/*.TTF)
	mkdir -p build
	$(TEST_WINDRES) --preprocessor="$(TEST_CC)" --preprocessor-arg=-E --preprocessor-arg=-xc --preprocessor-arg=-DRC_INVOKED -I assets assets/welcome95.rc -O coff -o $@

build/fonts_test64.exe: tests/fonts_test.c src/fonts.c src/fonts.h build/assets64.o Makefile
	$(TEST_CC) $(filter-out -march=i386 -mtune=i386,$(CFLAGS)) tests/fonts_test.c src/fonts.c build/assets64.o -o $@ -mwindows -nostdlib -Wl,--entry,fonts_test_entry -luser32 -lgdi32 -lkernel32

build/preview64.exe: src/welcome95.c src/native_logon.c src/profiles.c src/fonts.c src/power_dialog.c src/power_actions.c src/shutdown_dos.c src/startup.c src/control_panel.c build/assets64.o Makefile
	$(TEST_CC) $(filter-out -march=i386 -mtune=i386,$(CFLAGS)) src/welcome95.c src/native_logon.c src/profiles.c src/fonts.c src/power_dialog.c src/power_actions.c src/shutdown_dos.c src/startup.c src/control_panel.c build/assets64.o -o $@ -mwindows -nostdlib -Wl,--entry,welcome_entry -luser32 -lgdi32 -ladvapi32 -lkernel32 -lshell32

build/profiles_test64.exe: tests/profiles_test.c src/profiles.c src/profiles.h Makefile
	$(TEST_CC) $(filter-out -march=i386 -mtune=i386,$(CFLAGS)) tests/profiles_test.c -o $@ -mwindows -nostdlib -Wl,--entry,profiles_test_entry -luser32 -ladvapi32 -lkernel32

build/render_test64.exe: tests/render_test.c src/welcome95.c src/native_logon.c src/profiles.c src/fonts.c src/power_dialog.c src/power_actions.c src/shutdown_dos.c src/startup.c src/control_panel.c build/assets64.o Makefile
	$(TEST_CC) $(filter-out -march=i386 -mtune=i386,$(CFLAGS)) tests/render_test.c src/native_logon.c src/profiles.c src/fonts.c src/power_dialog.c src/power_actions.c src/shutdown_dos.c src/startup.c src/control_panel.c build/assets64.o -o $@ -mwindows -nostdlib -Wl,--entry,render_test_entry -luser32 -lgdi32 -ladvapi32 -lkernel32 -lshell32

POWER_SOURCES = src/native_logon.c src/profiles.c src/fonts.c src/power_dialog.c src/power_actions.c src/shutdown_dos.c src/startup.c src/control_panel.c
build/power_test64.exe: tests/power_test.c src/welcome95.c $(POWER_SOURCES) $(wildcard src/*.h) build/assets64.o Makefile
	$(TEST_CC) $(filter-out -march=i386 -mtune=i386,$(CFLAGS)) tests/power_test.c $(POWER_SOURCES) build/assets64.o -o $@ -mwindows -nostdlib -Wl,--entry,power_test_entry -luser32 -lgdi32 -ladvapi32 -lkernel32 -lshell32

build/dos_test64.exe: tests/dos_test.c tests/power_dos_cases.h src/shutdown_dos.c src/shutdown_dos.h Makefile
	$(TEST_CC) $(filter-out -march=i386 -mtune=i386,$(CFLAGS)) tests/dos_test.c src/shutdown_dos.c -o $@ -mwindows -nostdlib -Wl,--entry,dos_test_entry -luser32 -lkernel32 -lshell32

build/layout_test64.exe: tests/layout_test.c src/welcome95.c $(POWER_SOURCES) $(wildcard src/*.h) build/assets64.o Makefile
	$(TEST_CC) $(filter-out -march=i386 -mtune=i386,$(CFLAGS)) tests/layout_test.c $(POWER_SOURCES) build/assets64.o -o $@ -mwindows -nostdlib -Wl,--entry,layout_test_entry -luser32 -lgdi32 -ladvapi32 -lkernel32 -lshell32

build/startup_test64.exe: tests/startup_test.c src/startup.c src/startup.h Makefile
	$(TEST_CC) $(filter-out -march=i386 -mtune=i386,$(CFLAGS)) tests/startup_test.c -o $@ -mwindows -nostdlib -Wl,--entry,startup_test_entry -luser32 -ladvapi32 -lkernel32

build/session_test64.exe: src/bootstrap.c src/bootstrap.h tests/session_test.c src/welcome95.c $(POWER_SOURCES) $(wildcard src/*.h) build/assets64.o Makefile
	$(TEST_CC) $(filter-out -march=i386 -mtune=i386,$(CFLAGS)) tests/session_test.c src/bootstrap.c -DWELCOME95_BOOTSTRAP_NO_ENTRY $(filter-out src/profiles.c,$(POWER_SOURCES)) build/assets64.o -o $@ -mwindows -nostdlib -Wl,--entry,session_test_entry -luser32 -lgdi32 -ladvapi32 -lkernel32 -lshell32


build/w95boot.exe: src/bootstrap.c src/bootstrap.h src/startup.c src/startup.h Makefile
	$(CC) $(CFLAGS) src/bootstrap.c src/startup.c -o $@ $(LDFLAGS) -Wl,--entry,_boot_entry -luser32 -ladvapi32 -lkernel32

# Keep every consumer of ANSI UI text current, including the embedded bootstrap.
build/welcome95.exe build/w95boot.exe build/native_logon_test.exe build/native_logon_test64.exe build/topmost_test64.exe build/preview64.exe build/profiles_test64.exe build/render_test64.exe build/power_test64.exe build/layout_test64.exe build/session_test64.exe: src/ui_text.h

build/ui_test64.exe: tests/ui_test.c src/welcome95.c $(POWER_SOURCES) $(wildcard src/*.h) build/assets64.o Makefile
	$(TEST_CC) $(filter-out -march=i386 -mtune=i386,$(CFLAGS)) -DWELCOME95_TEST_RUSSIAN=1 tests/ui_test.c $(POWER_SOURCES) build/assets64.o -o $@ -mwindows -nostdlib -Wl,--entry,ui_test_entry -luser32 -lgdi32 -ladvapi32 -lkernel32 -lshell32

# Native Win95 applet: only metadata and a launcher, not a second settings UI.
build/w95panel.cpl: src/control_panel_launcher.c src/control_panel.h src/ui_text.h Makefile
	$(CC) $(CFLAGS) -shared src/control_panel_launcher.c -o $@ $(LDFLAGS) -Wl,--entry,_DllMain@12,--kill-at -luser32 -ladvapi32 -lkernel32

build/w95panel64.cpl: src/control_panel_launcher.c src/control_panel.h src/ui_text.h Makefile
	$(TEST_CC) $(filter-out -march=i386 -mtune=i386,$(CFLAGS)) -DW95_PANEL_TEST -shared src/control_panel_launcher.c -o $@ -mwindows -nostdlib -Wl,--entry,DllMain -luser32 -ladvapi32 -lkernel32

build/control_panel_test64.exe: tests/control_panel_test.c src/control_panel.c src/control_panel.h src/startup.c src/startup.h build/w95panel64.cpl Makefile
	$(TEST_CC) $(filter-out -march=i386 -mtune=i386,$(CFLAGS)) tests/control_panel_test.c src/control_panel.c src/startup.c -o $@ -mwindows -nostdlib -Wl,--entry,panel_test_entry -luser32 -ladvapi32 -lkernel32

build/welcome95.exe build/topmost_test64.exe build/preview64.exe build/render_test64.exe build/power_test64.exe build/layout_test64.exe build/session_test64.exe build/ui_test64.exe: src/control_panel.c src/control_panel.h
