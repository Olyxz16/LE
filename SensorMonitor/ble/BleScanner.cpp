#include "BleScanner.h"

BleScanner::BleScanner(QObject *parent) : QObject(parent)
{
    m_agent = new QBluetoothDeviceDiscoveryAgent(this);
    m_agent->setLowEnergyDiscoveryTimeout(10000); // 10 seconds

    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered, this, &BleScanner::onDeviceDiscovered);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::finished, this, &BleScanner::onScanFinished);
    connect(m_agent, QOverload<QBluetoothDeviceDiscoveryAgent::Error>::of(&QBluetoothDeviceDiscoveryAgent::error), this, &BleScanner::onError);
}

void BleScanner::startScan()
{
    m_agent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}

void BleScanner::stopScan()
{
    m_agent->stop();
}

void BleScanner::onDeviceDiscovered(const QBluetoothDeviceInfo &device)
{
    if (device.name() == TARGET_NAME) {
        m_agent->stop();
        emit deviceFound(device);
    }
}

void BleScanner::onScanFinished()
{
    // If we didn't find the device, the UI will still be in scanning state or we can emit something.
}

void BleScanner::onError(QBluetoothDeviceDiscoveryAgent::Error error)
{
    emit errorOccurred(m_agent->errorString());
}
