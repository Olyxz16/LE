#ifndef SENSORPAYLOADPARSER_H
#define SENSORPAYLOADPARSER_H

#include <QByteArray>
#include <QtEndian>

struct EnvironmentalSample {
    double tempC;
    double pressHpa;
    double humPct;
    quint16 timestamp;
};

class SensorPayloadParser
{
public:
    static EnvironmentalSample parseEnvironmental(const QByteArray &data);
};

#endif // SENSORPAYLOADPARSER_H
