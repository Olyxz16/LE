#ifndef SENSORDATA_H
#define SENSORDATA_H

#include <QObject>

class SensorData : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double temperature READ temperature WRITE setTemperature NOTIFY temperatureChanged)
    Q_PROPERTY(double pressure READ pressure WRITE setPressure NOTIFY pressureChanged)
    Q_PROPERTY(double humidity READ humidity WRITE setHumidity NOTIFY humidityChanged)

public:
    explicit SensorData(QObject *parent = nullptr) : QObject(parent), m_temperature(0), m_pressure(0), m_humidity(0) {}

    double temperature() const { return m_temperature; }
    void setTemperature(double t) { if (m_temperature != t) { m_temperature = t; emit temperatureChanged(); } }

    double pressure() const { return m_pressure; }
    void setPressure(double p) { if (m_pressure != p) { m_pressure = p; emit pressureChanged(); } }

    double humidity() const { return m_humidity; }
    void setHumidity(double h) { if (m_humidity != h) { m_humidity = h; emit humidityChanged(); } }

signals:
    void temperatureChanged();
    void pressureChanged();
    void humidityChanged();

private:
    double m_temperature;
    double m_pressure;
    double m_humidity;
};

#endif // SENSORDATA_H
