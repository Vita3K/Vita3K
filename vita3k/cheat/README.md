# Cheats

Support for the cheat databases of the PS Vita plugins [FinalCheat / VitaCheat][db], so that the
`*.psv` files those projects distribute can be used as they are.

## Using cheats

1. Put the database of a game in `<Vita3K data folder>/cheats/`, named after its title id
   (`PCSA00147.psv`). `.txt` is accepted as well, some databases ship that extension. A combined
   database covering many games at once (`cheat.db`) works too, see below.
2. Open **Manage ▸ Cheats**, or right click a game in the app list and pick **Manage Cheats**.
3. Tick the cheats to turn on. While the game is running they take effect immediately, otherwise
   they take effect the next time it boots. Selecting a cheat shows its codes below the list, so
   what it writes can be checked before turning it on.
4. **Save** writes the cheats that are on back to the file (as `_V1` markers), so that they come
   back on the next time the game boots.
5. **Open Cheat File** opens the database of the selected game in the default editor, and
   **Reload** reads it back. Editing a cheat file while its game runs does not need a restart.

The **Enable cheats** checkbox is the master switch (`enable-cheats` in `config.yml`). Turning it
off stops every cheat and puts back the instructions the ARM write codes replaced.

Codes run once per vblank, the same way the plugins do on hardware.

## File format

```
# PCSA00147 Freedom Wars      <- first comment, shown in the cheat manager

_V0 Max Gold                  <- cheat, off until the user turns it on
$0200 81000000 3B9AC9FF

_V1 Max MP                    <- cheat, on as soon as the game boots
$3201 81000000 000008D8
$3300 00000000 3B9AC9FF
```

Every code line is `$XXXX AAAAAAAA BBBBBBBB`, where the four digits of the control word are
`<type><width/operator><related codes or pointer level>`. Width digits are `0` for 8-bit, `1` for
16-bit and `2` for 32-bit.

Codes that span several lines put one word per line, in the second field of the line. Databases
write the control word of those continuation lines either as `$0000` or as the marker of the block
(`$33`, `$88`, `$89`), so it is not looked at.

## Combined databases

A database that carries every game in one file opens the codes of each with a `_S <title id>`
line, and Vita3K reads only the section of the game it is booting:

```
########################################
>>>Start Of VitaCheat Database (v0.1)<<<
########################################
_S PCSA00008
_V0 inf.HP
$3002 81F254C0 00000014
$0000 00000000 000005D3
$0000 00000000 00000047

_S PCSA00009 Game Name
_V0 inf.Money
$0200 82CF52A8 000BDE31
```

Such a file is looked up by its `.db` or `.txt` extension, and a file named after the title id
takes precedence over it. Turning cheats on and saving rewrites only the `_V` markers of that
game's section, the other sections are left untouched.

## Supported code types

| Type | Layout | What it does |
| --- | --- | --- |
| `$0X00` | `<address> <value>` | Write `value` at `address`. |
| `$3X<level>` | `<address> <offset>`, then one line per further offset, then one line with the value | Follow `level` pointers from `address`, adding one offset at each step, then write the value. Up to 5 levels. |
| `$4X01` | `<address> <value>` + `$<count> <address gap> <value gap>` | Repeat the write `count` times, stepping the address and the value by their gap. |
| `$5X00` | `<destination> <source>` | Copy the value at `source` to `destination`. |
| `$7X<level>` | a `$3` block whose closing line is `$77XX`, then `$<count> <address gap> <value gap>` | Follow the pointers as `$3` does, then repeat the write as `$4` does. |
| `$8X<level>` + `$8<4\|5\|6><level>` | each block is a `$3`-style chain closed by a `$88` / `$89` line | Same as `$5`, with a pointer chain on both sides. The first block is the destination. |
| `$AX00` | `<address> <instruction>` | Patch guest code (X is `1` for a 16-bit and `2` for a 32-bit instruction). The original instruction is restored when the cheat is turned off. |
| `$B2<module>` | `0000000<segment> 00000000` | Make the addresses of the following codes relative to a segment of a loaded module, so that one cheat works across game versions. Module `0` is the main executable. |
| `$C2<codes>` | `<pad type> <button mask>` | Run the next `codes` codes only while the buttons of `mask` are held. The mask uses the `SCE_CTRL_*` values, e.g. `0x300` for L+R. |
| `$DX<codes>` | `<address> <value>` | Run the next `codes` codes only when the comparison holds. X selects both the comparison and the width: `0`-`2` `==`, `3`-`5` `!=`, `6`-`8` `>`, `9`-`B` `<`. |

The related count is a number of **codes**, not of lines, so a multi-line pointer write below a
condition is skipped as a whole. A count of `00` guards the single code right below.

## Not supported

- **Pad types.** `$C2` ignores the pad type field, Vita3K exposes a single virtual pad whatever
  the physical controller is.

[db]: https://github.com/r0ah/vitacheat
