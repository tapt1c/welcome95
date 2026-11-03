# Welcome95 backlog

This filename (`BAKCLOG.md`) is intentional, as requested by the user.

## Current bug: selecting another user appears to freeze

Evidence: the user's 39-second Windows 95 recording shows responsive shutdown
but permanent “Preparing sign-in…” and inactive account selection. This establishes
that native attachment did not complete; it does not establish the exact reason.
The previous click handler refused to change selection unless native attachment
and the synchronous username update had already succeeded.

In this update:

- Card/page clicks update selection immediately and queue the native change.
- A blank initial native username no longer prevents attachment.
- The native edit is cleared before changing the username.
- Cross-process messages allow callbacks; timer servicing has a reentrancy guard.
- Identical layout calls avoid repeating style, region and geometry mutations.
- Failed attachment writes welcome95-diagnostics.txt beside the EXE. It contains
  reason codes and control metadata, never edit contents or passwords.

Confirmed diagnosis from the user's uploaded diagnostics: the native dialog has
14 controls, including active username Edit 21 and password Edit 23, plus hidden,
disabled Edit 25/29 and checkbox Button 26/30. The old classifier counted all
four Edit controls and rejected the dormant checkbox styles. It therefore never
attached despite the visible form being a simple username/password login.

Fixed: classify active roles after recording snapshots, ignoring controls only
when both hidden and disabled. A visible disabled or hidden enabled extra field
still rejects integration. Restore all original controls and states normally.
The window fixture reproduces all 14 class/ID/style combinations from the log.

Validation: 45 real-window checks passed under Wine64 (36 native integration,
9 canvas/priority checks), including account selection, input clearing, dormant
control preservation and rejection of extra fields that are not fully dormant.
The production x86 PE audit passed. Actual boot/login on the user's Win95 system
remains pending; later provider-specific failures are still possible.

## Required target-system checks

- [ ] Boot the updated EXE on Windows 95 and verify a blank/saved initial username.
- [ ] Select each account before typing; check actual native username and profile.
- [ ] Switch repeatedly: no old password input may carry into another account.
- [ ] Check Enter and green arrow with hidden original OK.
- [ ] Wrong password: accessible Windows error, then retry in the welcome screen.
- [ ] Confirm account discovery from actual ProfileList / SYSTEM.INI mappings.
- [ ] Verify 640×480, 800×600, multiple pages, and different ANSI/OEM localizations.
- [ ] Investigate the garbled first account name visible in the user's recording;
      determine its source and code page before applying any conversion.
- [ ] Verify shutdown, Disable, Trouble signing in? and safe-mode recovery.
- [ ] Cross-process integration with each supported Windows 95 network provider.

## Known architectural limits

- RunServices is asynchronous; appearing before the native dialog is not guaranteed.
- Run signals session startup, not authenticated success; Cancel may start a 95 session.
- No guarantee of covering Ctrl+Alt+Del, critical system screens or fullscreen DOS.
- Profiles are selected before login; live session switching is not implemented.
- Unknown provider forms require explicit native-dialog fallback.
- Up to 32 names and four cards per page; account creation/deletion belongs to Windows.
- All cards currently share the supplied chess avatar.

## Follow-up work

- Add a separate-process automated logon fixture with a temporarily unresponsive
  provider; current native tests use same-process fixtures under Wine64.
- Validate PWL mapping truncation/encoding and account enumeration errors.
- Add keyboard navigation and better visible feedback for failed profile changes.
- Confirm native restoration after resolution changes and provider-driven resizing.

## Welcome/shutdown state reconstruction and supplied fonts

Implemented loading states: one account has a lone greeting; multiple accounts
show only the selected account with loading text. The login/password phase remains
separate. Run's /session-ready now keeps a hidden session observer for ordinary
Windows shutdown notifications. Cancel restores the desktop; successful shutdown
exits through the message loop for font cleanup. Preview-only modes expire in 5 s.

Supplied Arial/Tahoma TTFs are embedded and temporarily registered via Win95 GDI.
XP resources identify Arial Bold Italic 36 pt with 2×3 shadow for welcome. Supplied
files are versions 7.x, so identical historical XP rasterization is not claimed.

Latest validation: 65 checks under Wine64: 36 native, 22 priority/state/observer,
7 font extraction/selection/cleanup checks. Three actual preview screenshots
were inspected. No real OS shutdown was triggered by tests.

- [ ] Validate one/many count and selected identity at real session-ready startup.
- [ ] Verify Russian CP1251 welcome/status rendering and Tahoma/Arial on Windows 95.
- [ ] Verify shutdown from Start menu, cancellation, Disable and helper exit on 95.
- [ ] Confirm temporary font cleanup on normal exits and observe forced-exit leftovers.
- [ ] Confirm final OS-owned shutdown screen handoff; ordinary EXE coverage ends
      when Windows closes the observer. No VxD or provider replacement is included.

## XP font replacement

The user supplied a second set extracted from Windows XP. Replaced the earlier
7.x font resources with Arial Regular 3.00, Arial Bold Italic 2.90 and Tahoma
Regular/Bold 3.15. Original bytes are preserved. Selected GDI font name tables
match the embedded Arial Bold Italic and Tahoma resources exactly under Wine64.
Latest test total: 67 (36 native, 22 states/priority, 9 font tests). Preview images
regenerated with the XP files. Acceptance/rasterization on real Win95 remains a
target-system check, not established by font-table equality under Wine.

## VGA layout, garbled cache alias and display casing

The user confirmed High/True Color and that the native Windows dialog says
Daniel. The old case-insensitive deduplication preserved an earlier uppercase
PWL alias. Accept the native Edit spelling as authoritative without creating
a separate Windows identity. Registry profile-directory spelling is an earlier
fallback. Windows account matching remains case-insensitive.

The old discovery added every PWL mapping as a user. Known registry profiles now
exclude unrelated cache aliases; PWL-only discovery checks mappings exist without
opening them. Control/undefined characters are rejected and valid non-ASCII names
are preserved. Native UI fonts use DEFAULT_CHARSET. Exact byte provenance of the
garbled entry in the user's screenshot is not independently established; verify
its removal on the target rather than claim its encoding was proven.

Fonts now follow the same 800x600 scaling as geometry. Source bitmaps are drawn
through DIBs with explicit COLORONCOLOR and double buffering; avoid legacy default
BLACKONWHITE and unsupported driver stretch modes. Native field container height
is limited to the Edit height. Experimental custom 8-bit palette code was tested
but discarded; it is not shipped and no pixel-perfect 256-color claim is made.

Validation: 82 checks at 640x480x16 and 800x600x24 under Wine64: 37 native, 23
canvas/states, 9 fonts, 10 profile-source/case checks and 3 actual GDI pixel checks.
VGA High/True Color screenshots inspected. Tests create isolated profile/INI
fixtures and never read real PWL data.

- [ ] Verify actual native Edit placement and removal of the extra strip on Win95.
- [ ] Verify the real user's Daniel spelling and unwanted-card removal after login.
- [ ] Verify Cyrillic source names on real ACP1251 Win95; do not infer success from
      an English Wine locale. Diagnostics now include resolution, depth and ACP only.


## Shutdown choice, fixed-pixel layout and one-account auto-login

Implemented the Explorer95-derived 314x200 shutdown choice with gradual GDI
snapshot dimming, cancellation/input restoration and optional Stand By/MS-DOS.
Shade and choice are removed before non-forced native power operations; shutdown
status no longer raises over other applications' save/cancel prompts. Preview
never dispatches a power action. Source provenance: docs/POWER_SOURCE.md.

Replaced resolution scaling with fixed artwork/control dimensions and shared
center/right/bottom anchors. Header/footer stay 78/96 px; fonts follow system DPI,
not screen size. Native and preview password fields both remain 167x22 px.
One discovered account gets one blank-password native OK at initial provider readiness;
multiple accounts and preview do not. Failed attempts leave manual input intact.
No PWL contents, stored passwords or synthetic success signals are involved.

Validation: 268 checks per resolution under Wine64/Xvfb at 640x480x16, 800x600x24
and 1024x768x24. Power/PIF callbacks are inert. Source compiles with C89/pedantic
warnings as errors; PE audit checks i386/Windows 4.0/no CRT and five baseline DLLs.
New preview screenshots show fixed-size layout at 1024 and the dimmed power choice.
These checks do not establish actual Windows 95 authentication or power behavior.

- [ ] Validate real Win95 single-profile empty-password success and password-required
      failure/manual recovery with each supported network provider.
- [ ] Validate APM suspend/resume, native save/cancel prompts and real shutdown/reboot.
- [ ] Validate the real-mode PIF path before logon and return from DOS on Win95.
- [ ] Check Russian ACP1251 labels and resolution changes with the real native Edit.
- [ ] Successful DOS dispatch retains its private PIF for asynchronous loading;
      consider conservative cleanup after confirmed later boot without deleting
      files that Windows may still need. Failures already clean their own files.


The user's additional 640x480 reference now has its own stacked composition:
logo/prompt above a two-row account viewport with a native vertical scrollbar.
A compact multi-account list starts collapsed; clicking even the initially
selected index opens its password card. Loading places the greeting above the
selected identity. Navigation and display changes use the same card anchors;
scroll selection queues provider work. Avatars use their native 48px bitmap
inside a 52px frame. The scrollbar uses the installed Windows theme.

Blank-login eligibility is consumed on the initial attached single-account
provider even if native OK is disabled. It stays manual in that case; enabling
OK after the user begins typing cannot trigger a delayed password-clearing attempt.
An already modified native Edit also remains manual (EM_GETMODIFY metadata only).
The current suite has 268 checks per resolution, including 37 layout/login checks.


## Recorded boot flicker, shell timing and logoff (current change)

Evidence: the supplied 27.35s recording shows a shell splash/desktop appearing
before welcome completes; the 9.3s logoff recording returns to the native network
password dialog without Welcome95. Existing code raised the canvas and then its
native input every 100ms, and exited both startup canvas and per-session observer.
This supports the fixes; a video alone does not prove every display-driver cause.

Implemented stable-priority checks and batched z-order updates. Added a registered
Win9x service controller, ordinary session-end notifier and per-cycle event reset.
Explicit logoff recreates login; reasonless session end releases fonts/canvas and
waits for the next provider or OS handshake. Shutdown cancellation preserves an
active login. Preview has its own instance lock and remains inert.

Enable installs a saved-shell proxy plus synchronous RunOnce gate. Both wait for
post-DestroyWindow acknowledgement and the existing >=1s loading interval. Pending
end-session notifications are serialized with new startup requests. The previous
shell command/presence is stored in the registry and a flushed, unique W95xxxx.BAK
beside the EXE before changing SYSTEM.INI. Readback failures roll back owned entries
and shell. Disable restores the shell, keeps recovery data and releases waiters.
Recursive shell configurations and foreign ownership are rejected. No process
suspension, password handling replacement or unrelated startup deletion is used.

Validation: 332 checks per resolution under Wine64/Xvfb: previous 268 plus 31 private startup
configuration/rollback/recovery cases and 33 real inter-process lifecycle checks.
The child shell is inert and only writes a time marker; logoff is synthetic.
Production remains CRT-free i386/Windows 4.0 with baseline system imports.
Implementation and DOS recovery: docs/BOOT_FLOW.md.

- [ ] Real Windows 95: confirm RegisterServiceProcess survival, actual MPR logoff
      notification behavior (including no reason), repeated logon and profile reload.
- [ ] Confirm SYSTEM.INI proxy + HKLM RunOnce order with both native Explorer and
      Explorer95; ordinary user startup must follow welcome closure.
- [ ] Recheck flicker with the actual display driver, password errors and topmost apps.
- [ ] Check shutdown cancellation, temporary-font cleanup and DOS recovery on the VM.

The old Run-only limitations above describe earlier versions; the current startup
model uses RunOnce plus a shell proxy. System services and unrelated RunOnce jobs
are not deferred by the welcome gate.


## Win95 login stall after shell replacement; non-ASCII desktop (current)

Evidence: a 40.75s recording shows responsive power UI but login never reaches the
welcome loading state. The user restored the previous SYSTEM.INI shell and login
worked again while delay disappeared. This localizes the regression to the shell
startup path, not a proven password-handler freeze. Restored shell makes enabled()
false and reactivates the old Run readiness signal, explaining loss of delay.
The main EXE was in Russian "Рабочий стол". The old gate assumed a short filename
was safe for the legacy loader; 8.3 is not an ASCII guarantee. Video alone does
not distinguish all OEM/ANSI, argument or loader-wait details on the actual VM.

Implemented an embedded i386 bootstrap deployed to a unique ASCII 8.3 directory
on the Windows drive. SYSTEM.INI points only to that executable, with no args.
It reads the owner through Win32 registry APIs and launches its full ANSI path
through CreateProcessA. All three startup entries also use the ASCII trampoline.
The bootstrap pumps messages during synchronous startup/shell waits. Original
shell backups, migration/rollback and post-close delay remain intact. Startup
errors release the overlay and show a visible error; bounded stage metadata is
written beside the bootstrap. No credentials or paths are logged.

The requested hidden Ctrl+Alt+Shift+L restores the existing native form and hides
the canvas without a fake logon or sending input. It is intentionally absent from
the UI. Same-provider unowned notices remain above canvas and input too.

Validation: 346 Wine checks per resolution, including 40 install/path cases and
38 real cross-process/bootstrap/rescue checks. Tests use private HKCU/INI paths,
ANSI non-ASCII bytes/spaces and an inert shell. Both production PE images compile
as i386/Windows 4.0 without CRT; the bootstrap imports only three baseline DLLs.

- [ ] Repeat the exact CP1251 Win95 desktop path and inspect W95START.LOG.
- [ ] Confirm real legacy-loader startup, delay, logoff and power cancellation.
- [ ] Verify physical Ctrl+Alt+Shift+L, native wrong-password and provider notices.


## Missing native input and black power button after logoff (current)

Evidence: the new 26.32s recording shows an attached-looking card without a caret;
Windows rejects a blank submit. The user confirms typing works in the restored
standard dialog. A separate screenshot shows a wholly black power-button area.
This establishes an integration/rendering failure, not its exact Win95 message
sequence. The previous bootstrap change remains in place.

The old layout cache checked the outer dialog only. Three new regressions fail
before the fix when the provider hides/moves its Edit or removes the clipping
region. Layout now validates and repairs the real child and clipping too, without
clearing input or enabling a native modal form. Border clicks request native
focus. Canvas paints queue an asynchronous repaint on the provider's Edit.
Metadata diagnostics now include rectangles, region type and hit class/ID only.

Owner-drawn buttons are explicitly repainted after their clipped parent buffer
is presented. Closing the shade invalidates the owner and children. The drawing
callback respects the item origin and saves/restores the caller's GDI state.
No repeated z-order writes or whole-screen timer redraws were added.

Validation: 363 checks per resolution under Wine64/Xvfb at 640x480x16,
800x600x24 and 1024x768x24: 43 native-control checks, 42 lifecycle/cross-process
checks, 7 rendering checks and 48 power checks among the existing suites.
Input in tests is synthetic; OS authentication, logoff and power remain inert.

- [ ] Verify typed stars/caret and mouse focus after real Win95 logoff/relogon.
- [ ] Repeat the native wrong-password prompt and ensure existing input survives.
- [ ] Recheck the power button after uncovering, focus changes and repeated Cancel
      on the user's actual Win95 GDI/display driver.
