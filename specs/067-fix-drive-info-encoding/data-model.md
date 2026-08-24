# Data Model: Fix Garbled Numbers in Drive Information Dialog

**Feature**: 067-fix-drive-info-encoding · No persistent data is involved;
the "data" of this feature is the in-memory formatted string and its encoding
state as it travels from producer to display sink.

## Entity: FormattedNumberString

A `char*` buffer produced by `NumberToStr`, `NumberToStr2`,
`PointToLocalDecimalSeparator` or `PrintDiskSize` and consumed by exactly one
display sink. Its single meaningful attribute is the **encoding state**:

| State | Definition | Producible by | Renders via strict-U8-first sink as |
|---|---|---|---|
| `U8` | valid UTF-8 throughout | pure number; `PrintDiskSize(..., u8=TRUE)`; `LoadStrU8`-composed template | correct (wide path) |
| `ANSI` | system code page throughout, no multi-byte UTF-8 sequences | `LoadStr` text with no spliced number | correct (legacy fallback renders ANSI faithfully) |
| `MIXED` | ANSI bytes and UTF-8 sequences in one buffer | ANSI `LoadStr` template + spliced UTF-8 separator/number | **garbled** (strict conversion fails → A fallback → UTF-8 bytes drawn as ANSI: `Â `) |

**Invariant introduced by this feature** (see
[contracts/number-format-encoding.md](contracts/number-format-encoding.md)):
no core code path may construct a `MIXED` string that reaches a display sink.
A composition is either all-`U8` (via `LoadStrU8` / `u8=TRUE`) or all-`ANSI`
(no grouped number spliced in).

### State transitions

```
digits (ASCII) --NumberToStr--> U8            (separator splice)
U8 number + LoadStr template  --> MIXED       (the defect; forbidden after fix)
U8 number + LoadStrU8 template--> U8          (the fix; feature-041 pattern)
ANSI template alone           --> ANSI        (legal; unaffected)
```

## Entity: DisplaySink (classification, not code)

Established by the audit (research.md R1/R2); each site's verdict is the pair
(encoding state, sink class):

| Class | Behavior on invalid UTF-8 | Representative |
|---|---|---|
| strict-U8-first | wide path, else legacy A call | `SalSetDlgItemTextU8` family, panel paint, message box |
| lenient-U8 | U+FFFD per bad byte, never A | information line (`SalU8ToWDisplayAlloc`) |
| genuinely-ANSI | bytes drawn as ANSI, always | ANSI tooltip `TTN_NEEDTEXT` (viewer — converted to wide by this feature), plugin-internal A sinks (out of scope) |

## Entity: LocalizedUnitText

The resource strings spliced into sizes, with the shipped-language exposure
that bounds the blast radius (research.md R1):

| Resource | IDs | Non-ASCII in shipped languages | Role |
|---|---|---|---|
| plural bytes template | 12820 (`IDS_PLURAL_X_BYTES`) | Czech, Hungarian | mode 1/2 — **the defect carrier** |
| unit abbreviations | 13980–13986 (`IDS_SIZE_B..EB`) | none (ASCII everywhere shipped) | modes 0/1/3/4 — byte-identical under `LoadStrU8` |
| not-enough-space message | 10181 (`IDS_NOTENOUGHSPACE`) | Czech (and others) | `zip.cpp:6566` companion fix |
| viewer offset tooltip | 10199 (`IDS_VIEWEROFFSETTIP`) | none shipped (ASCII) | sink-side fix only |

**Validation rule** derived from this table: for every shipped language,
`LoadStrU8(id) == LoadStr(id)` byte-for-byte whenever the string is ASCII —
which is what makes the `u8=TRUE` conversion regression-free outside the two
languages where it is the fix.
