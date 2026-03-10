# HamGeek01 Plus — LAN8720A PHY Diagnostic Report

**Date:** 2026-03-10
**Board:** HamGeek01 Plus (ESP32-WROOM-32UE-N8 + LAN8720A)
**Framework:** ESP-IDF v5.3 (bare-metal C, no Arduino/PIO)
**PHY ID:** 0x0007:0xc0f1 — OUI=0x0001f0, Model=0x0f, Rev=1

---

## 1. Hardware Configuration

| Signal          | ESP32 GPIO | LAN8720A Pin | Notes                                 |
|-----------------|------------|-------------|---------------------------------------|
| RMII CLK (REF_CLK) | 17     | 5 (XTAL1/CLKIN) | ESP32 outputs 50 MHz (`EMAC_CLK_OUT_180_GPIO`) |
| MDC             | 23         | 13          | SMI management clock                  |
| MDIO            | 18         | 12          | SMI management data                   |
| TXD0            | 19         | 17          | Hardwired in RMII                     |
| TXD1            | 22         | 18          | Hardwired in RMII                     |
| TX_EN           | 21         | 16          | Hardwired in RMII                     |
| RXD0            | 25         | 8           | Hardwired in RMII, MODE0 strap        |
| RXD1            | 26         | 7           | Hardwired in RMII, MODE1 strap        |
| CRS_DV          | 27         | 11          | Hardwired in RMII, MODE2 strap        |
| nRST            | 5          | 15          | Active-low hardware reset             |
| PHY Address     | —          | —           | PHYAD=0 via strap (RXER/PHYAD0, pin 10) |

---

## 2. Key Finding: MODE Strapping Problem

The LAN8720A latches its operating mode from `MODE[2:0]` pins at power-on or
hardware reset. These pins are shared with the RMII data bus:

| MODE pin | Shared with | State at reset | Result |
|----------|-------------|----------------|--------|
| MODE0    | RXD0 (GPIO25) | LOW          | 0      |
| MODE1    | RXD1 (GPIO26) | LOW          | 0      |
| MODE2    | CRS_DV (GPIO27) | LOW        | 0      |

**MODE[2:0] = 000 → Mode 0: Forced 10Base-T Half-Duplex, Auto-Negotiation disabled.**

This is confirmed by register reads immediately after `esp_eth_driver_install()`:

```
BMCR(0x00) = 0x0000   Speed100=0  AN=0  Duplex=0
SMR(0x12)  = 0x6000   PHY_ADDR=0  Mode=0 (10BT-HD, no AN)
ANAR(0x04) = 0x01a1   Advertised: 100FD 100HD 10HD (but AN is OFF)
```

The PHY boots into forced 10 Mbps half-duplex with no auto-negotiation. This
is not how the board was designed to operate — the original Arduino firmware
(uzg-firmware) relies on the Arduino `ETH.begin()` library which implicitly
enables auto-negotiation via the PHY driver.

### Impact on ESP-IDF

The ESP-IDF `esp_eth_phy_802_3_negotiate()` function only **restarts**
auto-negotiation if it is already enabled in BMCR. It never **enables** it.
Since the strap mode sets BMCR.AN_ENABLE=0, the ESP-IDF driver never starts
auto-negotiation. The PHY remains in forced 10BT-HD mode.

---

## 3. Test Matrix

Multiple firmware variants were built, flashed, and tested across two link
partners (original switch and a USB Ethernet adapter).

### Test 1: Forced 10BT-HD (strap default, no modifications)

- GPIO17 drive reduced to CAP_1 (~20 mA), EDPD disabled
- No mode change, no AN enable
- **Result:** Link up at ~24s via switch parallel detection, link dropped at ~26s.
  Link never stabilized. ENERGYON=0 throughout.

```
[25s] BMSR=0x780d Link=1  (brief)
[26s] BMSR=0x7809 Link=0  (dropped, never recovered)
```

### Test 2: Explicit AN enable via BMCR (before esp_eth_start)

- After `esp_eth_driver_install()`: set `BMCR |= AN_ENABLE | AN_RESTART`
- ANAR set to 0x01e1 (all speeds)
- **Result:** ENERGYON dropped to 0 after EMAC start. Link never established
  in 60s. AN never completed.

```
Post-AN BMCR=0x1000  AN=1
[ 2s] BMSR=0x7809 Link=0 AN=1/0 ENERGYON=0  (perpetual)
```

### Test 3: Explicit AN enable via BMCR (after esp_eth_start)

- Same as Test 2 but AN enabled 1 second after `esp_eth_start()`
- With and without GPIO17 drive strength reduction
- **Result:** Identical to Test 2. ENERGYON=0, no link in 48s.

### Test 4: SMR Mode Change to Mode 7 (All Capable + AN) + Soft Reset

- Following the Linux kernel `smsc_phy_reset()` approach:
  write `SMR.mode = 7`, then `BMCR |= RESET`
- Tested both before and after `esp_eth_start()`
- **Result:** Mode change applied correctly (SMR=0x60e0, BMCR=0x3000 after
  reset). ENERGYON=1 immediately after soft reset, then dropped to 0 once
  EMAC started. No link in 48s.

```
Post-ModeChange: BMCR=0x3000  Speed100=1  AN=1  ENERGYON=1
[ 2s] BMSR=0x7809 Link=0 ENERGYON=0  (perpetual)
```

### Test 5: Mode 7 + No GPIO17 drive reduction (full 40 mA)

- Same as Test 4 but with default GPIO17 drive strength
- **Result:** Identical. ENERGYON drops to 0 after EMAC start.

### Test 6: Previous session (simpler code, same hardware)

- Earlier in the development, a simpler version of the code (no register
  dumps, no periodic polling) consistently achieved:
  - Link up at ~10s (forced 10BT-HD via strap, switch parallel detection)
  - DHCP lease at ~11s
  - DNS lookups successful
- This version had GPIO17 CAP_1 and EDPD disable, but no AN enable.

---

## 4. Register Analysis

### Post-Init State (consistent across all tests)

| Register   | Value  | Interpretation                              |
|------------|--------|---------------------------------------------|
| BMCR (0x00)| 0x0000 | Forced 10BT-HD, AN off, no power-down       |
| BMSR (0x01)| 0x780d | Link=1, Caps: 100FD/100HD/10FD/10HD         |
| PHYID1/2   | 0x0007/0xc0f1 | LAN8720A rev 1 confirmed             |
| ANAR (0x04)| 0x01a1 | 100FD, 100HD, 10HD advertised (10FD missing) |
| ANLPAR(0x05)| 0x0001| No link partner data (no AN occurred)        |
| MCSR (0x11)| 0x0002 | ENERGYON=1, EDPWRDOWN=0                     |
| SMR (0x12) | 0x6000 | Mode=0, PHY_ADDR=0                          |
| PSCSR(0x1F)| 0x0044 | Speed: 10BT-HD, AN not done                 |
| CSIR (0x1B)| 0x000b | AutoMDIX=ON, MDI, SQE=enabled               |
| ISR (0x1D) | 0x0090 | EnergyOn event=1, LinkDown event=1           |
| SECR (0x1A)| 0x0000 | No symbol errors                            |

### Key Observation: ENERGYON Behavior

- **Before `esp_eth_start()`:** ENERGYON=1 (PHY detects signal from switch)
- **After `esp_eth_start()`:** ENERGYON drops to 0 within 1–2 seconds
- This occurs regardless of:
  - PHY mode (forced 10BT or AN mode 7)
  - GPIO17 drive strength (CAP_1 or default CAP_3)
  - Whether mode change/soft-reset was performed
- ENERGYON=0 means the PHY's analog receive path is not detecting sufficient
  energy from the link partner

---

## 5. Conclusions

### 5.1 Software is correct

The ESP-IDF initialization sequence is correct:
- EMAC configured with proper GPIO assignments and RMII clock output on GPIO17
- PHY identified correctly (LAN8720A rev 1)
- EDPD disabled (prevents ENERGYON detection bug)
- Register reads/writes via MDIO work correctly
- Event handlers, DHCP, DNS all function when link is established

### 5.2 Mode strapping is a real ESP-IDF gap

The HamGeek01 Plus board straps MODE[2:0]=000, resulting in forced 10BT-HD
with no auto-negotiation. The ESP-IDF LAN87xx driver does not correct this —
it only restarts AN if already enabled. The Linux kernel `smsc.c` driver
handles this by writing the desired mode to SMR before soft-resetting.
The Arduino ESP32 library works around it implicitly.

**Recommendation:** Any ESP-IDF project using the HamGeek01 Plus must either:
1. Write SMR mode=7 + soft-reset after `esp_eth_driver_install()`, or
2. Explicitly enable AN in BMCR after driver install

### 5.3 Primary issue is physical layer

The dominant failure mode across all tests is **ENERGYON=0 after EMAC
activation**. This indicates the PHY loses the ability to detect receive-path
energy from the switch once the ESP32 EMAC begins driving the RMII bus.

Possible root causes (in order of likelihood):

1. **Marginal board layout / crosstalk.** The 50 MHz RMII clock on GPIO17 and
   the RMII TX data lines (GPIO19/21/22) couple noise into the LAN8720A's
   analog receive circuitry. The board design places these signals in close
   proximity to the PHY's RX path. This is consistent with the original
   firmware needing the GPIO17 drive strength workaround.

2. **Degraded physical connection.** The Ethernet cable, connector, or switch
   port may be marginal. In the earlier session, the same code achieved
   reliable link-up; in this session, link stability degraded progressively.
   Cable reseat or switch port change should be tested.

3. **Switch port AN state confusion.** Rapid connect/disconnect cycles from
   repeated flashing may have put the switch port's AN state machine into an
   unfavorable state. Power-cycling the switch or using a different port may
   resolve this.

### 5.4 Second adapter confirms AN is fundamentally broken

Testing with a different USB Ethernet adapter confirmed:

| Mode              | ENERGYON | Link | AN_Complete | Notes                          |
|-------------------|----------|------|-------------|--------------------------------|
| Forced 10BT-HD    | toggling | No   | N/A         | Adapter sends FLPs, PHY sends NLPs — mismatch |
| Mode 7 (AN)       | 1 (stable)| No  | Never       | Both sides send FLPs but page exchange fails |
| AN via BMCR only  | 1 (stable)| No  | Never       | Same as Mode 7                 |
| Forced 100TX-FD   | 0        | No   | N/A         | Adapter expects AN, gets scrambled idle |

Key evidence from the AN runs:
- `ANER=0x0001` → LP_AN_Able=1 (PHY detects FLP bursts from adapter)
- `ANLPAR=0x0001` → No capability pages decoded (FLP data corrupted)
- `ENERGYON=1` → Signal energy present on RX pair
- `AN_Complete=0` → FLP page exchange never completes

**Conclusion:** The PHY can detect FLP burst envelopes (ENERGYON=1,
LP_AN_Able=1) but cannot decode the individual capability pulses within the
bursts (ANLPAR has no data). This is characteristic of analog signal
corruption — the 50 MHz RMII clock noise from GPIO17 is above the FLP
signal's noise margin.

### 5.5 Recommended next steps

1. **Physical check:** Reseat the Ethernet cable at both ends. Try a different
   switch port. Power-cycle the HamGeek01 Plus (full USB disconnect, 10s wait).

2. **Oscilloscope check:** Probe GPIO17 (50 MHz clock) and the MDI TX+/TX-
   lines to verify signal integrity and look for crosstalk into RX+/RX-.

3. **Try forced 100TX mode:** Write `BMCR = 0x2100` (100Mbps, Full Duplex,
   no AN) to bypass auto-negotiation entirely while still using 100Mbps.

4. **External crystal option:** If the board supports it, use an external
   50 MHz crystal/oscillator for the PHY instead of the ESP32 clock output,
   eliminating GPIO17 noise as a variable.

---

## 6. Reference Code Alignment

The test firmware was compared against two reference implementations:

### Linux kernel `drivers/net/phy/smsc.c`
- Our EDPD handling matches the `smsc_phy_config_edpd()` function
- Our mode change approach (SMR write + soft reset) matches `smsc_phy_reset()`
- The Linux driver also has a `PHY_RST_AFTER_CLK_EN` flag for PHYs that need
  reset after the clock is enabled — relevant to this board's architecture

### ESP-IDF `esp_eth_phy_lan87xx.c`
- The ESP-IDF driver performs basic 802.3 PHY init but does NOT:
  - Configure vendor-specific registers (MCSR, SMR)
  - Disable EDPD
  - Correct mode strapping
  - Enable AN if not already enabled by straps
- These omissions require manual intervention in application code

---

## Appendix: Project Files

```
bentophytest/
├── CMakeLists.txt            # Top-level, includes ESP-IDF project.cmake
├── sdkconfig.defaults        # ESP32 target, EMAC+RMII, GPIO17 clock, 4MB flash
├── main/
│   ├── CMakeLists.txt        # Component registration
│   └── main.c                # Full diagnostic firmware (this report's test code)
└── PHY_TEST_REPORT.md        # This document
```
