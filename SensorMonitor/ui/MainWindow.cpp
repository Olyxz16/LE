#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "../ble/BleScanner.h"
#include "../ble/BleController.h"
#include "../serial/SerialController.h"
#include <QDateTime>

#include <QComboBox>
#include <QRadioButton>
#include <QButtonGroup>

QT_CHARTS_USE_NAMESPACE

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_bleScanner(new BleScanner(this))
    , m_bleController(new BleController(this))
    , m_serialController(new SerialController(this))
{
    ui->setupUi(this);

    setupSerialUi();

    connect(ui->scanButton, &QPushButton::clicked, this, &MainWindow::onScanClicked);
    connect(ui->connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(ui->disconnectButton, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);

    // BLE Signals
    connect(m_bleScanner, &BleScanner::deviceFound, this, &MainWindow::onDeviceFound);
    connect(m_bleScanner, &BleScanner::errorOccurred, this, &MainWindow::onError);

    connect(m_bleController, &BleController::connected, this, &MainWindow::onConnected);
    connect(m_bleController, &BleController::disconnected, this, &MainWindow::onDisconnected);
    connect(m_bleController, &BleController::errorOccurred, this, &MainWindow::onError);
    connect(m_bleController, &BleController::environmentalUpdated, this, &MainWindow::onEnvironmentalUpdated);

    // Serial Signals
    connect(m_serialController, &SerialController::connected, this, &MainWindow::onConnected);
    connect(m_serialController, &SerialController::disconnected, this, &MainWindow::onDisconnected);
    connect(m_serialController, &SerialController::errorOccurred, this, &MainWindow::onError);
    connect(m_serialController, &SerialController::environmentalUpdated, this, &MainWindow::onEnvironmentalUpdated);

    setupCharts();

    log("Application started");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupSerialUi()
{
    // Mode selection
    QGroupBox *modeGroup = new QGroupBox("Mode", this);
    modeGroup->setObjectName("modeGroup");
    QHBoxLayout *modeLayout = new QHBoxLayout(modeGroup);
    QRadioButton *bleRadio = new QRadioButton("BLE", this);
    QRadioButton *serialRadio = new QRadioButton("Serial", this);
    bleRadio->setChecked(true);
    modeLayout->addWidget(bleRadio);
    modeLayout->addWidget(serialRadio);
    
    QButtonGroup *group = new QButtonGroup(this);
    group->addButton(bleRadio, BLE);
    group->addButton(serialRadio, SERIAL);
    
    connect(group, QOverload<int>::of(&QButtonGroup::buttonClicked), this, [this](int id){
        m_mode = static_cast<Mode>(id);
        onModeChanged();
    });

    // Serial Port selection
    QComboBox *portCombo = new QComboBox(this);
    portCombo->setObjectName("portCombo");
    
    QPushButton *refreshBtn = new QPushButton("Refresh", this);
    refreshBtn->setObjectName("refreshBtn");
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshPortsClicked);

    // Insert into toolbar
    ui->toolbarLayout->insertWidget(0, modeGroup);
    ui->toolbarLayout->insertWidget(1, portCombo);
    ui->toolbarLayout->insertWidget(2, refreshBtn);
    
    onRefreshPortsClicked();
    onModeChanged();
}

void MainWindow::onModeChanged()
{
    bool ble = (m_mode == BLE);
    ui->scanButton->setVisible(ble);
    
    QWidget *portCombo = findChild<QWidget*>("portCombo");
    if (portCombo) portCombo->setVisible(!ble);
    
    QWidget *refreshBtn = findChild<QWidget*>("refreshBtn");
    if (refreshBtn) refreshBtn->setVisible(!ble);

    ui->connectButton->setEnabled(false); // Reset connection button state
    
    if (ble) {
        ui->connectButton->setEnabled(m_targetDevice.isValid());
        ui->statusLabel->setText("Status: BLE Mode");
    } else {
        if (QComboBox *combo = qobject_cast<QComboBox*>(portCombo)) {
            ui->connectButton->setEnabled(combo->count() > 0);
        }
        ui->statusLabel->setText("Status: Serial Mode");
    }
}

void MainWindow::onRefreshPortsClicked()
{
    QComboBox *portCombo = findChild<QComboBox*>("portCombo");
    if (!portCombo) return;
    
    portCombo->clear();
    const auto infos = SerialController::availablePorts();
    for (const auto &info : infos) {
        portCombo->addItem(info.portName());
    }
    
    if (m_mode == SERIAL) {
        ui->connectButton->setEnabled(portCombo->count() > 0);
    }
}

void MainWindow::setupCharts()
{
    m_tempSeries = new QLineSeries(this);
    QChart *chart = new QChart();
    chart->addSeries(m_tempSeries);
    chart->setTitle("Temperature History");
    chart->legend()->hide();

    m_axisX = new QValueAxis();
    m_axisX->setRange(0, 60);
    m_axisX->setLabelFormat("%g");
    m_axisX->setTitleText("Samples");

    m_axisY = new QValueAxis();
    m_axisY->setRange(20, 30);
    m_axisY->setTitleText("Temp (°C)");

    chart->addAxis(m_axisX, Qt::AlignBottom);
    chart->addAxis(m_axisY, Qt::AlignLeft);
    m_tempSeries->attachAxis(m_axisX);
    m_tempSeries->attachAxis(m_axisY);

    QChartView *chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    ui->envLayout->addWidget(chartView);

    m_sampleCount = 0;
}

void MainWindow::onScanClicked()
{
    log("BLE Scan started");
    ui->statusLabel->setText("Status: Scanning...");
    ui->scanButton->setEnabled(false);
    ui->connectButton->setEnabled(false);
    m_bleScanner->startScan();
}

void MainWindow::onConnectClicked()
{
    if (m_mode == BLE) {
        if (m_targetDevice.isValid()) {
            log("Connecting (BLE) to " + m_targetDevice.name());
            ui->statusLabel->setText("Status: Connecting BLE...");
            ui->connectButton->setEnabled(false);
            m_bleController->connectToDevice(m_targetDevice);
        }
    } else {
        QComboBox *portCombo = findChild<QComboBox*>("portCombo");
        if (portCombo && portCombo->count() > 0) {
            QString portName = portCombo->currentText();
            log("Connecting (Serial) to " + portName);
            ui->statusLabel->setText("Status: Connecting Serial...");
            ui->connectButton->setEnabled(false);
            m_serialController->connectToPort(portName);
        }
    }
}

void MainWindow::onDisconnectClicked()
{
    log("Disconnecting...");
    if (m_mode == BLE) {
        m_bleController->disconnectFromDevice();
    } else {
        m_serialController->disconnectFromPort();
    }
}

void MainWindow::onDeviceFound(const QBluetoothDeviceInfo &device)
{
    m_targetDevice = device;
    log("Device found: " + device.name() + " (" + device.address().toString() + ")");
    if (m_mode == BLE) {
        ui->statusLabel->setText("Status: Device found");
        ui->scanButton->setEnabled(true);
        ui->connectButton->setEnabled(true);
    }
}

void MainWindow::onConnected()
{
    log("Connected");
    ui->statusLabel->setText("Status: Connected");
    ui->connectButton->setEnabled(false);
    ui->disconnectButton->setEnabled(true);
}

void MainWindow::onDisconnected()
{
    log("Disconnected");
    ui->statusLabel->setText("Status: Disconnected");
    ui->disconnectButton->setEnabled(false);
    onModeChanged(); // Restore buttons based on mode
}

void MainWindow::onError(const QString &message)
{
    log("Error: " + message);
    ui->statusLabel->setText("Status: Error");
    onModeChanged();
}

void MainWindow::onEnvironmentalUpdated(double tempC, double pressHpa, double humPct)
{
    ui->tempLabel->setText(QString("Temp: %1 °C").arg(tempC, 0, 'f', 1));
    ui->pressLabel->setText(QString("Press: %1 hPa").arg(pressHpa, 0, 'f', 2));
    ui->humLabel->setText(QString("Hum: %1 %").arg(humPct, 0, 'f', 1));

    m_tempSeries->append(m_sampleCount++, tempC);
    if (m_sampleCount > 60) {
        m_axisX->setRange(m_sampleCount - 60, m_sampleCount);
    }

    if (tempC < m_axisY->min() || tempC > m_axisY->max()) {
        m_axisY->setRange(qMin(m_axisY->min(), tempC - 1), qMax(m_axisY->max(), tempC + 1));
    }
}

void MainWindow::log(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    ui->logOutput->appendPlainText(QString("[%1] %2").arg(timestamp, message));
}
