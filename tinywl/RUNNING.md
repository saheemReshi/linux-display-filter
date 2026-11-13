# Running and testing TinyWL

This project is based on **wlroots 0.20.2** and modifies its TinyWL example compositor for system-wide visual-content filtering experiments.

## Build

Run these commands from the **wlroots repository root**, unless noted otherwise:

```bash
make -C tinywl
cd tinywl
```

`make` rebuilds TinyWL against the installed `wlroots-0.20` library. **If wlroots library code is modified later, this standalone Makefile will not automatically build or link against those local library changes.** Use an appropriately configured local wlroots build in that case.

## Launch TinyWL

### Nested inside Hyprland (recommended for development)

```bash
WLR_BACKENDS=wayland ./tinywl -s foot
```

This runs TinyWL using the Wayland backend: TinyWL is a Wayland client of Hyprland, and Foot is a Wayland client of TinyWL. Run it from a terminal in your active Wayland desktop session.

### Nested, with logs saved to a file

```bash
WLR_BACKENDS=wayland ./tinywl -s foot 2>&1 | tee tinywl.log
```

- `2>&1` redirects TinyWL's standard error (where wlroots diagnostics are normally printed) into standard output.
- `tee tinywl.log` displays the combined output and saves it in `tinywl.log` in the current directory.
- Use a more specific log filename for each experiment, as shown below. Logs are diagnostic outputs, not source files; avoid committing them.

### Run from a virtual terminal (TTY)

Switch to a TTY, log in, navigate to the `tinywl` directory, and run:

```bash
./tinywl -s foot
```

Without `WLR_BACKENDS=wayland`, wlroots selects a suitable backend for the environment; on a usable TTY this will commonly be the DRM/KMS backend. It requires appropriate device/session permissions. Use **Alt+Escape** to exit TinyWL and switch back to your desktop TTY afterward.

## Start applications

TinyWL's `-s` option runs the supplied command through `/bin/sh -c` after creating its Wayland socket.

| Command | What it does |
|---|---|
| `./tinywl -s foot` | Start TinyWL and launch Foot. |
| `./tinywl -s kitty` | Start TinyWL and launch Kitty (if installed). |
| `./tinywl -s firefox` | Start TinyWL and launch Firefox (if installed). |
| `./tinywl -s brave` | Start TinyWL and launch Brave (if installed). |
| `./tinywl -s 'foot & firefox & wait'` | Start Foot and Firefox in the background; the startup shell waits for both child commands to finish. |
| `./tinywl -s 'firefox & foot'` | Launch Firefox in the background and Foot in the foreground; the startup shell exits when the foreground command exits. |

Prefix these commands with `WLR_BACKENDS=wayland` when running nested inside Hyprland. Some browsers reuse an already running browser process, so check that a new window actually appears inside TinyWL.

`./tinywl -s 'wait'` does **not** launch an application; with no background jobs in that shell, `wait` returns immediately. It is not useful as a normal application-launch command.

## Experimental shortcuts

These shortcuts were added to, or are provided by, the modified TinyWL source:

| Shortcut | Behavior |
|---|---|
| **Alt+F2** | Show/hide the black filtering rectangle. |
| **Alt+Arrow** | Move the filtering rectangle by 20 layout units. |
| **Alt+B** | Enumerate application scene buffers and print information to the compositor log (when the buffer-inspection code is present). |
| **Alt+F1** | Cycle application-window focus. |
| **Alt+Escape** | Terminate TinyWL. |

## Experiment log commands

Run these from the `tinywl` directory after building. Each command starts the **same TinyWL executable**; the diagnostic output depends on which experiment's C code is currently compiled into it. **Changing the log filename does not activate a different feature.**

### Phase 2A — Enumerate application scene buffers

```bash
WLR_BACKENDS=wayland ./tinywl -s foot 2>&1 | tee buffer-inspection.log
```

Press **Alt+B**, then inspect the results:

```bash
grep -E 'Buffer:|Found [0-9]+ application scene buffers' buffer-inspection.log
```

Observed in development: 9 scene-buffer nodes with Foot; 11 after Firefox was opened. These are scene buffers, not 9 or 11 whole-screen framebuffers.

### Phase 2B — Test direct CPU access to application buffers

```bash
WLR_BACKENDS=wayland ./tinywl -s foot 2>&1 | tee readback.log
```

Press **Alt+B**, then inspect:

```bash
grep -E 'CPU access OK|CPU access unavailable|First four pixel bytes' readback.log
```

The tested application buffers reported **CPU access unavailable** through `wlr_buffer_begin_data_ptr_access()`. This does **not** establish that the content cannot be read through a GPU texture or renderer readback path.

### Output buffer interception — Before presentation

With the `output_frame()` modification that separates `wlr_scene_output_build_state()` from `wlr_output_commit_state()`:

```bash
WLR_BACKENDS=wayland ./tinywl -s foot 2>&1 | tee output-buffer.log
```

```bash
grep -E 'Output buffer:|Output render/commit failed' output-buffer.log
```

Observed once in development: `Output buffer: 1280x720`. The code logs only once when its `static bool logged` flag is used. This proves access to the **output-state buffer object**, not yet access to the pixel bytes or an unfiltered screenshot.

## Working with Git

From the repository root:

```bash
git status
git diff --check
git add tinywl/tinywl.c
git commit -m "Describe completed experiment"
git push
```

Stage only the files relevant to the milestone. Avoid accidentally committing backup source files, build outputs, or diagnostic logs. `display-filter` is the project's integration branch; keep significant experiments on feature branches when useful.

## Next milestone

Read pixels from the compositor's rendered output buffer, save a screenshot generated inside TinyWL, and then design an **unfiltered capture path that cannot accidentally present unfiltered content**. For now, the black rectangle is a scene overlay; ordinary scene rendering may include it in captures.
