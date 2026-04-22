#ifndef SERIALCONTROLLER_H
#define SERIALCONTROLLER_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QByteArray>

class SerialController : public QObject
{
    Q_OBJECT
public:
    explicit SerialController(QObject *parent = nullptr);
    ~SerialController();

    static QList<QSerialPortInfo> availablePorts();
    bool connectToPort(const QString &portName);
    void disconnectFromPort();
    bool isConnected() const;

signals:
    void connected();
    void disconnected();
    void environmentalUpdated(double tempC, double pressHpa, double humPct);
    void errorOccurred(const QString &message);

private slots:
    void onReadyRead();
    void onErrorOccurred(QSerialPort::SerialPortError error);

private:
    void processByte(uint8_t byte);
    void processFrame(uint8_t type, const QByteArray &payload);

    QSerialPort *m_serial;
    QByteArray m_buffer;

    enum State {
        IDLE,
        SYNC1,
        SYNC2,
        TYPE,
        LEN,
        PAYLOAD,
        CHECKSUM
    };

    State m_state = IDLE;
    uint8_t m_currentType;
    uint8_t m_currentLen;
    QByteArray m_currentPayload;
    uint8_t m_checksum;
};

#endif // SERIALCONTROLLER_H
