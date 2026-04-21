#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QBluetoothDeviceInfo>

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

QT_CHARTS_USE_NAMESPACE

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class BleScanner;
class BleController;
class SerialController;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onScanClicked();
    void onConnectClicked();
    void onDisconnectClicked();
    void onRefreshPortsClicked();
    void onModeChanged();
    
    void onDeviceFound(const QBluetoothDeviceInfo &device);
    void onConnected();
    void onDisconnected();
    void onError(const QString &message);
    void onEnvironmentalUpdated(double tempC, double pressHpa, double humPct);
    void onAccGyroMagUpdated(const QVector3D &acc, const QVector3D &gyro, const QVector3D &mag);
    void log(const QString &message);

private:
    void setupCharts();
    void setupSerialUi();

    Ui::MainWindow *ui;
    BleScanner *m_bleScanner;
    BleController *m_bleController;
    SerialController *m_serialController;
    
    enum Mode { BLE, SERIAL };
    Mode m_mode = BLE;

    QBluetoothDeviceInfo m_targetDevice;

    QLineSeries *m_tempSeries;
    QValueAxis *m_axisX;
    QValueAxis *m_axisY;
    int m_sampleCount;
};

#endif // MAINWINDOW_H
