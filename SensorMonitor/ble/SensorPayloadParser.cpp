#include "SensorPayloadParser.h"

EnvironmentalSample SensorPayloadParser::parseEnvironmental(const QByteArray &data)
{
    EnvironmentalSample sample = {0.0, 0.0, 0.0, 0};
    if (data.size() < 10) return sample;

    const uchar *p = reinterpret_cast<const uchar*>(data.constData());

    sample.timestamp = qFromLittleEndian<quint16>(p);
    qint32 press = qFromLittleEndian<qint32>(p + 2);
    quint16 hum = qFromLittleEndian<quint16>(p + 6);
    qint16 temp = qFromLittleEndian<qint16>(p + 8);

    sample.pressHpa = press / 100.0;
    sample.humPct = hum / 10.0;
    sample.tempC = temp / 10.0;

    return sample;
}

MotionSample SensorPayloadParser::parseMotion(const QByteArray &data)
{
    MotionSample sample;
    if (data.size() < 20) return sample;

    const uchar *p = reinterpret_cast<const uchar*>(data.constData());

    sample.timestamp = qFromLittleEndian<quint16>(p);
    
    qint16 ax = qFromLittleEndian<qint16>(p + 2);
    qint16 ay = qFromLittleEndian<qint16>(p + 4);
    qint16 az = qFromLittleEndian<qint16>(p + 6);
    
    qint16 gx = qFromLittleEndian<qint16>(p + 8);
    qint16 gy = qFromLittleEndian<qint16>(p + 10);
    qint16 gz = qFromLittleEndian<qint16>(p + 12);
    
    qint16 mx = qFromLittleEndian<qint16>(p + 14);
    qint16 my = qFromLittleEndian<qint16>(p + 16);
    qint16 mz = qFromLittleEndian<qint16>(p + 18);

    sample.acc = QVector3D(ax, ay, az);
    sample.gyro = QVector3D(gx, gy, gz);
    sample.mag = QVector3D(mx, my, mz);

    return sample;
}
