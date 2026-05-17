# GDB Stub

Vita3K includes a GDB remote serial protocol (RSP) server that lets you debug Vita applications with a standard ARM GDB client.

## Enabling

Set `gdbstub: true` in your `config.yml`. The server listens on **port 2159** and waits for a GDB client to connect before the app starts executing.

Optional: set `log-gdb-packets: true` to see every RSP packet in the Vita3K log (useful for debugging the stub itself).

## Connecting

### arm-none-eabi-gdb (recommended)

Install the [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads). Then:

```
arm-none-eabi-gdb path/to/eboot.elf -ex "target remote localhost:2159"
```

This GDB version has XML support, so Thumb mode is auto-detected and VFP registers are available.

### arm-vita-eabi-gdb (VitaSDK)

```
arm-vita-eabi-gdb path/to/eboot.elf -ex "target remote localhost:2159"
```

VitaSDK currently ships **GDB 15.2**, configured `--without-expat`, so it has **no XML target-description support by design** (it prints `Can not parse XML library list; XML support was disabled at compile time` and uses the legacy register layout). Thumb auto-detection still works because the stub now reports the real CPSR and GDB reads the T-bit from it. `si` after Ctrl-C may still not work reliably. If breakpoints fail, try `set arm fallback-mode thumb` before setting them.

**Note (Windows — missing runtime DLLs):** VitaSDK's `arm-vita-eabi-gdb.exe` depends on `libgcc_s_seh-1.dll` and `libstdc++-6.dll` but the package ships neither (only a matching `libwinpthread-1.dll`). On a clean machine GDB exits silently (`STATUS_DLL_NOT_FOUND`, `0xC0000135`). **Do not** just copy those two DLLs from `C:\Program Files\Git\mingw64\bin\` next to the exe — mixing them with the bundled older `libwinpthread-1.dll` instead yields `STATUS_ENTRYPOINT_NOT_FOUND` (`0xC0000139`). The three DLLs must be a **matched set from one mingw-w64**. Working fix: copy `arm-vita-eabi-gdb.exe` into a scratch directory together with `libgcc_s_seh-1.dll`, `libstdc++-6.dll` **and** `libwinpthread-1.dll` all from the *same* mingw (e.g. Git's `mingw64\bin`), and run it from there — this leaves the VitaSDK install untouched. (Upstream fix: VitaSDK should bundle the matched libgcc/libstdc++ or statically link GDB.)

### VS Code (Native Debug extension)

Install the [Native Debug](https://marketplace.visualstudio.com/items?itemName=webfreak.debug) extension, then add to `.vscode/launch.json`:

```json
{
    "type": "gdb",
    "request": "attach",
    "name": "Attach to Vita3K",
    "executable": "${workspaceFolder}/build/eboot.elf",
    "target": "localhost:2159",
    "remote": true,
    "cwd": "${workspaceFolder}",
    "gdbpath": "arm-none-eabi-gdb"
}
```

## Usage

Once connected:

- `b main` — set a breakpoint by symbol name
- `b *0x80004000` — set a breakpoint by address
- `c` — continue execution
- `si` — step one instruction
- `Ctrl-C` — interrupt a running target
- `info registers` — view CPU registers
- `info sharedlibrary` — list loaded Vita modules
- `info threads` — list all threads
- `detach` — disconnect cleanly (app resumes, can reconnect)

## Supported Features

- Software breakpoints (Thumb and ARM)
- Single-step execution
- Register read/write (r0-r15, cpsr, d0-d31, fpscr)
- Memory read/write (hex and binary)
- Thread enumeration and per-thread register access
- Module enumeration (`info sharedlibrary`)
- Symbol-relative breakpoints via `qOffsets`
- Ctrl-C interrupt during continue
- Disconnect and reconnect without restarting the emulator
- target.xml with ARM + VFP description (for XML-capable GDB clients)

## Known Limitations

- Data watchpoints (Z2/Z3/Z4) work but are slow. Enabling a watchpoint switches Dynarmic into callback mode for every memory access, which is ~100-1000x slower than JIT-inlined loads/stores. Watching a variable that is written every frame will appear to freeze the app. Best used on rarely-written globals or in simple test apps. A page-protection-based approach would be needed for real-time watchpoints.
- VitaSDK's GDB (15.2, built `--without-expat`, no XML) has limited `si` support after Ctrl-C interrupts.
- The stub reports the **real CPSR** (the Cortex-A9 runs both ARM and Thumb; the T-bit reflects the actual mode and is no longer force-set). Dynarmic's A32 core does not model the CPSR mode field, so `cpsr & 0x1f` reads `0` instead of `0x10` (User). This is cosmetic — GDB only needs the T-bit for ARM/Thumb decode and breakpoint kind, which is correct.
- The GDB Thumb-breakpoint bit is stripped (`align_down(addr, 2)`) before recording the breakpoint. Modern GDB (13.x/15.x) actually sends an even address plus `kind=2`, so this is a defensive no-op there; it is required by older GDB (≤9.2) that set bit 0 of the address.
- `qXfer:features:read` (target.xml) is not advertised to VitaSDK's GDB because it is built `--without-expat` and cannot parse it. The handler is present and is used by XML-capable clients (e.g. `arm-none-eabi-gdb`).
- Ctrl-C cannot cleanly interrupt a thread that is stuck in a crash loop (e.g. dynarmic's "Invalid read" spam from executing garbage code). The stop-the-world path will hang trying to suspend the spinning thread, and the emulator must be killed and restarted. Setting breakpoints *before* the crash point is the workaround.
- Setting 3+ breakpoints in rapid succession and then continuing can cause a `Remote communication error` disconnect immediately after `vCont;c` — root cause not yet identified. Workaround: set one breakpoint at a time with a `c` in between, or use `disable`/`enable` on a single breakpoint to swap targets.
