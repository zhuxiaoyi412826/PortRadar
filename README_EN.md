# PortLens

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Windows-0078D4.svg)
![Size](https://img.shields.io/badge/size-~1MB-brightgreen.svg)
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C.svg)

**English** | [简体中文](README.md)

A lightweight Windows port occupancy checker. Enter a port number to instantly see whether it is in use, which program owns it, and where the executable lives. Written in native C++, statically linked into a single ~1 MB executable with zero dependencies and no installation required — a handy companion for development debugging and ops troubleshooting.

## Demo

**Interactive menu** (double-click, or run without arguments):

```text
    +-------------------------------------------------------+
    |                                                       |
    |    PortLens v0.1.0 - Port Occupancy Checker           |
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

## Installing the `port` command

**Just double-click the exe once**: on first launch it automatically adds its own folder to the user PATH (no administrator rights required). Open a new CMD window afterwards, and `port` works from any directory.

Manual management is also available:

```bat
port --install-path      :: add to user PATH
port --uninstall-path    :: remove from user PATH
```

Notes: already-open CMD windows won't see the change — open a new one; only the current user's PATH is modified, leaving the system and other accounts untouched.

## Features

| # | Feature | Description |
|---|---------|-------------|
| 1 | Single port check | Checks TCP+UDP, shows process name, PID, full executable path, and well-known port descriptions (e.g. 3306 = MySQL) |
| 2 | List all ports | Lists every listening port, filterable by TCP / UDP / all |
| 3 | Port range scan | Scans a range (e.g. 1-1000) and reports all occupied ports |
| 4 | Kill process | One-click termination of the process holding a port (with confirmation) |
| 5 | Process details | Name, PID, parent PID, thread count, start time, memory usage, file version info, reveal-in-Explorer |
| 6 | Connection details | All connections on a port (LISTENING / ESTABLISHED / TIME_WAIT ...) with remote addresses |
| 7 | Find ports by process | Enter a process name (partial match) to list all ports it holds |
| 8 | Real-time monitoring | Auto-refresh at a chosen interval; highlights and beeps on occupied/released/process-changed events |
| 9 | Auto-find available port | Tries ports sequentially from a preferred one (e.g. 5400→5401→5402) until a free port is found |

## Download

- Grab `port.exe` from the [Releases](../../releases) page — runs on Windows 7+ with no runtime dependencies; double-clicking once also installs the `port` command (see "Installing the `port` command" above)
- Or build from source (see below)

## Usage

### Command-line mode

| Option | Description |
|--------|-------------|
| `-c <port>` | Check a single port (TCP+UDP) |
| `-l [proto]` | List all listening ports (`tcp` / `udp` / `all`, default `all`) |
| `-f <name>` | Find ports by process name (partial match, case-insensitive) |
| `-s <start> <end>` | Scan a port range |
| `-a <port>` | Auto-find an available port starting from the given one |
| `--install-path` | Add the program folder to the user PATH (the `port` command) |
| `--uninstall-path` | Remove the program folder from the user PATH |
| `-v, --version` | Show version |
| `-h, --help` | Show help |

Exit codes (script friendly): `0` = port free / success, `1` = port occupied / no result, `2` = invalid arguments or runtime error.

```bat
:: Who is using port 3306?
port -c 3306

:: Find a free port starting from 8000
port -a 8000

:: Which ports does node hold?
port -f node

:: Scan 1-1000 and save the result
port -s 1 1000 > scan_result.txt
```

### Interactive mode

| Key | Action |
|-----|--------|
| `1` ~ `9` | Select a menu feature |
| `0` | Exit |
| `Enter` / `Space` | Return to the menu after a feature completes |
| `y` / `n` | Confirm dangerous operations (e.g. kill process) |
| `ESC` / `Q` | Stop real-time monitoring |

Pressing Enter at numeric prompts accepts the default shown in brackets; Backspace edits input.

## Requirements

- **Run**: Windows 7 or later, no runtime libraries needed
- **Build**: MinGW-w64 GCC with C++17 support, e.g. [WinLibs](https://winlibs.com/) or MSYS2

## Building

### One-click

Double-click `build.bat`. The script locates g++ on PATH, or falls back to the `GCC_PATH` variable defined at the top of the script (edit it to point at your MinGW-w64 installation), then compiles and launches the program.

### Manual

```bash
windres version.rc -O coff -o version.res
g++ -Os -s -std=c++17 -static -static-libgcc -static-libstdc++ \
    main.cpp port_checker.cpp process_manager.cpp version.res \
    -o port.exe \
    -lws2_32 -liphlpapi -lpsapi -lversion -lshell32 -mconsole
```

The `-static` flags produce a single self-contained exe that runs on any Windows machine.

## Testing

```powershell
.\tests\run_tests.ps1
```

The smoke suite covers version output, every CLI option, PATH install/uninstall, and the exit-code contract (16 assertions). CI runs the same build and tests on every push / PR; pushing a `v*` tag automatically builds and attaches the exe to a GitHub Release.

## Project Layout

```
├── main.cpp               # Entry: menu, UI, input handling, CLI mode, 9 features
├── port_checker.h/.cpp    # Port detection: TCP/UDP tables, scanning, connections
├── process_manager.h/.cpp # Process management: info, paths, termination
├── console_util.h         # Console utilities: colors, clear screen, formatting
├── version.rc             # Executable version resource
├── build.bat              # One-click build script
├── tests/run_tests.ps1    # Smoke tests
└── .github/workflows/     # CI build & release
```

## Technical Notes

- Built entirely on native Windows APIs: `GetExtendedTcpTable` / `GetExtendedUdpTable` (port tables), `psapi` (process info), `ShellExecuteW` (reveal file location)
- All keyboard input goes through `_getch()` — no line-buffering artifacts, always responsive
- UTF-8 output (code page 65001) + Win32 console color API; no third-party dependencies

## Contributing

Issues and pull requests are welcome — see [CONTRIBUTING.md](CONTRIBUTING.md) (in Chinese).

## Changelog

See [CHANGELOG.md](CHANGELOG.md) (in Chinese).

## License

[MIT](LICENSE)
