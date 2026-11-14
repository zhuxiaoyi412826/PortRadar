# PortLens

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Windows-0078D4.svg)
![Size](https://img.shields.io/badge/size-~1MB-brightgreen.svg)
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C.svg)

**English** | [简体中文](README.md)

A lightweight Windows port occupancy checker. Enter a port number to instantly see whether it is in use, which program owns it, and where the executable lives. Written in native C++, statically linked into a single ~1 MB executable with zero dependencies and no installation required — a handy companion for development debugging and ops troubleshooting.

## Demo

**`port:<port>` instant check** (type it into the Win+R Run dialog or the browser address bar — a check window pops up):

```text
port:8001

  [TCP] Port 8001 is OCCUPIED
    Process: python.exe
    PID:     28712
    Path:    C:\...\Python313\python.exe

  Press [K] to terminate the occupying process (free the port), Enter/Space to quit
```

**`port <process name>` batch management** (type `port python` in CMD — lists every matching process with its ports, kill by number or all at once):

```text
> port python

  Processes matching "python" (2 processes, 2 listening ports):

  [1] python.exe  (PID: 21600)
      Path:  C:\...\Python313\python.exe
      Ports: TCP:54333

  [2] python.exe  (PID: 1084)
      Path:  C:\...\Python313\python.exe
      Ports: TCP:54334

  Press [K] to terminate all, [number] to terminate one, Enter/Space to quit
```

**Interactive menu** (double-click, or run without arguments):

```text
    +-------------------------------------------------------+
    |                                                       |
    |    PortLens v0.8.1 - Port Occupancy Checker           |
    |                                                       |
    +-------------------------------------------------------+

 -------------------------------------------------------------
  Main Menu
 -------------------------------------------------------------
  [1] Check single port
  [2] List all listening ports
  [3] Scan port range
  [4] Kill process (free the port)
  [5] Process details
  [6] Connection details
  [7] Find ports by process name
  [8] Real-time monitoring
  [9] Auto-find available port
  [0] Exit
 -------------------------------------------------------------
```

**Command-line mode** (run with arguments, exits after execution — script friendly):

```text
> port -c 5000
Port 5000:
  [TCP] OCCUPIED  node.exe (PID: 21624)
        Path:     D:\software\node\node-v24.16.0-win-x64\node.exe
  [UDP] FREE

> port -a 5000
Available port: 5001 (2nd port starting from 5000)
```

> Note: the program UI is in Chinese; the examples above are translated for illustration.

## Installing the `port` command and the `port:` protocol

**Just double-click the exe once**: on first launch it automatically adds its own folder to the user PATH (no administrator rights required) and registers the `port:` URI protocol. Afterwards:

- Open a new CMD window and `port` works from any directory
- **Type `port:8001` directly into the Win+R Run dialog, the browser address bar, or Start-menu search** to pop up an instant check window — process name, PID and file path, with a one-key `K` release, no CMD needed

Manual management is also available:

```bat
port --install-path      :: add to user PATH + register port: protocol
port --uninstall-path    :: remove from user PATH + unregister protocol
```

Notes: already-open CMD windows won't see the change — open a new one; only the current user's PATH is modified, leaving the system and other accounts untouched; if you move the exe, just double-click it once to refresh the protocol registration.

## Features

| # | Feature | Description |
|---|---------|-------------|
| 1 | Single port check | Checks TCP+UDP, shows process name, PID, full executable path, and well-known port descriptions (e.g. 3306 = MySQL); press K to terminate the occupying process and free the port; the main menu accepts a port number typed directly for a quick check |
| 2 | List all ports | Lists every listening port, filterable by TCP / UDP / all |
| 3 | Port range scan | Scans a range (e.g. 1-1000) and reports all occupied ports |
| 4 | Kill process | One-click termination of the process holding a port (with confirmation) |
| 5 | Process details | Name, PID, parent PID, thread count, start time, memory usage, file version info, reveal-in-Explorer |
| 6 | Connection details | All connections on a port (LISTENING / ESTABLISHED / TIME_WAIT ...) with remote addresses |
| 7 | Find ports by process | Enter a process name (partial match) to list all ports it holds |
| 8 | Real-time monitoring | Auto-refresh at a chosen interval; highlights and beeps on occupied/released/process-changed events |
| 9 | Auto-find available port | Tries ports sequentially from a preferred one (e.g. 5400→5401→5402) until a free port is found |
| 10 | Port watch | `port -w 8001`: alerts (time, process, PID, path, beep) whenever the port flips between occupied and free; `--popup` shows a desktop notification |
| 11 | Batch release | `port -k 8001,8002,8003` frees several ports in one command, handy for CI cleanup scripts |
| 12 | Export | `port -l --export csv` / `json` snapshots the port table; CSV ships with a UTF-8 BOM so Excel opens it correctly |
| 13 | Config file & aliases | `port.conf` customizes the default scan range, watch interval, history size, port aliases (`port mysql` ≡ `port 3306`) and port descriptions |
| 14 | Operation history | Records the last N terminations (time / PID / process / ports / path); review with `--history` to trace accidental kills |
| 15 | Favorites | `port fav add 8001`, `port fav` shows the status of your everyday ports in one view |
| 16 | Tray resident | `port --tray` minimizes to the system tray, shows balloon alerts when a port becomes occupied; right-click menu to show console / refresh / exit |
| 17 | Docker / WSL awareness | Marks ports held by Docker containers, WSL or Hyper-V (e.g. `[Docker: redis]`) |
| 18 | Process list filter & sort | `port java 8080` filters by process name + port; `--sort pid\|port\|name` |
| 19 | Desktop GUI | Double-click the exe for a graphical UI: port map, port list, detection, monitor timeline, favorites, history and settings — everything the CLI does |
| 20 | Port map | Process-centric map of "port → process → container" with live links, so you can see what holds which ports at a glance |

## Download

Grab the file that fits your environment from the [Releases](../../releases) page:

| File | Description |
|------|-------------|
| **port.exe** | Dual-mode main program (recommended): double-click for the GUI desktop, run `port` in CMD for the CLI; since v0.8.1 the WebView2 loader is statically linked, so it is a true single file with no DLL beside it (the GUI needs the WebView2 runtime bundled with Windows 10/11) |
| **port-cli.exe** | Pure CLI edition: statically linked single file with zero dependencies, for servers and Windows 7 machines without the WebView2 runtime |

Double-clicking once also installs the `port` command and the `port:` protocol (see "Installing the `port` command" above); or build from source (see below).

## Usage

### Dual mode: one exe, two ways to run

The same `port.exe` picks a mode based on how it was started; both modes share the same C++ core:

| Launch method | Mode |
|---------------|------|
| Double-click the exe in Explorer | GUI |
| Run `port` in CMD / PowerShell / Windows Terminal | CLI (interactive menu) |
| `port 8001`, `port java`, `port -l`, ... | CLI (runs and exits) |
| `port --gui` | Force GUI |
| `port --cli` | Force CLI |

The GUI has seven pages: **Port Map** (port → process → container links plus counters), **Port List** (sort / search / multi-select release / export CSV, JSON), **Detect** (TCP+UDP state, process detail, connections, alias shortcuts, free-port finder), **Monitor Timeline** (occupy/release events with flash and beep), **Favorites**, **History** and **Settings**. Closing the window minimizes to the system tray; the tray menu restores or exits.

> The GUI needs the system WebView2 runtime (bundled with Windows 10/11). Since v0.8.1 the WebView2 loader is statically linked into the exe — a single file, no DLL required.

### Command-line mode

| Option | Description |
|--------|-------------|
| `port <port>` | Instant check in CMD (e.g. `port 8001`) — shows process name, PID and path, with the K-key release |
| `port <name>` | Instant batch management in CMD (e.g. `port java`): lists every process whose name matches, with its listening ports; `K` terminates all, a number terminates that one, the list refreshes after each kill |
| `port <name> <port>` | Process name + port combined filter (e.g. `port java 8080`) |
| `port <alias>` | Alias check (e.g. `port mysql` ≡ `port 3306`; aliases live in `port.conf`) |
| `port :<port>` | Same as `port <port>`, colon form (e.g. `port :8001`) |
| `port:<port>` | URI form: type `port:8001` into Win+R / the browser address bar / Start-menu search to pop a check window. CMD cannot execute this form (Windows rejects the syntax) — use `port 8001` there |
| `-c <port>` | Check a single port (TCP+UDP) |
| `-l [proto]` | List all listening ports (`tcp` / `udp` / `all`, default `all`) |
| `-l --export csv\|json[:file]` | Export the port list as CSV / JSON (auto-named when no file is given) |
| `-s [start end]` | Scan a port range (uses the defaults from `port.conf` when omitted) |
| `-a <port>` | Auto-find an available port starting from the given one |
| `-w <port> [--interval ms] [--popup]` | Watch a port; alerts on occupied / free transitions, Ctrl+C to stop |
| `-k <port,...>` | Batch release, e.g. `-k 8001,8002,8003` |
| `--history` | Show the most recent terminations (time / PID / process / ports / path) |
| `--clear-history` | Clear the history file |
| `fav [list\|add <port>\|remove <port>]` | Port favorites, e.g. `fav add 8001` |
| `--tray [port...]` | Resident system-tray monitoring (falls back to favorites when no port is given) |
| `--config` | Show config values and paths; generates a `port.conf` template when missing |
| `-f <name>` | Find ports by process name (partial match, case-insensitive) |
| `--install-path` | Add the program folder to the user PATH (the `port` command) |
| `--uninstall-path` | Remove the program folder from the user PATH |
| `-v, --version` | Show version |
| `-h, --help` | Show help |

Exit codes (script friendly): `0` = port free / success, `1` = port occupied / no result, `2` = invalid arguments or runtime error.

```bat
:: Quick check in CMD (shows process details when occupied, K to release)
port 8001

:: List all java-related processes with their ports, kill by number or K for all
port java

:: Who is using port 3306?
port -c 3306

:: Find a free port starting from 8000
port -a 8000

:: Which ports does node hold?
port -f node

:: Scan 1-1000 and save the result
port -s 1 1000 > scan_result.txt

:: Only java on 8080, sorted by PID
port java 8080 --sort pid

:: Wait for 8001 to be released (Ctrl+C to stop)
port -w 8001

:: Release several ports at once (CI cleanup)
port -k 8001,8002,8003 > nul

:: Export a port snapshot (CSV carries a BOM, Excel-ready)
port -l --export csv:ports.csv
port -l tcp --export json

:: Pin the ports you use daily and check them in one view
port fav add 3306
port fav

:: Stay in the tray with balloon alerts (favorites are watched by default)
port --tray
```

### Interactive mode

| Key | Action |
|-----|--------|
| `1` ~ `9` | Select a menu feature |
| `0` | Exit |
| Type a port number | At the main menu, typing 2+ digits (e.g. `8001`) checks that port directly — no need to enter menu [1] first |
| `K` | After a port check result, one key terminates the occupying process and frees the port |
| `Enter` / `Space` | Return to the menu after a feature completes |
| `y` / `n` | Confirm dangerous operations (e.g. kill process) |
| `ESC` / `Q` | Stop real-time monitoring |
| `F` | Port favorites (status of saved ports) |
| `H` | Operation history (recent terminations) |
| `W` | Watch a single port |
| `T` | Resident tray monitoring |

Pressing Enter at numeric prompts accepts the default shown in brackets; Backspace edits input.

## Configuration file port.conf

Run `port --config` to inspect the active values and paths; a template is created automatically when the file is missing. Lookup order: `port.conf` next to the exe → `%LOCALAPPDATA%\PortLens\port.conf`.

```ini
[settings]
; Default scan range used by "port -s" without arguments
scan_start=1
scan_end=1024
; Maximum number of history entries
history_limit=50
; Watch / tray polling interval (ms)
watch_interval=1000

[alias]
; "port mysql" is equivalent to "port 3306"
mysql=3306
redis=6379
mongo=27017
postgres=5432
nginx=80

[desc]
; Custom port descriptions (override built-in ones)
8001=local test service
```

Without a config file the built-in aliases for mysql / redis / mongo / postgres / nginx are available out of the box.

Favorites and history live in `%LOCALAPPDATA%\PortLens\` (`favorites.txt`, `history.tsv`).

## Requirements

- **Run**: CLI mode / `port-cli.exe` works on Windows 7 or later with no runtime libraries; GUI mode (double-clicking `port.exe`) needs Windows 10/11 (system WebView2 runtime; since v0.8.1 the loader is statically linked, no DLL required)
- **Build**: MinGW-w64 GCC with C++17 support, e.g. [WinLibs](https://winlibs.com/) or MSYS2; the GUI additionally needs a Rust toolchain (`x86_64-pc-windows-gnu`)

## Building

### One-click

Double-click `build.bat`. The script locates g++ on PATH, or falls back to the `GCC_PATH` variable defined at the top of the script (edit it to point at your MinGW-w64 installation), then compiles and launches the program.

### Manual

```bash
windres version.rc -O coff -o version.res
g++ -Os -s -std=c++17 -static -static-libgcc -static-libstdc++ \
    main.cpp port_checker.cpp process_manager.cpp config_manager.cpp \
    special_process.cpp watch_tray.cpp version.res \
    -o port.exe \
    -lws2_32 -liphlpapi -lpsapi -lversion -lshell32 -luser32 -lgdi32 -mconsole
```

The `-static` flags produce a single self-contained exe that runs on any Windows machine.

### Graphical UI

Requires a Rust toolchain (`x86_64-pc-windows-gnu`). The frontend is plain HTML/CSS/JS embedded into the exe, so no Node build step is needed:

```powershell
.\build_gui.ps1          # produces single-file dist\port.exe
.\build_gui.ps1 -Clean   # clean and rebuild
```

The script maps a space-free drive letter with `subst` (MinGW `windres` cannot handle paths containing spaces) and collects the single-file Rust binary into `dist\` (the WebView2 loader is statically linked, no DLL needed).

## Testing

```powershell
.\tests\run_tests.ps1
```

The smoke suite covers version output, every CLI option, the `port:` form, process-name instant lookup, PATH / protocol install/uninstall, the exit-code contract, dual-mode routing (`--gui` / `--cli`), plus the v0.7 config, favorites, export, batch release, history and tray commands (41 assertions). CI runs the same build and tests on every push / PR; pushing a `v*` tag automatically builds and attaches the exe to a GitHub Release.

## Project Layout

```
├── main.cpp               # Entry: menu, UI, input handling, CLI mode
├── port_checker.h/.cpp    # Port detection: TCP/UDP tables, scanning, connections
├── process_manager.h/.cpp # Process management: info, paths, termination
├── config_manager.h/.cpp  # Config (port.conf), operation history, favorites
├── special_process.h/.cpp # Docker / WSL / Hyper-V attribution
├── watch_tray.h/.cpp      # Port watch and resident system tray
├── gui_api.h/.cpp         # GUI bridge: exposes the core as JSON for Rust
├── gui/                   # Desktop app (Tauri 2)
│   ├── src/               # Frontend: index.html / styles.css / app.js
│   └── src-tauri/         # Rust entry (mode routing, commands, tray)
│       ├── msvc_compat.cpp    # MSVC runtime compat symbols (for the static WebView2 loader)
│       └── webview2-static/   # WebView2Loader static archive (key to single-file shipping)
├── build_gui.ps1          # One-click GUI build (writes to dist/)
├── console_util.h         # Console utilities: colors, clear screen, formatting
├── version.rc             # Executable version resource
├── build.bat              # One-click build script (CLI edition)
├── tests/run_tests.ps1    # Smoke tests
└── .github/workflows/     # CI build & release
```

## Technical Notes

- Built entirely on native Windows APIs: `GetExtendedTcpTable` / `GetExtendedUdpTable` (port tables), `psapi` (process info), `ShellExecuteW` (reveal file location)
- The desktop app uses Tauri 2 (system WebView2, no Qt or Electron); the Rust entry reuses the C++ core through JSON APIs in `gui_api.cpp`, so CLI and GUI share one implementation
- Single-file shipping: the WebView2 loader is statically linked from the NuGet static library (converted to a GNU archive) and wins the link order over webview2-com-sys' DLL import library; the MSVC runtime symbols the static library needs are provided by `msvc_compat.cpp` (renamed to MSVC mangled names with objcopy, then force-linked via `--whole-archive`)
- Dual-mode routing: with no arguments it inspects the parent process (`explorer.exe` means a double-click) and the console process count, then calls `FreeConsole()` to hide the console in GUI mode while keeping the console subsystem so CMD output and exit codes stay correct
- All keyboard input goes through `_getch()` — no line-buffering artifacts, always responsive
- UTF-8 output (code page 65001) + Win32 console color API; no third-party dependencies

## Contributing

Issues and pull requests are welcome — see [CONTRIBUTING.md](CONTRIBUTING.md) (in Chinese).

## Changelog

See [CHANGELOG.md](CHANGELOG.md) (in Chinese).

## License

[MIT](LICENSE)
