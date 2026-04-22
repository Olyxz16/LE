#ifndef BLECONTROLLER_H
#define BLECONTROLLER_H

#include <QObject>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QBluetoothDeviceInfo>
#include <QBluetoothUuid>

class BleScanner;
class SerialController;

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
    
    const QBluetoothUuid HW_SERVICE_UUID = QBluetoothUuid(QUuid("{00000000-0001-11e1-9ab4-0002a5d5c51b}"));
    const QBluetoothUuid ENV_CHAR_UUID = QBluetoothUuid(QUuid("{001c0000-0001-11e1-ac36-0002a5d5c51b}"));
};

#endif // BLECONTROLLER_H