# STM32 Sensor Monitor (BLE & Serial)

This project consists of an STM32 firmware and a Qt-based desktop application to monitor environmental (temperature, pressure, humidity) and motion (accelerometer, gyroscope, magnetometer) sensor data.

## Features
- **Dual Connectivity**: Connect via Bluetooth Low Energy (BLE) or Serial Port (UART).
- **Live Visualization**: Real-time charts for temperature history.
- **IMU Monitoring**: Live display of emulated motion data.
- **Binary Protocol**: Efficient data transmission over Serial using a custom framed protocol.

## Hardware Requirements
- **STM32L476RG Nucleo** board.
- **X-NUCLEO-IDB05A1** (BlueNRG-M0) or similar BLE expansion board.
- **Physical Sensors**: HTS221 (Temp/Hum) and LPS22HH (Pressure) over I2C.

---

## Building and Running

### 1. STM32 Firmware
The firmware is located in the root directory and is designed for **STM32CubeIDE**.

1.  Open **STM32CubeIDE**.
2.  Import the project (`File > Import... > Existing Projects into Workspace`).
3.  Build the project.
4.  Flash the `.bin` or `.elf` file to your Nucleo-L476RG board.
5.  Data will start transmitting over UART2 (115200 baud) and BLE (`CEDNAI`).

### 2. Qt Desktop Application
The application is located in the `SensorMonitor/` directory.

#### Prerequisites (Linux)
Install the necessary Qt 5 development libraries:
```bash
sudo apt update
sudo apt install qtbase5-dev qtconnectivity5-dev libqt5serialport5-dev libqt5charts5-dev
```

#### Compilation
Navigate to the application directory and use `qmake`:
```bash
cd SensorMonitor
/usr/lib/qt5/bin/qmake
make -j$(nproc)
```

#### Running
```bash
./SensorMonitor
```

---

## Connectivity Modes

### BLE Mode
1. Click **Scan** to find the `CEDNAI` device.
2. Click **Connect** once the device is found.
3. Live data will update automatically via GATT notifications.

### Serial Mode
1. Select **Serial** in the Mode selector.
2. Choose the correct COM/TTY port (typically `/dev/ttyACM0` on Linux).
3. Click **Connect**.
4. The app will de-frame the binary protocol and display the values.

---

## Documentation
For technical details on the serial protocol and application architecture, see [MAINTENANCE.md](./MAINTENANCE.md).
