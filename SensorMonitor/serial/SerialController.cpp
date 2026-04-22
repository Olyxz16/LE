#include "SerialController.h"
#include "../ble/SensorPayloadParser.h"
#include <QDebug>

SerialController::SerialController(QObject *parent) : QObject(parent)
{
    m_serial = new QSerialPort(this);
    connect(m_serial, &QSerialPort::readyRead, this, &SerialController::onReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred, this, &SerialController::onErrorOccurred);
}

SerialController::~SerialController()
{
    disconnectFromPort();
}

QList<QSerialPortInfo> SerialController::availablePorts()
{
    return QSerialPortInfo::availablePorts();
}

bool SerialController::connectToPort(const QString &portName)
{
    if (m_serial->isOpen()) {
        m_serial->close();
    }

    m_serial->setPortName(portName);
    m_serial->setBaudRate(QSerialPort::Baud115200);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (m_serial->open(QIODevice::ReadWrite)) {
        m_state = IDLE;
        emit connected();
        return true;
    } else {
        emit errorOccurred(tr("Failed to open port %1: %2")
                           .arg(portName).arg(m_serial->errorString()));
        return false;
    }
}

void SerialController::disconnectFromPort()
{
    if (m_serial->isOpen()) {
        m_serial->close();
        emit disconnected();
    }
}

bool SerialController::isConnected() const
{
    return m_serial->isOpen();
}

void SerialController::onErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::ResourceError) {
        emit errorOccurred(tr("Serial port error: %1").arg(m_serial->errorString()));
        disconnectFromPort();
    }
}

void SerialController::onReadyRead()
{
    QByteArray data = m_serial->readAll();
    qDebug() << "Serial Received:" << data.toHex(' ');
    for (int i = 0; i < data.size(); ++i) {
        processByte(static_cast<uint8_t>(data[i]));
    }
}

void SerialController::processByte(uint8_t byte)
{
    // qDebug() << "State:" << m_state << "Byte:" << QString::number(byte, 16);
    switch (m_state) {
    case IDLE:
        if (byte == 0xA5) m_state = SYNC2;
        break;
    case SYNC2:
        if (byte == 0x5A) m_state = TYPE;
        else if (byte == 0xA5) m_state = SYNC2;
        else m_state = IDLE;
        break;
    case TYPE:
        m_currentType = byte;
        m_state = LEN;
        break;
    case LEN:
        m_currentLen = byte;
        m_currentPayload.clear();
        if (m_currentLen > 40) { // Sanity check
            m_state = IDLE;
        } else if (m_currentLen > 0) {
            m_state = PAYLOAD;
        } else {
            m_state = CHECKSUM;
        }
        break;
    case PAYLOAD:
        m_currentPayload.append(static_cast<char>(byte));
        if (m_currentPayload.size() >= m_currentLen) {
            m_state = CHECKSUM;
        }
        break;
    case CHECKSUM:
        {
            uint8_t expectedChecksum = m_currentType ^ m_currentLen;
            for (int i = 0; i < m_currentPayload.size(); ++i) {
                expectedChecksum ^= static_cast<uint8_t>(m_currentPayload[i]);
            }
            if (byte == expectedChecksum) {
                qDebug() << "Valid Serial Frame, Type:" << m_currentType;
                processFrame(m_currentType, m_currentPayload);
            } else {
                qDebug() << "Serial Checksum Fail! Type:" << m_currentType << "Exp:" << expectedChecksum << "Got:" << byte;
            }
            m_state = IDLE;
        }
        break;
    default:
        m_state = IDLE;
        break;
    }
}
void SerialController::processFrame(uint8_t type, const QByteArray &payload)
{
    if (type == 1) { // Environmental
        if (payload.size() >= 10) {
            EnvironmentalSample sample = SensorPayloadParser::parseEnvironmental(payload);
            emit environmentalUpdated(sample.tempC, sample.pressHpa, sample.humPct);
        }
    }
}
