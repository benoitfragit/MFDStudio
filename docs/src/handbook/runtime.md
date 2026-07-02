# Runtime

`mfd_window` is the runtime host. It loads one window JSON, owns rendering and
runtime state, and accepts public commands over UDP from a client.

## Launch

The simplest path is a shipped launcher:

- `.\Scripts\Start-MfdDemo.bat`
- `.\Scripts\Start-MfdCockpit.bat`
- `.\Scripts\Start-MfdMinimal.bat`

For an explicit window, use the generic launcher:

```powershell
.\Scripts\Start-MfdWindow.bat assets/windows/demo_pages.json
```

It can also pass a framebuffer plugin:

```powershell
.\Scripts\Start-MfdWindow.bat assets/windows/demo_pages_minimal.json --framebuffer-plugin mfd_framebuffer_stdout_plugin.dll
```

For heavy live-debug sessions, `mfd_window` also accepts `--no-snapshot`:

```powershell
.\Scripts\Start-MfdWindow.bat assets/windows/demo_pages_minimal.json --no-snapshot
```

This keeps earlier commands of the same runtime batch applied when a later
command fails. The default remains transactional: without `--no-snapshot`, a
multi-command batch still rolls back as one unit.

The transactional rollback snapshot only covers the pages a batch actually
touches (plus scene-wide state such as the active page and the window display),
so the default mode stays cheap even on large scenes with high-rate batches.
`--no-snapshot` is therefore mostly a debugging escape hatch, not a performance
requirement.

`Start-MfdMinimal.bat` starts `mfd_window` with the sample framebuffer plugin
enabled.

## Debug overlay

Press `F1` inside `mfd_window` to open the integrated runtime debug overlay. It
lets you inspect:

- the active page
- the reticle tree
- transport state
- temporary local bypasses, including structured time-primitive overrides

This is the fastest way to distinguish an authored-asset issue from a
client-side command issue or a runtime-side state issue.

## Checking the transport quickly

In `client_mockup`, use the `Window display` controls: toggle `Invert colors`,
change `Brightness`, then click `Send window display`. If the runtime reacts,
the UDP control path is alive.

## Client relationship

`client_mockup` is a normal standalone UDP client. It loads the same window JSON
locally for discovery, then sends public commands to the runtime — it shares no
memory with `mfd_window`, which is exactly why it is a good reference client.
