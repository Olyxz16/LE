#ifndef SENSORPAYLOADPARSER_H
#define SENSORPAYLOADPARSER_H

#include <QByteArray>
#include <QVector3D>
#include <QtEndian>

struct EnvironmentalSample {
    double tempC;
    double pressHpa;
    double humPct;
    quint16 timestamp;
};

struct MotionSample {
    QVector3D acc;
    QVector3D gyro;
    QVector3D mag;
    quint16 timestamp;
};

class SensorPayloadParser
{
public:
    static EnvironmentalSample parseEnvironmental(const QByteArray &data);
    static MotionSample parseMotion(const QByteArray &data);
};

#endif // SENSORPAYLOADPARSER_H
