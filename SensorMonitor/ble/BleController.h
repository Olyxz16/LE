#ifndef BLECONTROLLER_H
#define BLECONTROLLER_H

#include <QObject>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QBluetoothDeviceInfo>
#include <QVector3D>
#include <QBluetoothUuid>

class BleController : public QObject
{
    Q_OBJECT
public:
    explicit BleController(QObject *parent = nullptr);
    void connectToDevice(const QBluetoothDeviceInfo &device);
    void disconnectFromDevice();

signals:
    void connected();
    void disconnected();
    void environmentalUpdated(double tempC, double pressHpa, double humPct);
    void accGyroMagUpdated(const QVector3D &acc, const QVector3D &gyro, const QVector3D &mag);
    void errorOccurred(const QString &message);

private slots:
    void onControllerConnected();
    void onControllerDisconnected();
    void onDiscoveryFinished();
    void onServiceStateChanged(QLowEnergyService::ServiceState newState);
    void onCharacteristicChanged(const QLowEnergyCharacteristic &characteristic, const QByteArray &newValue);
    void onError(QLowEnergyController::Error error);

private:
    QLowEnergyController *m_controller = nullptr;
    QLowEnergyService *m_hwService = nullptr;
    
    // UUIDs
    const QBluetoothUuid HW_SERVICE_UUID = QBluetoothUuid(QUuid("{00000000-0001-11e1-9ab4-0002a5d5c51b}"));
    const QBluetoothUuid ENV_CHAR_UUID = QBluetoothUuid(QUuid("{001c0000-0001-11e1-ac36-0002a5d5c51b}"));
    const QBluetoothUuid ACC_CHAR_UUID = QBluetoothUuid(QUuid("{00e00000-0001-11e1-ac36-0002a5d5c51b}"));
    
    const QBluetoothUuid SW_SERVICE_UUID = QBluetoothUuid(QUuid("{00000000-0002-11e1-9ab4-0002a5d5c51b}"));
    const QBluetoothUuid QUAT_CHAR_UUID = QBluetoothUuid(QUuid("{00000100-0001-11e1-ac36-0002a5d5c51b}"));
};

#endif // BLECONTROLLER_H
