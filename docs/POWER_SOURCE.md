# Shutdown dialog source

Ported at the repository owner's request from
[tapt1c/explorer95](https://github.com/tapt1c/explorer95/tree/0f33e8f691f1093d95dfecf5c05e3c18f46ff7af),
commit `0f33e8f691f1093d95dfecf5c05e3c18f46ff7af`:

- `src/shutdown.c`: compact 314×200 dialog, GDI snapshot fade, action drawing,
  optional suspend capability and removal of the overlay before OS prompts.
- `src/shutdown_dos.c`, `src/shutdown_dos.h`: packed Win95 PIF program group,
  real-mode setup, saved-property verification and isolated temporary files.
- `tests/shutdown_dos_cases.h`: inert PIF-manager fault-injection cases.
- `resources/Bitmap143.bmp`: Windows flag, copied to `assets/power-logo.bmp`.

Welcome95 adapts these to C89, the existing message loop, ANSI Russian/English
strings, original native password ownership and the CRT-free i386 build. Its
power operations are separated from the dialog so tests can use inert callbacks.
No Explorer95 shell, modern UI framework or third-party archive is bundled.
Windows flag artwork belongs to Microsoft.

Stand By is optional: power functions are resolved dynamically. Turn Off and
Restart use non-forced ExitWindowsEx. MS-DOS restart is a separate Win95-only PIF
path and never falls back to ordinary shutdown on preparation failure. The
system's existing PIFs and boot configuration are not modified. A successfully
dispatched PIF remains in its private directory beneath %WINDIR%\LOGON because Windows may read it
asynchronously; failed preparations remove only their own files.

The shade/choice windows close and input is restored before dispatch. Save,
cancellation and error prompts must remain accessible. Preview dispatches no
power operation. Tests verify this lifecycle and the PIF protocol with synthetic
fixtures; actual APM power transitions, pre-logon PIF execution and return from
MS-DOS still require Windows 95.


Welcome95 now explicitly invalidates/repaints its owner and child controls after
removing the shade. This is a local rendering fix for stale owner-drawn button
pixels; power dispatch, snapshot dimming and original enabled-state restoration
are unchanged. Regression checks inspect the actual welcome power-button pixels.


Russian choice labels use 10px Tahoma Bold: the supplied XP font measures
"Ждущий режим" at 90px with the former 11px setting, beyond the fixed 88px
column. The dialog/icon/control dimensions are unchanged. CP1251 UI tests measure
the actual single-line label area and cover all three power choices.


Managed installation keeps successful DOS PIFs in unique subdirectories beneath
%WINDIR%\LOGON instead of TEMP. The PIF protocol, verification, asynchronous
retention and failure cleanup are unchanged. Inert DOS tests now use a private
Windows/LOGON fixture. No real DOS or power request is made by the tests.


Buffered native-button painting follows the same approach as Explorer95's
[LinkProc](https://github.com/tapt1c/explorer95/blob/cb970dac91a1c31ac8433df27a1063a9967f8d1a/src/browser_control_panel.c):
intercept WM_ERASEBKGND/WM_PAINT while forwarding other messages to the original
BUTTON procedure. Welcome95 shares this through painted_button.h for its power,
submit and three power-choice action buttons, composing each frame off-screen.
It does not subclass Windows' password Edit or change native authentication.
CallWindowProcA is the classic Win95 subclassing API used by that reference;
this is the only added production import (USER32), audited with the x86 binary.

The choice's own client is also buffered. Parent damage only repairs custom
buttons that intersect its damage bounds; unrelated footer controls stay idle.
Cancel restores enabled states while the shade still covers the canvas, retaining
the active/closing guard through teardown and focus restoration. This blocks a
reentrant welcome timer from lifting the canvas over the closing shade.

Fade phases are selected from elapsed time (16 phases at nominal 35ms intervals).
Slow paints skip obsolete phases using one combined desaturation/dimming transform
per pixel, rather than replaying sixteen full-screen copies. No alpha/layered
window API, extra full-screen pixel buffer, OS power change or forced action was
added. The driver still determines the time taken to present each physical frame.
