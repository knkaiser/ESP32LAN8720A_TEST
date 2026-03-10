# LAN8720A Ethernet PHY — Complete Register & Driver Reference

Sources:
- LAN8720A/LAN8720Ai Datasheet DS00002165 Rev 1.4 (08-23-12), SMSC/Microchip
- Linux kernel `drivers/net/phy/smsc.c` (mainline)
- Linux kernel `include/linux/smscphy.h`
- Linux kernel `include/uapi/linux/mii.h`
- Microchip product page: https://www.microchip.com/en-us/product/LAN8720A
- Datasheet PDF: https://ww1.microchip.com/downloads/aemDocuments/documents/UNG/ProductDocuments/DataSheets/LAN8720A-LAN8720Ai-Data-Sheet-DS00002165.pdf

---

## 1. Device Overview

- **PHY ID**: `0x0007c0f0` (OUI=0x00800f, Model=0x0f)
- **Interface**: RMII (Reduced Media Independent Interface)
- **Speeds**: 10BASE-T / 100BASE-TX
- **Package**: 24-pin QFN (4x4mm)
- **Supply**: Single 3.3V with integrated 1.2V regulator
- **I/O voltage**: 1.6V–3.6V (variable I/O)
- **Features**: Auto-negotiation, HP Auto-MDIX, Energy Detect Power-Down (EDPD), flexPWR
- **Clock**: 50MHz REF_CLK input, or 25MHz crystal/oscillator with internal PLL
- **Temperature**: 0°C to +85°C (commercial), -40°C to +85°C (industrial, LAN8720Ai)
- **Status**: In Production

---

## 2. Complete Register Map

### Table 4.2 — SMI Register Map (from datasheet)

| Reg | Hex  | Name                                | Access | Default  |
|-----|------|-------------------------------------|--------|----------|
| 0   | 0x00 | Basic Control Register (BMCR)       | R/W    | Mode dep |
| 1   | 0x01 | Basic Status Register (BMSR)        | RO     | Mode dep |
| 2   | 0x02 | PHY Identifier 1                    | RO     | 0x0007   |
| 3   | 0x03 | PHY Identifier 2                    | RO     | 0xC0Fx   |
| 4   | 0x04 | Auto-Neg Advertisement              | R/W    | 0x01E1   |
| 5   | 0x05 | Auto-Neg Link Partner Ability       | RO     | 0x0000   |
| 6   | 0x06 | Auto-Neg Expansion                  | RO     | 0x0000   |
| 7-16|      | *Reserved*                          |        |          |
| 17  | 0x11 | Mode Control/Status                 | R/W    | 0x0000   |
| 18  | 0x12 | Special Modes                       | R/W    | Mode dep |
| 19-25|     | *Reserved*                          |        |          |
| 26  | 0x1A | Symbol Error Counter                | RO     | 0x0000   |
| 27  | 0x1B | Special Control/Status Indications  | R/W    | 0x0000   |
| 28  | 0x1C | *Reserved*                          |        |          |
| 29  | 0x1D | Interrupt Source Flags               | RO/LH  | 0x0000   |
| 30  | 0x1E | Interrupt Mask                      | R/W    | 0x0000   |
| 31  | 0x1F | PHY Special Control/Status          | R/W    | Mode dep |

---

## 3. Standard MII Registers (Regs 0–6)

### Register 0 — Basic Control Register (BMCR) [0x00]

| Bit(s) | Name            | R/W  | Default | Description                              |
|--------|-----------------|------|---------|------------------------------------------|
| 15     | Soft Reset      | R/W/SC| 0      | 1 = PHY soft reset. Self-clearing.       |
| 14     | Loopback        | R/W  | 0       | 1 = Enable loopback mode                |
| 13     | Speed Select    | R/W  | Mode    | 1 = 100Mbps, 0 = 10Mbps                 |
| 12     | AN Enable       | R/W  | Mode    | 1 = Enable auto-negotiation              |
| 11     | Power Down      | R/W  | 0       | 1 = General power-down mode              |
| 10     | Isolate         | R/W  | 0       | 1 = Electrically isolate PHY from RMII   |
| 9      | Restart AN      | R/W/SC| 0      | 1 = Restart auto-negotiation             |
| 8      | Duplex Mode     | R/W  | Mode    | 1 = Full duplex                          |
| 7      | Collision Test   | R/W  | 0       | 1 = Enable COL signal test               |
| 6:0    | Reserved        |      |         |                                          |

Linux defines:
```c
#define BMCR_RESET      0x8000
#define BMCR_LOOPBACK   0x4000
#define BMCR_SPEED100   0x2000
#define BMCR_ANENABLE   0x1000
#define BMCR_PDOWN      0x0800
#define BMCR_ISOLATE    0x0400
#define BMCR_ANRESTART  0x0200
#define BMCR_FULLDPLX   0x0100
#define BMCR_CTST       0x0080
#define BMCR_SPEED10    0x0000
```

### Register 1 — Basic Status Register (BMSR) [0x01]

| Bit(s) | Name               | R/W | Default | Description                           |
|--------|--------------------|-----|---------|---------------------------------------|
| 15     | 100BASE-T4         | RO  | 0       | 1 = 100BASE-T4 capable (not for 8720)|
| 14     | 100BASE-TX FD      | RO  | 1       | 1 = 100BASE-TX full-duplex capable   |
| 13     | 100BASE-TX HD      | RO  | 1       | 1 = 100BASE-TX half-duplex capable   |
| 12     | 10BASE-T FD        | RO  | 1       | 1 = 10BASE-T full-duplex capable     |
| 11     | 10BASE-T HD        | RO  | 1       | 1 = 10BASE-T half-duplex capable     |
| 10:7   | Reserved           |     |         |                                       |
| 6      | Preamble Suppress  | RO  | 1       | 1 = Preamble suppress capable         |
| 5      | AN Complete        | RO  | 0       | 1 = Auto-negotiation complete         |
| 4      | Remote Fault       | RO/LH| 0     | 1 = Remote fault condition detected   |
| 3      | AN Ability         | RO  | 1       | 1 = Auto-negotiation capable          |
| 2      | Link Status        | RO/LL| 0     | 1 = Link is up (latching low)         |
| 1      | Jabber Detect      | RO/LH| 0     | 1 = Jabber condition detected         |
| 0      | Extended Capability| RO  | 1       | 1 = Extended register capability      |

Linux defines:
```c
#define BMSR_100FULL     0x4000
#define BMSR_100HALF     0x2000
#define BMSR_10FULL      0x1000
#define BMSR_10HALF      0x0800
#define BMSR_ANEGCOMPLETE 0x0020
#define BMSR_RFAULT      0x0010
#define BMSR_ANEGCAPABLE 0x0008
#define BMSR_LSTATUS     0x0004
#define BMSR_JCD         0x0002
#define BMSR_ERCAP       0x0001
```

### Register 2 — PHY Identifier 1 [0x02]

| Bit(s) | Name       | Default | Description                      |
|--------|------------|---------|----------------------------------|
| 15:0   | PHY ID MSB | 0x0007  | Bits [18:3] of OUI (= 0x0007)  |

### Register 3 — PHY Identifier 2 [0x03]

| Bit(s) | Name           | Default | Description                                |
|--------|----------------|---------|--------------------------------------------|
| 15:10  | PHY ID [2:0]   | 110000  | Bits [2:0] of OUI + padding                |
| 9:4    | Model Number   | 001111  | Model# = 0x0F (LAN8710/LAN8720)           |
| 3:0    | Revision       | varies  | Silicon revision                           |

Full PHY ID = `0x0007C0Fx` (where x = revision).

### Register 4 — Auto-Negotiation Advertisement [0x04]

| Bit(s) | Name              | R/W | Default | Description                        |
|--------|-------------------|-----|---------|------------------------------------|
| 15     | Next Page         | R/W | 0       | 1 = Next page capable              |
| 14     | Reserved          |     |         |                                    |
| 13     | Remote Fault      | R/W | 0       | 1 = Advertise remote fault         |
| 12     | Reserved          |     |         |                                    |
| 11     | Asymmetric Pause  | R/W | 0       | 1 = Asymmetric pause supported     |
| 10     | Pause             | R/W | 0       | 1 = Pause supported                |
| 9      | 100BASE-T4        | RO  | 0       | Not supported                      |
| 8      | 100BASE-TX FD     | R/W | 1       | 1 = 100 Mbps full-duplex           |
| 7      | 100BASE-TX HD     | R/W | 1       | 1 = 100 Mbps half-duplex           |
| 6      | 10BASE-T FD       | R/W | 1       | 1 = 10 Mbps full-duplex            |
| 5      | 10BASE-T HD       | R/W | 1       | 1 = 10 Mbps half-duplex            |
| 4:0    | Selector          | R/W | 00001   | IEEE 802.3 selector                |

Default = 0x01E1 (all capable + 802.3 selector).

Linux defines:
```c
#define ADVERTISE_CSMA      0x0001
#define ADVERTISE_10HALF    0x0020
#define ADVERTISE_10FULL    0x0040
#define ADVERTISE_100HALF   0x0080
#define ADVERTISE_100FULL   0x0100
#define ADVERTISE_PAUSE_CAP 0x0400
#define ADVERTISE_ALL       (ADVERTISE_10HALF | ADVERTISE_10FULL | \
                             ADVERTISE_100HALF | ADVERTISE_100FULL)
```

### Register 5 — Auto-Negotiation Link Partner Ability [0x05]

Same bit layout as Register 4, but read-only. Shows partner's advertised capabilities after AN completes.

### Register 6 — Auto-Negotiation Expansion [0x06]

| Bit(s) | Name                     | R/W | Description                             |
|--------|--------------------------|-----|-----------------------------------------|
| 4      | Parallel Detect Fault    | RO/LH | 1 = Parallel detect fault             |
| 3      | LP Next Page Able        | RO  | 1 = LP supports next page              |
| 2      | Local Next Page Able     | RO  | 1 = Local supports next page           |
| 1      | Page Received            | RO/LH | 1 = New page received                |
| 0      | LP AN Able               | RO  | 1 = LP supports auto-negotiation       |

---

## 4. Vendor-Specific Registers (Regs 17–31)

### Register 17 — Mode Control/Status Register [0x11]

| Bit(s) | Name                | R/W | Default | Description                              |
|--------|---------------------|-----|---------|------------------------------------------|
| 15:14  | Reserved            |     |         |                                          |
| 13     | EDPWRDOWN           | R/W | 0       | 1 = Enable Energy Detect Power-Down      |
| 12:10  | Reserved            |     |         |                                          |
| 9      | FARLOOPBACK         | R/W | 0       | 1 = Enable far loopback (100BASE-TX only)|
| 8:7    | Reserved            |     |         |                                          |
| 6      | ALTINT              | R/W | 0       | 1 = Use alternate interrupt scheme       |
| 5:2    | Reserved            |     |         |                                          |
| 1      | ENERGYON            | RO  | 1       | 1 = Energy detected on line              |
| 0      | Reserved            |     |         |                                          |

Linux defines:
```c
#define MII_LAN83C185_CTRL_STATUS  17  /* Mode/Status Register */
#define MII_LAN83C185_EDPWRDOWN    (1 << 13)  /* bit 13 */
#define MII_LAN83C185_ENERGYON     (1 << 1)   /* bit 1 */
```

### Register 18 — Special Modes Register [0x12]

| Bit(s) | Name           | R/W | Default | Description                              |
|--------|----------------|-----|---------|------------------------------------------|
| 15:8   | Reserved       |     |         |                                          |
| 7:5    | MODE[2:0]      | R/W | Strap   | PHY mode of operation                    |
| 4:0    | PHYAD[4:0]     | R/W | Strap   | PHY address                              |

**MODE[2:0] values:**

| MODE | Description                                          |
|------|------------------------------------------------------|
| 000  | 10BASE-T half-duplex, AN disabled                    |
| 001  | 10BASE-T full-duplex, AN disabled                    |
| 010  | 100BASE-TX half-duplex, AN disabled (RMII only)      |
| 011  | 100BASE-TX full-duplex, AN disabled (RMII only)      |
| 100  | 100BASE-TX half-duplex, AN enabled                   |
| 101  | Reserved                                              |
| 110  | Power-Down mode                                       |
| 111  | All capable, AN enabled (default)                     |

Linux defines:
```c
#define MII_LAN83C185_SPECIAL_MODES  18
#define MII_LAN83C185_MODE_MASK      0xE0
#define MII_LAN83C185_MODE_POWERDOWN 0xC0   /* MODE = 110 */
#define MII_LAN83C185_MODE_ALL       0xE0   /* MODE = 111 */
```

### Register 26 — Symbol Error Counter Register [0x1A]

| Bit(s) | Name             | R/W | Description                                   |
|--------|------------------|-----|-----------------------------------------------|
| 15:0   | Symbol Errors    | RO  | Count of symbol errors since last read. Saturates at 0xFFFF. Clears on read. |

### Register 27 — Special Control/Status Indications Register [0x1B]

| Bit(s) | Name             | R/W | Default | Description                              |
|--------|------------------|-----|---------|------------------------------------------|
| 15     | AMDIXCTRL        | R/W | 0       | 0 = Enable Auto-MDIX, 1 = Disable       |
| 14     | Reserved         | RO  |         |                                          |
| 13     | CH_SELECT        | R/W | 0       | 0 = MDI, 1 = MDIX (when bit 15 = 1)     |
| 12     | Reserved         |     |         |                                          |
| 11     | SQEOFF           | R/W | 0       | 1 = Disable SQE test (heartbeat)        |
| 10:5   | Reserved         |     |         |                                          |
| 4      | XPOL             | RO  | 0       | 1 = Polarity state reversed (10BASE-T)   |
| 3:0    | Reserved         |     |         |                                          |

Linux defines:
```c
#define SPECIAL_CTRL_STS                27
#define SPECIAL_CTRL_STS_OVRRD_AMDIX_   0x8000  /* bit 15 */
#define SPECIAL_CTRL_STS_AMDIX_ENABLE_  0x4000  /* bit 14 */
#define SPECIAL_CTRL_STS_AMDIX_STATE_   0x2000  /* bit 13 */
```

**Auto-MDIX control (per datasheet Rev 1.4):**

| Bit 15 (AMDIXCTRL) | Bit 13 (CH_SELECT) | Result          |
|---------------------|--------------------|-----------------|
| 0                   | x                  | Auto-MDIX enabled (default) |
| 1                   | 0                  | Force MDI       |
| 1                   | 1                  | Force MDI-X     |

### Register 29 — Interrupt Source Flags Register [0x1D]

| Bit(s) | Name                    | R/W   | Description                         |
|--------|-------------------------|-------|-------------------------------------|
| 7      | INT7: ENERGYON          | RO/LH | Energy detected on line             |
| 6      | INT6: AN Complete       | RO/LH | Auto-negotiation completed          |
| 5      | INT5: Remote Fault      | RO/LH | Remote fault detected               |
| 4      | INT4: Link Down         | RO/LH | Link has gone down                  |
| 3      | INT3: AN LP Ack         | RO/LH | AN link partner acknowledge         |
| 2      | INT2: Parallel Det Fault| RO/LH | Parallel detection fault            |
| 1      | INT1: AN Page Rx        | RO/LH | AN page received                    |
| 0      | Reserved                |       |                                     |

Reading this register clears pending interrupts.

Linux defines:
```c
#define MII_LAN83C185_ISF        29
#define MII_LAN83C185_ISF_INT7   (1<<7)  /* ENERGYON */
#define MII_LAN83C185_ISF_INT6   (1<<6)  /* AN Complete */
#define MII_LAN83C185_ISF_INT5   (1<<5)  /* Remote Fault */
#define MII_LAN83C185_ISF_INT4   (1<<4)  /* Link Down */
#define MII_LAN83C185_ISF_INT3   (1<<3)  /* AN LP Ack */
#define MII_LAN83C185_ISF_INT2   (1<<2)  /* Parallel Detect Fault */
#define MII_LAN83C185_ISF_INT1   (1<<1)  /* AN Page Received */

#define MII_LAN83C185_ISF_INT_PHYLIB_EVENTS \
    (MII_LAN83C185_ISF_INT6 | MII_LAN83C185_ISF_INT4 | MII_LAN83C185_ISF_INT7)
```

### Register 30 — Interrupt Mask Register [0x1E]

Same bit layout as Register 29. Writing 1 to a bit enables that interrupt source.

```c
#define MII_LAN83C185_IM  30
```

### Register 31 — PHY Special Control/Status Register [0x1F]

| Bit(s) | Name                | R/W | Default | Description                              |
|--------|---------------------|-----|---------|------------------------------------------|
| 15:13  | Reserved            |     |         |                                          |
| 12     | AUTODONE            | RO  | 0       | 1 = Auto-negotiation done                |
| 11:5   | Reserved            | R/W | 0000010 | Write as 0000010b, ignore on read        |
| 4:2    | Speed Indication    | RO  |         | Speed/duplex result                      |
| 1:0    | Reserved            |     |         |                                          |

**Speed Indication [4:2] values:**

| Bits 4:2 | Speed & Duplex              |
|----------|-----------------------------|
| 001      | 10BASE-T half-duplex        |
| 101      | 10BASE-T full-duplex        |
| 010      | 100BASE-TX half-duplex      |
| 110      | 100BASE-TX full-duplex      |

### Register 16 — EDPD NLP / Crossover Time (PHY_EDPD_CONFIG)

| Bit(s) | Name                  | R/W | Description                                |
|--------|-----------------------|-----|--------------------------------------------|
| 0      | EXT_CROSSOVER         | R/W | 1 = Extend manual AutoMDIX crossover timer |
| 15:1   | Various EDPD config   |     |                                            |

Linux defines:
```c
#define PHY_EDPD_CONFIG                  16
#define PHY_EDPD_CONFIG_EXT_CROSSOVER_   0x0001
```

---

## 5. Initialization Sequences

### 5.1 Linux Kernel Canonical Init (from smsc.c)

The Linux kernel driver for LAN8710/LAN8720 (`phy_id = 0x0007c0f0`) uses these callbacks:

```
probe       → smsc_phy_probe()
soft_reset  → smsc_phy_reset()
config_init → smsc_phy_config_init()
config_aneg → lan95xx_config_aneg_ext()
read_status → lan87xx_read_status()
```

#### Step 1: Probe — `smsc_phy_probe()`
```c
// Allocate private data
priv->edpd_enable = true;
priv->edpd_max_wait_ms = 640;  // EDPD_MAX_WAIT_DFLT_MS

// Check device tree for "smsc,disable-energy-detect" property
if (device_property_present(dev, "smsc,disable-energy-detect"))
    priv->edpd_enable = false;

// Request optional 50MHz reference clock
refclk = devm_clk_get_optional_enabled_with_rate(dev, NULL, 50000000);
```

#### Step 2: Reset — `smsc_phy_reset()`
```c
// Read Special Modes register (reg 18)
rc = phy_read(phydev, MII_LAN83C185_SPECIAL_MODES);  // reg 18

// If PHY is in power-down mode (MODE=110), set to "all capable" (MODE=111)
if ((rc & 0xE0) == 0xC0) {     // MII_LAN83C185_MODE_POWERDOWN
    rc |= 0xE0;                 // MII_LAN83C185_MODE_ALL
    phy_write(phydev, 18, rc);
}

// Perform standard soft reset (write bit 15 of reg 0, wait for clear)
genphy_soft_reset(phydev);  // writes BMCR_RESET to reg 0
```

#### Step 3: Config Init — `smsc_phy_config_init()`
```c
// Don't use EDPD in interrupt mode unless user overrode
if (!priv->edpd_mode_set_by_user && phydev->irq != PHY_POLL)
    priv->edpd_enable = false;

// Configure EDPD
if (priv->edpd_enable)
    phy_set_bits(phydev, 17, (1 << 13));   // Set EDPWRDOWN in reg 17
else
    phy_clear_bits(phydev, 17, (1 << 13)); // Clear EDPWRDOWN in reg 17
```

#### Step 4: Config Auto-Negotiation — `lan95xx_config_aneg_ext()`
```c
// For LAN9500A/LAN9505A (phy_id 0x0007c0f0): extend crossover timer
if (phydev->phy_id == 0x0007c0f0) {
    phy_set_bits(phydev, 16, 0x0001);  // PHY_EDPD_CONFIG_EXT_CROSSOVER_
}

// Then proceed to standard AN config via lan87xx_config_aneg()
// which handles Auto-MDIX settings in reg 27 and calls genphy_config_aneg()
```

### 5.2 Bare-Metal Initialization Sequence (Recommended)

Based on the datasheet and Linux driver, here is the recommended bare-metal init:

```c
// 1. Hardware reset: assert nRST low for >= 25ms, then release
//    Wait >= 50ms after nRST goes high for internal POR to complete

// 2. Read PHY ID to verify communication
uint16_t id1 = mdio_read(phy_addr, 2);  // Should be 0x0007
uint16_t id2 = mdio_read(phy_addr, 3);  // Should be 0xC0Fx

// 3. Check if PHY is in power-down mode, fix if so
uint16_t special_modes = mdio_read(phy_addr, 18);
if ((special_modes & 0xE0) == 0xC0) {
    special_modes |= 0xE0;  // Set MODE = 111 (all capable)
    mdio_write(phy_addr, 18, special_modes);
}

// 4. Soft reset
mdio_write(phy_addr, 0, 0x8000);  // BMCR_RESET
// Poll reg 0 bit 15 until it clears (self-clearing)
while (mdio_read(phy_addr, 0) & 0x8000) { /* wait */ }

// 5. Configure EDPD (Energy Detect Power-Down) in reg 17
//    Enable for power savings when cable unplugged:
uint16_t ctrl_status = mdio_read(phy_addr, 17);
ctrl_status |= (1 << 13);  // Set EDPWRDOWN
mdio_write(phy_addr, 17, ctrl_status);

// 6. Configure Auto-MDIX (reg 27) - enable auto-MDIX override
uint16_t special_ctrl = mdio_read(phy_addr, 27);
special_ctrl &= ~(0x8000 | 0x4000 | 0x2000);  // Clear bits 15,14,13
special_ctrl |= (0x8000 | 0x4000);              // Override + Enable Auto-MDIX
mdio_write(phy_addr, 27, special_ctrl);

// 7. Set advertisement register (reg 4) - advertise all capabilities
mdio_write(phy_addr, 4, 0x01E1);  // 10/100 half/full + 802.3

// 8. Enable and restart auto-negotiation
mdio_write(phy_addr, 0, 0x1200);  // ANENABLE | ANRESTART

// 9. Wait for link (poll reg 1 bit 2)
while (!(mdio_read(phy_addr, 1) & 0x0004)) { /* wait for link */ }

// 10. Read negotiated speed/duplex from reg 31 bits [4:2]
uint16_t phy_status = mdio_read(phy_addr, 31);
uint8_t speed_ind = (phy_status >> 2) & 0x07;
// 001 = 10H, 101 = 10F, 010 = 100H, 110 = 100F
```

---

## 6. EDPD (Energy Detect Power-Down) Handling

### 6.1 Overview

EDPD is controlled by bit 13 of Register 17 (Mode Control/Status). When enabled:
- PHY enters low-power state when no energy is detected on the cable
- PHY periodically sends Normal Link Pulses (NLPs) to detect cable insertion
- Saves ~220 mW when cable is unplugged

### 6.2 Known Issue — ENERGYON Bit Unreliable

From the Linux driver comment:

> "The LAN87xx suffers from rare absence of the ENERGYON-bit when Ethernet cable
> plugs in while LAN87xx is in Energy Detect Power-Down mode. This leads to
> unstable detection of plugging in Ethernet cable."

### 6.3 Linux Workaround (`lan87xx_read_status`)

The canonical workaround used in the Linux kernel:

```c
// Called during polling to check link status
int lan87xx_read_status(struct phy_device *phydev)
{
    err = genphy_read_status(phydev);  // Standard status read

    if (!phydev->link && priv->edpd_enable && priv->edpd_max_wait_ms) {
        // 1. Disable EDPD temporarily to wake up PHY
        rc = phy_read(phydev, 17);     // MII_LAN83C185_CTRL_STATUS
        phy_write(phydev, 17, rc & ~(1 << 13));  // Clear EDPWRDOWN

        // 2. Wait up to 640ms for ENERGYON bit to appear
        //    Poll reg 17 bit 1 (ENERGYON)
        read_poll_timeout(phy_read, rc,
            rc & (1 << 1) || rc < 0,   // ENERGYON bit
            10000,                       // poll interval: 10ms
            640000,                      // max wait: 640ms
            true, phydev, 17);

        // 3. Re-enable EDPD
        rc = phy_read(phydev, 17);
        phy_write(phydev, 17, rc | (1 << 13));  // Set EDPWRDOWN
    }
}
```

### 6.4 EDPD Configuration Rules

1. **Polling mode**: EDPD is safe. Use the workaround above to handle the ENERGYON bug.
2. **Interrupt mode**: **Do NOT use EDPD**. The ENERGYON unreliability makes link change detection via interrupts unreliable.
3. **User override**: The `smsc,disable-energy-detect` device tree property disables EDPD entirely.
4. **Default**: EDPD enabled with 640ms max wait time.

### 6.5 Bare-Metal EDPD Workaround

```c
bool lan8720a_check_link_with_edpd_workaround(uint8_t phy_addr)
{
    // Standard link check
    uint16_t bmsr = mdio_read(phy_addr, 1);
    if (bmsr & 0x0004)  // BMSR_LSTATUS
        return true;    // Link is up

    // Link is down — apply EDPD workaround
    // 1. Disable EDPD
    uint16_t reg17 = mdio_read(phy_addr, 17);
    mdio_write(phy_addr, 17, reg17 & ~(1 << 13));

    // 2. Poll for ENERGYON (bit 1 of reg 17) for up to 640ms
    for (int i = 0; i < 64; i++) {
        delay_ms(10);
        reg17 = mdio_read(phy_addr, 17);
        if (reg17 & (1 << 1))  // ENERGYON
            break;
    }

    // 3. Re-enable EDPD
    reg17 = mdio_read(phy_addr, 17);
    mdio_write(phy_addr, 17, reg17 | (1 << 13));

    // 4. Re-check link
    bmsr = mdio_read(phy_addr, 1);
    return (bmsr & 0x0004) != 0;
}
```

---

## 7. Configuration Straps (Hardware Pins at Reset)

These pins are sampled during hardware reset (nRST rising edge):

| Pin         | Strap        | Pull   | Description                                   |
|-------------|--------------|--------|-----------------------------------------------|
| RXD0        | MODE0        | Pull-up| PHY operating mode bit 0                      |
| RXD1        | MODE1        | Pull-up| PHY operating mode bit 1                      |
| CRS_DV      | MODE2        | Pull-up| PHY operating mode bit 2                      |
| RXER        | PHYAD0       | Pull-dn| PHY SMI address bit 0 (bits [4:1] = 0001)    |
| LED1        | REGOFF       |        | 0 = Use internal 1.2V reg, 1 = External       |
| LED2        | nINTSEL      |        | 0 = nINT/REFCLKO outputs REFCLKO, 1 = nINT    |

Default strap for MODE[2:0] = 111 = "All capable, AN enabled" (all pull-ups).

Default PHYAD = 0b00001 = address 1 (PHYAD0 pull-down = 0, bits [4:1] hardwired to 0001).

---

## 8. Interrupt Configuration

### Primary Interrupt System

The nINT pin (active low, directly usable when nINTSEL strap = 1):

```c
// Enable interrupts for link change, AN complete, ENERGYON
mdio_write(phy_addr, 30, 0x00D0);  // INT4 | INT6 | INT7

// Clear pending by reading ISF register
mdio_read(phy_addr, 29);

// In ISR: read reg 29 to determine cause and clear
uint16_t isf = mdio_read(phy_addr, 29);
if (isf & (1 << 4)) { /* link down */ }
if (isf & (1 << 6)) { /* AN complete */ }
if (isf & (1 << 7)) { /* ENERGYON */ }
```

### Alternate Interrupt System

When bit 2 of reg 17 (ALTINT) is set, interrupt behavior changes:
- nINT asserts on **any** unmasked interrupt event
- nINT stays asserted until all interrupt flags are cleared by reading reg 29

---

## 9. Power-Down Modes

| Mode                    | How to Enter                              | Power     |
|-------------------------|-------------------------------------------|-----------|
| General Power-Down      | Set reg 0 bit 11 (BMCR_PDOWN)            | ~100 µA   |
| Energy Detect Power-Down| Set reg 17 bit 13 (EDPWRDOWN)            | Saves ~220mW |
| Mode Strap Power-Down   | MODE[2:0] = 110 at reset                 | Low power |

To wake from General Power-Down:
```c
// Clear power-down bit
uint16_t bmcr = mdio_read(phy_addr, 0);
mdio_write(phy_addr, 0, bmcr & ~0x0800);
// Wait for PHY to stabilize (~2ms typical)
```

---

## 10. Clock Configuration

The LAN8720A supports three clocking modes:

1. **REF_CLK In Mode** (nINTSEL=1): External 50MHz clock on XTAL1/CLKIN pin. nINT/REFCLKO pin is interrupt output.

2. **REF_CLK Out Mode with Crystal** (nINTSEL=0): 25MHz crystal between XTAL1 and XTAL2. Internal PLL generates 50MHz. REFCLKO outputs 50MHz to MAC.

3. **REF_CLK Out Mode with Oscillator** (nINTSEL=0): External 25MHz oscillator on XTAL1/CLKIN (XTAL2 unconnected). Internal PLL generates 50MHz. REFCLKO outputs 50MHz to MAC.

---

## 11. Quick Reference — All Register Defines from Linux

```c
/* Standard MII registers */
#define MII_BMCR        0x00    /* Basic mode control register */
#define MII_BMSR        0x01    /* Basic mode status register */
#define MII_PHYSID1     0x02    /* PHYS ID 1 */
#define MII_PHYSID2     0x03    /* PHYS ID 2 */
#define MII_ADVERTISE   0x04    /* Advertisement control reg */
#define MII_LPA         0x05    /* Link partner ability reg */
#define MII_EXPANSION   0x06    /* Expansion register */

/* SMSC/LAN8720A vendor-specific registers */
#define PHY_EDPD_CONFIG                  16  /* EDPD NLP / crossover config */
#define MII_LAN83C185_CTRL_STATUS        17  /* Mode Control/Status */
#define MII_LAN83C185_SPECIAL_MODES      18  /* Special Modes (MODE + PHYAD) */
/* reg 26 = Symbol Error Counter */
#define SPECIAL_CTRL_STS                 27  /* Special Control/Status Indications */
/* reg 29 = Interrupt Source Flags (ISF) */
#define MII_LAN83C185_ISF               29
/* reg 30 = Interrupt Mask (IM) */
#define MII_LAN83C185_IM                30
/* reg 31 = PHY Special Control/Status (speed indication, AUTODONE) */

/* Register 17 bits */
#define MII_LAN83C185_EDPWRDOWN   (1 << 13)
#define MII_LAN83C185_ENERGYON    (1 << 1)

/* Register 18 bits */
#define MII_LAN83C185_MODE_MASK      0xE0
#define MII_LAN83C185_MODE_POWERDOWN 0xC0
#define MII_LAN83C185_MODE_ALL       0xE0

/* Register 27 bits */
#define SPECIAL_CTRL_STS_OVRRD_AMDIX_   0x8000
#define SPECIAL_CTRL_STS_AMDIX_ENABLE_  0x4000
#define SPECIAL_CTRL_STS_AMDIX_STATE_   0x2000

/* Register 29 bits (Interrupt Source Flags) */
#define MII_LAN83C185_ISF_INT7   (1<<7)   /* ENERGYON */
#define MII_LAN83C185_ISF_INT6   (1<<6)   /* AN Complete */
#define MII_LAN83C185_ISF_INT5   (1<<5)   /* Remote Fault */
#define MII_LAN83C185_ISF_INT4   (1<<4)   /* Link Down */
#define MII_LAN83C185_ISF_INT3   (1<<3)   /* AN LP Ack */
#define MII_LAN83C185_ISF_INT2   (1<<2)   /* Parallel Detect Fault */
#define MII_LAN83C185_ISF_INT1   (1<<1)   /* AN Page Received */

/* EDPD config register 16 */
#define PHY_EDPD_CONFIG_EXT_CROSSOVER_   0x0001

/* PHY ID for LAN8710/LAN8720 */
#define LAN8720_PHY_ID       0x0007c0f0
#define LAN8720_PHY_ID_MASK  0xfffffff0

/* PHY flag */
#define PHY_RST_AFTER_CLK_EN  /* PHY needs reset after clock enable */
```

---

## 12. Application Notes

The datasheet references **SMSC Application Note AN 17.18** for POE applications.

Microchip does not publish a standalone "initialization guide" application note for the LAN8720A. The datasheet itself (DS00002165) serves as the primary reference for register programming. The Linux kernel driver (`drivers/net/phy/smsc.c`) is the de facto reference implementation.

Key documents:
- **DS00002165** — LAN8720A/LAN8720Ai Datasheet (register map, app diagrams)
- **Linux smsc.c** — Canonical driver with EDPD workaround, Auto-MDIX override, reset sequence
- **Linux smscphy.h** — Register/bit definitions

---

## 13. Key Takeaways for Driver Implementation

1. **Always check MODE at reset**: If MODE=110 (power-down), write MODE=111 (all capable) to reg 18 before soft reset.
2. **Soft reset**: Write 0x8000 to reg 0, poll bit 15 until clear.
3. **EDPD**: Enable via reg 17 bit 13 for power savings. Implement the ENERGYON polling workaround when link is down.
4. **Auto-MDIX**: Override the strap by setting reg 27 bits [15:13] = 110 (override + enable Auto-MDIX).
5. **Interrupts vs Polling**: If using interrupts, disable EDPD. If polling, EDPD is safe with the workaround.
6. **Speed detection**: After AN completes, read reg 31 bits [4:2] for speed/duplex indication.
7. **PHY_RST_AFTER_CLK_EN**: The LAN8720 needs a reset after its reference clock is enabled (Linux sets this flag).
8. **Extended crossover timer**: For LAN8720 specifically, set reg 16 bit 0 to extend the manual AutoMDIX crossover timer.
