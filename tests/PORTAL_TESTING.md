# GlobalShortcuts Backend Verification

Use a GlobalShortcuts v2 broker (upstream 1.22 or a complete backport) and matching
`dde-services` plugin/provider. The service must export
`PortalSessionShortcutsChanged(ssa(ssssasasbb))` on `org.deepin.dde.Keybinding1`.
The matching service release is proposed as 1.0.51; the package dependency is
`dde-services (>= 1.0.51~)`. Build with Qt 6.8+. The cooperating service now uses
Treeland shortcut-manager v3 interface version 1 for Wayland registration/capture.
Do not mix pre-fix and post-fix packages even if their package versions appear
compatible. The public and backend v2 signatures have not changed.

## Automated Tests

From the repository root, with a configured Qt6 build and local D-Bus sockets
permitted:

```bash
cmake --build build --target xdg-desktop-portal-dde \
  test-global-shortcuts-contract test-global-shortcuts-controller -j4
ctest --test-dir build --output-on-failure -R '^global-shortcuts-'
ctest --test-dir build --output-on-failure \
  -R '^global-shortcuts-controller$' --repeat until-fail:3
```

The controller uses a real isolated D-Bus connection with a fake service. Tests
cover protocol signatures, caller ownership, Close races, session isolation,
Configure notifications, paired event recipients, bounded recovery, and rapid
service restarts. These are not physical-keyboard or compositor acceptance tests.
Additional regressions check that invalid parent windows retain no oversized
declarations, cancelled List replies cannot complete a reused request path, and
valid supplementary Unicode is accepted without accepting malformed surrogates.

## Expected Behavior

- An application snapshot can refresh or shrink existing bindings, but cannot
  promote an unbound Session. Only confirmed per-Session results can expand it.
- Configure's Session signal validates app ID, token, and Bound state. Unchanged
  snapshots do not emit duplicate public notifications.
- Release goes only to live Sessions that received Press, even after a binding
  was removed. Joining while held must not produce an unmatched Release.
- Restore is serial. Busy receives at most five additional attempts with
  100/200/400/800/1600 ms delays. Exhaustion logs the error and continues the queue;
  closed Sessions are skipped and late success is deactivated.
- Old service generations cannot overwrite restored state. Temporary empty
  snapshots must not erase the remembered recovery subset. Backend/broker
  restarts invalidate old Sessions and require the application to Create/Bind.

## Manual Acceptance

Use an identified desktop application or real Flatpak through the public broker,
not a short-lived direct backend call. Keep one D-Bus connection alive throughout
Create/Bind/List/Configure/Close. Capture each Session's response and event log.

For an unsandboxed test client, install its own `.desktop` entry and call
`org.freedesktop.host.portal.Registry.Register` on that connection before any
portal method. Running inside an `app-*.service` unit is not sufficient on every
broker build. Real Flatpak clients already have a sandbox identity; do not grant
them direct access to Keybinding1/provider or register a host ID.

Reproduce an empty-bound Session alongside a successfully bound same-app Session;
only the latter may trigger until Configure explicitly grants references. Check
disable/re-enable while held, then service-only restart with two different apps.
Repeat the restart ten times on Treeland and include closing during recovery.
Only restart after successful Bind. Use `busctl --user status
org.deepin.dde.Keybinding1` to identify its actual user unit and wait for a new
D-Bus owner plus restored lists before input. On the X11 QA machine that unit is
`deepin-service-group@dde.service`.

Also verify provider window parenting/capture, conflicts, real persistence,
Flatpak isolation, and existing desktop shortcuts on both X11 and Wayland. Record
the exact installed builds. A sudden service crash clears the public snapshot
and pressed-recipient state, without guaranteeing a cross-clock synthetic release.

The cooperating workspace's `portal-manual-test-plan.md` and `portal/gs_manual.py`
provide the detailed Chinese procedure and client; they are separate workspace
artifacts. On 2026-09-08 all three backend CTest suites and three additional
controller repetitions passed. On 2026-09-11 real X11/Flatpak tests passed for
session isolation (including a same-action empty successful subset), held
disable/rebind, close and multi-session service recovery. These used XTest and
multiple connections under one Flatpak app ID, not physical input or two apps.
Those historical results describe the September implementation, before the
current review fixes and latest-master integration.

On 2026-09-14 the host Registry + Create/empty Bind/List workflow passed. The
historical service provider also gained a locally checked Treeland v1 capture guard that is
not yet in the installed QA packages. Full Wayland, physical input, cross-app,
persistence-failure and release-matrix acceptance remains pending. Workspace
evidence is indexed by `portal/results/README.md`.
