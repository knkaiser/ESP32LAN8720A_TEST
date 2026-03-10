# bentophytest — LAN8720A Ethernet PHY Diagnostic Tool

Bare-metal ESP-IDF diagnostic firmware for testing the Ethernet PHY on HamGeek01 Plus
(and compatible UZG-01) boards. Built without Arduino or PlatformIO — uses ESP-IDF v5.3
directly.

## Purpose

This tool was created to diagnose Ethernet connectivity problems on the HamGeek01 Plus
Zigbee gateway. It exercises the ESP32 EMAC + LAN8720A PHY over RMII and provides
detailed register-level diagnostics to identify signal integrity issues, auto-negotiation
failures, and link-up problems.

The firmware:

- Initializes the ESP32 internal EMAC with the LAN8720A PHY
- Disables Energy Detect Power-Down (EDPD) to avoid the known ENERGYON detection bug
- Dumps all PHY registers at key events (init, link up, link down, timeout)
- Polls PHY status every second while waiting for DHCP
- Attempts auto-negotiation after EMAC stabilization
- Performs DNS lookups to verify end-to-end connectivity

## Hardware

**Board:** HamGeek01 Plus (ESP32-WROOM-32UE-N8 + LAN8720A)

### RMII Pin Mapping

| Signal      | ESP32 GPIO | LAN8720A Pin | LAN8720A Name    |
|-------------|------------|-------------|------------------|
| REF_CLK     | 17         | 5           | XTAL1/CLKIN      |
| MDC         | 23         | 13          | MDC              |
| MDIO        | 18         | 12          | MDIO             |
| TXD0        | 19         | 17          | TXD0             |
| TXD1        | 22         | 18          | TXD1             |
| TX_EN       | 21         | 16          | TXEN             |
| RXD0        | 25         | 8           | RXD0/MODE0       |
| RXD1        | 26         | 7           | RXD1/MODE1       |
| CRS_DV      | 27         | 11          | CRS_DV/MODE2     |
| nRST        | 5          | 15          | nRST             |

PHY address: 0 (RXER/PHYAD0 pin 10, active-low strap).

The ESP32 outputs the 50 MHz RMII reference clock on GPIO17
(`EMAC_CLK_OUT_180_GPIO`). GPIO17 drive strength is reduced to `GPIO_DRIVE_CAP_0`
(lowest) to minimize noise coupling into the PHY analog section.

## Prerequisites

- macOS, Linux, or Windows with WSL
- Python 3.8+
- Git
- USB-to-serial adapter connected to the board (e.g. `/dev/cu.usbserial-2110`)

## Installing ESP-IDF

ESP-IDF is Espressif's official development framework for the ESP32. This project
requires **v5.3** or any compatible v5.x release.

### macOS

Install system dependencies:

```bash
brew install cmake ninja dfu-util
```

Clone and install ESP-IDF:

```bash
mkdir -p ~/esp
cd ~/esp
git clone -b v5.3 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32
```

### Linux (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install -y git wget flex bison gperf python3 python3-pip python3-venv \
  cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0

mkdir -p ~/esp
cd ~/esp
git clone -b v5.3 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32
```

### Activate ESP-IDF (required in every new terminal)

```bash
. ~/esp/esp-idf/export.sh
```

You should see output confirming the IDF path and `idf.py` becoming available.
Add this line to your shell profile (`.zshrc` / `.bashrc`) if you want it
activated automatically.

### Verify Installation

```bash
idf.py --version
```

Should print something like `ESP-IDF v5.3`.

## Serial Port Setup

### macOS

The USB-to-serial adapter typically appears as `/dev/cu.usbserial-XXXX`.
List available ports:

```bash
ls /dev/cu.usbserial-*
```

On macOS you may need to install the CH340 or CP2102 driver depending on your
adapter chip. Most modern macOS versions include these drivers.

### Linux

The port is typically `/dev/ttyUSB0` or `/dev/ttyACM0`. You may need to add
your user to the `dialout` group:

```bash
sudo usermod -aG dialout $USER
```

Log out and back in for this to take effect.

## Build

From the project directory:

```bash
cd bentophytest
idf.py set-target esp32
idf.py build
```

The first build takes several minutes as it compiles the entire ESP-IDF framework.
Subsequent builds are incremental and much faster.

If the build succeeds you will see:

```
Project build complete. To flash, run:
 idf.py flash
```

## Flash

Connect the board via USB, then:

```bash
idf.py -p /dev/cu.usbserial-2110 flash
```

Replace the port with your actual serial device. If the board does not enter
bootloader mode automatically, hold the **BOOT** button while pressing **RST**,
then release both.

## Monitor

```bash
idf.py -p /dev/cu.usbserial-2110 monitor
```

Or combine flash and monitor in one step:

```bash
idf.py -p /dev/cu.usbserial-2110 flash monitor
```

Press `Ctrl+]` to exit the monitor.

## Clean Build

If you need to start fresh (e.g. after changing `sdkconfig.defaults`):

```bash
idf.py fullclean
idf.py build
```

## What to Expect

### Successful Run

```
========================================
 HamGeek01 Plus Ethernet DNS Test
========================================
PHY ID: 0x0007:0xc0f1  OUI=0x0001f0  Model=0x0f  Rev=1
...
EDPD already disabled (good)
Ethernet started (10BT-HD strap mode)
AN enabled after EMAC stabilised (BMCR=0x1200)
Waiting for link & DHCP ...
[ 1s] BMCR=0x1200 BMSR=0x7809 Link=0 AN=1/0 ENERGYON=1 Speed: not resolved
...
>>> LINK UP  MAC xx:xx:xx:xx:xx:xx
>>> GOT IP ADDRESS
  IP:      192.168.x.x
  Netmask: 255.255.255.0
  Gateway: 192.168.x.1
Resolving "www.google.com" ...
  www.google.com -> 142.250.x.x
========================================
 Test complete
========================================
```

### Timeout (No Link)

If the link never comes up within 60 seconds, the firmware prints a full register
dump labeled `Timeout` and exits. Key things to look for:

| Register Field | Meaning |
|---------------|---------|
| `ENERGYON=0`  | PHY cannot detect signal from link partner — possible signal integrity issue |
| `ENERGYON=1, AN_Complete=0` | PHY sees energy but cannot decode FLP capability pages — noise corruption |
| `Link=0` throughout | No link established at any speed |
| `ANLPAR=0x0001` | Link partner detected as AN-capable but zero capabilities decoded |

## Diagnostic Output

The firmware dumps these LAN8720A registers at each event:

| Register | Addr | Key Fields |
|----------|------|------------|
| BMCR     | 0x00 | Speed, AN enable, duplex, power-down, isolate |
| BMSR     | 0x01 | Link status, AN complete, remote fault, capabilities |
| PHYID1/2 | 0x02/03 | OUI, model, revision |
| ANAR     | 0x04 | Our advertised capabilities |
| ANLPAR   | 0x05 | Link partner capabilities (populated after AN) |
| ANER     | 0x06 | LP AN ability, page received, parallel detect fault |
| MCSR     | 0x11 | ENERGYON, EDPWRDOWN, far loopback |
| SMR      | 0x12 | MODE strap value, PHY address |
| SECR     | 0x1A | Symbol error count |
| CSIR     | 0x1B | Auto-MDIX state, crossover, polarity |
| ISR      | 0x1D | Interrupt source flags |
| IMR      | 0x1E | Interrupt mask |
| PSCSR    | 0x1F | Auto-negotiation done, resolved speed/duplex |

## Known Issues

### MODE Strapping

The board straps MODE[2:0] = 000 at reset (RXD0/RXD1/CRS_DV are all low), forcing
the LAN8720A into 10Base-T half-duplex with auto-negotiation disabled. The firmware
compensates by explicitly enabling AN after the EMAC stabilizes.

### PoE Circuit Signal Degradation

Community reports (GitHub issue
[mercenaruss/uzg-firmware#47](https://github.com/mercenaruss/uzg-firmware/issues/47))
confirm that the PoE extraction circuit on these boards degrades the Ethernet analog
signal path. Symptoms:

- Auto-negotiation fails (FLP data pulses corrupted by insertion loss / impedance mismatch)
- ENERGYON drops to 0 after EMAC starts (RX signal below squelch threshold)
- 10Base-T may work via parallel detection with strong link partners
- PoE power delivery continues to work even when data link fails
- **Connecting USB while PoE is active can permanently damage the LAN chip**

### Link Partner Sensitivity

Different link partners produce different results due to varying TX power levels
and parallel detection implementations. A link partner with a stronger transmitter
is more likely to establish a 10BT link through the lossy PoE circuit.

## Project Structure

```
bentophytest/
├── CMakeLists.txt          # Top-level ESP-IDF project
├── sdkconfig.defaults      # ESP32, EMAC+RMII, GPIO17 clock, 4MB flash
├── main/
│   ├── CMakeLists.txt      # Component registration
│   └── main.c              # Diagnostic firmware
├── PHY_TEST_REPORT.md      # Detailed test results and register analysis
├── LAN8720A_reference.md   # PHY register reference (from datasheet + Linux driver)
├── LAN8720A_Datasheet.pdf  # SMSC/Microchip datasheet Rev 1.4
└── README.md               # This file
```

## References

- [LAN8720A Datasheet (DS00002165 Rev 1.4)](https://ww1.microchip.com/downloads/aemDocuments/documents/UNG/ProductDocuments/DataSheets/LAN8720A-LAN8720Ai-Data-Sheet-DS00002165.pdf)
- [Linux kernel SMSC PHY driver (`drivers/net/phy/smsc.c`)](https://github.com/torvalds/linux/blob/master/drivers/net/phy/smsc.c)
- [ESP-IDF Ethernet examples](https://github.com/espressif/esp-idf/tree/master/examples/ethernet)
- [UZG-01 firmware (board reference)](https://github.com/mercenaruss/uzg-firmware)
