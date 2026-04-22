# SensorMonitor Qt Application — Complete Reference

## Table of Contents

1. [Qt Framework Fundamentals](#1-qt-framework-fundamentals)
   - 1.1 [What is Qt?](#11-what-is-qt)
   - 1.2 [The Event Loop](#12-the-event-loop)
   - 1.3 [Signals & Slots](#13-signals--slots)
   - 1.4 [The Meta-Object Compiler (MOC)](#14-the-meta-object-compiler-moc)
   - 1.5 [Parent-Child Memory Management](#15-parent-child-memory-management)
   - 1.6 [Essential Qt Types](#16-essential-qt-types)
2. [Qt Modules Used in This Project](#2-qt-modules-used-in-this-project)
   - 2.1 [QtCore](#21-qtcore)
   - 2.2 [QtWidgets](#22-qtwidgets)
   - 2.3 [QtBluetooth](#23-qtbluetooth)
   - 2.4 [QtSerialPort](#24-qtserialport)
   - 2.5 [QtCharts](#25-qtcharts)
3. [Build System](#3-build-system)
   - 3.1 [qmake and .pro Files](#31-qmake-and-pro-files)
   - 3.2 [Build Process](#32-build-process)
4. [Application Architecture](#4-application-architecture)
   - 4.1 [Project Structure](#41-project-structure)
   - 4.2 [Class Relationships](#42-class-relationships)
   - 4.3 [Dual-Mode Design (BLE / Serial)](#43-dual-mode-design-ble--serial)
5. [Component Reference](#5-component-reference)
   - 5.1 [main.cpp — Application Entry Point](#51-maincpp--application-entry-point)
   - 5.2 [BleScanner — BLE Device Discovery](#52-blescanner--ble-device-discovery)
   - 5.3 [BleController — BLE Connection & GATT](#53-blecontroller--ble-connection--gatt)
   - 5.4 [SerialController — UART State Machine](#54-serialcontroller--uart-state-machine)
   - 5.5 [SensorPayloadParser — Binary Data Decoding](#55-sensorpayloadparser--binary-data-decoding)
   - 5.6 [SensorData — Q_PROPERTY Data Model](#56-sensordata--q_property-data-model)
   - 5.7 [MainWindow — UI & Orchestration](#57-mainwindow--ui--orchestration)
6. [BLE Protocol from the Qt Side](#6-ble-protocol-from-the-qt-side)
   - 6.1 [GATT Service & Characteristic UUIDs](#61-gatt-service--characteristic-uuids)
   - 6.2 [Connection Lifecycle](#62-connection-lifecycle)
   - 6.3 [Notification Subscription (CCCD)](#63-notification-subscription-cccd)
7. [Serial Protocol from the Qt Side](#7-serial-protocol-from-the-qt-side)
   - 7.1 [Frame Format](#71-frame-format)
   - 7.2 [State Machine](#72-state-machine)
   - 7.3 [Checksum Verification](#73-checksum-verification)
8. [Data Flow](#8-data-flow)
   - 8.1 [BLE Data Path](#81-ble-data-path)
   - 8.2 [Serial Data Path](#82-serial-data-path)
9. [UI Layout & Dynamic Widgets](#9-ui-layout--dynamic-widgets)
   - 9.1 [Designer Layout (.ui File)](#91-designer-layout-ui-file)
   - 9.2 [Programmatically Added Widgets](#92-programmatically-added-widgets)
   - 9.3 [Temperature Chart](#93-temperature-chart)
   - 9.4 [Mode Switching Logic](#94-mode-switching-logic)
10. [Signal & Slot Reference](#10-signal--slot-reference)
11. [Key Qt Patterns & Idioms](#11-key-qt-patterns--idioms)
12. [Building & Running](#12-building--running)

---

## 1. Qt Framework Fundamentals

### 1.1 What is Qt?

Qt (pronounced "cute") is a cross-platform C++ application framework. It provides:

- **A GUI toolkit** (widgets) for building desktop applications
- **An event-driven architecture** built around a central event loop
- **A rich set of modules** for networking, Bluetooth, serial I/O, charts, and more
- **Build tools** like `qmake` and `moc` that handle C++ extensions and project configuration

The project uses **Qt 5** with C++17.

### 1.2 The Event Loop

Every Qt GUI application creates a `QApplication` object and enters an event loop:

```cpp
int main(int argc, char *argv[]) {
    QApplication a(argc, argv);   // Initialize Qt, parse args
    MainWindow w;                   // Create the main window
    w.show();                       // Show it on screen
    return a.exec();                // Enter the event loop — blocks until exit
}
```

`QApplication::exec()` starts the event loop. It processes:
- **Input events** (mouse clicks, key presses)
- **I/O events** (BLE notifications, serial data ready)
- **Timer events** (delayed callbacks)
- **Posted events** (cross-thread communication)

The loop continues until `QApplication::quit()` is called or the last window is closed.

### 1.3 Signals & Slots

Signals and slots are Qt's mechanism for **inter-object communication**. It replaces callback functions with a type-safe, loosely-coupled system.

**Declaring signals** (in any `QObject` subclass with `Q_OBJECT`):

```cpp
class BleController : public QObject {
    Q_OBJECT
signals:
    void environmentalUpdated(double tempC, double pressHpa, double humPct);
};
```

**Declaring slots**:

```cpp
class MainWindow : public QMainWindow {
    Q_OBJECT
private slots:
    void onEnvironmentalUpdated(double tempC, double pressHpa, double humPct);
};
```

**Connecting them**:

```cpp
connect(m_bleController, &BleController::environmentalUpdated,
        this, &MainWindow::onEnvironmentalUpdated);
```

When `BleController` emits `environmentalUpdated(25.3, 1013.25, 45.0)`, Qt's meta-object system calls `MainWindow::onEnvironmentalUpdated(25.3, 1013.25, 45.0)`.

Key properties:
- **Type-safe**: The compiler checks that signal and slot signatures are compatible
- **Loose coupling**: The sender doesn't need to know about the receiver
- **Connection types**: `Qt::AutoConnection` (default) — direct call if same thread, queued if cross-thread
- **Multiple connections**: One signal can be connected to multiple slots, and one slot can receive from multiple signals

**emit** is a macro that expands to a regular function call — it triggers the signal's invocation mechanism.

### 1.4 The Meta-Object Compiler (MOC)

Qt extends C++ with features like `signals`, `slots`, `Q_OBJECT`, and `Q_PROPERTY`. These are not standard C++. The **MOC** (Meta-Object Compiler) is a preprocessor that reads C++ headers and generates additional C++ source files (`moc_*.cpp`) that:

- Implement the signal dispatching code
- Generate metadata (`QObject::metaObject()`) enabling runtime type introspection
- Handle `Q_PROPERTY` declarations for the property system
- Implement `qobject_cast<>()` for safe dynamic casting

Without MOC, `signals:` would be just empty declarations and `Q_OBJECT` would have no effect. The build system (`qmake` → `Makefile`) automatically runs `moc` on headers that contain `Q_OBJECT`.

**Consequence for developers**: Every class using signals/slots must:
1. Inherit from `QObject` (directly or indirectly)
2. Declare `Q_OBJECT` in the private section
3. Have its header file processed by `moc` (listed in `HEADERS` in the `.pro` file)

The `.pro` file's `HEADERS` variable tells `qmake` which files to run through `moc`.

### 1.5 Parent-Child Memory Management

Qt uses a **parent-owns-child** memory model. When a `QObject` is created with a parent:

```cpp
QSerialPort *m_serial = new QSerialPort(this);  // "this" is the parent
```

The parent takes ownership and **automatically deletes the child in its own destructor**. This eliminates manual `delete` calls for member objects in most cases.

In the project:
- `BleScanner`, `BleController`, and `SerialController` are created with `this` (MainWindow) as parent
- `QSerialPort` is created with `this` (SerialController) as parent
- `QBluetoothDeviceDiscoveryAgent` is created with `this` (BleScanner) as parent
- `QChart`, `QValueAxis`, `QLineSeries` are created with `this` or the chart as parent

This ensures proper cleanup when `MainWindow` is destroyed.

### 1.6 Essential Qt Types

| Qt Type | Standard C++ Equivalent | Notes |
|---------|------------------------|-------|
| `QString` | `std::string` | Unicode string; used throughout Qt APIs |
| `QByteArray` | `std::vector<uint8_t>` | Raw byte container; used for BLE data and serial payloads |
| `quint16` | `uint16_t` | Qt's guaranteed-size typedef |
| `qint16` | `int16_t` | Qt's guaranteed-size typedef |
| `qint32` | `int32_t` | Qt's guaranteed-size typedef |
| `quint8` | `uint8_t` | Qt's guaranteed-size typedef |
| `QList<T>` | `std::vector<T>` | Qt's container class (used for port list) |
| `qreal` | `double` | Default floating-point type in Qt |

---

## 2. Qt Modules Used in This Project

### 2.1 QtCore

The foundation module, always included. Provides:
- `QObject` — base class for all Qt objects with signal/slot capability
- `QApplication` — the GUI application class and event loop
- `QTimer`, `QDateTime` — time-related utilities
- `qDebug()` — debug output stream
- `QtEndian` — byte-order conversion (`qFromLittleEndian<>`)

### 2.2 QtWidgets

Provides the classic desktop UI components:
- `QMainWindow` — the application's main window
- `QPushButton`, `QLabel`, `QGroupBox`, `QComboBox`, `QRadioButton` — basic widgets
- `QVBoxLayout`, `QHBoxLayout` — layout managers
- `QPlainTextEdit` — multi-line text display (used for event log)
- `.ui` files — XML-based UI design parsed by `uic` (User Interface Compiler)

### 2.3 QtBluetooth

 Provides BLE (Bluetooth Low Energy) functionality:
- `QBluetoothDeviceInfo` — represents a discovered BLE device (name, address, RSSI)
- `QBluetoothDeviceDiscoveryAgent` — scans for nearby BLE peripherals
- `QBluetoothUuid` — represents BLE UUIDs (services, characteristics, descriptors)
- `QLowEnergyController` — manages a BLE connection (connect, discover, interact)
- `QLowEnergyService` — represents a GATT service on the remote device
- `QLowEnergyCharacteristic` — represents a GATT characteristic
- `QLowEnergyDescriptor` — represents a characteristic descriptor (e.g., CCCD)

This module is central to the BLE communication path.

### 2.4 QtSerialPort

Provides serial (UART) communication:
- `QSerialPort` — read/write to a serial port
- `QSerialPortInfo` — enumerate available serial ports

Configuration used: 115200 baud, 8 data bits, no parity, 1 stop bit, no flow control.

### 2.5 QtCharts

Provides charting capabilities:
- `QChart` — the chart container
- `QChartView` — a widget that renders a `QChart`
- `QLineSeries` — a series of points connected by lines
- `QValueAxis` — a numeric axis with configurable range

Used for the real-time temperature history graph.

---

## 3. Build System

### 3.1 qmake and .pro Files

qmake is Qt's build system generator. It reads a `.pro` file and produces a `Makefile`. The project file (`SensorMonitor.pro`) declares:

```
QT += core gui widgets bluetooth charts serialport    # Required Qt modules
CONFIG += c++17                                         # C++ standard

SOURCES += \                                            # C++ source files
    main.cpp ui/MainWindow.cpp ble/BleScanner.cpp \
    ble/BleController.cpp ble/SensorPayloadParser.cpp \
    serial/SerialController.cpp

HEADERS += \                                            # C++ header files (also fed to moc)
    ui/MainWindow.h ble/BleScanner.h ble/BleController.h \
    ble/SensorPayloadParser.h model/SensorData.h \
    serial/SerialController.h

FORMS += ui/MainWindow.ui                               # Qt Designer UI files (fed to uic)
```

**How qmake processes sources**:
- `SOURCES` → compiled by `g++` into `.o` files
- `HEADERS` → scanned by `moc` if they contain `Q_OBJECT`; produces `moc_*.cpp` files
- `FORMS` → processed by `uic` (User Interface Compiler) into `ui_*.h` files (e.g., `ui_MainWindow.h`)

The generated `ui_MainWindow.h` contains a `Ui::MainWindow` class with a `setupUi()` method that creates and configures all widgets defined in the `.ui` XML file.

### 3.2 Build Process

The complete build chain:

```
SensorMonitor.pro
       │
       ▼
    qmake ──────────────────────────────────────────────
       │                                                  │
       │  Generates Makefile                              │
       ▼                                                  │
    make                                                 │
       │                                                  │
       ├── uic ui/MainWindow.ui → ui_MainWindow.h         │
       ├── moc *.h → moc_*.cpp                            │
       ├── g++ -c *.cpp → *.o                            │
       └── g++ -o SensorMonitor *.o  (linking)           │
                  │                                       │
                  ▼                                       │
           SensorMonitor (executable)                    │
```

**Build commands**:
```bash
cd SensorMonitor
/usr/lib/qt5/bin/qmake        # Generate Makefile from .pro
make -j$(nproc)               # Compile and link
```

---

## 4. Application Architecture

### 4.1 Project Structure

```
SensorMonitor/
├── SensorMonitor.pro           # qmake project file
├── main.cpp                    # QApplication entry point
├── ble/
│   ├── BleScanner.h/.cpp       # BLE device discovery
│   ├── BleController.h/.cpp    # BLE GATT connection & data reception
│   └── SensorPayloadParser.h/.cpp  # Binary payload parsing (shared by BLE & Serial)
├── serial/
│   └── SerialController.h/.cpp # UART connection & framed protocol parser
├── model/
│   └── SensorData.h            # Q_PROPERTY data model (unused placeholder)
└── ui/
    ├── MainWindow.h/.cpp       # Main window (UI + orchestration)
    └── MainWindow.ui           # Qt Designer layout file
```

### 4.2 Class Relationships

```
                    ┌──────────────┐
                    │  QApplication │
                    │   (event loop)│
                    └──────┬───────┘
                           │ creates
                    ┌──────▼───────┐
                    │  MainWindow   │
                    │  (orchestrator)│
                    └──┬───┬───┬───┘
                       │   │   │
           ┌───────────┘   │   └───────────┐
           │               │               │
    ┌──────▼──────┐  ┌─────▼──────┐  ┌─────▼──────────┐
    │  BleScanner  │  │BleController│  │SerialController│
    │ (discovery)  │  │ (BLE comm)  │  │  (UART comm)   │
    └──────────────┘  └──────┬──────┘  └──────┬─────────┘
                             │                 │
                    ┌────────▼─────────────────▼────────┐
                    │      SensorPayloadParser           │
                    │  (shared binary decoding utility)  │
                    └────────────────────────────────────┘
```

**Ownership pattern**: `MainWindow` owns all controller objects (parent=`this`). It connects their signals to its own slots and serves as the central hub.

### 4.3 Dual-Mode Design (BLE / Serial)

The application can operate in two communication modes:

| Aspect | BLE Mode | Serial Mode |
|--------|----------|-------------|
| Discovery | `BleScanner` scans for "CEDRNAI" device | `QSerialPortInfo::availablePorts()` lists ports |
| Connection | `QLowEnergyController::connectToDevice()` | `QSerialPort::open()` at 115200 baud |
| Data reception | GATT notifications via `characteristicChanged` signal | `readyRead` signal → byte-by-byte state machine |
| Parsing | `SensorPayloadParser::parseEnvironmental()` | Same parser (`SensorPayloadParser`) |
| UI toggle | Scan button visible, port combo hidden | Port combo visible, scan button hidden |

Both modes emit the same `environmentalUpdated(double, double, double)` signal, so `MainWindow` doesn't need to know which mode produced the data.

---

## 5. Component Reference

### 5.1 main.cpp — Application Entry Point

**File**: `main.cpp` (10 lines)

```cpp
int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
```

Minimal entry point: creates `QApplication` (initializes Qt resources and starts the event loop), constructs the main window, shows it, and enters `a.exec()` which blocks until the application exits.

### 5.2 BleScanner — BLE Device Discovery

**Files**: `ble/BleScanner.h`, `ble/BleScanner.cpp`

**Class**: `BleScanner : public QObject`

**Responsibility**: Scan for BLE peripherals advertising the name "CEDRNAI".

| Member | Type | Description |
|--------|------|-------------|
| `m_agent` | `QBluetoothDeviceDiscoveryAgent*` | Qt's built-in BLE scanner |
| `TARGET_NAME` | `const QString` | Hard-coded to `"CEDRNAI"` — filters scan results |

**Workflow**:
1. `startScan()` → calls `m_agent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod)`
2. `onDeviceDiscovered()` → checks `device.name() == "CEDRNAI"`, stops scanning, emits `deviceFound(device)`
3. `onScanFinished()` → no-op (scan already stopped if device found)
4. `onError()` → emits `errorOccurred(message)`

**Key detail**: The 10-second timeout (`setLowEnergyDiscoveryTimeout(10000)`) prevents indefinite scanning. The scan stops immediately when the target device is found.

**Signals**:
- `deviceFound(const QBluetoothDeviceInfo &)` — target device discovered
- `errorOccurred(const QString &)` — scan error

### 5.3 BleController — BLE Connection & GATT

**Files**: `ble/BleController.h`, `ble/BleController.cpp`

**Class**: `BleController : public QObject`

**Responsibility**: Manage the full BLE GATT lifecycle — connection, service discovery, notification subscription, and data parsing.

**BLE UUID Constants**:

| Constant | UUID | Purpose |
|----------|------|---------|
| `HW_SERVICE_UUID` | `{00000000-0001-11e1-9ab4-0002a5d5c51b}` | Hardware Sensor Service |
| `ENV_CHAR_UUID` | `{001c0000-0001-11e1-ac36-0002a5d5c51b}` | Environmental Characteristic |

**Members**:

| Member | Type | Description |
|--------|------|-------------|
| `m_controller` | `QLowEnergyController*` | Manages the BLE connection |
| `m_hwService` | `QLowEnergyService*` | Pointer to the discovered HW service |

**Connection lifecycle**:

1. `connectToDevice(device)` → creates `QLowEnergyController`, connects signals, calls `connectToDevice()`
2. `onControllerConnected()` → emits `connected()`, calls `discoverServices()`
3. `onDiscoveryFinished()` → creates service object for `HW_SERVICE_UUID`, calls `discoverDetails()`
4. `onServiceStateChanged(ServiceDiscovered)` → finds the Environmental characteristic, writes `0x0100` to its CCCD to enable notifications
5. `onCharacteristicChanged()` → UUID match check, parse payload with `SensorPayloadParser`, emit `environmentalUpdated()`

**Important**: `QLowEnergyController::createCentral(device)` creates a new controller each time `connectToDevice()` is called. The previous controller is cleaned up (`disconnectFromDevice()` + `delete`).

**CCCD write** (enabling notifications):
```cpp
QLowEnergyDescriptor cccd = c.descriptor(QBluetoothUuid::ClientCharacteristicConfiguration);
if (cccd.isValid()) {
    service->writeDescriptor(cccd, QByteArray::fromHex("0100"));
}
```
The value `0x0100` (little-endian) enables notifications. `0x0000` disables them.

**Signals**:
- `connected()` — BLE connection established
- `disconnected()` — BLE connection lost
- `environmentalUpdated(double tempC, double pressHpa, double humPct)` — parsed env data
- `errorOccurred(const QString &message)` — BLE error

### 5.4 SerialController — UART State Machine

**Files**: `serial/SerialController.h`, `serial/SerialController.cpp`

**Class**: `SerialController : public QObject`

**Responsibility**: Manage the serial port connection and implement a byte-by-byte state machine to parse framed binary data from the STM32.

**Members**:

| Member | Type | Description |
|--------|------|-------------|
| `m_serial` | `QSerialPort*` | The serial port handle |
| `m_state` | `State` | Current state machine position |
| `m_currentType` | `uint8_t` | Accumulated frame type byte |
| `m_currentLen` | `uint8_t` | Accumulated frame length byte |
| `m_currentPayload` | `QByteArray` | Accumulated payload bytes |
| `m_checksum` | `uint8_t` | Not used (checksum computed inline) |

**State Machine States** (`State` enum):

```
IDLE ──[0xA5]──► SYNC2 ──[0x5A]──► TYPE ──► LEN ──► PAYLOAD ──► CHECKSUM ──► IDLE
                      │                   │        │                      │
                   [0xA5] stays       [!0xA5]     [=0] →             fail →
                   in SYNC2            → IDLE    CHECKSUM
```

| State | Condition | Action |
|-------|-----------|--------|
| `IDLE` | byte == `0xA5` | → `SYNC2` |
| `SYNC2` | byte == `0x5A` → `TYPE`; byte == `0xA5` → stay `SYNC2`; else → `IDLE` |
| `TYPE` | any | Store as `m_currentType`, → `LEN` |
| `LEN` | any | Store as `m_currentLen`; if >40 → `IDLE` (sanity); if 0 → `CHECKSUM`; else → `PAYLOAD` |
| `PAYLOAD` | appended bytes | Append to `m_currentPayload`; when size == `m_currentLen` → `CHECKSUM` |
| `CHECKSUM` | any | Compute XOR of type+length+payload; compare to received byte; if match → `processFrame()`; → `IDLE` |

**UART configuration**: 115200 baud, 8 data bits, no parity, 1 stop bit, no flow control.

**Error handling**: If `QSerialPort::ResourceError` occurs (e.g., cable unplugged), the controller auto-disconnects and emits `errorOccurred`.

**Frame processing** (`processFrame`):
- Type `1` (Environmental): calls `SensorPayloadParser::parseEnvironmental()`, emits `environmentalUpdated()`

**Signals** (same interface as `BleController`):
- `connected()`, `disconnected()`, `environmentalUpdated()`, `errorOccurred()`

### 5.5 SensorPayloadParser — Binary Data Decoding

**Files**: `ble/SensorPayloadParser.h`, `ble/SensorPayloadParser.cpp`

**Class**: `SensorPayloadParser` (static utility — no instances, no `Q_OBJECT`)

**Responsibility**: Decode raw `QByteArray` payloads into typed C++ structs. Shared by both `BleController` and `SerialController`.

**Data structure**:

```cpp
struct EnvironmentalSample {
    double tempC;       // Temperature in °C
    double pressHpa;    // Pressure in hPa
    double humPct;      // Humidity in %
    quint16 timestamp; // Raw timestamp (HAL_GetTick() >> 3)
};
```

**Parsing logic** (`parseEnvironmental`):

Requires at least 10 bytes. Reads little-endian values using `qFromLittleEndian<T>(pointer)`:

| Offset | Size | Type | Field | Conversion |
|--------|------|------|-------|------------|
| 0 | 2 | `quint16` | Timestamp | Raw value, `HAL_GetTick() >> 3` |
| 2 | 4 | `qint32` | Pressure | `value / 100.0` → hPa |
| 6 | 2 | `quint16` | Humidity | `value / 10.0` → % |
| 8 | 2 | `qint16` | Temperature | `value / 10.0` → °C |

**`qFromLittleEndian<T>(p)`** is a Qt function that reads `sizeof(T)` bytes starting at pointer `p` and converts from little-endian to the host's native byte order. On x86 (little-endian host), this is essentially a no-op copy, but on big-endian architectures it reverses the byte order.

### 5.6 SensorData — Q_PROPERTY Data Model

**File**: `model/SensorData.h`

**Class**: `SensorData : public QObject`

**Status**: Defined but **currently unused**. It provides `Q_PROPERTY` declarations for temperature, pressure, and humidity with change notifications.

```cpp
Q_PROPERTY(double temperature READ temperature WRITE setTemperature NOTIFY temperatureChanged)
Q_PROPERTY(double pressure READ pressure WRITE setPressure NOTIFY pressureChanged)
Q_PROPERTY(double humidity READ humidity WRITE setHumidity NOTIFY humidityChanged)
```

**Purpose**: `Q_PROPERTY` enables Qt's property system, which is required for:
- **QML bindings** — automatic UI updates when properties change
- **Animation frameworks** — properties can be animated
- **Dynamic introspection** — `QObject::property("temperature")` works at runtime

The current implementation wires data directly from signals to UI labels, bypassing this model. It exists as a scaffold if the project were to adopt a QML-based UI or more formal Model-View separation.

### 5.7 MainWindow — UI & Orchestration

**Files**: `ui/MainWindow.h`, `ui/MainWindow.cpp`, `ui/MainWindow.ui`

**Class**: `MainWindow : public QMainWindow`

**Responsibility**: The central coordinator. Creates all other objects, wires their signals to slots, manages the UI, and handles mode switching.

**Members**:

| Member | Type | Description |
|--------|------|-------------|
| `ui` | `Ui::MainWindow*` | Generated UI class from `.ui` file |
| `m_bleScanner` | `BleScanner*` | BLE device discovery |
| `m_bleController` | `BleController*` | BLE data connection |
| `m_serialController` | `SerialController*` | Serial data connection |
| `m_mode` | `Mode` | Current mode (BLE or SERIAL) |
| `m_targetDevice` | `QBluetoothDeviceInfo` | Last discovered BLE device |
| `m_tempSeries` | `QLineSeries*` | Chart data series |
| `m_axisX` | `QValueAxis*` | Chart X-axis (samples) |
| `m_axisY` | `QValueAxis*` | Chart Y-axis (temperature) |
| `m_sampleCount` | `int` | Number of data points received |

**Constructor flow**:
1. `ui->setupUi(this)` — creates all widgets from `.ui` file
2. `setupSerialUi()` — creates mode selector, port combo, refresh button
3. Connect button signals → `MainWindow` slots
4. Connect `BleScanner` signals → `MainWindow` slots
5. Connect `BleController` signals → `MainWindow` slots
6. Connect `SerialController` signals → `MainWindow` slots
7. `setupCharts()` — creates temperature chart, inserts into `envLayout`
8. `log("Application started")`

---

## 6. BLE Protocol from the Qt Side

### 6.1 GATT Service & Characteristic UUIDs

The STM32 firmware exposes one BLE service with one characteristic:

| Item | UUID |
|------|------|
| HW Sensor Service | `00000000-0001-11e1-9ab4-0002a5d5c51b` |
| Environmental Char | `001c0000-0001-11e1-ac36-0002a5d5c51b` |

These are stored as `QBluetoothUuid` constants in `BleController`:

```cpp
const QBluetoothUuid HW_SERVICE_UUID = QBluetoothUuid(QUuid("{00000000-0001-11e1-9ab4-0002a5d5c51b}"));
const QBluetoothUuid ENV_CHAR_UUID   = QBluetoothUuid(QUuid("{001c0000-0001-11e1-ac36-0002a5d5c51b}"));
```

UUIDs use big-endian string notation (standard UUID format). The firmware stores them as little-endian byte arrays. Qt's `QUuid` constructor handles the conversion.

### 6.2 Connection Lifecycle

```
BleScanner::startScan()
       │
       ▼  (BLE advertisement from "CEDRNAI")
BleScanner::onDeviceDiscovered()
       │
       │  emit deviceFound(device)
       ▼
MainWindow::onDeviceFound()
  stores device → enables Connect button
       │
       │  (user clicks Connect)
       ▼
BleController::connectToDevice(device)
       │
       │  QLowEnergyController::connectToDevice()
       ▼
BleController::onControllerConnected()
       │
       │  emit connected()
       │  discoverServices()
       ▼
BleController::onDiscoveryFinished()
       │  creates service object for HW_SERVICE_UUID
       │  discoverDetails()
       ▼
BleController::onServiceStateChanged(ServiceDiscovered)
       │  finds ENV_CHAR_UUID
       │  writes 0x0100 to CCCD
       ▼
   (Notifications arrive)
       │
BleController::onCharacteristicChanged()
       │  UUID check → parseEnvironmental()
       │  emit environmentalUpdated(tempC, pressHpa, humPct)
       ▼
MainWindow::onEnvironmentalUpdated()
  updates labels and chart
```

### 6.3 Notification Subscription (CCCD)

GATT notifications must be explicitly enabled by writing to the Client Characteristic Configuration Descriptor (CCCD, UUID `0x2902`). The value `0x0100` enables notifications:

```cpp
QLowEnergyDescriptor cccd = characteristic.descriptor(
    QBluetoothUuid::ClientCharacteristicConfiguration);
if (cccd.isValid()) {
    service->writeDescriptor(cccd, QByteArray::fromHex("0100"));
}
```

Without this write, the peripheral won't send any data even though the BLE connection is established.

---

## 7. Serial Protocol from the Qt Side

### 7.1 Frame Format

The STM32 sends binary frames over USART2 (115200 baud, 8N1). Each frame has this structure:

```
┌──────┬──────┬──────┬──────┬───────────┬──────────┐
│ 0xA5 │ 0x5A │ TYPE │ LEN  │  PAYLOAD  │ CHECKSUM │
│ sync1│sync2 │ 1B   │ 1B   │  LEN B    │   1B     │
└──────┴──────┴──────┴──────┴───────────┴──────────┘
```

Checksum = TYPE XOR LEN XOR (all PAYLOAD bytes)

Currently only **Type 1 (Environmental)** is used, with a 10-byte payload.

### 7.2 State Machine

The `SerialController::processByte()` method implements a byte-by-byte state machine that processes incoming serial data one character at a time. This is necessary because serial data arrives in arbitrary chunk sizes — a single `readyRead` signal may contain a partial frame, or multiple frames, or anything in between.

The state machine:
- Starts in `IDLE` — scanning for the sync byte `0xA5`
- Uses a two-byte sync header (`0xA5`, `0x5A`) to avoid false triggers from random data
- Validates the payload length (must be ≤ 40 bytes) as a sanity check
- Verifies the checksum before dispatching the frame
- Resets to `IDLE` on any error (bad sync, checksum mismatch)

This approach is robust against:
- Partial frames (data split across multiple `readyRead` events)
- Noise on the serial line (the two-byte sync header reduces false positive probability)
- Debug prints interleaved with protocol data (non-sync bytes are ignored in `IDLE`)

### 7.3 Checksum Verification

When the `CHECKSUM` state is reached:

```cpp
uint8_t expectedChecksum = m_currentType ^ m_currentLen;
for (int i = 0; i < m_currentPayload.size(); ++i) {
    expectedChecksum ^= static_cast<uint8_t>(m_currentPayload[i]);
}
if (byte == expectedChecksum) {
    processFrame(m_currentType, m_currentPayload);
}
```

The XOR checksum is simple but sufficient for short, low-noise serial connections. If it fails, the frame is discarded and the state machine resets.

---

## 8. Data Flow

### 8.1 BLE Data Path

```
STM32 Firmware
       │
       │  Environmental_Update(press*100, temp*10, hum*10)
       │     └── aci_gatt_update_char_value() → BLE notification
       │
       ▼
BlueNRG-2 BLE Radio
       │  (over-the-air BLE GATT notification)
       ▼
BleController::onCharacteristicChanged(characteristic, newValue)
       │
       │  Check UUID == ENV_CHAR_UUID
       │  SensorPayloadParser::parseEnvironmental(newValue)
       │     ├── timestamp = qFromLittleEndian<quint16>(ptr)
       │     ├── pressHpa  = qFromLittleEndian<qint32>(ptr+2) / 100.0
       │     ├── humPct    = qFromLittleEndian<quint16>(ptr+6) / 10.0
       │     └── tempC     = qFromLittleEndian<qint16>(ptr+8) / 10.0
       │
       │  emit environmentalUpdated(tempC, pressHpa, humPct)
       ▼
MainWindow::onEnvironmentalUpdated(tempC, pressHpa, humPct)
       ├── tempLabel  → "Temp: XX.X °C"
       ├── pressLabel → "Press: XXXX.XX hPa"
       ├── humLabel   → "Hum: XX.X %"
       ├── m_tempSeries->append(m_sampleCount++, tempC)
       └── Auto-adjust chart axes if needed
```

### 8.2 Serial Data Path

```
STM32 Firmware
       │
       │  Environmental_Update(...) → Serial_SendBinary(1, buff, 10)
       │     └── HAL_UART_Transmit(0xA5 5A 01 0A [10 bytes] [checksum])
       │
       ▼
USB/Serial Cable
       │
       ▼
SerialController::onReadyRead()
       │
       │  m_serial->readAll()
       │  for each byte: processByte(byte)
       │
       │  State machine: IDLE → SYNC2 → TYPE → LEN → PAYLOAD → CHECKSUM
       │
       │  On valid checksum:
       │     processFrame(type=1, payload)
       │        SensorPayloadParser::parseEnvironmental(payload)
       │        emit environmentalUpdated(tempC, pressHpa, humPct)
       │
       ▼
MainWindow::onEnvironmentalUpdated(tempC, pressHpa, humPct)
  (same as BLE path)
```

---

## 9. UI Layout & Dynamic Widgets

### 9.1 Designer Layout (.ui File)

The `.ui` file defines the static widget tree. It's processed by `uic` into `ui_MainWindow.h`, which provides `Ui::MainWindow::setupUi()`:

```
QMainWindow "MainWindow" (800 × 600, title: "STM32 BLE Sensor Monitor")
└── QWidget "centralwidget"
    └── QVBoxLayout "verticalLayout"
        ├── QHBoxLayout "toolbarLayout"
        │   ├── QPushButton "scanButton"        [text: "Scan"]
        │   ├── QPushButton "connectButton"     [text: "Connect", enabled: false]
        │   ├── QPushButton "disconnectButton"  [text: "Disconnect", enabled: false]
        │   ├── QSpacerItem "horizontalSpacer"
        │   └── QLabel "statusLabel"            [text: "Status: Idle"]
        │
        ├── QHBoxLayout "contentLayout"
        │   └── QGroupBox "environmentalGroupBox" [title: "Environmental"]
        │       └── QVBoxLayout "envLayout"
        │           ├── QLabel "tempLabel"   [text: "Temp: -- °C"]
        │           ├── QLabel "pressLabel"  [text: "Press: -- hPa"]
        │           ├── QLabel "humLabel"    [text: "Hum: -- %"]
        │           └── (QChartView added at runtime)
        │
        └── QPlainTextEdit "logOutput" [readOnly: true]
```

**Accessing widgets in code**: The `ui` pointer (`Ui::MainWindow *ui`) provides direct access:
```cpp
ui->tempLabel->setText("Temp: 25.3 °C");
ui->statusLabel->setText("Status: Connected");
```

### 9.2 Programmatically Added Widgets

`setupSerialUi()` dynamically creates and inserts widgets into the toolbar at runtime:

| Widget | Object Name | Type | Purpose |
|--------|-------------|------|---------|
| Mode group | `"modeGroup"` | `QGroupBox` | Contains BLE/Serial radio buttons |
| BLE radio | — | `QRadioButton` | Select BLE mode (default checked) |
| Serial radio | — | `QRadioButton` | Select Serial mode |
| Port combo | `"portCombo"` | `QComboBox` | Lists available serial ports |
| Refresh button | `"refreshBtn"` | `QPushButton` | Refresh port list |

These are inserted at the start of `toolbarLayout` (positions 0, 1, 2), shifting the existing Scan/Connect/Disconnect buttons to the right.

The mode radio buttons are grouped with `QButtonGroup`, which assigns each an integer ID (`BLE=0`, `SERIAL=1`). When clicked, a lambda sets `m_mode` and calls `onModeChanged()`.

Widgets are looked up at runtime using `findChild<T*>("objectName")`, since they were created programmatically (not in the `.ui` file).

### 9.3 Temperature Chart

`setupCharts()` creates a real-time chart programmatically:

```cpp
m_tempSeries = new QLineSeries(this);
QChart *chart = new QChart();
chart->addSeries(m_tempSeries);
chart->setTitle("Temperature History");
chart->legend()->hide();

m_axisX = new QValueAxis;  // Range: 0–60, scrolls after 60 samples
m_axisY = new QValueAxis;  // Range: 20–30, auto-expands

chart->addAxis(m_axisX, Qt::AlignBottom);
chart->addAxis(m_axisY, Qt::AlignLeft);
m_tempSeries->attachAxis(m_axisX);
m_tempSeries->attachAxis(m_axisY);

QChartView *chartView = new QChartView(chart);
chartView->setRenderHint(QPainter::Antialiasing);
ui->envLayout->addWidget(chartView);  // Added to the Environmental group box
```

**Behavior**:
- X-axis starts at 0–60, then scrolls (window of last 60 samples)
- Y-axis starts at 20–30 °C, auto-expands by ±1 unit when values exceed range
- Every call to `onEnvironmentalUpdated()` appends a data point: `m_tempSeries->append(count++, tempC)`

### 9.4 Mode Switching Logic

`onModeChanged()` toggles widget visibility and connection button state:

| Mode | Scan button | Port combo + Refresh | Connect enables when |
|------|-------------|---------------------|---------------------|
| BLE | Visible | Hidden | `m_targetDevice.isValid()` (device found during scan) |
| Serial | Hidden | Visible | `portCombo->count() > 0` (at least one port listed) |

The Connect and Disconnect buttons call different underlying methods depending on `m_mode`:

```cpp
void MainWindow::onConnectClicked() {
    if (m_mode == BLE)
        m_bleController->connectToDevice(m_targetDevice);
    else
        m_serialController->connectToPort(portName);
}
```

---

## 10. Signal & Slot Reference

All connections are made in the `MainWindow` constructor. The following tables list every explicit connection:

### Button Signals

| Sender | Signal | Receiver | Slot |
|--------|--------|----------|------|
| `scanButton` | `clicked()` | `MainWindow` | `onScanClicked()` |
| `connectButton` | `clicked()` | `MainWindow` | `onConnectClicked()` |
| `disconnectButton` | `clicked()` | `MainWindow` | `onDisconnectClicked()` |
| `refreshBtn` | `clicked()` | `MainWindow` | `onRefreshPortsClicked()` |
| mode `QButtonGroup` | `buttonClicked(int)` | Lambda | Sets `m_mode`, calls `onModeChanged()` |

### BLE Scanner Signals

| Signal | Slot | Description |
|--------|------|-------------|
| `BleScanner::deviceFound(device)` | `MainWindow::onDeviceFound(device)` | Target BLE device discovered |
| `BleScanner::errorOccurred(msg)` | `MainWindow::onError(msg)` | BLE scan error |

### BLE Controller Signals

| Signal | Slot | Description |
|--------|------|-------------|
| `BleController::connected()` | `MainWindow::onConnected()` | BLE link established |
| `BleController::disconnected()` | `MainWindow::onDisconnected()` | BLE link lost |
| `BleController::errorOccurred(msg)` | `MainWindow::onError(msg)` | BLE error |
| `BleController::environmentalUpdated(t, p, h)` | `MainWindow::onEnvironmentalUpdated(t, p, h)` | New sensor data |

### Serial Controller Signals

| Signal | Slot | Description |
|--------|------|-------------|
| `SerialController::connected()` | `MainWindow::onConnected()` | Serial port opened |
| `SerialController::disconnected()` | `MainWindow::onDisconnected()` | Serial port closed |
| `SerialController::errorOccurred(msg)` | `MainWindow::onError(msg)` | Serial error |
| `SerialController::environmentalUpdated(t, p, h)` | `MainWindow::onEnvironmentalUpdated(t, p, h)` | New sensor data |

### Internal Connections (within controllers)

| Object | Signal | Slot | File |
|--------|--------|------|------|
| `BleScanner::m_agent` | `deviceDiscovered` | `BleScanner::onDeviceDiscovered` | `BleScanner.cpp:8` |
| `BleScanner::m_agent` | `finished` | `BleScanner::onScanFinished` | `BleScanner.cpp:9` |
| `BleScanner::m_agent` | `error` | `BleScanner::onError` | `BleScanner.cpp:10` |
| `BleController::m_controller` | `connected` | `BleController::onControllerConnected` | `BleController.cpp:19` |
| `BleController::m_controller` | `disconnected` | `BleController::onControllerDisconnected` | `BleController.cpp:20` |
| `BleController::m_controller` | `discoveryFinished` | `BleController::onDiscoveryFinished` | `BleController.cpp:21` |
| `BleController::m_controller` | `error` | `BleController::onError` | `BleController.cpp:22` |
| `BleController::m_hwService` | `stateChanged` | `BleController::onServiceStateChanged` | `BleController.cpp:55` |
| `BleController::m_hwService` | `characteristicChanged` | `BleController::onCharacteristicChanged` | `BleController.cpp:56` |
| `SerialController::m_serial` | `readyRead` | `SerialController::onReadyRead` | `SerialController.cpp:8` |
| `SerialController::m_serial` | `errorOccurred` | `SerialController::onErrorOccurred` | `SerialController.cpp:9` |

---

## 11. Key Qt Patterns & Idioms

### Q_OBJECT Macro

Every class that uses signals, slots, or properties must contain the `Q_OBJECT` macro in its private section:

```cpp
class BleController : public QObject {
    Q_OBJECT  // Enables meta-object features (signals, slots, qobject_cast)
    ...
};
```

This macro tells `moc` to generate the meta-object code for this class. Without it:
- Signals won't work (no `moc_*.cpp` generated for the class)
- `qobject_cast<>()` won't work
- `Q_PROPERTY` won't work
- Linker errors about missing `vtable` or `staticMetaObject` will appear

**Note**: `SensorPayloadParser` does **not** have `Q_OBJECT` because it's a pure static utility class with no signals or slots. This is correct — `moc` only processes classes that need meta-object features.

### The `ui` Pointer Pattern

```cpp
class MainWindow : public QMainWindow {
    Ui::MainWindow *ui;  // Forward-declared, generated from .ui file
};
```

In the constructor:
```cpp
ui(new Ui::MainWindow)
```

After `ui->setupUi(this)`, all widgets named in the `.ui` file become accessible as `ui->widgetName`. This is the standard pattern for using Qt Designer files alongside hand-written code.

### Lambda Slot with QButtonGroup

```cpp
connect(group, QOverload<int>::of(&QButtonGroup::buttonClicked), this, [this](int id){
    m_mode = static_cast<Mode>(id);
    onModeChanged();
});
```

This uses a **lambda expression** as a slot — an inline function that captures `this` and calls `onModeChanged()` after setting the mode. The `QOverload<int>::of()` syntax resolves the overloaded `buttonClicked` signal to its `int` version.

### `qFromLittleEndian<T>(pointer)`

```cpp
quint16 timestamp = qFromLittleEndian<quint16>(p);
```

This Qt function reads `sizeof(T)` bytes from the pointer and converts from little-endian to host byte order. On x86 (little-endian), it's a simple memory copy. On big-endian systems, it reverses the byte order. This ensures correct parsing regardless of the host CPU architecture.

### `QOverload<>::of()` for Overloaded Signals

```cpp
connect(m_agent, QOverload<QBluetoothDeviceDiscoveryAgent::Error>::of(
    &QBluetoothDeviceDiscoveryAgent::error), this, &BleScanner::onError);
```

Several Qt signals have overloaded versions (e.g., `error()` might have `error(QBluetoothDeviceDiscoveryAgent::Error)` and an older deprecated overload). `QOverload<>::of()` selects the specific overload by its parameter types.

### `findChild<T*>()` for Dynamic Widgets

```cpp
QComboBox *portCombo = findChild<QComboBox*>("portCombo");
```

Since the mode selector, port combo, and refresh button are created programmatically (not in the `.ui` file), they can't be accessed via `ui->`. Instead, `findChild<T*>("objectName")` searches the widget tree for a child with the matching object name and type. This is a common pattern for dynamically-created widgets.

### `Q_UNUSED` Macro

```cpp
void BleController::onError(QLowEnergyController::Error error)
{
    Q_UNUSED(error);
    emit errorOccurred(m_controller->errorString());
}
```

`Q_UNUSED` suppresses the "unused parameter" compiler warning. It's used when a slot must accept a parameter (to match the signal signature) but doesn't need that parameter in its implementation.

### Object Names for Dynamic Lookup

When creating widgets programmatically:
```cpp
QComboBox *portCombo = new QComboBox(this);
portCombo->setObjectName("portCombo");
```

Setting `objectName` enables `findChild<>()` to locate the widget later. Without it, `findChild` would return `nullptr`.

---

## 12. Building & Running

### Dependencies (Linux)

```bash
sudo apt update
sudo apt install qtbase5-dev qtconnectivity5-dev libqt5serialport5-dev libqt5charts5-dev build-essential
```

| Package | Provides |
|---------|----------|
| `qtbase5-dev` | QtCore, QtGui, QtWidgets, qmake, moc, uic |
| `qtconnectivity5-dev` | QtBluetooth (BLE classes) |
| `libqt5serialport5-dev` | QtSerialPort (QSerialPort, QSerialPortInfo) |
| `libqt5charts5-dev` | QtCharts (QChart, QLineSeries, QValueAxis) |
| `build-essential` | g++, make |

### Build

```bash
cd SensorMonitor
/usr/lib/qt5/bin/qmake
make -j$(nproc)
```

This produces the `SensorMonitor` executable in the project directory.

### Run

```bash
./SensorMonitor
```

### Linux Permissions

BLE and serial port access require group membership:

```bash
sudo usermod -aG dialout,lp,bluetooth $USER
# Log out and back in for changes to take effect
```

| Group | Purpose |
|-------|---------|
| `dialout` | Serial port access (`/dev/ttyACM0`, `/dev/ttyUSB0`) |
| `lp` | Some distributions require this for Bluetooth |
| `bluetooth` | BLE adapter access |

### Debug Logging

Enable Qt BLE debug output:
```bash
QT_LOGGING_RULES="qt.bluetooth*=true" ./SensorMonitor
```

The serial controller also outputs debug logs via `qDebug()`:
```
"Serial Received: a5 5a 01 0a ..."
"Valid Serial Frame, Type: 1"
```