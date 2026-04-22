# STM32 BLE Sensor Monitor — Exhaustive Technical Documentation

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [System Architecture](#2-system-architecture)
3. [Hardware Platform](#3-hardware-platform)
4. [STM32 Firmware](#4-stm32-firmware)
   - 4.1 [Project Structure](#41-project-structure)
   - 4.2 [Boot & Initialization Sequence](#42-boot--initialization-sequence)
   - 4.3 [Clock Configuration](#43-clock-configuration)
   - 4.4 [Peripheral Configuration](#44-peripheral-configuration)
   - 4.5 [I2C Sensor Drivers](#45-i2c-sensor-drivers)
   - 4.6 [BLE Stack (BlueNRG-2)](#46-ble-stack-bluenrg-2)
   - 4.7 [GATT Database & Services](#47-gatt-database--services)
   - 4.8 [Data Update Cycle](#48-data-update-cycle)
   - 4.9 [Serial Binary Protocol](#49-serial-binary-protocol)
5. [Qt Desktop Application (SensorMonitor)](#5-qt-desktop-application-sensormonitor)
   - 5.1 [Project Structure](#51-project-structure)
   - 5.2 [Build System](#52-build-system)
   - 5.3 [Application Entry Point](#53-application-entry-point)
   - 5.4 [BLE Scanner (BleScanner)](#54-ble-scanner-blescanner)
   - 5.5 [BLE Controller (BleController)](#55-ble-controller-blecontroller)
   - 5.6 [Serial Controller (SerialController)](#56-serial-controller-serialcontroller)
   - 5.7 [Payload Parser (SensorPayloadParser)](#57-payload-parser-sensorpayloadparser)
   - 5.8 [Data Model (SensorData)](#58-data-model-sensordata)
   - 5.9 [Main Window & UI](#59-main-window---ui)
6. [Data Flow](#6-data-flow)
7. [BLE Protocol Specification](#7-ble-protocol-specification)
8. [Serial Protocol Specification](#8-serial-protocol-specification)
9. [Signal & Slot Wiring](#9-signal--slot-wiring)
10. [Configuring & Building](#10-configuring--building)
11. [Troubleshooting](#11-troubleshooting)

---

## 1. Project Overview

This project is an **embedded IoT sensor monitoring system** composed of two components:

| Component | Technology | Purpose |
|-----------|-----------|---------|
| **STM32 Firmware** | C (STM32CubeIDE, HAL) | Reads physical environmental sensors and broadcasts data over BLE and UART |
| **Qt Desktop Application** | C++17, Qt 5 (Widgets, Bluetooth, SerialPort, Charts) | Connects to the STM32 board via BLE or Serial, parses the data, and displays live sensor values in a GUI |

The system monitors:

- **Environmental data** — temperature (HTS221 sensor, °C), humidity (HTS221 sensor, %), and barometric pressure (LPS22HH sensor, hPa)

Data is transmitted concurrently over two channels:
1. **Bluetooth Low Energy (BLE)** — GATT notifications
2. **Serial UART** — Custom binary framed protocol on USART2 (115200 baud, 8N1)

---

## 2. System Architecture

```
┌─────────────────────────────────────────────────────┐
│                   STM32L476RG Nucleo                 │
│                                                      │
│  ┌──────────┐     I2C1      ┌─────────┐             │
│  │  HTS221   │◄──────────────┤         │             │
│  │ (Temp/Hum)│               │  STM32  │             │
│  └──────────┘                │  L476   │    SPI1     │
│                              │         │─────────────┤──► BlueNRG-M0
│  ┌──────────┐     I2C1      │         │             │    (BLE Module)
│  │  LPS22HH  │◄─────────────┤         │             │
│  │ (Pressure)│               │         │    USART2   │
│  └──────────┘                │         │─────────────┤──► USB/Serial
│                              └─────────┘             │
└─────────────────────────────────────────────────────┘
                         │                │
                    BLE (GATT)     UART (Binary Protocol)
                         │                │
                         ▼                ▼
              ┌──────────────────────────────────────┐
              │        Qt SensorMonitor App          │
              │                                      │
              │  ┌──────────────┐  ┌──────────────┐ │
              │  │ BleController │  │SerialController│ │
              │  │  (BLE path)  │  │  (UART path)  │ │
              │  └──────┬───────┘  └──────┬────────┘ │
              │         │     SensorPayloadParser     │
              │         └──────────┬────────────────┘ │
              │                    │                   │
              │            ┌───────▼───────┐          │
              │            │   MainWindow   │          │
              │            │  (UI + Charts)  │          │
              │            └─────────────────┘          │
              └──────────────────────────────────────┘
```

---

## 3. Hardware Platform

### 3.1 Microcontroller

- **MCU**: STM32L476RGTx (ARM Cortex-M4, 80 MHz, 1 MB Flash, 128 KB RAM)
- **Board**: NUCLEO-L476RG development board

### 3.2 BLE Module

- **X-NUCLEO-IDB05A1** expansion board with **BlueNRG-M0** (BlueNRG-2) BLE network processor
- Connected via **SPI1** (PA5=CLK, PA6=MISO, PA7=MOSI, PA1=CS)
- **IRQ line**: PA0 (EXTI0)
- **Reset line**: PA8
- The BlueNRG-2 runs its own BLE stack; the STM32 communicates via the HCI SPI transport layer

### 3.3 Environmental Sensors (I2C1)

| Sensor | Address | Bus | Function |
|--------|---------|-----|----------|
| **HTS221** | 0x5F (7-bit) | I2C1 (PB8=SCL, PB9=SDA) | Temperature and humidity |
| **LPS22HH** | 0xB9 (7-bit, high) | I2C1 | Barometric pressure |

### 3.4 Debug/Serial UART

- **USART2**: PA2 (TX) and PA3 (RX)
- 115200 baud, 8 data bits, no parity, 1 stop bit
- Used for both debug prints and the binary framed data protocol

### 3.5 GPIO Configuration

| Pin | Function |
|-----|----------|
| PA0 | BlueNRG SPI IRQ (EXTI0 interrupt) |
| PA1 | BlueNRG SPI chip select (output) |
| PA5 | SPI1 CLK |
| PA6 | SPI1 MISO |
| PA7 | SPI1 MOSI |
| PA8 | BlueNRG reset (output) |
| PB8 | I2C1 SCL |
| PB9 | I2C1 SDA |
| PC13 | User button (BSP) |
| PA2 | USART2 TX |
| PA3 | USART2 RX |
| PA13/PA14 | SWD debug (JTMS/JTCK) |

---

## 4. STM32 Firmware

### 4.1 Project Structure

```
.
├── Inc/                              # Header files
│   ├── app_bluenrg_2.h               # BlueNRG-2 init/process prototypes
│   ├── ble_list_utils.h              # BLE utility definitions
│   ├── bluenrg_conf.h                # BlueNRG configuration
│   ├── bsp.h                          # BSP (empty, from CubeMX)
│   ├── gatt_db.h                     # GATT service/characteristic definitions
│   ├── gpio.h                         # GPIO init prototype
│   ├── hci_tl_interface.h            # HCI transport layer SPI pin definitions
│   ├── i2c.h                          # I2C handle declaration
│   ├── main.h                         # Main header, pin defines
│   ├── RTE_Components.h              # CMSIS RTE component selection
│   ├── sensor.h                       # SENSOR_DEMO_NAME, GAP callbacks
│   ├── stm32l4xx_hal_conf.h          # HAL configuration
│   ├── stm32l4xx_it.h               # ISR prototypes
│   ├── stm32l4xx_nucleo_bus.h       # BSP SPI bus handle
│   └── stm32l4xx_nucleo_conf.h       # BSP configuration
│
├── Src/                              # Source files
│   ├── main.c                         # Entry point, clock config
│   ├── app_bluenrg_2.c               # BlueNRG init, User_Process, sensor reading
│   ├── bsp.c                          # Empty BSP file
│   ├── gatt_db.c                     # GATT DB creation, update functions, serial protocol
│   ├── gpio.c                         # GPIO initialization
│   ├── hci_tl_interface.c            # HCI SPI transport layer implementation
│   ├── i2c.c                         # I2C1 initialization
│   ├── sensor.c                      # BLE GAP advertising, event handler
│   ├── stm32l4xx_hal_msp.c           # HAL MSP callbacks
│   ├── stm32l4xx_it.c               # ISR handlers (SysTick, EXTI)
│   ├── stm32l4xx_nucleo_bus.c       # BSP SPI1 bus driver (BlueNRG SPI)
│   └── system_stm32l4xx.c           # System clock init (CMSIS)
│
├── Drivers/                           # STM32 HAL + CMSIS libraries
│   └── STM32CubeIDE/Drivers/MesDrivers/
│       ├── hts221_reg.c/.h            # HTS221 temp/humidity driver
│       └── lps22hh_reg.c/.h          # LPS22HH pressure driver
├── Middlewares/ST/                    # BlueNRG-2 BLE stack middleware
├── STM32CubeIDE/                     # CubeIDE project and build output
└── SensorDemo_BLE_l476rg.ioc        # STM32CubeMX configuration file
```

### 4.2 Boot & Initialization Sequence

The firmware follows this initialization order (see `main.c` and `app_bluenrg_2.c`):

```
main()
  │
  ├── HAL_Init()                        # HAL library, SysTick
  ├── SystemClock_Config()              # 80 MHz HSI+PLL
  ├── MX_GPIO_Init()                    # PA0 (IRQ), PA1 (CS), PA8 (RST), PA0 (button)
  ├── MX_I2C1_Init()                    # I2C1 at 400 kHz timing 0x10707DBC
  │
  ├── MX_BlueNRG_2_Init()
  │     ├── hts221_init()               # Configure HTS221 sensor (ODR 1 Hz, BDU, power-on)
  │     ├── lps22hh_init()              # Configure LPS22HH sensor (ODR 10 Hz low-noise, BDU)
  │     ├── User_Init()                 # BSP button, LED, COM (UART)
  │     ├── hci_init()                  # Initialize BlueNRG HCI layer
  │     └── Sensor_DeviceInit()         # BLE GAP/GATT setup
  │           ├── hci_reset()            # Soft reset BLE module
  │           ├── HAL_Delay(2000)       # Wait for BlueNRG boot
  │           ├── getBlueNRGVersion()   # Read HW/FW version
  │           ├── aci_hal_read_config_data() # Read static random address
  │           ├── aci_hal_set_tx_power_level(1, 4) # -2 dBm TX power
  │           ├── aci_gatt_init()        # Initialize GATT layer
  │           ├── aci_gap_init(GAP_PERIPHERAL_ROLE) # GAP peripheral role
  │           ├── aci_gatt_update_char_value() # Set device name "CEDRNAI"
  │           ├── aci_gap_set_authentication_requirement() # BLE security
  │           └── Add_HWServW2ST_Service() # Register HW GATT service
  │
  └── while(1) loop:
        MX_BlueNRG_2_Process()
          ├── hci_user_evt_proc()       # Process pending BLE events
          └── User_Process()            # Sensor update cycle
```

### 4.3 Clock Configuration

The system clock is configured in `SystemClock_Config()` (`main.c:111`):

| Parameter | Value |
|-----------|-------|
| Oscillator | HSI (16 MHz internal) |
| PLL Source | HSI |
| PLLM | 1 |
| PLLN | 8 |
| PLLR | 2 |
| SYSCLK | 80 MHz (PLL = 16MHz × 8 / 2 / 1) |
| AHB Prescaler | /1 → 80 MHz |
| APB1 Prescaler | /1 → 80 MHz |
| APB2 Prescaler | /1 → 80 MHz |
| Flash Latency | 3 WS |

### 4.4 Peripheral Configuration

#### SPI1 (BlueNRG-2 Communication)

- Master mode, configured by BSP (`stm32l4xx_nucleo_bus.c`)
- Used as the transport layer for HCI commands to the BlueNRG-2 BLE module

#### I2C1 (Sensor Communication)

- Timing register: `0x10707DBC` (approximately 400 kHz fast mode)
- 7-bit addressing mode
- Analog and digital filters enabled
- Pins: PB8 (SCL), PB9 (SDA)

#### USART2 (Debug + Binary Data)

- 115200 baud, 8N1
- PA2 (TX), PA3 (RX)
- Used for debug output and the binary framed data protocol

### 4.5 I2C Sensor Drivers

#### HTS221 — Temperature & Humidity Sensor

`hts221_init()` (`app_bluenrg_2.c:118`):

1. Verify device ID (`whoamI == HTS221_ID`)
2. Read calibration coefficients for humidity and temperature linear interpolation
3. Enable Block Data Update (BDU)
4. Set output data rate to **1 Hz** (`HTS221_ODR_1Hz`)
5. Power on the sensor

Temperature and humidity are read using linear interpolation from raw ADC values:

```c
float_t linear_interpolation(lin_t *lin, int16_t x) {
    return ((lin->y1 - lin->y0) * x + ((lin->x1 * lin->y0) - (lin->x0 * lin->y1)))
           / (lin->x1 - lin->x0);
}
```

The I2C platform wrappers (`hts221_platform_write/read`) set bit 0x80 (auto-increment) on the register address for multi-byte reads.

#### LPS22HH — Barometric Pressure Sensor

`lps22hh_init()` (`app_bluenrg_2.c:155`):

1. Wait for boot time (5 ms)
2. Verify device ID (`whoamI == LPS22HH_ID`)
3. Software reset (`lps22hh_reset_set`)
4. Enable Block Data Update
5. Set output data rate to **10 Hz low-noise** (`LPS22HH_10_Hz_LOW_NOISE`)

Pressure is converted from raw LSB to hPa using `lps22hh_from_lsb_to_hpa()`.

### 4.6 BLE Stack (BlueNRG-2)

The BlueNRG-2 module is a standalone BLE network processor. Communication happens over SPI1 using the HCI transport layer (`hci_tl_interface.c`). The STM32 acts as the BLE host, and the BlueNRG-2 runs the BLE controller and link layer.

**Key BLE configuration** (in `Sensor_DeviceInit()`, `app_bluenrg_2.c:365`):

| Setting | Value |
|---------|-------|
| GAP Role | Peripheral |
| TX Power | -2 dBm (level 1, value 4) |
| Device Name | "CEDRNAI" (7 chars) |
| Bonding | Enabled |
| MITM Protection | Required |
| Secure Connections | Supported |
| Fixed PIN | 123456 |
| Address Type | Static Random |
| Advertising Type | `ADV_DATA_TYPE` (configurable in `bluenrg_conf.h`) |

**Advertising payload** (in `Set_DeviceConnectable()`, `sensor.c:60`):
- Complete Local Name: "CEDRNAI"
- TX Power Level: 0 dBm
- Manufacturer Data: SDK version, sensor flags (Temp + Pressure + Humidity), BLE MAC address

**BLE Event Processing** (`APP_UserEvtRx()` in `sensor.c:110`):
- Dispatches LE meta events, vendor-specific events, and standard HCI events
- Connection events are handled by `hci_le_connection_complete_event()` and `hci_disconnection_complete_event()`

### 4.7 GATT Database & Services

One primary GATT service is registered:

#### Service 1 — Hardware Sensor Service (HW_SENS_W2ST)

| Item | UUID (128-bit, little-endian) | Properties |
|------|-------------------------------|------------|
| Service | `00000000-0001-11e1-9ab4-0002a5d5c51b` | Primary |
| Environmental Char | `001c0000-0001-11e1-ac36-0002a5d5c51b` | Notify + Read |

> **Note on Environmental UUID**: The base UUID `00000000-0001-11e1-ac36-0002a5d5c51b` has flag bits OR'd into byte 14: `0x04` (temperature), `0x08` (humidity), `0x10` (pressure), resulting in byte 14 = `0x04 | 0x08 | 0x10 = 0x1C`. This yields the full UUID `001c0000-0001-11e1-ac36-0002a5d5c51b`.

#### Data Update Functions

**`Environmental_Update(int32_t press, int16_t temp, uint16_t hum)`** (`gatt_db.c`):
- Builds a 10-byte little-endian payload
- Updates the GATT characteristic value
- Sends a binary frame over UART2 via `Serial_SendBinary(1, buff, 10)`

### 4.8 Data Update Cycle

The main loop calls `User_Process()` (`app_bluenrg_2.c:502`) every iteration:

1. **If not connected**: Makes the device discoverable via `Set_DeviceConnectable()`
2. **If connected** (or when `USE_BUTTON == 0`, which is the default):
   - Toggles LED2
   - Reads real sensor data: `Set_Environmental_Values()` reads HTS221 and LPS22HH
   - Calls `Environmental_Update()` with pressure (hPa × 100), temperature (°C × 10), humidity (% × 10)
   - Waits 1000 ms (when `USE_BUTTON == 0`)

### 4.9 Serial Binary Protocol

The firmware sends binary frames over USART2 to allow the Qt app to receive the same data via a serial cable. This coexists with any debug ASCII prints by using a unique sync header.

**Frame format**: `[0xA5][0x5A][TYPE][LEN][PAYLOAD...][CHECKSUM]`

| Field | Size | Value |
|-------|------|-------|
| SYNC1 | 1 byte | `0xA5` |
| SYNC2 | 1 byte | `0x5A` |
| TYPE | 1 byte | `1`=Environmental |
| LEN | 1 byte | Payload length in bytes |
| PAYLOAD | LEN bytes | Little-endian binary data (identical to BLE characteristic values) |
| CHECKSUM | 1 byte | XOR of TYPE, LEN, and all PAYLOAD bytes |

A 10 ms delay (`HAL_Delay(10)`) is inserted before each serial frame to avoid mixing with potential debug print output.

---

## 5. Qt Desktop Application (SensorMonitor)

### 5.1 Project Structure

```
SensorMonitor/
├── SensorMonitor.pro          # qmake project file
├── main.cpp                    # Application entry point
├── ble/
│   ├── BleScanner.h/.cpp      # BLE device discovery
│   ├── BleController.h/.cpp   # BLE connection & GATT management
│   └── SensorPayloadParser.h/.cpp  # Binary payload decoding
├── serial/
│   └── SerialController.h/.cpp    # UART serial port management
├── model/
│   └── SensorData.h            # Q_PROPERTY data model (unused in current impl)
└── ui/
    ├── MainWindow.h/.cpp/.ui  # Main window GUI
```

### 5.2 Build System

**File**: `SensorMonitor.pro`

```
QT += core gui widgets bluetooth charts serialport
CONFIG += c++17
```

Required Qt modules:
- **core** — Core non-GUI functionality
- **gui** — Window system integration
- **widgets** — Qt Widgets (labels, buttons, layouts)
- **bluetooth** — QLowEnergyController, QBluetoothDeviceInfo for BLE
- **charts** — QChart, QLineSeries, QValueAxis for temperature chart
- **serialport** — QSerialPort, QSerialPortInfo for UART communication

**Build commands (Linux)**:
```bash
cd SensorMonitor
/usr/lib/qt5/bin/qmake
make -j$(nproc)
```

### 5.3 Application Entry Point

**File**: `main.cpp`

```cpp
int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
```

Standard Qt Widgets application entry point. Creates a `MainWindow` instance and starts the event loop.

### 5.4 BLE Scanner (BleScanner)

**Files**: `ble/BleScanner.h`, `ble/BleScanner.cpp`

**Purpose**: Discovers BLE peripheral devices and filters for the target device name `CEDRNAI`.

**Class**: `BleScanner : public QObject`

| Member | Description |
|--------|-------------|
| `m_agent` | `QBluetoothDeviceDiscoveryAgent*` — Qt BLE scanner |
| `TARGET_NAME` | `"CEDRNAI"` — filters discovered devices by this name |

**Flow**:
1. `startScan()` → calls `m_agent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod)`
2. Sets a 10-second low-energy discovery timeout
3. `onDeviceDiscovered()` → checks if `device.name() == "CEDRNAI"`, stops scanning, emits `deviceFound(device)`
4. `onScanFinished()` → called when the scan completes (no special handling)
5. `onError()` → emits `errorOccurred` with error string

**Signals**:
- `deviceFound(const QBluetoothDeviceInfo &device)` — emitted when the target device is found
- `errorOccurred(const QString &message)` — emitted on scan errors

### 5.5 BLE Controller (BleController)

**Files**: `ble/BleController.h`, `ble/BleController.cpp`

**Purpose**: Manages the full BLE GATT lifecycle — connection, service discovery, characteristic notification subscription, and data parsing.

**Class**: `BleController : public QObject`

**BLE UUID Constants**:

| Constant | UUID |
|----------|------|
| `HW_SERVICE_UUID` | `{00000000-0001-11e1-9ab4-0002a5d5c51b}` |
| `ENV_CHAR_UUID` | `{001c0000-0001-11e1-ac36-0002a5d5c51b}` |

**Flow**:
1. `connectToDevice(device)` → creates a `QLowEnergyController`, connects signals, calls `connectToDevice()`
2. `onControllerConnected()` → emits `connected()`, calls `discoverServices()`
3. `onDiscoveryFinished()` → creates service object for `HW_SERVICE_UUID`, calls `discoverDetails()`
4. `onServiceStateChanged(ServiceDiscovered)` → iterates characteristics, enables notifications by writing `0x0100` to the CCCD descriptor for `ENV_CHAR_UUID`
5. `onCharacteristicChanged()` → identifies the characteristic by UUID, parses the payload using `SensorPayloadParser`, and emits typed signals

**Signals emitted**:
- `connected()` — BLE connection established
- `disconnected()` — BLE connection lost
- `environmentalUpdated(double tempC, double pressHpa, double humPct)` — parsed env data
- `errorOccurred(const QString &message)` — BLE error

### 5.6 Serial Controller (SerialController)

**Files**: `serial/SerialController.h`, `serial/SerialController.cpp`

**Purpose**: Manages the UART serial connection and implements a state machine to parse the framed binary protocol.

**Class**: `SerialController : public QObject`

**UART Configuration**:
- Baud rate: 115200
- Data bits: 8
- Parity: None
- Stop bits: 1
- Flow control: None

**State Machine** (`SerialController::processByte()`):

```
IDLE ──[0xA5]──► SYNC2 ──[0x5A]──► TYPE ──► LEN ──► PAYLOAD ──► CHECKSUM ──► IDLE
                     │                   │       │                     │
                  [0xA5] stays      [!0xA5]      [=0] -->            fail -->
                  in SYNC2           → IDLE     CHECKSUM
```

| State | Transition |
|-------|------------|
| `IDLE` | If byte == `0xA5`, go to `SYNC2`; else stay in `IDLE` |
| `SYNC2` | If byte == `0x5A`, go to `TYPE`; if `0xA5`, stay in `SYNC2`; else go to `IDLE` |
| `TYPE` | Store byte as frame type, go to `LEN` |
| `LEN` | Store byte as payload length; if > 40, reset to `IDLE` (sanity check); if 0, go to `CHECKSUM`; else go to `PAYLOAD` |
| `PAYLOAD` | Append byte to current payload; if payload complete (size == LEN), go to `CHECKSUM` |
| `CHECKSUM` | Verify checksum (XOR of type, len, and all payload bytes); if valid, call `processFrame()`; go to `IDLE` |

**Frame Processing** (`processFrame()`):
- Type `1` (Environmental): Reuses `SensorPayloadParser::parseEnvironmental()`

**Signals emitted** (same interface as `BleController`):
- `connected()`, `disconnected()`, `environmentalUpdated()`, `errorOccurred()`

### 5.7 Payload Parser (SensorPayloadParser)

**Files**: `ble/SensorPayloadParser.h`, `ble/SensorPayloadParser.cpp`

**Purpose**: Stateless utility class that decodes raw `QByteArray` payloads from either BLE notifications or serial frames into typed C++ structs.

**Structures**:

```cpp
struct EnvironmentalSample {
    double tempC;      // Temperature in °C
    double pressHpa;   // Pressure in hPa
    double humPct;     // Humidity in %
    quint16 timestamp; // Raw timestamp (HAL_GetTick() >> 3)
};
```

**Parsing — Environmental** (`parseEnvironmental`):
- Minimum data size: 10 bytes
- Byte layout:

| Offset | Size | Parse | Conversion |
|--------|------|-------|------------|
| 0 | 2 | `qFromLittleEndian<quint16>` | Timestamp |
| 2 | 4 | `qFromLittleEndian<qint32>` | `value / 100.0` → pressure in hPa |
| 6 | 2 | `qFromLittleEndian<quint16>` | `value / 10.0` → humidity in % |
| 8 | 2 | `qFromLittleEndian<qint16>` | `value / 10.0` → temperature in °C |

### 5.8 Data Model (SensorData)

**File**: `model/SensorData.h`

A `QObject`-based data model with `Q_PROPERTY` declarations for temperature, pressure, and humidity. Currently defined but **not actively used** in the UI — the `MainWindow` directly receives data via signal/slot connections from the controllers.

```cpp
class SensorData : public QObject {
    Q_OBJECT
    Q_PROPERTY(double temperature READ temperature WRITE setTemperature NOTIFY temperatureChanged)
    Q_PROPERTY(double pressure READ pressure WRITE setPressure NOTIFY pressureChanged)
    Q_PROPERTY(double humidity READ humidity WRITE setHumidity NOTIFY humidityChanged)
    // ...
};
```

This class is intended for future use with QML or declarative bindings but is not wired into the current implementation.

### 5.9 Main Window & UI

**Files**: `ui/MainWindow.h`, `ui/MainWindow.cpp`, `ui/MainWindow.ui`

#### UI Designer Layout (MainWindow.ui)

The `.ui` file defines a `QMainWindow` (800×600, title: "STM32 BLE Sensor Monitor") with:

```
QMainWindow "MainWindow"
└── QWidget "centralwidget"
    └── QVBoxLayout "verticalLayout"
        ├── QHBoxLayout "toolbarLayout"
        │   ├── QPushButton "scanButton"     [text: "Scan"]
        │   ├── QPushButton "connectButton"   [text: "Connect", enabled: false]
        │   ├── QPushButton "disconnectButton" [text: "Disconnect", enabled: false]
        │   ├── QSpacerItem "horizontalSpacer"
        │   └── QLabel "statusLabel"           [text: "Status: Idle"]
        │
        ├── QHBoxLayout "contentLayout"
        │   └── QGroupBox "environmentalGroupBox" [title: "Environmental"]
        │       └── QVBoxLayout "envLayout"
        │           ├── QLabel "tempLabel"   [text: "Temp: -- °C"]
        │           ├── QLabel "pressLabel"  [text: "Press: -- hPa"]
        │           ├── QLabel "humLabel"    [text: "Hum: -- %"]
        │           └── (Temperature chart added at runtime)
        │
        └── QPlainTextEdit "logOutput"  [readOnly: true]
```

#### Dynamically Added UI Elements (in `setupSerialUi()`)

The following controls are added programmatically to `toolbarLayout` at runtime:

| Widget | Object Name | Purpose |
|--------|-------------|---------|
| `QGroupBox` | `"modeGroup"` | Contains mode selection radio buttons |
| `QRadioButton` | — | "BLE" mode (default checked) |
| `QRadioButton` | — | "Serial" mode |
| `QComboBox` | `"portCombo"` | Lists available serial ports |
| `QPushButton` | `"refreshBtn"` | Refresh serial port list |

#### Mode Switching

The `MainWindow::onModeChanged()` slot toggles UI elements based on the selected mode:

| Mode | Visible | Hidden | Connect activates |
|------|---------|--------|-------------------|
| BLE | Scan button | Port combo, Refresh button | BLE `connectToDevice()` |
| Serial | Port combo, Refresh button | Scan button | Serial `connectToPort()` |

#### Chart Setup (`setupCharts()`)

A real-time temperature chart is created programmatically:

```
QChartView
└── QChart "Temperature History"
    ├── QLineSeries "m_tempSeries" (temperature values)
    ├── QValueAxis "m_axisX" (range: 0–60, title: "Samples")
    ├── QValueAxis "m_axisY" (range: 20–30, title: "Temp (°C)")
```

- The X-axis scrolls: when `m_sampleCount > 60`, the range shifts to `(count-60, count)`
- The Y-axis auto-expands when a value falls outside the current range (±1 unit margin)
- The chart is rendered with `QPainter::Antialiasing`
- The chart view is inserted into `envLayout` (the Environmental group box layout)

#### Data Display

When `onEnvironmentalUpdated(tempC, pressHpa, humPct)` is called:
- `tempLabel` → `"Temp: XX.X °C"` (1 decimal)
- `pressLabel` → `"Press: XXXX.XX hPa"` (2 decimals)
- `humLabel` → `"Hum: XX.X %"` (1 decimal)
- Temperature value is appended to the chart line series
- Axis ranges auto-adjust as data accumulates

#### Logging

The `log()` method appends timestamped messages to `logOutput`:
```
[HH:MM:SS] Message text
```

---

## 6. Data Flow

### 6.1 BLE Path

```
STM32 Sensors → User_Process() (1 Hz)
    │
    ├── HTS221 (I2C) → hts221_temperature_raw_get() / hts221_humidity_raw_get()
    │   └── linear_interpolation() → °C, %RH
    │
    ├── LPS22HH (I2C) → lps22hh_pressure_raw_get()
    │   └── lps22hh_from_lsb_to_hpa() → hPa
    │
    ▼
    Environmental_Update(press*100, temp*10, hum*10)
        │
        ├── aci_gatt_update_char_value() → BLE GATT Notification
        │                                       │
        │                          ┌──────────────┴────────────────┐
        │                     BlueNRG-2                       Qt App
        │                  (BLE Radio)              BleController::onCharacteristicChanged()
        │                                                    │
        │                                          SensorPayloadParser::parseEnvironmental()
        │                                                    │
        │                                          emit environmentalUpdated(...)
        │                                                    │
        │                                          MainWindow::onEnvironmentalUpdated()
        │                                                    │
        │                                          Update QLabels + QChart
        └──────────────────────────────────────────────────┘
```

### 6.2 Serial Path

```
STM32 Sensors → User_Process()
    │
    ├── Environmental_Update()
    │       │
    │       ├── aci_gatt_update_char_value() → BLE path (above)
    │       └── Serial_SendBinary(type, buff, len)
    │               │
    │               ├── HAL_UART_Transmit(&huart2, header, 4, 100)
    │               ├── HAL_UART_Transmit(&huart2, payload, len, 100)
    │               └── HAL_UART_Transmit(&huart2, &checksum, 1, 100)
    │                       │
    │               USB/Serial Cable (/dev/ttyACM0)
    │                       │
    └──────────────────► SerialController::onReadyRead()
                            │
                            m_serial->readAll()
                            │
                            processByte() × N → State machine
                            │
                            processFrame(type, payload)
                                │
                                SensorPayloadParser::parseEnvironmental()
                                │
                                emit environmentalUpdated(...)
                                │
                                MainWindow::onEnvironmentalUpdated()
```

---

## 7. BLE Protocol Specification

### 7.1 GATT Services & Characteristics

#### Hardware Sensor Service

- **Service UUID**: `00000000-0001-11e1-9ab4-0002a5d5c51b`
- **Type**: Primary Service

| Characteristic | UUID | Properties | Size | Description |
|---------------|------|-----------|------|-------------|
| Environmental | `001c0000-0001-11e1-ac36-0002a5d5c51b` | Notify + Read | 10 bytes | Temp, Pressure, Humidity |

### 7.2 Notification Subscription

To enable notifications, the client writes `0x0100` to the Client Characteristic Configuration Descriptor (CCCD) of the characteristic:

```cpp
QLowEnergyDescriptor cccd = characteristic.descriptor(
    QBluetoothUuid::ClientCharacteristicConfiguration);
if (cccd.isValid()) {
    service->writeDescriptor(cccd, QByteArray::fromHex("0100"));
}
```

### 7.3 UUID Byte Order

The firmware stores UUIDs as little-endian byte arrays. Qt's `QUuid` uses big-endian string representation. The `BleController` constructs UUIDs using `QBluetoothUuid(QUuid("{...}"))` format, matching the standard big-endian string notation.

---

## 8. Serial Protocol Specification

### 8.1 Frame Format

| Byte Index | Field | Value |
|-----------|-------|-------|
| 0 | SYNC1 | `0xA5` |
| 1 | SYNC2 | `0x5A` |
| 2 | TYPE | `1` (Environmental) |
| 3 | LEN | Payload length in bytes |
| 4..(4+LEN-1) | PAYLOAD | Little-endian data (matches BLE characteristic format) |
| 4+LEN | CHECKSUM | XOR of TYPE, LEN, and all PAYLOAD bytes |

### 8.2 Environmental Payload (Type 1, 10 bytes)

| Offset | Size | Type | Field | Conversion |
|--------|------|------|-------|------------|
| 0 | 2 | uint16 | Timestamp | `HAL_GetTick() >> 3` (ms / 8) |
| 2 | 4 | int32 | Pressure | `value / 100.0` → hPa |
| 6 | 2 | uint16 | Humidity | `value / 10.0` → % |
| 8 | 2 | int16 | Temperature | `value / 10.0` → °C |

---

## 9. Signal & Slot Wiring

All connections are made in `MainWindow::MainWindow()` (`ui/MainWindow.cpp:25`):

### Button Connections

| Signal | Slot |
|--------|------|
| `scanButton::clicked()` | `MainWindow::onScanClicked()` |
| `connectButton::clicked()` | `MainWindow::onConnectClicked()` |
| `disconnectButton::clicked()` | `MainWindow::onDisconnectClicked()` |

### BLE Scanner Signals

| Signal | Slot |
|--------|------|
| `BleScanner::deviceFound(device)` | `MainWindow::onDeviceFound(device)` |
| `BleScanner::errorOccurred(msg)` | `MainWindow::onError(msg)` |

### BLE Controller Signals

| Signal | Slot |
|--------|------|
| `BleController::connected()` | `MainWindow::onConnected()` |
| `BleController::disconnected()` | `MainWindow::onDisconnected()` |
| `BleController::errorOccurred(msg)` | `MainWindow::onError(msg)` |
| `BleController::environmentalUpdated(tempC, pressHpa, humPct)` | `MainWindow::onEnvironmentalUpdated(tempC, pressHpa, humPct)` |

### Serial Controller Signals

| Signal | Slot |
|--------|------|
| `SerialController::connected()` | `MainWindow::onConnected()` |
| `SerialController::disconnected()` | `MainWindow::onDisconnected()` |
| `SerialController::errorOccurred(msg)` | `MainWindow::onError(msg)` |
| `SerialController::environmentalUpdated(tempC, pressHpa, humPct)` | `MainWindow::onEnvironmentalUpdated(tempC, pressHpa, humPct)` |

### Mode Selection

| Signal | Slot |
|--------|------|
| `QButtonGroup::buttonClicked(int)` | Lambda → sets `m_mode`, calls `onModeChanged()` |
| `refreshBtn::clicked()` | `MainWindow::onRefreshPortsClicked()` |

---

## 10. Configuring & Building

### 10.1 STM32 Firmware

**Requirements**:
- STM32CubeIDE (tested with version generating .ioc for L476RG)
- X-CUBE-BLE2 middleware (included in project)
- STM32Cube HAL library for L476xx

**Steps**:
1. Open STM32CubeIDE
2. Import the project: `File > Import... > Existing Projects into Workspace`
3. Build the project (Project > Build All)
4. Flash to the NUCLEO-L476RG board (Run > Debug or Run > Run)
5. The firmware will start advertising as "CEDRNAI" and sending data over USART2

### 10.2 Qt Application

**Dependencies (Linux)**:
```bash
sudo apt update
sudo apt install qtbase5-dev qtconnectivity5-dev libqt5serialport5-dev libqt5charts5-dev build-essential
```

**Build**:
```bash
cd SensorMonitor
/usr/lib/qt5/bin/qmake
make -j$(nproc)
```

**Run**:
```bash
./SensorMonitor
```

**Linux Permissions** (required for Bluetooth and serial port access):
```bash
sudo usermod -aG dialout,lp,bluetooth $USER
# Log out and log back in for groups to take effect
```

---

## 11. Troubleshooting

### BLE Issues

| Problem | Cause | Solution |
|---------|-------|----------|
| Device not found during scan | BLE adapter not available | Check `rfkill list` — ensure Bluetooth is not blocked. Install BlueZ 5.x |
| Connection fails immediately | BLE security mismatch | Try removing authentication or using a different BlueZ version |
| Services not discovered | UUID mismatch | Verify UUID constants in `BleController.h` match the firmware GATT definitions |
| No notifications received | CCCD not written | Ensure `writeDescriptor(cccd, 0x0100)` is called. Check BlueZ permissions |
| Data values seem wrong | Byte order / scaling | Verify little-endian parsing and scale factors (e.g., temp / 10.0) |

### Serial Issues

| Problem | Cause | Solution |
|---------|-------|----------|
| Port not listed | Permission denied | Add user to `dialout` group |
| Garbled data | Baud rate mismatch | Confirm 115200 8N1 on both sides |
| Frames not parsing | Noise on serial line | The state machine will re-sync on `0xA5 0x5A` patterns |
| Checksum failures | Data corruption | Verify cable quality; check for debug prints interleaving |

### Firmware Issues

| Problem | Cause | Solution |
|---------|-------|----------|
| HTS221 not found | I2C bus issue | Check PB8/PB9 connections; verify I2C addressing |
| LPS22HH not found | I2C bus issue | Check sensor is powered; verify I2C address 0xB9 (high) |
| BlueNRG not responding | SPI wiring | Verify PA0/PA1/PA5/PA6/PA7/PA8 connections to X-NUCLEO-IDB05A1 |
| Hard fault at boot | Clock config issue | Verify HSI PLL settings match target frequency |
| LED2 stays on | BLE init failed | Check `Error_Handler()`, verify BlueNRG-2 module is functional |

### Debug Tips

- **Enable Qt BLE debug logging**: Set `QT_LOGGING_RULES="qt.bluetooth*=true"` before running the app
- **Serial debug output**: The firmware prints debug messages via USART2; these can be viewed with a serial monitor alongside the binary protocol data
- **BlueNRG-2 debug**: `PRINT_DBG()` macro outputs to USART2; can be enabled/disabled in `bluenrg_conf.h`

---

## 12. Changelog — Bug Fixes Applied

### Fix 1: Environmental_Update timestamp was hardcoded to 0

**File**: `Src/gatt_db.c:311`

**Before**: `HOST_TO_LE_16(buff, 0);`
**After**: `HOST_TO_LE_16(buff, HAL_GetTick()>>3);`

The `Environmental_Update()` function was sending a zero timestamp. Now it uses the proper tick-based timestamp.

### Fix 2: Read_Request_CB now reads real sensor data

**Files**: `Src/gatt_db.c:279-293`, `Src/app_bluenrg_2.c:566`, `Inc/app_bluenrg_2.h:54`

**Before**: `Read_Request_CB()` used hardcoded emulated values (`data_t = 27.0 + rand()`, `data_h = 0.f`) when a GATT Read was received on the Environmental characteristic.
**After**: The function now calls `Set_Environmental_Values()` (made non-static, prototype added to `app_bluenrg_2.h`) to read real HTS221 and LPS22HH data before calling `Environmental_Update()`.

### Fix 3: Removed all motion/IMU code (LSM6DSO, LIS2MDL, AccGyroMag, Quaternions)

The project only uses environmental sensors (HTS221 for temperature/humidity, LPS22HH for pressure). Motion sensors (LSM6DSO accelerometer/gyroscope and LIS2MDL magnetometer) were never intended to be part of this project. All motion-related code has been removed from both the firmware and Qt application.

**Firmware changes**:
- Removed LSM6DSO and LIS2MDL driver files from `STM32CubeIDE/Drivers/MesDrivers/`
- Removed LSM6DSO/LIS2MDL includes, defines, externs, platform wrappers, init functions, and `Set_Real_Motion_Values`/`Set_Random_Motion_Values`/`Reset_Motion_Values` from `Src/app_bluenrg_2.c`
- Removed `Add_SWServW2ST_Service` call and `srand` call from `MX_BlueNRG_2_Init`
- Removed AccGyroMag characteristic and SW service (Quaternions) from GATT database in `Src/gatt_db.c`
- Removed `Acc_Update` and `Quat_Update` functions, `AxesRaw_t` typedef, serial frame types 2 and 3
- Removed AccGyroMagCharHandle and QuaternionsCharHandle from `Inc/gatt_db.h`
- Removed motion variables from `Src/sensor.c`

**Qt app changes**:
- Removed `MotionSample` struct and `parseMotion()` from `SensorPayloadParser`
- Removed `accGyroMagUpdated` signal from `BleController` and `SerialController`
- Removed SW service discovery and ACC/QUAT characteristic subscriptions from `BleController`
- Removed frame type 2 (Motion) handling from `SerialController::processFrame()`
- Removed `onAccGyroMagUpdated` slot and signal connections from `MainWindow`
- Removed Motion (IMU) GroupBox (accLabel, gyroLabel, magLabel) from `MainWindow.ui`
- Removed `QVector3D` dependencies throughout the Qt app