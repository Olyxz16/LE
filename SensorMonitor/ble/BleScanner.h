#ifndef BLESCANNER_H
#define BLESCANNER_H

#include <QObject>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>

class BleScanner : public QObject
{
    Q_OBJECT
public:
    explicit BleScanner(QObject *parent = nullptr);
    void startScan();
    void stopScan();

signals:
    void deviceFound(const QBluetoothDeviceInfo &device);
    void errorOccurred(const QString &message);

private slots:
    void onDeviceDiscovered(const QBluetoothDeviceInfo &device);
    void onScanFinished();
    void onError(QBluetoothDeviceDiscoveryAgent::Error error);

private:
    QBluetoothDeviceDiscoveryAgent *m_agent;
    const QString TARGET_NAME = "CEDRNAI";
};

#endif // BLESCANNER_H
