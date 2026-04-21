# Developer & Maintenance Guide: STM32 Sensor Monitor

This document provides technical details for developers maintaining or extending the STM32 BLE/Serial Sensor Monitor.

## 1. System Architecture

The project consists of two main components:
1.  **STM32 Firmware:** Runs on a Nucleo-L476RG. It reads physical sensors (I2C) and emulates motion data, then broadcasts it via BLE (BlueNRG-2) and Serial (UART2).
2.  **Qt Application:** A desktop monitor that connects via BLE or Serial to visualize live sensor data.

## 2. Serial Protocol (UART)

To allow Serial connectivity to coexist with debug prints, a binary framing protocol is used on UART2 (115200 8N1).

### Frame Format
`[SYNC1][SYNC2][TYPE][LEN][PAYLOAD][CHECKSUM]`

| Field | Value | Description |
|---|---|---|
| SYNC1 | `0xA5` | Start of frame byte 1 |
| SYNC2 | `0x5A` | Start of frame byte 2 |
| TYPE | `uint8_t` | `1`: Environmental, `2`: Motion, `3`: Quaternions |
| LEN | `uint8_t` | Length of the payload in bytes |
| PAYLOAD | `bytes` | Little-endian binary data (matches BLE GATT format) |
| CHECKSUM | `uint8_t` | XOR of `TYPE`, `LEN`, and all `PAYLOAD` bytes |

### Payload Structures (Little-Endian)
- **Type 1 (Environmental - 10 bytes):**
  - `[0-1]`: Timestamp (uint16)
  - `[2-5]`: Pressure (int32, value/100 -> hPa)
  - `[6-7]`: Humidity (uint16, value/10 -> %)
  - `[8-9]`: Temperature (int16, value/10 -> °C)
- **Type 2 (Motion - 20 bytes):**
  - `[0-1]`: Timestamp (uint16)
  - `[2-7]`: Accel X, Y, Z (3x int16)
  - `[8-13]`: Gyro X, Y, Z (3x int16)
  - `[14-19]`: Mag X, Y, Z (3x int16)

## 3. Qt Application Structure

The application uses a "Controller" pattern to abstract the connection hardware:

- **`BleController`**: Manages `QLowEnergyController` for GATT discovery and notifications.
- **`SerialController`**: Manages `QSerialPort` and implements the state-machine for de-framing the binary protocol.
- **`SensorPayloadParser`**: A stateless utility used by **both** controllers to convert raw byte arrays into structured C++ types.

### Switching Modes
The `MainWindow` switches between these controllers based on the UI mode selection. Both controllers emit the same signals (`environmentalUpdated`, `accGyroMagUpdated`), allowing the UI logic to remain decoupled from the transport layer.

## 4. Firmware Integration

Sensor updates are centralized in `Src/gatt_db.c`. Any call to `Environmental_Update`, `Acc_Update`, or `Quat_Update` will automatically:
1. Update the BLE GATT database.
2. Send a binary frame over UART2 using the `Serial_SendBinary` helper.

The main loop in `Src/app_bluenrg_2.c` (`User_Process`) triggers these updates every 1000ms.

## 5. Troubleshooting & Dependencies

### Qt Dependencies
Ensure the following modules are installed in your Qt environment:
- `Qt Bluetooth`
- `Qt Serial Port`
- `Qt Charts`

### Linux Permissions
To access Bluetooth and Serial ports on Linux, the user may need to be in the `lp`, `dialout`, and `bluetooth` groups:
```bash
sudo usermod -aG dialout,lp,bluetooth $USER
```
