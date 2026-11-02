# GDB Stub

Vita3K includes a GDB remote serial protocol (RSP) server that lets you debug Vita applications with a standard ARM GDB client.

## Enabling

Set `gdbstub: true` in your `config.yml`. The server listens on **port
2159**, and the app runs no guest code, `module_start` functions included,
until a GDB client connects and continues. The game window stays invisible
until the client continues, while the main Vita3K window stays visible. If
the server can't start, for example because the port is taken, the app runs
without it.

Optional: set `log-gdb-packets: true` to see every RSP packet in the Vita3K log (useful for debugging the stub itself).

## Connecting

### arm-none-eabi-gdb (recommended)

Install the [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads). Then:

```
arm-none-eabi-gdb path/to/eboot.elf -ex "target remote localhost:2159"
```

This GDB version has XML support, so VFP registers are available.

### arm-vita-eabi-gdb (VitaSDK)

```
arm-vita-eabi-gdb path/to/eboot.elf -ex "target remote localhost:2159"
```

The stub needs a GDB built with XML support (`--with-expat`), which you can check with `arm-vita-eabi-gdb --configuration`. It advertises `target.xml` for the registers and has no legacy register layout.

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
- `b *0x80004000` — set a breakpoint by address. Without a symbol there, GDB picks ARM or Thumb from the current frame, and a thread stopped in an import stub is in ARM. A 4-byte ARM breakpoint in Thumb code also overwrites the next instruction, and the game crashes. Most game code is Thumb, so run `set arm force-mode thumb` first, or `set arm force-mode arm` for ARM code.
- `watch *(int *)0x81170000` — stop when a value changes
- `c` — continue execution
- `si` — step one instruction. A thread that hasn't started can't step, so while `module_start` runs, gdb starts on the thread running it.
- `Ctrl-C` — interrupt a running target
- `info registers` — view CPU registers
- `info sharedlibrary` — list loaded Vita modules
- `info threads` — list all threads
- `detach` — disconnect cleanly (app resumes, can reconnect)

## Static Analysis

Give GDB the decrypted `eboot.bin` ELF for its symbols, as in the commands above. `info files` and `qOffsets` show where the segments loaded, and where those match the ELF's `p_vaddr`, addresses in GDB and in a disassembler like Ghidra are the same. An offset for a patch file is the address minus the start of its segment.

Writing to memory from GDB also invalidates the JIT cache, so a change to code takes effect the next time the code runs.

## Supported Features

- Software breakpoints (Thumb and ARM)
- Data watchpoints
- Single-step execution
- Register read/write (r0-r15, cpsr, d0-d31, fpscr, q0-q15)
- Memory read/write (hex and binary)
- Thread enumeration and per-thread register access
- Module enumeration (`info sharedlibrary`)
- Symbol-relative breakpoints via `qOffsets`
- Ctrl-C interrupt during continue
- Disconnect and reconnect without restarting the emulator
- target.xml with ARM + VFP description (for XML-capable GDB clients)

## Known Limitations

- Data watchpoints (Z2/Z3/Z4) work but are slow. Enabling a watchpoint switches Dynarmic into callback mode for every memory access, which is ~100-1000x slower than JIT-inlined loads/stores. Watching a variable that is written every frame will appear to freeze the app. Best used on rarely-written globals or in simple test apps. A page-protection-based approach would be needed for real-time watchpoints.
- The stub reports the **real CPSR** (the Cortex-A9 runs both ARM and Thumb; the T-bit reflects the actual mode and is no longer force-set). Dynarmic's A32 core does not model the CPSR mode field, so `cpsr & 0x1f` reads `0` instead of `0x10` (User). This is cosmetic — GDB only needs the T-bit for ARM/Thumb decode and breakpoint kind, which is correct.
- The GDB Thumb-breakpoint bit is stripped (`align_down(addr, 2)`) before recording the breakpoint. Modern GDB (13.x/15.x) actually sends an even address plus `kind=2`, so this is a defensive no-op there; it is required by older GDB (≤9.2) that set bit 0 of the address.
- Modules from the boot image have no file of their own, so they aren't in the library list and GDB has no symbols for them.
- Ctrl-C cannot cleanly interrupt a thread that is stuck in a crash loop (e.g. dynarmic's "Invalid read" spam from executing garbage code). GDB gets the stop after 200ms, but that thread keeps running, so its registers can't be trusted. Setting breakpoints *before* the crash point is the workaround.
- Setting 3+ breakpoints in rapid succession and then continuing can cause a `Remote communication error` disconnect immediately after `vCont;c` — root cause not yet identified. Workaround: set one breakpoint at a time with a `c` in between, or use `disable`/`enable` on a single breakpoint to swap targets.
