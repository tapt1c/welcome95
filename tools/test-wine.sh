#!/bin/sh
# Optional real-window tests; uses its own display and Wine prefix, never login.
set -eu
cd "$(dirname "$0")/.."
local_root=/workspace/.tools/win-tests
if [ -x "$local_root/usr/lib/wine/wine64" ]; then
    compiler="$local_root/usr/bin/x86_64-w64-mingw32-gcc-win32"
    loader="$local_root/usr/lib/wine/wine64"
    server="$local_root/usr/lib/wine/wineserver64"
    xvfb="$local_root/usr/bin/Xvfb"
    prefix="$local_root/prefix2"
    run_wine() {
        env DISPLAY=":$test_display" WINEPREFIX="$prefix" \
            WINEDATADIR="$local_root/usr/share/wine" \
            WINEDLLPATH="$local_root/usr/lib/x86_64-linux-gnu/wine/x86_64-windows" \
            LD_LIBRARY_PATH="$local_root/usr/lib/x86_64-linux-gnu:$local_root/usr/lib/x86_64-linux-gnu/wine/x86_64-unix" \
            WINEDEBUG=-all WINEDLLOVERRIDES='mscoree,mshtml=' "$loader" "$@"
    }
else
    compiler=x86_64-w64-mingw32-gcc
    loader=$(command -v wine64 || command -v wine || printf '%s' /usr/lib/wine/wine64)
    server=$(command -v wineserver || printf '%s' /usr/lib/wine/wineserver64)
    xvfb=Xvfb
    prefix="$PWD/build/wine-prefix"
    run_wine() {
        env DISPLAY=":$test_display" WINEPREFIX="$prefix" WINEARCH=win64 \
            WINEDEBUG=-all WINEDLLOVERRIDES='mscoree,mshtml=' "$loader" "$@"
    }
fi
resource_compiler="$(dirname "$compiler")/x86_64-w64-mingw32-windres"
if command -v x86_64-w64-mingw32-windres >/dev/null 2>&1; then resource_compiler=x86_64-w64-mingw32-windres; fi
make test-helper64 TEST_CC="$compiler" TEST_WINDRES="$resource_compiler"
rm -f native-logon-test.log topmost-test.log fonts-test.log build/test-display
"$xvfb" -displayfd 3 -screen 0 800x600x24 -nolisten tcp \
    3>build/test-display >build/xvfb-test.log 2>&1 &
display_pid=$!
cleanup() {
    env WINEPREFIX="$prefix" "$server" -k 2>/dev/null || true
    kill "$display_pid" 2>/dev/null || true
    wait "$display_pid" 2>/dev/null || true
}
trap cleanup EXIT HUP INT TERM
attempt=0
while [ ! -s build/test-display ]; do
    if ! kill -0 "$display_pid" 2>/dev/null || [ "$attempt" -ge 50 ]; then
        cat build/xvfb-test.log >&2
        exit 1
    fi
    attempt=$((attempt + 1))
    sleep 0.1
done
test_display=$(cat build/test-display)
case "$test_display" in ''|*[!0-9]*) echo 'Invalid Xvfb display' >&2; exit 1;; esac
run_wine build/native_logon_test64.exe
run_wine build/topmost_test64.exe
run_wine build/fonts_test64.exe
python3 - <<'PY'
from pathlib import Path
count = 0
for name in ('native-logon-test.log', 'topmost-test.log', 'fonts-test.log'):
    lines = Path(name).read_text().splitlines()
    if not lines or any(not line.startswith('PASS: ') for line in lines):
        raise SystemExit(f'FAIL: {name}: missing or failed checks')
    count += len(lines)
    print(f'PASS: {name}: {len(lines)} checks')
print(f'PASS: {count} real-window checks under Wine64; Windows 95 boot remains untested')
PY
