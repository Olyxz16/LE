# Qt Application Specification
## STM32 BLE Sensor Monitor

---

## 1. Project Overview

### 1.1 Context

The STM32 firmware (`SensorDemo_BLE_l476rg`) runs on an **STM32L476RG Nucleo** board and exposes sensor data over **Bluetooth Low Energy (BLE)** using a **BlueNRG-2** module. The board reads from two physical sensors over I2C:

- **HTS221** — temperature and humidity sensor
- **LPS22HH** — barometric pressure sensor

Motion data (accelerometer, gyroscope, magnetometer) is currently **emulated** in software (random-walk generation).

The Qt application must act as a BLE Central device, connect to the board, subscribe to its GATT notifications, parse the binary payloads, and display the live sensor values.

---

## 2. STM32 BLE Interface — What the Qt App Must Speak

### 2.1 BLE Advertisement

The device advertises with the local name **`CEDNAI`** (defined as `SENSOR_DEMO_NAME` in `sensor.h`). The Qt app must scan for this name to find the device.

### 2.2 GATT Services and Characteristics

The firmware defines **two GATT services**, each with custom 128-bit UUIDs from the ST W2ST profile family.

#### Service 1 — Hardware Sensor Service (`HW_SENS_W2ST`)

| Item | UUID (128-bit, little-endian) |
|------|-------------------------------|
| Service | `00 00 00 00 00 01 11 E1 9A B4 00 02 A5 D5 C5 1B` |
| Environmental Characteristic | `00 00 00 00 00 01 11 E1 AC 36 00 02 A5 D5 C5 1B` (with byte 14 flags OR'd in) |
| AccGyroMag Characteristic | `00 E0 00 00 00 01 11 E1 AC 36 00 02 A5 D5 C5 1B` |

> **Note:** The Environmental UUID has three flag bits OR'd into byte 14 at runtime: `0x04` (temperature), `0x08` (humidity), `0x10` (pressure). The final UUID byte 14 value is `0x1C`.

#### Service 2 — Software Sensor Service (`SW_SENS_W2ST`)

| Item | UUID (128-bit, little-endian) |
|------|-------------------------------|
| Service | `00 00 00 00 00 02 11 E1 9A B4 00 02 A5 D5 C5 1B` |
| Quaternions Characteristic | `00 00 01 00 00 01 11 E1 AC 36 00 02 A5 D5 C5 1B` |

### 2.3 Payload Formats (Little-Endian)

All multi-byte integers are **little-endian**.

#### Environmental Characteristic — 10 bytes total

| Offset | Size | Type | Field | Scale / Unit |
|--------|------|------|-------|--------------|
| 0 | 2 | `uint16_t` | Timestamp | `HAL_GetTick() >> 3` (ms / 8) |
| 2 | 4 | `int32_t` | Pressure | value / 100 → hPa |
| 6 | 2 | `uint16_t` | Humidity | value / 10 → % RH |
| 8 | 2 | `int16_t` | Temperature | value / 10 → °C |

**Example parsing (C++ / Qt):**
```cpp
quint16 ts   = qFromLittleEndian<quint16>(data.mid(0, 2));
qint32  press = qFromLittleEndian<qint32>(data.mid(2, 4));
quint16 hum  = qFromLittleEndian<quint16>(data.mid(6, 2));
qint16  temp  = qFromLittleEndian<qint16>(data.mid(8, 2));

double pressureHpa = press / 100.0;
double humidityPct = hum  / 10.0;
double tempCelsius = temp / 10.0;
```

#### AccGyroMag Characteristic — 20 bytes total

| Offset | Size | Type | Field | Notes |
|--------|------|------|-------|-------|
| 0 | 2 | `uint16_t` | Timestamp | `HAL_GetTick() >> 3` |
| 2 | 2 | `int16_t` | Acc X | negated on firmware side |
| 4 | 2 | `int16_t` | Acc Y | |
| 6 | 2 | `int16_t` | Acc Z | negated on firmware side |
| 8 | 2 | `int16_t` | Gyro X | |
| 10 | 2 | `int16_t` | Gyro Y | |
| 12 | 2 | `int16_t` | Gyro Z | |
| 14 | 2 | `int16_t` | Mag X | |
| 16 | 2 | `int16_t` | Mag Y | |
| 18 | 2 | `int16_t` | Mag Z | |

#### Quaternions Characteristic — 8 bytes total (SEND_N_QUATERNIONS = 1)

| Offset | Size | Type | Field |
|--------|------|------|-------|
| 0 | 2 | `uint16_t` | Timestamp |
| 2 | 2 | `int16_t` | Q X |
| 4 | 2 | `int16_t` | Q Y |
| 6 | 2 | `int16_t` | Q Z |

### 2.4 Notification vs Read

- **Environmental**: supports both `NOTIFY` and `READ`. The firmware updates the value on every BLE `Read_Request_CB` and also pushes notifications on a timer.
- **AccGyroMag**: `NOTIFY` only — subscribing is required.
- **Quaternions**: `NOTIFY` only.

The Qt app should **enable notifications** (write `0x0100` to the CCCD) on all three characteristics after connecting.

---

## 3. Qt Application Architecture

### 3.1 Recommended Module Structure

```
SensorMonitor/
├── main.cpp
├── SensorMonitor.pro          # or CMakeLists.txt
│
├── ble/
│   ├── BleScanner.h/.cpp      # Device discovery
│   ├── BleController.h/.cpp   # Connection & GATT management
│   └── SensorPayloadParser.h/.cpp  # Binary payload decoding
│
├── model/
│   └── SensorData.h           # Plain data structs / Q_PROPERTY holders
│
└── ui/
    ├── MainWindow.h/.cpp/.ui
    ├── EnvironmentalWidget.h/.cpp
    └── MotionWidget.h/.cpp
```

### 3.2 Qt Modules Required

Add to `.pro` or `CMakeLists.txt`:

```
QT += core gui widgets bluetooth charts
```

### 3.3 Class Responsibilities

#### `BleScanner`

Wraps `QBluetoothDeviceDiscoveryAgent`. Emits `deviceFound(QBluetoothDeviceInfo)` when a device advertising the name `CEDNAI` is detected. Stops scanning automatically once the target is found, or after a configurable timeout.

#### `BleController`

Wraps `QLowEnergyController` and manages the full BLE lifecycle:

1. Create controller for the target device info.
2. Call `connectToDevice()` and handle `connected()` signal.
3. Call `discoverServices()` and handle `discoveryFinished()`.
4. For each expected service UUID, get the `QLowEnergyService` object and call `discoverDetails()`.
5. Once `stateChanged(ServiceDiscovered)` fires, locate each characteristic by UUID, write `0x0100` to its CCCD descriptor to enable notifications.
6. Connect `characteristicChanged(QLowEnergyCharacteristic, QByteArray)` → forward to parser.

**Signals to expose:**
```cpp
signals:
  void connected();
  void disconnected();
  void environmentalUpdated(double tempC, double pressHpa, double humPct);
  void accGyroMagUpdated(QVector3D acc, QVector3D gyro, QVector3D mag);
  void quaternionUpdated(QVector3D q);
  void errorOccurred(QString message);
```

#### `SensorPayloadParser`

Stateless helper with static methods. Accepts a raw `QByteArray` and returns a structured value. Contains all the byte-offset and scaling logic described in Section 2.3.

```cpp
struct EnvironmentalSample { double tempC, pressHpa, humPct; quint16 timestamp; };
struct MotionSample { QVector3D acc, gyro, mag; quint16 timestamp; };

static EnvironmentalSample parseEnvironmental(const QByteArray &data);
static MotionSample parseMotion(const QByteArray &data);
```

#### `SensorData` (Model)

A `QObject` with `Q_PROPERTY` declarations for each sensor value. Used to drive the UI via Qt's property system or direct signal/slot connections.

---

## 4. UI Specification

### 4.1 Main Window Layout

```
┌──────────────────────────────────────────────────────┐
│  [Scan]  [Connect]  [Disconnect]   Status: Connected │
├──────────────────────────────────────────────────────┤
│  ┌──────────────────┐  ┌───────────────────────────┐ │
│  │  Environmental   │  │      Motion (IMU)         │ │
│  │                  │  │                           │ │
│  │  Temp:   24.3 °C │  │  Acc:  X:  12  Y: -5  Z:3 │ │
│  │  Press: 1013 hPa │  │  Gyro: X:   0  Y:  1  Z:0 │ │
│  │  Hum:   55.0 %   │  │  Mag:  X: 400  Y:200  Z:50│ │
│  │                  │  │                           │ │
│  │  [Temp chart]    │  │  [3-axis strip chart]     │ │
│  └──────────────────┘  └───────────────────────────┘ │
│  ┌──────────────────────────────────────────────────┐ │
│  │  Log / Status                                    │ │
│  └──────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────┘
```

### 4.2 Toolbar / Controls

| Control | Behavior |
|---------|----------|
| **Scan** button | Starts `BleScanner`; populates a device combo-box (or auto-selects `CEDNAI`) |
| **Connect** button | Initiates BLE connection to selected device |
| **Disconnect** button | Calls `disconnectFromDevice()` on `QLowEnergyController` |
| Status label | Shows: `Idle` / `Scanning…` / `Connecting…` / `Connected` / `Error: <msg>` |

### 4.3 Environmental Widget

Display as numeric labels and optionally a scrolling `QChart` (line series) for temperature and/or pressure history. Recommended history: 60 seconds at ~1 Hz.

| Field | Format |
|-------|--------|
| Temperature | `24.3 °C` |
| Pressure | `1013.25 hPa` |
| Humidity | `55.0 %` |

### 4.4 Motion Widget

Display current X/Y/Z values for each of the three sensor groups as `QLabel` or a `QTableWidget`. Optionally add a scrolling 3-axis `QChart` for accelerometer data.

Units are raw sensor LSBs (the firmware does not apply a physical-unit conversion for the emulated data). If real IMU sensors are added later, apply the appropriate scale factor (typically mg for accelerometer, mdps for gyroscope, mGauss for magnetometer).

### 4.5 Log Panel

A `QPlainTextEdit` (read-only) appended to with timestamped log lines for BLE events:
```
[12:34:01] Scan started
[12:34:03] Device found: CEDNAI (AA:BB:CC:DD:EE:FF)
[12:34:04] Connected
[12:34:04] Services discovered: 2
[12:34:05] Notifications enabled on Environmental characteristic
```

---

## 5. BLE UUID Reference for Qt

Use `QBluetoothUuid` constructed from the 128-bit values. Qt's constructor takes a `QUuid` or a string in `{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}` format.

```cpp
// Hardware Service
const QBluetoothUuid HW_SERVICE_UUID(
    QUuid("{00000000-0001-11e1-9ab4-0002a5d5c51b}"));

// Environmental characteristic (byte 14 = 0x1C after flag OR-ing)
const QBluetoothUuid ENV_CHAR_UUID(
    QUuid("{00001c00-0001-11e1-ac36-0002a5d5c51b}"));

// AccGyroMag characteristic
const QBluetoothUuid ACC_CHAR_UUID(
    QUuid("{00e00000-0001-11e1-ac36-0002a5d5c51b}"));

// Software Service
const QBluetoothUuid SW_SERVICE_UUID(
    QUuid("{00000000-0002-11e1-9ab4-0002a5d5c51b}"));

// Quaternions characteristic
const QBluetoothUuid QUAT_CHAR_UUID(
    QUuid("{00000100-0001-11e1-ac36-0002a5d5c51b}"));
```

> **UUID byte order note:** The 128-bit arrays in the firmware source are stored in little-endian order (index 0 = LSB). Qt's `QUuid` string representation is big-endian. Verify UUIDs match by comparing the raw bytes when first connecting.

---

## 6. Error Handling

| Scenario | Recommended Handling |
|----------|----------------------|
| No device found after scan timeout | Show dialog; offer retry |
| Connection dropped | Emit `disconnected()`; update status label; offer reconnect |
| GATT service not found | Log a warning; disable the corresponding UI panel |
| Payload too short | Discard packet; log a warning |
| BLE not available on host | Show error at startup using `QBluetoothLocalDevice::isValid()` |

---

## 7. Implementation Notes

### 7.1 Platform

Qt Bluetooth requires platform BLE support:
- **Linux**: BlueZ 5.x; may need `sudo` or `cap_net_admin` capability.
- **Windows**: Windows 10 RS2+ with WinRT backend (Qt ≥ 5.12).
- **macOS**: CoreBluetooth backend.

Set `QT_LOGGING_RULES="qt.bluetooth*=true"` during development for verbose BLE logs.

### 7.2 Threading

`QLowEnergyController` must live on the **main thread** (it is event-loop driven). Do not move it to a worker thread. Keep payload parsing synchronous in the `characteristicChanged` slot — it is fast enough.

### 7.3 Notification Rate

The firmware sends Environmental updates on user-button press and on periodic timer (approximately every 100 ms based on `APP_UPDATE_PERIOD`). AccGyroMag is also periodic. Design the UI to handle up to **10 updates/second** without blocking the event loop.

### 7.4 Physical Sensor vs Emulated Data

Currently in the firmware:
- **HTS221 temperature and humidity** are read from the real sensor over I2C.
- **LPS22HH pressure** is read from the real sensor over I2C.
- **Accelerometer/Gyroscope/Magnetometer** are **software-emulated** (random walk). No physical IMU is connected yet. The `X_OFFSET`, `Y_OFFSET`, `Z_OFFSET` constants in `gatt_db.h` are defined but not applied in the current emulation path.

The Qt application should work identically whether the data is real or emulated.

---

## 8. Recommended Qt Version

Qt 6.x (6.4 or later) is recommended for its improved Bluetooth stability on all platforms. Qt 5.15 LTS is the minimum viable version.

---

## 9. Suggested Development Order

1. **Step 1 — Skeleton project**: Create Qt Widgets application with `MainWindow`, toolbar, and placeholder panels.
2. **Step 2 — BLE scanning**: Implement `BleScanner`, display found devices, verify `CEDNAI` is discovered.
3. **Step 3 — Connection**: Implement `BleController`, connect, discover services, log results.
4. **Step 4 — Notifications**: Enable CCCDs, receive raw `QByteArray` payloads, log hex dumps.
5. **Step 5 — Parsing**: Implement `SensorPayloadParser`, verify decoded values match expectations.
6. **Step 6 — UI**: Wire parsed values to labels and charts.
7. **Step 7 — Polish**: Error handling, reconnect logic, log panel, styling.
