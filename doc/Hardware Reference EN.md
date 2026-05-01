# PiCard Hardware Architecture — Technical Introduction

---

## Hardware Architecture Overview

PiCard V3 is a ZX Spectrum **NemoBus-compatible extension card** whose entire active logic is implemented inside a single **WeAct Studio RP2350B** module. Where a conventional clone card would contain dozens of discrete TTL/CMOS ICs, PLDs, or FPGAs to emulate the legacy peripheral chipset, PiCard replaces all of them with the RP2350's programmable GPIO fabric, three PIO state machines, two DMA subsystems, and a software Z80 core running on Core 0.

The board has two well-separated electrical domains:

| Domain | Voltage | Devices |
|--------|---------|---------|
| **ZX NemoBus / Spectrum bus** | +5 V CMOS | Bus edge connectors (J1/X1A, J2/X1B), 74HC245 (U1), 74HC04 (U2), BC817 transistors (Q1–Q4) |
| **RP2350B digital logic** | +3.3 V CMOS | WeAct RP2350B module, PCF8575 (U3), TDA1387T (U4), UART, SD card (SPI1) |

A local **AMS1117-3.3 LDO regulator (U6)** derives the +3.3 V rail from the +5 V supply drawn from the Spectrum edge connector. C3, C10, C19 (10 µF each) provide local decoupling on the +3.3 V rail; C20 (220 µF) is the bulk hold-up capacitor on the +5 V rail. Three solder jumpers (JP_5V0/JP_5V1/JP_5V2) allow independently connecting or bridging the +5 V power zones, giving flexibility for bench testing without a live Spectrum.

The RP2350B's **48 GPIOs** are carved into five functional groups:

1. **ZX bus signals** — GPIO 1–7 (control), GPIO 8–23 (A0–A15), GPIO 24–31 (D0–D7)
2. **Audio I2S output** — GPIO 44 (DATA), GPIO 45 (BCLK), GPIO 46 (LRCK)
3. **SD card SPI** — GPIO 40–43
4. **Joystick + UART** — GPIO 32–37 (shared I2C/UART/DIV\_OFF)
5. **PS/2 keyboard** — GPIO 38–39

---

## Key Component Breakdown

### WeAct Studio RP2350B Module

The module integrates the Raspberry Pi RP2350 MCU and an **8 MB PSRAM** (APS6404L) on the same board. The PSRAM is connected directly to the RP2350's **QMI (QSPI Memory Interface)** controller and is memory-mapped at `0x11000000` via XIP CS1 (`SFE_RP2350_XIP_CSI_PIN = GPIO 0`). The GS card emulation uses this PSRAM to hold up to 62 banks × 16 KB = **992 KB** of emulated GS RAM (`RAM_BANK1`/`RAM_BANK2` arrays declared with the `PSRAM` linker attribute), far exceeding what any RP2040-based board could offer.

The MCU is over-clocked to **351 MHz** via `vreg_set_voltage(VREG_VOLTAGE_1_60)` + `set_sys_clock_hz(351×10⁶, 1)`. This headroom is necessary to simultaneously run a software Z80 at the equivalent of its original clock speed, synthesise dual AY audio every 9 µs, and service the 3.5 MHz ZX bus in a tight polling loop.

**Complete GPIO allocation:**

| GPIO | Net name (schematic) | Firmware symbol (`config.h`) | Function |
|------|----------------------|------------------------------|---------|
| 0 | XIP\_CS1 | `SFE_RP2350_XIP_CSI_PIN` | PSRAM chip-select (QMI) |
| 1 | BLK | `PIN_ROM_BLK` | Spectrum ROM block (drives external buffer via Q4) |
| 2 | RDR/ | `PIN_DATA_CTRL` | 74HC245 direction/enable (PIO side-set) |
| 3 | \*M1/ | `PIN_M1` | Z80 /M1 — opcode fetch detect |
| 4 | \*WR/ | `PIN_WR` | Z80 /WR |
| 5 | \*RD/ | `PIN_RD` | Z80 /RD (PIO JMP pin for bus SM) |
| 6 | \*IORQ/ | `PIN_IORQ` | Z80 /IORQ |
| 7 | \*MREQ/ | `PIN_MREQ` | Z80 /MREQ |
| 8–23 | \*A0–\*A15 | `PIN_A0` (base) | 16-bit Z80 address bus (inputs) |
| 24–31 | \*D0–\*D7 | `PIN_D0` (base) | 8-bit Z80 data bus (bidirectional via U1) |
| 32 | SDA | `PIN_I2C_SDA` | I2C0 SDA → PCF8575 (joystick) |
| 33 | SCL | `PIN_I2C_SCL` | I2C0 SCL → PCF8575 |
| 34 | UART\_CTS / DIV\_OFF | `PIN_UART_CTS` / `DIV_OFF_PIN` | UART1 CTS or DivMMC/ZC-SD select jumper |
| 35 | \*NMI/ | `PIN_NMI` | Z80 /NMI — open-drain via transistor |
| 36 | UART\_TX / PS2\_CLK2 | `PIN_UART_TX` | UART1 TX (ZX UNO protocol) |
| 37 | UART\_RX / PS2\_DATA2 | `PIN_UART_RX` | UART1 RX |
| 38 | PS2\_CLK1 | `PIN_PS2_CLK` | PS/2 keyboard clock (PIO0 JMP pin) |
| 39 | PS2\_DATA1 | `PIN_PS2_DATA` | PS/2 keyboard data (PIO0 IN pin) |
| 40 | SD\_MISO | `SD_SPI_RX_PIN` | SPI1 RX from micro-SD |
| 41 | SD\_CS0 | `SD_SPI_CS0_PIN` | SPI1 CS0 for micro-SD |
| 42 | SD\_CLK | `SD_SPI_CLK_PIN` | SPI1 CLK |
| 43 | SD\_MOSI | `SD_SPI_TX_PIN` | SPI1 TX to micro-SD |
| 44 | I2S\_DATA (I2S\_R) | `I2S_DATA_PIN` | PIO2 I2S serial data → TDA1387T |
| 45 | I2S\_CLK (BCLK) | `I2S_CLK_BASE_PIN` | PIO2 I2S BCLK → TDA1387T SCLK |
| 46 | I2S\_LRCK | `I2S_CLK_BASE_PIN+1` | PIO2 I2S LRCK → TDA1387T LRCK |
| 47 | \*RST/ | `PIN_RESET` | Spectrum /RESET (active-low output) |

> **Note on PIO assignment:**  
> `pio0` — PS/2 keyboard receiver  
> `pio1` — ZX bus data driver (`PIO_data_z80`)  
> `pio2` — I2S audio output (`PIO_I2S`)  
> Because GPIOs 44–46 are in the upper bank (≥32), the I2S PIO uses `pio_set_gpio_base(pio, 16)` to shift its GPIO window up by 16, a feature unique to RP2350.

---

### TDA1387T I2S DAC (U4)

The **TDA1387T** (Philips/NXP) is a complete 16-bit stereo I2S DAC in an SO-8 package. It accepts a standard Philips I2S serial stream (SCLK, LRCK, DATA) and produces two filtered analog voltages on A\_LEFT (pin 6) and A\_RIGHT (pin 8).

**I2S PIO state machine (`audio_i2s_program`):**

The 8-instruction PIO program in `src/i2s/i2s.c` implements the I2S protocol completely in hardware, using the **side-set** mechanism to toggle BCLK and LRCK simultaneously with each data bit:

```
side-set bits[1:0]: { LRCK, BCLK }

set  x, 14          side 3  ; BCLK=1 LRCK=1, set 15-bit counter
out  pins, 1        side 2  ; MSB of left sample, BCLK=1 LRCK=0
jmp  x--, prev      side 3  ; 14 more left bits...
out  pins, 1        side 0  ; last left bit, BCLK=0 LRCK=0
set  x, 14          side 1  ; BCLK=0 LRCK=1
out  pins, 1        side 0  ; MSB of right sample...
jmp  x--, prev      side 1  ; 14 more right bits...
(wrap)
```

The shift register is configured for **32-bit auto-pull, MSB first** (`sm_config_set_out_shift(&c, false, true, 32)`), so one `i2s_out(L, R)` call packs both 16-bit samples into a single 32-bit word and writes it to the PIO TX FIFO.

The PIO clock divider is computed as:
```c
uint32_t divider = clock_get_hz(clk_sys) * 4 / (44100 * 8);
```
This generates exactly 44,100 LRCK edges per second regardless of the 351 MHz CPU overclock.

**DMA chaining for zero-CPU audio output:**  
`i2s_init()` sets up two chained DMA channels. The primary channel (`dma_i2s`) repeatedly copies the single `i2s_data` word into the PIO TX FIFO, using the PIO DREQ signal to pace transfers. The secondary channel (`dma_i2s_ctrl`) reloads the primary channel's transfer count on completion, creating an infinite 1024-word burst loop. The net result is that `i2s_out()` only needs to update a single `uint32_t` variable; hardware does the rest.

**TDA1387T filter pin:**  
The FLT pin (pin 7) connects to an external RC network (C15 = 0.1 µF and associated passives) that programmes the internal 4th-order reconstruction filter cutoff frequency, suppressing I2S quantisation images above the audio band.

---

### PCF8575DBR I2C I/O Expander (U3)

The **PCF8575** provides 16 quasi-bidirectional I/O pins (P0–P7 and P10–P17) on a single I2C slave at address `0x20` (pins A0=A1=A2=GND). It is polled continuously by Core 0 at 400 kHz via I2C0 (GPIO 32/33).

**Joystick connectivity:**  
Port pins P0–P8 connect to J10 (JOY-1, the 9-pin DE-9 Atari/Kempston-standard connector) through 10 kΩ pull-up resistors (R20–R28). With no joystick connected all lines float high, giving an idle state of `0xFF`. When a direction or fire button is pressed, the corresponding P-pin is pulled to GND by the joystick's internal switch.

The port is expanded to J9 (a secondary connector bank) for a possible second joystick on P10–P17 with the same pull-up arrangement.

**Non-blocking I2C transfer:**  
The firmware function `i2c_transfer_non_blocking()` (`src/i2c/i2c_joy.cpp`) bypasses the SDK's blocking `i2c_write_blocking` / `i2c_read_blocking` API. Instead, it:
1. Reads the previous result directly from the I2C hardware RX FIFO.
2. Loads the new read command into the TX FIFO with `I2C_IC_DATA_CMD_CMD_BITS | STOP`.

This ping-pong approach finishes in a few CPU cycles and returns immediately, so Core 0's 9 µs audio tick is never stalled waiting for an I2C transaction.

**Button-to-Kempston conversion:**  
`joy_proc()` calls `i2c_transfer_non_blocking()` each tick and converts the raw 16-bit PCF8575 reading to Kempston format using a precomputed 256-entry lookup table (`nes_to_kempston_convert[]` or `sega_to_kempston_convert[]`). The Kempston byte is stored in `joy_state` and served to the Spectrum on I/O port `0x1F` reads by Core 1.

**Status LED:**  
D6 (LED) with R46/R47 (10 kΩ each) is connected to the PCF8575 `~INT` pin, giving a visual indication of I2C activity.

---

### 74HC245 Bus Transceiver (U1) and 74HC04 Inverters (U2)

#### 74HC245 — Bidirectional Data Bus Buffer

U1 is the critical interface between the **5 V ZX Spectrum data bus** (B-side pins B0–B7, connected to the NemoBus edge connector) and the **3.3 V RP2350B data lines** (A-side pins A0–A7, connected to GPIO 24–31). It runs from the +5 V rail.

| 74HC245 pin | Net | Signal |
|-------------|-----|--------|
| 1 (DIR) | RDR/ | Direction control — driven by transistor Q1 from GPIO 2 |
| 19 (CE/) | GND / transistor collector | Chip enable (active-low) |
| 2–9 (A0–A7) | GPIO 24–31 | 3.3 V side (RP2350B) |
| 11–18 (B0–B7) | \*D0–\*D7 | 5 V NemoBus data bus |
| 20 (VCC) | +5 V | Supply |

**Direction control from PIO side-set (GPIO 2 = `PIN_DATA_CTRL`):**

The PIO bus driver program (`src/PIO/pio_data_z80_bus.cpp`) dedicates its 1-bit side-set to GPIO 2. The transistor circuitry around Q1 (BC817, R23/R27 base dividers, R29 collector load to +5 V) performs two jobs:

1. **Level translation**: converts the 3.3 V GPIO 2 output to a 5 V logic signal suitable for driving the 74HC245 DIR/CE pins which operate at 5 V CMOS thresholds.
2. **Polarity inversion**: the NPN collector is logically inverted — GPIO 2 HIGH → Q1 saturated → collector pulled LOW.

The PIO program exploits this to implement a "pre-load then enable" atomic sequence:

```
step 0: mov pindirs, null   side 1  → GPIO2=1 → Q1 on → DIR LOW (B→A, Spectrum→RP2350) → GPIO24-31 = inputs
step 1: pull block          side 1  → wait for TX FIFO data
step 2: jmp pin,0           side 1  → abort if /RD not asserted (no read cycle active)
step 3: out pins, 8         side 1  → stage output byte into GPIO24-31 output register (still inputs)
step 4: mov pindirs, ~null  side 0  → GPIO2=0 → Q1 off → DIR HIGH (A→B, RP2350→Spectrum) → GPIO24-31 = outputs
step 5: jmp pin,0           side 0  → /RD deasserted? → return to step 0
step 6: jmp 5               side 0  → hold bus driven while /RD still low
```

Steps 3 and 4 are the critical pair: the byte is written to the pin **output register** while the direction is still INPUT, then the direction flips to OUTPUT in a single PIO clock cycle (~2.8 ns at 351 MHz). This eliminates any glitch where an incorrect value would appear on the bus for even one clock.

#### 74HC04 — Hex Inverter (U2)

U2 (74HC04, supply from +5 V) provides multiple single-gate inversions used for:

- **NMI and RESET level shifting**: The 3.3 V GPIO 35 (/NMI) and GPIO 47 (/RESET) outputs are too weak to directly assert 5 V open-collector lines against the Spectrum bus pull-ups. U2E/U2F invert and buffer these signals; diodes D1–D3 (1N4148W) provide clamping and isolation to prevent backfeed.
- **ROM\_BLK routing**: GPIO 1 (BLK) is passed through a 74HC04 gate before driving the external ROM-blocking buffer circuit, ensuring the signal meets 5 V drive requirements.
- **SW1/SW2 debounce**: The NMI pushbutton (SW1) and RESET pushbutton (SW2) feed through two cascaded 74HC04 gates with RC networks (the 10 µF decoupling capacitors at the switch outputs) for hardware debouncing before the signal reaches the RP2350B GPIO or the bus.

**Why 74HC04 rather than a dedicated level shifter?**  
At the 3.5 MHz Spectrum bus speed the propagation delay of a 74HC04 gate (~7 ns typical at 5 V) is negligible. Using a standard logic gate with a known ±0.8 V hysteresis (CMOS threshold) is simpler and cheaper than a bidirectional level-shifter for these unidirectional control signals.

---

### MCP602 Op-Amps (U5A, U5B, U5C)

Two **MCP602** dual rail-to-rail op-amp packages implement the **post-DAC analog conditioning** stage between the TDA1387T analog outputs and the PJ325 stereo headphone jack (J5).

**Signal path — right channel (U5A):**

```
TDA1387T A_RIGHT (pin 8)
    → R39 / R40 (10 kΩ)       AC-coupling / input impedance set
    → C12 (10 µF, electrolytic) DC blocking capacitor (HPF: fc ≈ 1.6 Hz)
    → U5A (+) non-inverting input (pin 3)
    → U5A output (pin 1)
    → R41 (62 kΩ) feedback to U5A (−) input (pin 2)
         → C16 (470 pF)        HF roll-off in feedback: fc = 1/(2π·62k·470p) ≈ 5.5 kHz
    → R36 (620 Ω)              Output series protection resistor (short-circuit / capacitive load)
    → J4 (out_A, R channel)
    → J5 (PJ325, Ring — right)
```

**Signal path — left channel (U5B):**

```
TDA1387T A_LEFT (pin 6)
    → R40 / C11 (10 µF)        same DC-blocking stage
    → U5B (+) input (pin 5)
    → U5B output (pin 7)
    → R42 (62 kΩ) → C17 (470 pF)  same HF filter
    → R35 (620 Ω)
    → J3 (in_A, L channel)
    → J5 (PJ325, Tip — left)
```

**U5C** is the power supply terminal of the second MCP602 package (V− = pin 4, V+ = pin 8), decoupled by C9 (10 µF) and C4 (0.1 µF). No signal amplification occurs here.

**Gain calculation:**  
In this non-inverting configuration with equal feedback and ground resistors, the closed-loop gain is:

$$
A_v = 1 + \frac{R_{fb}}{R_{in}}
$$

With R41 = R42 = 62 kΩ as the feedback resistor and the TDA1387T internal output impedance (~1 kΩ typical) as the effective `R_in`, the gain is approximately unity (≈1 buffer) — the primary function is **impedance transformation** (TDA1387T high-Z output → low-Z headphone drive) and **HF noise suppression** via the 470 pF cap in feedback.

The **5.5 kHz pole** from R41·C16 is the reconstruction filter; since the RP2350B synthesises audio at ~111 kHz (9 µs sample period) decimated to 44.1 kHz at the TDA1387T, images at multiples of 44.1 kHz are already attenuated by the TDA1387T's internal 4th-order reconstruction filter. The op-amp stage adds a further 1st-order roll-off as a safety margin.

---

## ZX Spectrum NemoBus Interfacing

### Edge Connectors

The board carries two **31-position PCB edge connectors** (J1/X1A and J2/X1B) that mate with the ZX Spectrum's expansion port. The connector spans both sides of the PCB (A-side = X1A = J1, B-side = X1B = J2), providing access to the full 31-pin × 2-side NemoBus. Key bus lines from the edge connector:

| NemoBus signal | Connected to |
|----------------|-------------|
| A0–A15 | GPIO 8–23 (via RP1/RP2 resistor network) |
| D0–D7 | 74HC245 U1 B-side (5 V); A-side → GPIO 24–31 |
| /MREQ, /IORQ, /RD, /WR, /M1 | GPIO 7, 6, 5, 4, 3 (via RP1/RP2) |
| /NMI | Q3 BC817 open-collector collector (active-low) |
| /RESET | GPIO 47 via Q2 BC817 (open-collector) |
| IORQGE | J8 (NemoBus auxiliary header) |
| BLK (ROM block) | GPIO 1 via Q4 BC817 → 74HC04 buffer |
| +5 V, GND | AMS1117-3.3 input, bulk caps |

**The J8 auxiliary header (Conn\_02x08\_Odd\_Even)** breaks out the NemoBus-specific control signals that do not have direct GPIO equivalents: `IORQGE`, `CSR/`, `DOS/`, `RDR/`, plus routing for the two PS/2 port pairs and the ZX edge address lines A13, A15, B4, A25.

### 5V → 3.3V Level Shifting Strategy

The RP2350 GPIO pads are rated at **3.3 V maximum** (absolute max 3.63 V). The NemoBus runs at 5 V TTL/CMOS. The PiCard V3 uses a **current-limiting resistor array** approach:

1. **RP1 and RP2** are resistor-array components mounted inline between the edge connector and the RP2350B GPIO pins, providing individual series protection resistors (values labelled "10", meaning 10 kΩ in the schematic context) for every address and control line.

2. When a 5 V NemoBus signal drives a GPIO pin through 10 kΩ, the internal ESD clamp diode of the RP2350 (which conducts when V_pin > V_IOVDD + 0.5 V) sinks the excess current:

$$
I_{clamp} = \frac{5\text{ V} - 3.8\text{ V}}{10\text{ k}\Omega} \approx 0.12\text{ mA}
$$

This is well within the safe clamp current specification for the RP2350 pads.

3. **R1–R8 and R20–R21** (also 10 kΩ) are individual series resistors on specific control signals (/M1, /WR, /RD, /IORQ, /MREQ) for the same purpose.

4. **Data bus (D0–D7):** Protected by the 74HC245 itself — the A-side of U1 is driven by the RP2350B at 3.3 V. When the 74HC245 is in B→A mode (Spectrum reading from RP2350), the RP2350 drives 3.3 V output through the buffer; the 74HC245's output high level in this direction is limited to 3.3 V by the RP2350 driving side. When in A→B mode (RP2350 reading the Spectrum bus), the 74HC245 translates the 5 V Spectrum signals to its 5 V A-side, but the B-side (RP2350) sees the attenuated level through the IC's output stage.

### Critical Control Signals to RP2350B

The bus monitor loop in `ZX_bus_task()` (Core 1) reads **all 48 GPIOs simultaneously** with a single `gpio_get_all()` instruction and extracts the relevant signals via bit-masking:

```c
uint32_t datain32 = gpio_get_all();
uint32_t io_mrq_rd_wr = (~datain32) & (MREQ_Z80 | IORQ_Z80 | RD_Z80 | WR_Z80);
```

Active-low signals are inverted in software so the `switch(io_mrq_rd_wr)` statement directly matches the four possible bus cycle types. The 16-bit address is extracted as:

```c
uint16_t addr16 = (datain32 >> PIN_A0);  // right-shift by 8, gives A0-A15 in bits [0:15]
```

The data byte is re-sampled just before use (`gpio_get_all() >> PIN_D0`) rather than trusting the initial snapshot, guaranteeing the data byte is valid even if it settled slightly after the control lines:

```c
uint8_t d8 = gpio_get_all() >> PIN_D0;
```

### /NMI and /RESET as Open-Collector Outputs

The `/NMI` and `/RESET` lines on the ZX bus must be **open-collector/open-drain** — the RP2350B cannot force 5 V onto these lines and must never fight the Spectrum's internal pull-ups. The firmware implements this correctly:

**NMI (`PIN_NMI = GPIO 35`):**
```c
void NMI_press() {
    gpio_put(PIN_NMI, 0);
    gpio_set_dir(PIN_NMI, GPIO_OUT);   // drive low → assert NMI
    busy_wait_ms(50);
    gpio_set_dir(PIN_NMI, GPIO_IN);    // tri-state → release NMI
}
```
GPIO 35 feeds the base of a BC817 transistor (via a 10 kΩ base resistor R25). The transistor's collector, pulled to +5 V through R30 (300 Ω), drives the NemoBus `/NMI` line. When GPIO 35 outputs LOW, the transistor conducts, pulling `/NMI` to near GND. When GPIO 35 is tri-stated (input), the transistor is off and the bus line floats to +5 V through R30 and the Spectrum's own pull-up.

**ROM\_BLK (`PIN_ROM_BLK = GPIO 1`):**  
Similarly, GPIO 1 controls Q4 (BC817), whose collector, loaded through a 74HC04 buffer stage, drives the active-low **BLK** line. This line enables an external buffer IC that physically blocks the Spectrum's onboard ROM from responding to MREQ cycles, allowing PiCard to substitute its own ROM image (the ESXDOS DivMMC ROM, `ROM_DIVMMC[]`).

---

## Physical I/O and Connectivity

### Audio Output — PJ325 Stereo Jack (J5)

The audio signal path from firmware to headphone socket:

```
Core 0 (every 9 µs):
  mix_sound_out()
    → i2s_out(L, R)
      → packs 32-bit word into i2s_data variable
         → DMA transfers to PIO2 TX FIFO
            → PIO2 state machine clocks bits → GPIO44/45/46
               → TDA1387T (U4) receives I2S stream, decodes to analog
                  → A_LEFT / A_RIGHT pins (±500 mV approx)
                     → C12/C11 (10 µF, DC block), R40/R39 (10 kΩ input)
                        → MCP602 U5A (right), U5B (left) — buffer/filter stage
                           → R36/R35 (620 Ω output protection)
                              → J5 PJ325 stereo 3.5 mm jack (Tip=L, Ring=R, Sleeve=GND)
```

The PJ325 is a standard 3.5 mm TRS jack (J5, labelled "Audio OUT"). R in the schematic adjacent to the jack represents the right channel, L represents the left. The output impedance presented to a headphone is approximately 620 Ω (R36/R35) plus the op-amp's near-zero output impedance — suitable for driving 32 Ω headphones directly.

DC offset at the output is blocked by the electrolytic coupling capacitors C12/C11 (10 µF, rated ≥10 V) on the TDA1387T output.

The +5VA net (TDA1387T pin 5 / VA supply) is filtered separately by C8 (1 µF) and R36 forms a π-filter with C8 to isolate digital noise from the analog supply.

---

### SD Card — Dual Connector Scheme

Two connectors share the **same SPI1 bus** (GPIO 40=MISO, 42=CLK, 43=MOSI) but use the same chip-select line (SD\_CS0 = GPIO 41):

| Connector | Type | Usage |
|-----------|------|-------|
| **J7 (Conn\_SD0)** | 6-pin 2.54 mm pin header | Board-to-board connection; routes SD signals to an off-board connector or a full-size SD socket daughter board |
| **J\_SD1 (Micro\_SD\_Card)** | Micro-SD card slot | Direct on-board micro-SD socket |

J7 carries: SD\_CS0, SD\_MOSI, SD\_CLK, SD\_MISO, +3.3 V, GND — a self-contained 6-wire SPI-SD connection. Both connectors are populated, but because they share a single CS line, **only one SD card should be inserted at a time** in normal operation. The design allows swapping between a soldered-on micro-SD slot and a flying-lead connection to an external slot for different enclosure requirements.

**SPI1 configuration** (from firmware):
```c
spi_init(SD_SPI, SD_SPI_FREQ_CLK);   // 10 MHz
```
Data is transferred via direct register access (`spi_get_hw(SD_SPI)->dr`) rather than SDK DMA, achieving single-byte latency for the DivMMC/ZC-SD port handlers in the Core 1 bus loop.

C7 (47 µF) and C6 (100 µF) provide bulk decoupling for the SD card +3.3 V supply, absorbing the current spikes during SD write operations.

---

### Joystick — J10 (JOY-1, DE-9)

J10 is a **9-pin Atari/Kempston-compatible DE-9 joystick port**. Pin mapping follows the standard Atari convention:

| J10 pin | Signal | PCF8575 pin | Kempston bit |
|---------|--------|-------------|--------------|
| 1 | UP | P0 | bit 3 |
| 2 | DOWN | P1 | bit 2 |
| 3 | LEFT | P2 | bit 1 |
| 4 | RIGHT | P3 | bit 0 |
| 5 | POT Y | — | — |
| 6 | FIRE | P4 | bit 4 |
| 7 | +5V | — | — |
| 8 | GND | — | — |
| 9 | POT X | P5 | — |

Pull-up resistors R20–R28 (10 kΩ each) hold all PCF8575 inputs at logic HIGH when no joystick is connected. A joystick button shorts the corresponding pin to GND.

The firmware **NES mode** (`NES_JOY_EN=1`) supports serial NES-style pads connected via the PCF8575 port using a latch/clock sequence that shifts out 8 button states over 3 GPIO pins (P1=CLK, P2=LATCH, P3=DATA via the PCF8575 port). The result is translated through `nes_to_kempston_convert[256]` and served on Kempston port `0x1F`.

---

### PS/2 Keyboard — J6 (Mini-DIN-6)

J6 is a **6-pin Mini-DIN socket** (PS/2 standard):

| Mini-DIN pin | Signal | GPIO |
|--------------|--------|------|
| 1 | KB DATA1 | 39 (PIN\_PS2\_DATA) |
| 3 | GND | GND |
| 4 | +5 V | +5 V (via R14 1 kΩ current limit) |
| 5 | KB CLK1 | 38 (PIN\_PS2\_CLK) |

R12 and R13 (1 kΩ each) are **series protection resistors** in the CLK and DATA lines between the Mini-DIN connector and the GPIO pins. They limit ESD-discharge current and also limit the current when the 5 V PS/2 device drives a 3.3 V GPIO pin. Combined with the RP2350B's internal ESD clamp diodes (clamping at ≈3.8 V), these keep the GPIO inputs safe:

$$
I_{clamp} = \frac{5\text{ V} - 3.8\text{ V}}{1\text{ k}\Omega} = 1.2\text{ mA}
$$

This is acceptable for brief ESD events; steady-state the PS/2 open-drain lines are pulled high through the keyboard's own ~10 kΩ pull-ups, so steady-state GPIO current is near zero.

The **secondary PS/2 port** (PS2\_CLK2 = GPIO 36, PS2\_DATA2 = GPIO 37) shares the Mini-DIN connector's pin routing via the J8 NemoBus auxiliary header. These pins overlap with UART1 (TX/RX). At compile time, only one function is active: `HW_UART_EN=1` (default) claims GPIO 36/37 for UART1, and a second PS/2 port is not used. Switching to `KB_PS2_EN` on the alternate port would require recompilation.

**PIO0 PS/2 receive state machine:**  
The PS/2 CLK line (GPIO 38) is configured as the PIO JMP pin. The 5-instruction PIO program clocks in 11 bits per frame (start + 8 data + parity + stop) at whatever rate the keyboard supplies them (typically 10–16 kHz). The PIO clock is divided to ≈200 kHz (`fdiv = clock_get_hz(clk_sys) / 200000`) to ensure clean edge sampling. Two chained DMA channels stream received 32-bit PIO FIFO words into a 100-entry ring buffer (`PS_2_BUS[100]`) with **no CPU involvement** — Core 0 only examines the ring buffer header every ~576 µs to decode new scan codes.

---

### Manual Control — SW1 (NMI), SW2 (RESET)

| Button | Function | GPIO path |
|--------|----------|-----------|
| **SW1 (SW\_NMI)** | Forces a Z80 NMI to the Spectrum | SW1 → 74HC04 (U2F) → GPIO 39 then to NMI transistor driver |
| **SW2 (SW\_RST)** | Asserts Spectrum /RESET for 500 ms | SW2 → 74HC04 (U2D/U2E) → GPIO 47 → Q2 → /RESET bus line |

Both buttons connect through 74HC04 inverter stages (which both debounce and provide 5V→3.3V CMOS threshold matching) before reaching the RP2350B GPIO or the bus transistor. R9 and R10 (10 kΩ) provide pull-ups on the switch inputs.

In firmware, the `NMI_press()` function can also be triggered by the **Insert** key on the PS/2 keyboard, and the **Ctrl+Alt+Del** combination calls `reset_spectrum()` which asserts `/RESET` for 500 ms via the GPIO, mirroring the hardware SW2 function exactly.

---

### Status LEDs

| LED | Resistor | Net | Meaning |
|-----|---------|-----|---------|
| D4 | R31 (300 Ω) | +3.3 V\_2 | SD card / DivMMC active |
| D5 | R43 (300 Ω) | +5 V rail | Board power present |
| D6 | R46/R47 (10 kΩ) | PCF8575 ~INT | Joystick activity / I2C interrupt |

---

## Power Distribution Summary

```
ZX Spectrum edge (J1/X1A, J2/X1B)
    +5 V ────────────────────────────────── JP_5V0 / JP_5V1 / JP_5V2 jumpers
                │
                ├── C20 (220 µF bulk) ── GND
                │
                ├── D5 (power LED, R43 300Ω)
                │
                ├── U6 AMS1117-3.3
                │       VI (+5V in)
                │       VO (+3.3V out) ── C19, C3, C10 (10µF each, decoupling)
                │               │
                │               ├── WeAct RP2350B (IOVDD, DVDD)
                │               ├── PCF8575 VCC (pin 24)
                │               ├── SD card VDD (J_SD1 pin 4, J7 pin 5)
                │               └── TDA1387T VA (+5VA after filter R32, C7)
                │
                ├── 74HC245 VCC (pin 20) ── +5 V (bus side logic)
                ├── 74HC04 VCC (pin 14) ── +5 V
                └── J10 JOY-1 pin 7 ── +5 V (joystick power)
```

The AMS1117-3.3 is rated for 1 A output current, well above the combined load of the RP2350B (≈100 mA @ 351 MHz), PCF8575 (~2 mA), and TDA1387T (~5 mA).
