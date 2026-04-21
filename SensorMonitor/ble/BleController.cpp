#include "BleController.h"
#include "SensorPayloadParser.h"
#include <QLowEnergyDescriptor>

BleController::BleController(QObject *parent) : QObject(parent)
{
}

void BleController::connectToDevice(const QBluetoothDeviceInfo &device)
{
    if (m_controller) {
        m_controller->disconnectFromDevice();
        delete m_controller;
        m_controller = nullptr;
    }

    m_controller = QLowEnergyController::createCentral(device, this);
    
    connect(m_controller, &QLowEnergyController::connected, this, &BleController::onControllerConnected);
    connect(m_controller, &QLowEnergyController::disconnected, this, &BleController::onControllerDisconnected);
    connect(m_controller, &QLowEnergyController::discoveryFinished, this, &BleController::onDiscoveryFinished);
    connect(m_controller, QOverload<QLowEnergyController::Error>::of(&QLowEnergyController::error), this, &BleController::onError);

    m_controller->connectToDevice();
}

void BleController::disconnectFromDevice()
{
    if (m_controller) {
        m_controller->disconnectFromDevice();
    }
}

void BleController::onControllerConnected()
{
    emit connected();
    m_controller->discoverServices();
}

void BleController::onControllerDisconnected()
{
    emit disconnected();
}

void BleController::onDiscoveryFinished()
{
    qDebug() << "BLE Discovery Finished. Services found:" << m_controller->services().size();
    for (const QBluetoothUuid &uuid : m_controller->services()) {
        qDebug() << " - Service:" << uuid.toString();
    }

    m_hwService = m_controller->createServiceObject(HW_SERVICE_UUID, this);
    if (m_hwService) {
        qDebug() << "Hardware Service created";
        connect(m_hwService, &QLowEnergyService::stateChanged, this, &BleController::onServiceStateChanged);
        connect(m_hwService, &QLowEnergyService::characteristicChanged, this, &BleController::onCharacteristicChanged);
        m_hwService->discoverDetails();
    } else {
        qDebug() << "Hardware Service NOT found!";
    }

    // Also try to create SW Service
    QLowEnergyService *swService = m_controller->createServiceObject(SW_SERVICE_UUID, this);
    if (swService) {
        qDebug() << "Software Service created";
        connect(swService, &QLowEnergyService::stateChanged, this, &BleController::onServiceStateChanged);
        connect(swService, &QLowEnergyService::characteristicChanged, this, &BleController::onCharacteristicChanged);
        swService->discoverDetails();
    }
}

void BleController::onServiceStateChanged(QLowEnergyService::ServiceState newState)
{
    QLowEnergyService *service = qobject_cast<QLowEnergyService*>(sender());
    if (!service) return;

    if (newState == QLowEnergyService::ServiceDiscovered) {
        qDebug() << "Service Details Discovered:" << service->serviceUuid().toString();
        
        for (const QLowEnergyCharacteristic &c : service->characteristics()) {
            qDebug() << "  - Char:" << c.uuid().toString();
            
            if (c.uuid() == ENV_CHAR_UUID || c.uuid() == ACC_CHAR_UUID || c.uuid() == QUAT_CHAR_UUID) {
                QLowEnergyDescriptor cccd = c.descriptor(QBluetoothUuid::ClientCharacteristicConfiguration);
                if (cccd.isValid()) {
                    qDebug() << "    Enabling notifications for" << c.uuid().toString();
                    service->writeDescriptor(cccd, QByteArray::fromHex("0100"));
                }
            }
        }
    }
}

void BleController::onCharacteristicChanged(const QLowEnergyCharacteristic &characteristic, const QByteArray &newValue)
{
    // qDebug() << "BLE Char Changed:" << characteristic.uuid().toString() << "Data:" << newValue.toHex(' ');

    if (characteristic.uuid() == ENV_CHAR_UUID) {
        EnvironmentalSample sample = SensorPayloadParser::parseEnvironmental(newValue);
        emit environmentalUpdated(sample.tempC, sample.pressHpa, sample.humPct);
    } else if (characteristic.uuid() == ACC_CHAR_UUID) {
        MotionSample sample = SensorPayloadParser::parseMotion(newValue);
        emit accGyroMagUpdated(sample.acc, sample.gyro, sample.mag);
    }
}

void BleController::onError(QLowEnergyController::Error error)
{
    Q_UNUSED(error);
    emit errorOccurred(m_controller->errorString());
}
