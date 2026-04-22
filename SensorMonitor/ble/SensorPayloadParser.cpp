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


