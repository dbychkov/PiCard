# PiCard Firmware & Hardware — Unified Technical Reference

> This document synthesises the firmware source code analysis with the physical schematic (ZX\_PICARD\_V3) to provide a single authoritative reference for the PiCard RP2350B ZX Spectrum extension card.

---

## High-Level Overview

### Summary

PiCard is a ZX Spectrum **NemoBus-compatible extension card** that replaces an entire rack of classic peripheral ICs with a single **WeAct Studio RP2350B** module (Raspberry Pi RP2350 MCU + 8 MB on-board PSRAM). The firmware emulates the **General Sound (GS)** coprocessor card, a **Turbo Sound** dual-AY chip board, a **DivMMC**-compatible SD-card interface, a **Z-Controller** SD interface, a **PS/2 keyboard** adapter, and a **Kempston joystick** interface. All audio sources are mixed in real time and delivered to a **TDA1387T I2S DAC** (U4 on the schematic) through an MCP602 op-amp output stage to the PJ325 stereo headphone jack.

The firmware is written in **C/C++** targeting the Arduino-RP2350 toolchain (Pico SDK underneath).

---

### Hardware Architecture

The RP2350B is over-clocked to **351 MHz** (core voltage boosted to 1.60 V via `vreg_set_voltage(VREG_VOLTAGE_1_60)`) to give sufficient headroom to run a software Z80, service the 3.5 MHz ZX bus on a polling loop, and synthesise multi-source audio every 9 µs.

The board operates in two separate voltage domains:

| Domain | Voltage | Key components |
|--------|---------|---------------|
| ZX NemoBus | +5 V CMOS | Edge connectors J1/X1A + J2/X1B, 74HC245 U1, 74HC04 U2, BC817 transistors Q1–Q4 |
| RP2350B digital logic | +3.3 V | WeAct module, PCF8575 U3, TDA1387T U4, UART1, SPI1 (SD card) |

An **AMS1117-3.3 LDO** (U6) derives the +3.3 V rail from the +5 V bus. The +5 V supply is drawn from the Spectrum edge connector via solder jumpers JP\_5V0/JP\_5V1/JP\_5V2.

#### NemoBus Interface

The Spectrum bus lines are mapped directly to RP2350 GPIO banks:

| Signal | GPIO | Direction | Schematic path |
|--------|------|-----------|---------------|
| A0–A15 | 8–23 | Input | Through RP1/RP2 resistor networks (10 kΩ) to ESD-clamp |
| D0–D7 | 24–31 | Bidirectional | Through 74HC245 U1 (A-side = 3.3 V, B-side = 5 V NemoBus) |
| /M1 | 3 | Input | RP2 → GPIO 3 (series 10 kΩ) |
| /WR | 4 | Input | RP2 → GPIO 4 |
| /RD | 5 | Input | RP1 → GPIO 5 (also PIO1 JMP pin) |
| /IORQ | 6 | Input | RP1 → GPIO 6 |
| /MREQ | 7 | Input | RP1 → GPIO 7 |
| DATA\_CTRL | 2 | Output | GPIO 2 → BC817 Q1 base → Q1 collector drives 74HC245 DIR/CE (5 V) |
| ROM\_BLK | 1 | Output | GPIO 1 → BC817 Q4 → 74HC04 U2 buffer → BLK net on edge connector |
| /NMI | 35 | Open-drain | GPIO 35 → BC817 Q3 → R30 (300 Ω) pull-up to +5 V → NemoBus /NMI |
| /RESET | 47 | Open-drain | GPIO 47 → BC817 Q2 → NemoBus /RESET |

`PIN_DATA_CTRL` (GPIO 2) is driven by the PIO `side-set` pin, enabling the 74HC245 output in hardware for only the nanoseconds required to present data on the bus. `PIN_ROM_BLK` (GPIO 1) drives the ROM-blocking buffer, allowing the RP2350 to substitute DivMMC or test ROM images over the Spectrum's internal ROM space.

#### Peripheral Connections

| Peripheral | Interface | Pins | Physical component |
|-----------|-----------|------|--------------------|
| SD card | SPI1 (10 MHz) | RX=40, CS=41, CLK=42, TX=43 | J\_SD1 Micro-SD socket + J7 6-pin header |
| I2S DAC | PIO2 | DATA=44, BCLK=45, LRCK=46 | TDA1387T U4 pins 3/1/2 |
| PCF8575 joystick expander | I2C0 (400 kHz) | SDA=32, SCL=33 | PCF8575DBR U3 pins 23/22 |
| PS/2 keyboard | PIO0 | CLK=38, DATA=39 | Mini-DIN-6 J6 pins 5/1 via R12/R13 (1 kΩ) |
| UART (ZX UNO protocol) | UART1 (115200) | TX=36, RX=37, CTS=34 | J8 auxiliary header / shared with PS/2 port 2 |
| PSRAM | QMI/XIP CS1 | GPIO 0 | On-module APS6404L (WeAct RP2350B) |

---

### Codebase Map

```
PiCard3.ino          Main entry point, audio mixer, ZX bus event dispatcher
config.h             All compile-time feature flags and pin assignments

src/PIO/
  pio_data_z80_bus.cpp/h    PIO1 state machine — zero-latency Z80 data-bus driver

src/GS/
  GS.cpp/h           General Sound Z80 card emulation (software Z80 + PSRAM banking)
  z80/               Z80 CPU core (Z80.c by Marcel de Kogel)
  gs105b.rom.h       GS firmware ROM as a C array

src/TS/
  ts.cpp/h           Turbo Sound (dual AY-3-8910) emulation

src/i2s/
  i2s.c/h            PIO2 I2S audio output driver → TDA1387T U4

src/ps2/
  PS2_PIO.c/h        PIO0 PS/2 keyboard receiver + dual-DMA scan-code decoder

src/kbd/
  kb_u_codes.c/h     Internal key-code definitions

src/zx_util/
  zx_kb.c/h          PS/2 → ZX Spectrum 8×5 key-matrix translator

src/i2c/
  i2c_joy.cpp/h      PCF8575 U3 I2C joystick driver (NES + Sega modes)

src/UART/
  uart.cpp/h         UART1 hardware driver (ZX UNO register protocol)

ROMS/
  ROM_DIVMMC.h       DivMMC ESXDOS ROM as a C array
  testrom.h          Optional test ROM
```

---

## Emulated Devices Deep Dive

### General Sound (GS) Card

The GS card is a standalone Z80-based sound coprocessor board. The firmware emulates the entire card in software, using the on-module PSRAM for banked RAM storage and the TDA1387T I2S DAC for audio output.

#### Z80 CPU Emulation

The GS Z80 is emulated by a portable software Z80 core (`src/GS/z80/Z80.c`, based on Marcel de Kogel's emulator), instantiated as a single global `Z80 cpu` object on **Core 0**. `GS.cpp` provides the four mandatory callbacks:

- **`RdZ80(address)`** — memory read; routes to ROM, SRAM, or QMI PSRAM depending on address and banking state. Decorated `__time_critical_func` to force placement in SRAM (not XIP flash), avoiding any cache miss latency during Z80 execution.
- **`WrZ80(address, value)`** — memory write; updates the appropriate RAM bank.
- **`InZ80(port)`** — Z80 I/O read; exposes the inter-CPU mailbox registers to the GS Z80.
- **`OutZ80(port, value)`** — Z80 I/O write; handles paging (MPAG), channel volumes, and mailbox writes.

The Z80 is stepped in Core 0's main loop using the time remaining before the next 9 µs audio tick:

```c
int32_t dt = int_tick - tick_time;
ExecZ80(&cpu, dt * 12);  // ~12 Z80 cycles per µs at emulated 12 MHz
```

At each 9 µs audio tick, an IRQ is injected and the latest GS stereo sample is captured:

```c
IntZ80(&cpu, INT_IRQ);
snd_out_GS = GS_get_sound_LR_sample();
```

NMI and reset are triggered by writing to the `GSCTR` register (port `0x33`) from the Spectrum side, with bits `0x40` (NMI) and `0x80` (reset) handled in the Core 0 loop.

#### Memory Banking — Backed by On-Module PSRAM

The GS card's 64-page banked address space is stored in the **8 MB PSRAM** chip that is part of the WeAct RP2350B module (APS6404L, connected to the RP2350 via QMI). The firmware initialises the QMI interface through `psram_init(SFE_RP2350_XIP_CSI_PIN)`:

```c
// QMI divider set so PSRAM clock ≈ 166 MHz (APS6404L maximum)
// At 351 MHz sys clock, divisor = 3 → PSRAM clock ≈ 117 MHz
gpio_set_function(cs_pin, GPIO_FUNC_XIP_CS1);
qmi_hw->m[1].timing = ... rxdelay=3, divisor=3 ...
xip_ctrl_hw->ctrl |= XIP_CTRL_WRITABLE_M1_BITS;  // enable writes to PSRAM via XIP
```

The large banked arrays are placed in PSRAM via the `PSRAM` section attribute:

```c
static uint8_t RAM_BANK1[62][0x4000] PSRAM;  // pages 2–63, 62×16 KB = 992 KB
static uint8_t RAM_BANK2[62][0x4000] PSRAM;  // second set, another 992 KB
```

Pages 0 and 1 use fast local RP2350 SRAM (`RAM_BANK1_1[2]`, `RAM_BANK1_2[2]`) so the most-used low pages do not incur PSRAM latency.

Four slot pointers (`ram_slot1`–`ram_slot3`) are updated by `OutZ80(port=0x00, value=page)` to point at the correct PSRAM page for the current bank:

| Slot | Address range | Default content |
|------|---------------|----------------|
| slot0 | `0x0000–0x3FFF` | GS ROM (`gs105b_rom[]`, in flash) |
| slot1 | `0x4000–0x7FFF` | Fixed SRAM page (RAM_BANK1_2[1]) |
| slot2 | `0x8000–0xBFFF` | Banked (PSRAM page `&RAM_BANK1[page-2]`) |
| slot3 | `0xC000–0xFFFF` | Banked (PSRAM page `&RAM_BANK2[page-2]`) |

#### PCM Channel Capture

A clever trick inside `RdZ80()` auto-captures PCM sample data: when the GS Z80 reads from addresses `0x6000–0x7FFF`, address bits A8 and A9 silently identify which of the four audio channels this byte is headed for:

| A9 | A8 | Channel captured |
|----|----|-----------------|
| 0  | 0  | `channel1` (0x6000–0x60FF) |
| 0  | 1  | `channel2` (0x6100–0x61FF) |
| 1  | 0  | `channel3` (0x6200–0x62FF) |
| 1  | 1  | `channel4` (0x6300–0x63FF) |

This happens passively during normal GS Z80 memory reads, with no interrupt or DMA required.

#### ZX-Side I/O Interface

The Spectrum communicates with the GS via three I/O ports decoded in Core 1's bus polling loop:

| Port | Direction | Function |
|------|-----------|---------|
| `0xB3` | Write | ZX writes data byte to GS (`GSDAT`); sets `DataBIT` in `GSSTAT` |
| `0xB3` | Read | ZX reads last GS response (`ZXDATWR`); clears `DataBIT` |
| `0xBB` | Write | ZX sends command byte to GS (`GSCOM`); sets `CommandBIT` in `GSSTAT` |
| `0xBB` | Read | ZX reads GS status byte (`GSSTAT`) |
| `0x33` | Write | ZX writes GS control (`GSCTR`): bit7=reset, bit6=NMI |

These ports write to `volatile uint8_t` variables shared across Core 0 (GS Z80 callbacks) and Core 1 (bus loop). Single-byte `volatile` writes are inherently single-cycle and therefore safe without additional synchronisation primitives on ARMv8-M.

#### Audio Hardware Path — From GS Z80 to TDA1387T

```
GS Z80 (Core 0, software) → channel1–channel4 (uint8_t, PCM 8-bit unsigned)
  → GS_get_sound_LR_sample()
       applies volume (port 0x06–0x09) and stereo cross-feed:
       snd.L = ch3*vol3 + ch4*vol4 + ch1*(vol1/3) + ch2*(vol2/3)
       snd.R = ch1*vol1 + ch2*vol2 + ch3*(vol3/3) + ch4*(vol4/3)
  → snd_out_GS (volatile sound_LR, 2×int16_t)
  → mix_sound_out() sums with TS, Beeper, COVOX, SND_DRIVE
  → i2s_out(L_clipped, R_clipped)
       packs into static uint32_t i2s_data = (L<<16)|R
  → DMA channel dma_i2s reads i2s_data (no increment) → PIO2 TX FIFO
       paced by PIO2 DREQ at 44100 Hz
  → PIO2 state machine (audio_i2s_program) serialises bits → GPIO 44 (DATA)
       side-set toggles GPIO 45 (BCLK) and GPIO 46 (LRCK)
  → TDA1387T U4 (I2S slave): DATA pin 3, SCLK pin 1, LRCK pin 2
  → TDA1387T A_LEFT / A_RIGHT analog outputs
  → MCP602 U5A/U5B buffer + 5.5 kHz anti-alias filter
  → PJ325 J5 stereo 3.5 mm jack (Tip=L, Ring=R)
```

---

### Turbo Sound — Dual AY-3-8910 on One I2S Stream

Turbo Sound presents the Spectrum with **two AY-3-8910** chips on the same I/O address. There is no dedicated hardware for the AY chips on the schematic; the entire synthesis is performed in software by `TS::step()` running on **Core 0**, and the output is mixed into the same `snd_out_TS` stereo sample that flows to the TDA1387T.

#### Port Decoding

Both AY register-select (`0xFFFD`) and data-write (`0xBFFD`) ports are decoded in Core 1's bus loop using masked comparisons that match the original AY-bus convention where A14 and A15 select the chip function and A1 selects the chip:

```c
// Latch / register select
if ((addr16 & 0xC003) == (0xFFFC & 0xC003)) ts.select_reg(d8);
// Data write
if ((addr16 & 0xC003) == (0xBFFC & 0xC003)) ts.set_reg(d8);
// Data read
if ((addr16 & 0xC003) == (0xFFFC & 0xC003)) put_dataZ80(ts.get_reg());
```

Chip selection is done by the sentinel values written to the latch port (`0xFF` → chip 0, `0xFE` → chip 1).

#### Synthesis Engine (`TS::step()`) — Called at 111 kHz on Core 0

`step()` is called once per 9 µs audio tick. Each call processes **both chips** in a loop. For each chip:

1. **Tone generators** (channels A, B, C): A counter per channel is incremented by `DELTA=2`; when it exceeds the 12-bit period register it flips the output bit. Sub-sample interpolation (`dA`, `dB`, `dC` remainders) reduces high-frequency aliasing by blending the old and new amplitude values.
2. **Noise generator**: A 32-bit LFSR (`get_random()`) advances at the rate set by R6 (register 6). Noise-enabled channels XOR their output with the current noise bit.
3. **Envelope generator**: A 32-entry shape table `Envelopes[16][32]` (pre-computed at startup from `Envelopes_const` and `ampls_AY`) is stepped at the rate set by R11/R12. Looping shapes restart at index 0; hold shapes freeze at step 31.
4. **Amplitude lookup**: Output level is read from `ampls_AY[16]` (log-scale, values up to 21845).
5. **Stereo mixing**: Chip 0 routes A+B+½B to Right, C+B+½B to Left. Chip 1 adds identically. Both chips accumulate into `sample_out`.

The `sound_LR` result is stored in `snd_out_TS` and summed into the global mix in `mix_sound_out()`.

#### Audio Hardware Path

```
TS::step() (Core 0, every 9 µs) → snd_out_TS (sound_LR)
  → mix_sound_out() → i2s_out() → DMA → PIO2 → TDA1387T U4 → MCP602 → J5
```

Identical final path to GS — both emulated devices share the single TDA1387T I2S DAC and the MCP602 analogue output stage. The TDA1387T receives one merged 32-bit I2S word per sample, containing the fully-mixed left and right channels from all simultaneously-active sound sources.

---

### DivMMC — SD-Card Interface via SPI1 and NemoBus M1 Trapping

DivMMC is an SD-card memory-mapped interface that intercepts specific Z80 opcode-fetch (M1) addresses to activate a 16 KB ROM/RAM window over the bottom of the Spectrum address space.

#### Physical SD Interface

The SD card connects to **SPI1** (hardware SPI peripheral) via:

| SPI1 signal | GPIO | Schematic net | Connector |
|-------------|------|---------------|-----------|
| MISO (RX) | 40 | SD\_MISO | J\_SD1 micro-SD + J7 pin 4 |
| CS0 | 41 | SD\_CS0 | J\_SD1 + J7 pin 1 |
| CLK | 42 | SD\_CLK | J\_SD1 + J7 pin 3 |
| MOSI (TX) | 43 | SD\_MOSI | J\_SD1 + J7 pin 2 |

SPI1 is initialised at **10 MHz** in `ZX_bus_task()` on Core 1. Individual bytes are transferred by directly accessing the SPI hardware data register to achieve single-byte latency without SDK overhead:

```c
static inline uint8_t READ_SD_BYTE() {
    uint8_t data = spi_get_hw(SD_SPI)->dr;
    spi_get_hw(SD_SPI)->dr = 0xFF;  // pre-load next dummy byte
    return data;
}

static inline void WRITE_SD_BYTE(uint8_t data) {
    volatile uint8_t dummy = spi_get_hw(SD_SPI)->dr;  // drain RX
    spi_get_hw(SD_SPI)->dr = data;
}
```

The firmware uses **two physical SD connectors** (J\_SD1 and J7) wired in parallel to the same SPI1 bus and the same CS0 line (GPIO 41). Only one card may be inserted at a time.

#### Memory Window

When DivMMC is active (`divMMC_ON` or `divMMC_ON_PORT`):

| Address | Content | Physical source |
|---------|---------|----------------|
| `0x0000–0x1FFF` | DivMMC ROM (`ROM_DIVMMC[]`, 8 KB ESXDOS) | C array in flash |
| `0x2000–0x3FFF` | Selected 8 KB RAM page from `divMMC_RAM[16][8192]` | RP2350 SRAM |

`PIN_ROM_BLK` (GPIO 1 → Q4 BC817 → 74HC04 → BLK net) is asserted LOW to block the Spectrum's own ROM. The Spectrum's ROM IC sees its chip-enable deasserted and immediately goes high-impedance on D0–D7. Core 1 then drives the DivMMC image onto the bus via the 74HC245 U1 and the PIO1 state machine.

#### Auto-Activation via M1 Trapping

Core 1 checks every MREQ+/RD bus cycle for the `/M1` signal (GPIO 3 = `PIN_M1`). When M1 is asserted (opcode fetch), the fetched address triggers state machine transitions:

| Address range | Action |
|---------------|--------|
| `0x0000`, `0x0008`, `0x0038`, `0x0066`, `0x04C6`, `0x0562` (RST vectors) | Set `divMMC_SW = true` — flip `divMMC_ON` at end of cycle |
| `0x3D00–0x3DFF` (ESXDOS BDOS entry) | Force `divMMC_ON = true` |
| `0x1FF8–0x1FFF` (ESXDOS exit trampoline) | Set `divMMC_SW = true` — flip `divMMC_ON` at end of cycle |

`divMMC_SW` (delayed flip) ensures the current opcode byte is fully served before the memory window switches, preventing the Spectrum CPU from fetching a stale value.

#### NMI and ROM\_BLK Hardware Interaction

Whenever DivMMC is active, the `/NMI` line is pulled low via GPIO 35 → BC817 Q3 → R30 (300 Ω) to +5 V → NemoBus `/NMI`. This generates the hardware NMI that causes the ESXDOS ROM to intercept DOS calls. When DivMMC deactivates, GPIO 35 is switched back to input (high-impedance) and Q3 turns off, releasing the NMI line:

```c
if (divMMC_ON || divMMC_ON_PORT) {
    gpio_put(PIN_NMI, 1);
    gpio_set_dir(PIN_NMI, GPIO_OUT);  // Q3 on → /NMI asserted LOW
} else {
    gpio_set_dir(PIN_NMI, GPIO_IN);   // Q3 off → /NMI released
}
```

#### Port Registers

| Port | Direction | Function |
|------|-----------|---------|
| `0xE3` | Write | Control: bit7=port-enable, bits 0–3=RAM page (0–15) |
| `0xE7` | Write | SPI CS: bit0=CS (0=asserted, routes to GPIO 41 → SD\_CS0 net) |
| `0xEB` | Write | SPI data byte → `WRITE_SD_BYTE()` → SPI1 DR → GPIO 43 (MOSI) |
| `0xEB` | Read | SPI data byte → `READ_SD_BYTE()` → SPI1 DR → GPIO 40 (MISO) |

---

### Z-Controller (ZC-SD)

The Z-Controller is a simpler SD-card interface, **mutually exclusive** with DivMMC at build time (`DIVMMC_EN`/`ZC_SD_EN` in `config.h`). It shares the same SPI1 bus and GPIO 41 CS0 line as DivMMC. When both are compiled in, GPIO 37 (`DIV_OFF_PIN`) reads a physical jumper on J8 to select the active mode at runtime; changing the jumper triggers a full Spectrum reset via `reset_spectrum()`.

ZC-SD ports:

| Port | Direction | Function |
|------|-----------|---------|
| `0x57` | Write | SPI data byte → `WRITE_SD_BYTE()` |
| `0x57` | Read | SPI data byte → `READ_SD_BYTE()` |
| `0x77` | Write | Control: bit0=`is_SD_active`, bit1=CS state → GPIO 41 |
| `0x77` | Read | Returns `0xFC` (status) |

---

### Keyboard & Joystick

#### PS/2 Keyboard — PIO0 + Dual DMA + Mini-DIN-6 Connector (J6)

The physical PS/2 keyboard plugs into **J6**, a 6-pin Mini-DIN socket on the board. Schematic protection:

- R12 (1 kΩ) in series with CLK → GPIO 38 (`PIN_PS2_CLK`)
- R13 (1 kΩ) in series with DATA → GPIO 39 (`PIN_PS2_DATA`)

The 1 kΩ series resistors limit the ESD clamp current to ~1.2 mA when a 5 V PS/2 device drives a 3.3 V GPIO input, keeping the RP2350B safe while still providing valid logic levels (PS/2 open-drain lines pull-up through the keyboard's own 10 kΩ resistors, giving a steady-state GPIO input close to 3.3 V).

**PIO0 receive state machine (`pio_program_PS2`):**

A 5-instruction PIO program continuously receives 11-bit PS/2 frames (start + 8 data + parity + stop):

```
set  x, 10          ; count down 11 bits
wait 0 gpio, CLK    ; falling edge of PS/2 CLK (JMP pin = GPIO 38)
in   pins, 1        ; sample DATA pin (GPIO 39)
wait 1 gpio, CLK    ; rising edge
jmp  x--, 1        ; repeat for all 11 bits
```

The PIO clock divider is set to `clock_get_hz(clk_sys) / 200000` — approximately **200 kHz**, ensuring the state machine can sample PS/2 CLK edges (which occur at 10–16 kHz) reliably even at the 351 MHz system clock. The receive shift register is configured for 11-bit right-in, auto-push into the RX FIFO.

**Zero-CPU DMA ring buffer:**

`init_PS2()` sets up two chained DMA channels:

- **dma\_chan0**: PIO0 RX FIFO → `PS_2_BUS[100]`, 100-word burst, paced by PIO0 DREQ, chains to dma\_chan1 on completion.
- **dma\_chan1**: Writes `&PS_2_BUS[0]` back into dma\_chan0's write address register, then chains back to dma\_chan0.

Result: PS/2 frames arrive silently into the 100-word ring buffer with zero CPU involvement on either core.

**Scan-code decode:**

`decode_PS2()` runs on **Core 0** every ~576 µs (every 3rd tick → every 64th iteration). It extracts bytes from the ring buffer, validates parity, handles `0xE0`/`0xE1` extended prefixes, and calls `translate_scancode()` to produce a `kb_state_t` bitmask.

**ZX 8×5 key-matrix translation (`zx_kb.c`):**

`set_zx_kb_state()` maps each `KEY_*` code to one or more bits in the `zx_kb_state.a[8]` array, which models the ZX Spectrum keyboard matrix exactly. Multi-key sequences handle punctuation (e.g. `KEY_PERIOD` → CAPS SHIFT + `N`).

**Bus-side response on Core 1:**

Any IORQ+/RD cycle with `addr16 & 1 == 0` (Spectrum keyboard port) is answered by decoding the 8 high address bits as row selects:

```c
uint8_t addrh = addr16 >> 8;
uint8_t d8 = 0;
if ((addrh & 0x01) == 0) d8 |= kb_data[0];  // row 0: CAPS SHIFT / Z / X / C / V
if ((addrh & 0x02) == 0) d8 |= kb_data[1];  // row 1: A / S / D / F / G
...
put_dataZ80(~d8);  // active-low bits, inverted before driving GPIO 24–31
```

Special key combos: **Ctrl+Alt+Del** → `reset_spectrum()`; **Insert** → `NMI_press()` (drives Q3, asserts /NMI for 50 ms); **F12** → watchdog forced reboot.

#### Kempston Joystick — PCF8575DBR (U3) via I2C0 → J10 (DE-9)

The physical joystick plugs into **J10**, a 9-pin DE-9 connector that maps to the classic Atari/Kempston pin layout. Each directional and fire pin connects through a 10 kΩ pull-up resistor (R20–R28) to the corresponding P-port of **PCF8575DBR U3**:

| J10 pin | Signal | PCF8575 pin | Kempston bit |
|---------|--------|-------------|--------------|
| 1 | UP | P0 | bit 3 |
| 2 | DOWN | P1 | bit 2 |
| 3 | LEFT | P2 | bit 1 |
| 4 | RIGHT | P3 | bit 0 |
| 6 | FIRE | P4 | bit 4 |

PCF8575 is accessed via **I2C0** at 400 kHz (GPIO 32 = SDA, GPIO 33 = SCL, device address `0x20`, A0=A1=A2=GND). The firmware uses a custom non-blocking I2C function `i2c_transfer_non_blocking()` that:
1. Reads the previous I2C result from the hardware RX FIFO (result of the previous poll).
2. Issues the next read command into the TX FIFO with a STOP bit.

This returns immediately, never stalling Core 0's 9 µs audio tick. The raw 16-bit PCF8575 reading is converted to Kempston format via a precomputed 256-entry lookup table:

```c
// NES mode example (NES_JOY_EN=1):
data_kmpst = (nes_keys & 0x0F) | ((nes_keys >> 2) & 0x30)
           | ((nes_keys << 3) & 0x80) | ((nes_keys << 1) & 0x40);
data_kmpst ^= 0xFF;   // active-low → active-high Kempston polarity
```

The resulting `joy_state` byte is served by Core 1 when port `0x1F` is read. `kempston_port_enable` is cleared while the Spectrum CPU is fetching from ROM space (address < `0x3FFF`) to prevent conflicts with Spectrum software that uses that address range for ROM data.

---

## Critical Timing & Concurrency

### Dual-Core Division of Labour

| Core | Function |
|------|----------|
| **Core 0** | Audio synthesis (TS, GS, mixer → TDA1387T), GS Z80 emulation, PS/2 decode, I2C joystick polling, watchdog |
| **Core 1** | ZX Spectrum bus monitor (tight polling loop, all I/O and MREQ transactions) |

The cores share a handful of `volatile uint8_t` variables as the inter-core communication channel (`GSDAT`, `GSCOM`, `GSSTAT`, `ZXDATWR`, `snd_out_GS`, `snd_out_TS`, `kempston_state`, `zx_kb_state`). No FIFO or spinlock is used on the hot path, relying on the inherent atomicity of single-byte writes on ARM Cortex-M33.

### Core 0 — Timed Audio Loop (9 µs period)

Core 0 runs a tight timer-driven loop (`int_tick += 9`). At each tick:

1. `mix_sound_out()` — sums GS, TS, Beeper, COVOX, SND\_DRIVE contributions into a 32-bit accumulator, applies DC blocker and soft limiter, then calls `i2s_out()` to update `i2s_data`.
2. `ts.step()` — advances both AY chips by one synthesis step.
3. Every 3rd tick: PS/2 keyboard DMA buffer scan (`decode_PS2()`) and I2C joystick poll (`joy_proc()`).
4. Every 3rd tick (different phase): GS Z80 IRQ injection (`IntZ80`) and `check_div_status()`.

Between ticks, Core 0 continuously executes GS Z80 instructions:

```c
int32_t dt = int_tick - tick_time;
ExecZ80(&cpu, dt * 12);  // ~12 Z80 cycles per µs at emulated 12 MHz
```

The 9 µs tick gives an audio update rate of **≈111 kHz**. The TDA1387T consumes samples at 44.1 kHz (determined by the PIO2 clock divider), so the DMA reads each `i2s_data` value approximately 2.5 times before it is updated — a "nearest sample" approach rather than a ring buffer.

### Core 1 — ZX Bus Monitor (polling loop)

`ZX_bus_task()` is launched on Core 1 via `multicore_launch_core1`. It runs an infinite `for(;;)` loop reading `gpio_get_all()` on every iteration:

1. If `IORQ` and `MREQ` are both inactive → `is_new_cmd = true`, continue.
2. If `RD` and `WR` are both inactive → not a valid cycle, continue.
3. Four control bits (`MREQ`, `IORQ`, `RD`, `WR`) are inverted and used as a `switch` selector for the four bus cycle types.
4. `is_new_cmd` ensures each cycle is handled exactly once, then reset when control lines return to idle.

### PIO1 Bus Driver — Glitch-Free Data Output

When Core 1 calls `put_dataZ80(data)`:

```c
PIO_data_z80->txf[sm_data_z80] = 0x00FFFFFF | (data << 24);
```

The PIO1 state machine (`pio_dataz80_program`) handles everything autonomously at 351 MHz (one instruction → 2.8 ns):

```
0: mov pindirs, null   side 1  → DIR=1 (B→A), GPIO24-31 = inputs; DATA_CTRL=HIGH, Q1 off, U1 disabled
1: pull block          side 1  → wait for TX FIFO word
2: jmp pin, 0          side 1  → /RD still HIGH (no read active)? discard, restart
3: out pins, 8         side 1  → stage byte into GPIO output register (still inputs)
4: mov pindirs, ~null  side 0  → DATA_CTRL=LOW → Q1 on → U1 enabled; GPIO24-31 = outputs (byte appears on bus)
5: jmp pin, 0          side 0  → monitor /RD; when it returns HIGH → go back to step 0
6: jmp 5               side 0  → hold bus driven while /RD low
```

Steps 3–4 are the critical atomic pair: the byte is pre-loaded into the output register while pins are still inputs, then `pindirs` is flipped in a single 2.8 ns PIO cycle with the side-set simultaneously asserting `DATA_CTRL`. There is no window during which an incorrect value can appear on D0–D7.

### I2S Audio Path — DMA-Driven, CPU-Free

The `audio_i2s_program` (8 instructions, `pio2`) implements the I2S protocol entirely in PIO:

- **Data pin**: GPIO 44; **BCLK**: GPIO 45; **LRCK**: GPIO 46.
- **Shift register**: 32-bit auto-pull (left-justified, MSB first), mapping to 16-bit L + 16-bit R samples.
- `side-set` bits control BCLK and LRCK simultaneously with each data bit.
- **Clock divider** is computed from `sys_clock / (44100 × 8 × 4)` to achieve exactly 44,100 Hz sample rate independent of the CPU overclock.

After `i2s_out(L, R)` updates `i2s_data`:

```c
inline void i2s_out(int16_t l, int16_t r) {
    i2s_data = (((uint16_t)l) << 16) | ((uint16_t)r);
}
```

Two chained DMA channels (`dma_i2s` + `dma_i2s_ctrl`) feed the PIO2 TX FIFO:

- **dma\_i2s**: Reads `i2s_data` (no read increment) → PIO2 FIFO, 1024 words, paced by PIO2 DREQ at 44.1 kHz. On completion, chains to `dma_i2s_ctrl`.
- **dma\_i2s\_ctrl**: Writes `1024` back into `dma_i2s.transfer_count` and chains back to `dma_i2s`, restarting the burst.

The net effect: the TDA1387T receives a continuous, jitter-free I2S stream entirely driven by DMA + PIO. Even if Core 0 is temporarily delayed by a long `ExecZ80()` call, the DMA pair will simply keep sending the last `i2s_data` value, producing a held sample (brief tonal artefact) rather than a hard pop.

### Audio Mixing and Protection

`mix_sound_out()` sums signed 16-bit contributions from GS, TS, Beeper (`port & 0x0000` bits 3/4), COVOX (`port 0xFB`), and SND_DRIVE (`ports 0x0F/0x1F/0x4F/0x5F`) into a 32-bit accumulator for each channel.

A **DC blocker** tracks the long-term mean using a first-order IIR (±1 per sample):

```c
snd_L_C += SIGN_F(snd_L_in - snd_L_C);  // ±1 per sample
snd_L_out = snd_L_in - snd_L_C;
```

A **soft limiter** (piecewise linear compressor) prevents hard clipping.
Below `P_LIN = 20000`: linear passthrough.  
Above `P_LIN`: compressive curve approaching `P_MAX = 32760`:

$$
y = \frac{x \cdot P_{MAX} - P_{LIN}^2}{x - 2 \cdot P_{LIN} + P_{MAX}}
$$

---

## Feature Flag Summary (`config.h`)

| Flag | Default | Effect |
|------|---------|--------|
| `GS_EN` | 1 | General Sound Z80 card emulation |
| `TS_EN` | 1 | Turbo Sound dual AY emulation |
| `DIVMMC_EN` | 1 | DivMMC SD interface and memory mapper |
| `ZC_SD_EN` | 0 | Z-Controller SD interface (mutually exclusive with DivMMC) |
| `KB_PS2_EN` | 1 | PS/2 keyboard PIO0 receiver |
| `KEMPSTON_EN` | 1 | Kempston joystick port emulation |
| `JOY_I2C` | 1 | PCF8575 U3 I2C joystick expander |
| `BEEPER_EN` | 1 | ZX Spectrum Beeper output |
| `COVOX_EN` | 1 | Covox DAC port (`0xFB`) |
| `SND_DRIVE_EN` | 1 | SoundDrive ports (`0x0F/0x1F/0x4F/0x5F`) |
| `HW_UART_EN` | 1 | UART bridge (ZX UNO register protocol) |
| `TEST_ROM_EN` | 0 | Substitute built-in test ROM for Spectrum ROM |

---

## Potential Weaknesses, Bugs, and Bottlenecks

---

### 1. I2C Joystick Latency via PCF8575 — Input Lag and NES Bit-Bang Stall

**Problem:**

The PCF8575 (U3) is polled via I2C0 at 400 kHz. A complete I2C read transaction (start + address byte + ACK + 2 data bytes + STOP) requires a minimum of **~45 µs** of bus time at 400 kHz. In the current `NES_MODE` path, `joy_proc()` performs a multi-step bit-bang sequence over the PCF8575 port — asserting LATCH, toggling CLK 8 times, reading data — with each step requiring a separate I2C write (to update port outputs) and a subsequent I2C read (to sample DATA). For a full NES shift cycle this is approximately **8 × 2 × 45 µs = 720 µs** minimum, spread over multiple calls to `joy_proc()`.

The `i2c_transfer_non_blocking()` function mitigates the direct stall by pipelining: it issues the next transaction into the hardware FIFO and reads the previous result in the same call. However:

- **Polling rate**: `joy_proc()` is called only every 3rd × 9 µs tick = every **27 µs**. At this rate, a complete NES poll takes ≈ 27 µs × 20+ steps ≈ **540+ µs** round trip.
- **I2C FIFO overflow check**: `i2c_transfer_non_blocking()` checks `i2c_get_write_available(i2c) < 16` before issuing the next command. If the previous transaction has not yet completed (I2C bus still busy from a prior call), the function bails out and returns `false`, silently dropping that poll step. There is no retry or error indication to the caller.
- **Kempston update granularity**: `joy_state` is updated at the end of the NES sequence (case 20 in `joy_proc()`). If the Spectrum's game loop reads port `0x1F` between NES polls, it sees a stale value. At a 50 Hz game frame rate (20 ms), the NES poll loop must complete in < 20 ms — it does — but fast-polling games (e.g. those that spin on `IN 0x1F` waiting for a direction change) may see up to **1 ms of extra latency** between a physical button press and the Spectrum reading the updated Kempston value.

**Symptoms:**
- Slightly sluggish joystick response in fast-action games.
- Occasional missed single-frame inputs in rhythm-based or fast-twitch games.
- In NES mode: if I2C is congested, `joy_state` may freeze at the last fully-completed value for multiple frames.

**Recommended Fixes:**

1. **Use a dedicated GPIO for one joystick direction per physical output pin**, bypassing I2C entirely for the most latency-critical lines. The RP2350B has enough free GPIO capacity — Kempston needs only 5 bits.
2. **For NES mode**: Use the RP2350B's PIO to bit-bang the NES latch/clock protocol natively (identical to the `DENDY_JOY_3_PINS` path that already exists in the codebase, compiled in when `DENDY_JOY_3_PINS=1`). This would eliminate I2C latency entirely and reduce total poll time to a handful of PIO clock cycles.
3. **For PCF8575 parallel mode** (Sega/Atari): Switch from I2C0 to a polled I2C burst (read both bytes in a single transaction). Pre-compute the full I2C command sequence as a DMA transfer triggered by a repeating timer, writing results into a memory buffer that `get_joy_data()` reads lock-free.
4. **Double-buffer `joy_state`**: Use `__atomic_store_n(&joy_state, value, __ATOMIC_RELAXED)` and `__atomic_load_n` to prevent Core 1 from reading a partially-updated byte (currently safe for `uint8_t`, but explicit atomics document the intent).

---

### 2. Bus Contention and Level-Shifting Race Conditions

**Problem A — Transistor propagation delay in DATA\_CTRL path:**

GPIO 2 (`PIN_DATA_CTRL`) controls the 74HC245 U1 direction and output-enable via BC817 transistor Q1. The NPN transistor introduces a **turn-on delay** of approximately 5–15 ns (base-charge storage time at low base current set by R23/R27 = 10 kΩ). This means there is a window after PIO1 asserts `DATA_CTRL` (step 4 of the PIO program) where the 74HC245 has not yet fully enabled its output:

```
PIO writes pindirs (~null)  → GPIO24-31 driven at 3.3V   [t=0, instant]
PIO side-set DATA_CTRL=LOW  → Q1 base voltage begins falling [t=~0.5 ns]
Q1 collector reaches Vce(sat) → U1 CE/DIR active              [t=~10 ns]
```

During this ~10 ns window, GPIO 24–31 are already driving their output values, but the 74HC245 outputs (B-side, 5 V) are still in tri-state. The Spectrum's /RD strobe may already be active, and the Spectrum CPU will see undefined data on D0–D7.

**Problem B — Short /RD windows on fast Spectrum clones:**

On a standard 3.5 MHz ZX Spectrum, the /RD strobe is active for approximately **140–200 ns** after address setup. The Core 1 polling loop must detect /RD, push a byte to the PIO1 FIFO, and have PIO1 drive the bus, all within this window. At 351 MHz, Core 1 can execute ~3 instructions every nanosecond. The tight polling loop benchmarks to approximately 5–8 loop iterations per ns, giving a worst-case response latency from /RD assertion to `put_dataZ80()` call of:

```
worst case = time for Core 1 to complete its current case handler
           + function call overhead for put_dataZ80
           + PIO FIFO write + PIO startup
```

For a simple IORQ read (no PSRAM access) this is comfortably within the /RD window. However:

- **MREQ read with PSRAM**: When `RdZ80()` accesses a PSRAM-backed page (slot2 or slot3), the QMI controller introduces **~8–25 ns additional latency** per access depending on the page boundary and `COOLDOWN` setting. For MREQ cycles where the address maps to PSRAM, Core 1 must call `put_dataZ80()` within the same /RD window after a PSRAM fetch. On a 7 MHz Spectrum clone or Timex machine with 4 MHz operation, the /RD window may be as short as **80–100 ns**, which is tight.

**Problem C — ROM\_BLK assertion race on DivMMC activation:**

When DivMMC activates mid-sequence, `gpio_put(PIN_ROM_BLK, 0)` asserts the BLK line. This goes through Q4 (BC817) and the 74HC04 buffer, adding another **10–20 ns**. If the Spectrum's ROM simultaneously drives D0–D7 (because it has not yet seen its CE deasserted), there is a brief bus fight between the Spectrum's ROM output buffers (5 V CMOS) and the RP2350B's 3.3 V data via the 74HC245 at the moment of transition.

**Symptoms:**
- **Data bus glitches**: Spectrum reads incorrect bytes from emulated ROM or I/O ports during the transistor propagation window, causing random crashes or incorrect I/O responses.
- **Bus lockup**: On fast Spectrum clones, Core 1 may fail to respond within the /RD window, causing the Spectrum CPU to sample floating data (bus capacitance retains the last valid value) and producing unpredictable behaviour.
- **DivMMC activation artefacts**: First byte fetched after DivMMC activates may be corrupted if the ROM\_BLK transition races against the Spectrum ROM's output drive.

**Recommended Fixes:**

1. **Replace BC817 with a faster MOSFET or dedicated logic-level translator** (e.g. SN74AHCT1G125 single-gate buffer with sub-1 ns propagation). This eliminates the transistor charge-storage delay in the DATA\_CTRL path.
2. **Add a short PIO delay before enabling pindirs**: Insert a `nop` or `nop [1]` between the `out pins, 8` and `mov pindirs, ~null` instructions. This gives the transistor and 74HC245 time to fully enable before data appears on the B-side, at the cost of 2–3 extra ns. The current PIO program already has commented-out `nop` instructions suggesting this was considered:
   ```
   // 0xa442, // 7: nop  side 0 [4]
   ```
3. **Validate PSRAM timing at 7 MHz clock**: For compatibility with Timex/Pentagon 7 MHz clones, profile the worst-case PSRAM access latency and ensure the total Core 1 response time (including PSRAM) stays below 100 ns.
4. **Prevent Spectrum ROM bus-fight on DivMMC**: Assert ROM\_BLK one full CPU cycle before the DivMMC window is expected to activate. The current M1-trap logic can pre-assert BLK at the fetch of `0x3CFF` (one address before the `0x3D00` trap point) to guarantee the Spectrum ROM is silent before DivMMC's data arrives.

---

### 3. I2S Audio "Held Sample" Artifact — Not a Traditional Underrun but a Functional Equivalent

**Problem:**

The I2S DMA system uses a **single static `uint32_t i2s_data`** variable as its read source (no ring buffer). The DMA is configured with `channel_config_set_read_increment(&cfg_dma, false)`, meaning it reads from the same address (`&i2s_data`) on every transfer. The PIO2 DREQ signal paces these transfers at exactly **44,100 Hz**.

Core 0 calls `mix_sound_out()` → `i2s_out(L, R)` at **≈111,111 Hz** (every 9 µs), which is 2.52× faster than the DAC consumption rate. This means:

- Between any two DMA reads, Core 0 updates `i2s_data` approximately **2–3 times**.
- Intermediate computed samples are **overwritten before they reach the DAC**, making the effective audio resolution ~44,100 Hz despite the 111 kHz synthesis rate.
- This is intentional and benign under normal conditions.

However, if Core 0 is **delayed for more than ~22 µs** (one DAC sample period at 44,100 Hz), the DMA will re-read the previous `i2s_data` value and clock it into the TDA1387T again — producing a held sample artefact. Sources of such delays on Core 0:

1. **Long `ExecZ80()` bursts**: The call `ExecZ80(&cpu, dt * 12)` is made with `dt` = remaining µs before the next audio tick. If `dt` is large (e.g. after a system start-up or a long idle gap), this executes hundreds of Z80 cycles in one shot. Each Z80 instruction that touches PSRAM adds ~8–25 ns, and a long burst with many PSRAM accesses (e.g. during GS audio DMA playback) could briefly extend the total `ExecZ80` runtime beyond a safe threshold.
2. **Watchdog update path**: `watchdog_update()` is called inside the Core 0 loop, but not inside `ExecZ80()`. If `ExecZ80()` runs for > 500 ms (the watchdog timeout), the watchdog fires — resetting the entire system. This is unlikely but theoretically possible with a pathological GS ROM.
3. **check\_div\_status() and SD jumper detection**: This function may call `reset_spectrum()`, which executes `busy_wait_ms(10–50 = 500 ms)`. During this time, Core 0 is blocked, and the I2S DMA simply repeats the last sample for 500 ms — producing an audible "stuck note".

**Symptoms:**
- Brief tonal repetitions or "stutter" during heavy GS ROM PSRAM access (e.g. loading a large sample bank).
- A loud **sustained tone** for 500 ms whenever the DivMMC/ZC-SD jumper is changed (the `reset_spectrum()` call).
- Potential watchdog reset during GS ROM-heavy workloads.

**Recommended Fixes:**

1. **Replace the single `i2s_data` word with a small ring buffer** (e.g. 8 or 16 stereo samples). The DMA would use a read-incrementing channel that wraps at the buffer end, and Core 0 would write into the head. This decouples the synthesis rate from the DMA rate and eliminates held-sample artefacts for delays up to `buffer_size / 44100` seconds.

   Example DMA configuration change:
   ```c
   // Replace:
   channel_config_set_read_increment(&cfg_dma, false);
   // With:
   channel_config_set_read_increment(&cfg_dma, true);
   channel_config_set_ring(&cfg_dma, false, 5);  // 2^5 = 32 byte = 8-sample ring
   ```

2. **Cap `ExecZ80()` burst length**: Add an upper bound to `dt` before the `ExecZ80` call to prevent unexpectedly long single bursts:
   ```c
   int32_t dt = int_tick - tick_time;
   if (dt > 100) dt = 100;  // cap to 100 µs = 1200 Z80 cycles max per burst
   ExecZ80(&cpu, dt * 12);
   ```

3. **Call `watchdog_update()` inside `ExecZ80()` at regular intervals** by implementing a step-count check inside `LoopZ80()` (which is called by the Z80 core between instructions). Currently `LoopZ80()` simply returns `INT_NONE`; it could call `watchdog_update()` every N cycles.

4. **Mute audio during `reset_spectrum()`**: Before the `busy_wait_ms` calls, zero out `i2s_data` or write silence:
   ```c
   static void reset_spectrum() {
       i2s_out(0, 0);  // write silence before blocking
       ROM_UNLOCK;
       ...
   }
   ```

---

### 4. Shared Volatile Variables — Missing Memory Barriers Between Cores

**Problem:**

Several key variables are shared between Core 0 (GS Z80 callbacks, audio mixer) and Core 1 (bus polling loop) without explicit memory barrier instructions:

```c
volatile uint8_t GSDAT, GSCOM, GSSTAT, ZXDATWR;
static sound_LR snd_out_GS, snd_out_TS;  // NOT volatile
```

On a **single-core** processor, `volatile` alone is sufficient — it prevents the compiler from caching the value in a register. On the **dual-core Cortex-M33** in the RP2350, each core has its own pipeline, and **write-buffering** can delay the visibility of a write by Core 1 to Core 0 and vice versa. Although ARM guarantees that single-byte aligned writes are atomic, the Cortex-M33 (unlike the older M0/M0+) includes a **write buffer** that can reorder stores. Without a `__DMB()` (Data Memory Barrier) instruction, a sequence of writes to shared variables may be observed by the other core in a different order.

Concretely:

```c
// Core 1 writes:
GSDAT = d8;         // store 1
GSSTAT |= DataBIT;  // store 2 (read-modify-write on volatile uint8_t)

// Core 0 reads:
if (GSSTAT & DataBIT)  // may observe DataBIT set before new GSDAT value
    process(GSDAT);    // reads stale GSDAT
```

The `GSSTAT |= DataBIT` line is a read-modify-write that compiles to a LOAD + ORR + STORE sequence. The STORE of `GSSTAT` could be observed by Core 0 before the STORE of `GSDAT` if the write buffer drains non-sequentially.

Additionally, `snd_out_GS` and `snd_out_TS` (`sound_LR`, two `int16_t`) are **not declared `volatile`**. Core 0 writes them; Core 1 does not read them — but if the compiler ever decides to optimise one or both writes into a register across the tick boundary, the audio mixer may read stale values.

**Symptoms:**
- Rare GS communication glitches: the GS Z80 sees `GSSTAT.DataBIT` set but reads `GSDAT = 0x00` (old value), causing it to process a null data byte instead of the real command.
- Very rarely, corrupted `GSCOM`/`GSDAT` pairs causing the GS firmware to stall or play incorrect audio.
- These bugs are **non-deterministic** and may not reproduce on a scope — they manifest as occasional audio glitches or GS lockups under heavy use.

**Recommended Fixes:**

1. **Add `__DMB()` after writes to shared flag-plus-data pairs:**
   ```c
   GSDAT = d8;
   __DMB();            // ensure GSDAT store is visible before GSSTAT update
   GSSTAT |= DataBIT;
   ```
   And symmetrically on Core 0:
   ```c
   uint8_t dat = GSDAT;
   __DMB();
   GSSTAT &= ~DataBIT;
   ```

2. **Use the Pico SDK `__sev()`/`__wfe()` primitives or `multicore_fifo_push_blocking()`** for the command/data mailbox path. For low-frequency command passing (GS commands arrive at human-interaction rates) the FIFO overhead is negligible and correctness is guaranteed.

3. **Declare `snd_out_GS` and `snd_out_TS` `volatile`** to prevent the compiler from deferring their writes across the tick boundary.

---

### 5. Dual SD Connector Wiring — Silent Bus Contention

**Problem:**

J7 (6-pin header) and J\_SD1 (micro-SD socket) are wired in **parallel** to the same SPI1 bus lines (GPIO 40/42/43) and to the same CS0 line (GPIO 41). There is no second chip-select to individually gate the two cards. The schematic provides no hardware protection against inserting cards into both connectors simultaneously.

If **two SD cards are inserted at the same time**:

- Both cards will have their MISO output enabled simultaneously on their MISO pin. Standard SD cards do not tri-state their MISO when CS is deasserted (this is done in SPI mode only if CMD0 has been properly sent with the individual CS). At power-on, before card initialisation, both cards may simultaneously drive conflicting logic levels onto GPIO 40 (SD\_MISO), causing a short-circuit bus contention on the 3.3 V MISO line.
- GPIO 40 is protected only by the SPI1 peripheral's internal clamp diodes and no series resistor. Sustained bus contention can damage the GPIO pad.

**Symptoms:**
- SD card initialisation failure or ESXDOS boot failure when two cards are present.
- GPIO 40 latching up to a mid-rail voltage (~1.6 V), causing SPI data to appear as `0xFF` or `0x00` on all reads.
- In extreme cases, GPIO 40 pad damage causing permanent SPI1 RX failure.

**Recommended Fixes:**

1. **Add a series 33–47 Ω resistor** on each SD card's MISO line before they join the common net. This limits the short-circuit current to ~100 mA even in the worst case and prevents GPIO damage. The resistors also improve signal integrity by damping reflections on the MISO net.

2. **Use separate CS lines for each connector** (dedicate one free GPIO, e.g. GPIO 31, to J7 CS). Update the firmware to use the correct CS depending on which card is active, and document that only one card should be populated.

3. **Add a hardware OR gate or diode-AND on MISO** (two 1N4148 diodes with cathodes to a common pull-up) to prevent a non-selected card from driving the MISO line.

---

### 6. PSRAM Clock Rate Miscalculation in `psram_init()`

**Problem:**

The `psram_init()` function computes the QMI divisor using a **hardcoded clock constant of 400 MHz** rather than calling `clock_get_hz(clk_sys)` to query the actual running frequency:

```c
const int clock_hz = 400000000;  // HARDCODED — does not read actual sys clock
int divisor = (clock_hz + max_psram_freq - 1) / max_psram_freq;
// With clock_hz=400M, max_psram_freq=166M: divisor = ceil(400/166) = 3
// Actual sys clock = 351 MHz → 351/3 = 117 MHz → within spec (OK in this case)
```

At the current 351 MHz system clock with divisor = 3, the PSRAM clock is 117 MHz — well within the APS6404L's 133 MHz (max at 3.3 V) or 166 MHz (max at VDD = 3.6 V) specification. However:

- If `CPU_FREQ` is changed in `config.h` to a value between 332 MHz (where divisor = 3 still gives 110 MHz, safe) and 498 MHz (where divisor = 3 gives 166 MHz, marginal), the hardcoded 400 MHz assumption means the firmware will **underestimate the divisor** for clocks > 400 MHz, potentially setting the PSRAM clock above its rated frequency.
- `rxdelay` is also computed from `clock_hz = 400 MHz`, not the actual clock. If actual clock > 400 MHz, the read-data capture window could be too late, causing QMI read errors that manifest as random memory corruption in the PSRAM-backed GS RAM banks.

**Symptoms:**
- Random GS Z80 crashes or audio glitches when changing `CPU_FREQ` above the current 351 MHz value.
- Data corruption in `RAM_BANK1[n]`/`RAM_BANK2[n]` arrays manifesting as incorrect audio playback or GS firmware hangs.
- The bug is **latent** at 351 MHz but will activate if the overclock is pushed higher.

**Recommended Fix:**

Replace the hardcoded constant with a `clock_get_hz()` call. The comment in the existing code even acknowledges this:

```c
// Before (buggy):
const int clock_hz = 400000000;

// After (correct):
const int clock_hz = clock_get_hz(clk_sys);
```

This is a **one-line fix** with no side effects at the current 351 MHz operating point.

---

### 7. GS ROM Address Range Gap — Unreachable Addresses Return 0

**Problem:**

In `RdZ80()`, the memory map has a gap in the coverage logic. Examining the conditions:

```c
if (address < 0x4000)  → ROM (slot0)
if (address > 0x3fff && address < 0x8000)  → slot1 (fixed SRAM) or channel capture
if (address > 0x7fff && slot23_is_ram != 1) → ROM (slot0 masked to 0x7FFF)
else if (address > 0x7fff && address < 0xc000) → slot2
else if (address > 0xbfff && address <= 0xffff) → slot3
return 0;  → DEFAULT
```

The condition `address > 0x3fff && address < 0x8000` covers `0x4000–0x7FFF`. But the exact value `address == 0x3FFF` is not covered by the first branch (`address < 0x4000` is `address <= 0x3FFE` semantically but C comparison is strict `<`, so `0x3FFF < 0x4000` is TRUE — this case IS covered). Similarly `0x7FFF < 0x8000` is TRUE so `0x7FFF` is covered by the slot1 branch.

However, `address == 0x8000` falls into the `address > 0x7FFF` branch — covered. But `address == 0xFFFF && slot23_is_ram == 1`:

The condition is `address > 0xBFFF && address <= 0xFFFF`. For `address = 0xFFFF` (a `word` = `uint16_t`): `0xFFFF <= 0xFFFF` is TRUE. This is covered.

The real gap: if `slot23_is_ram == 0` and `address > 0x7FFF`, the code falls through to `return 0` instead of returning the GS ROM at `address & 0x7FFF`. The correct path is:

```c
if ((address > 0x7fff) && (slot23_is_ram != 1)) {
    return gs105b_rom[address & 0x7fff];  // → correct
}
```

This *is* implemented — but only because the `else if` chain is broken by the early `return` inside the first `if`. Reading carefully: the `else if` for `slot23_is_ram != 1` returns a ROM value. The logic is correct but fragile — the `else if` chain depends on the exact structure, and a future refactor that reorders the conditions could silently introduce a gap returning `0x00` for addresses `0x8000–0xFFFF` when banking is active.

A deeper issue: when `slot23_is_ram == 0` (ROM mode) AND `address` is in `0x8000–0xFFFF`, the code correctly returns `gs105b_rom[address & 0x7FFF]`. The GS ROM is **only 32 KB** (`gs105b_rom[]` in `gs105b.rom.h`). Mirroring it across `0x8000–0xFFFF` is the correct GS hardware behaviour. But if the ROM header ever changes size or alignment, this silent mirroring could produce wrong data.

**Symptoms:**
- If a future maintainer adds an `else` clause or reorders the `RdZ80` conditions, the GS Z80 may execute `0x00` (NOP) instructions from unmapped areas instead of ROM, causing it to spin in a NOP sled and halt audio playback.

**Recommended Fix:**

Refactor `RdZ80()` to use an explicit switch-case or range table rather than a chain of `if`/`else if` with overlapping conditions:

```c
byte RdZ80(word addr) {
    if (addr < 0x4000)
        return (slot0_is_ram) ? ram_slot0[addr] : gs105b_rom[addr];
    if (addr < 0x8000)
        return ram_slot1[addr & 0x3FFF];   // also triggers channel capture side-effect
    if (!slot23_is_ram)
        return gs105b_rom[addr & 0x7FFF];  // ROM mirror in upper 32K
    if (addr < 0xC000)
        return ram_slot2[addr & 0x3FFF];
    return ram_slot3[addr & 0x3FFF];
}
```

This eliminates all overlapping conditions and makes the default return explicit for every address.

---

### Summary Table

| # | Issue | Severity | Affected Feature | Fix Complexity |
|---|-------|----------|-----------------|----------------|
| 1 | PCF8575 I2C joystick latency (NES mode) | Medium | Kempston joystick | Medium — add PIO bit-bang path |
| 2 | Transistor delay in DATA\_CTRL path | Medium | All bus read cycles | Low — add PIO `nop [1]` delay |
| 2b | ROM\_BLK race on DivMMC activation | Low–Medium | DivMMC first-byte | Medium — pre-assert BLK one cycle earlier |
| 3 | I2S held-sample artefact during Core 0 stall | Low | All audio output | Medium — add small ring buffer |
| 3b | 500 ms audio block during `reset_spectrum()` | Low | All audio output | Trivial — write silence before `busy_wait_ms` |
| 4 | Missing memory barriers on cross-core shared state | Low–Medium | GS communication | Low — add `__DMB()` before flag writes |
| 5 | Dual SD connector MISO bus contention | Medium | SD/DivMMC | Low HW — add 33 Ω series resistors |
| 6 | Hardcoded 400 MHz constant in `psram_init()` | Latent/Low | GS PSRAM (at higher CPU clocks) | Trivial — one-line fix |
| 7 | Fragile `RdZ80()` address decode chain | Maintenance | GS Z80 memory map | Low — refactor to sequential `if` chain |
