#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "config.h"
#include "ublox_structs.h"
#include "settings.h"
#include "status.h"
#include "tcpmessage_keys.h"
#include "map.h"
#include "i2cform.h"
#include "calibform.h"
#include "calibscandialog.h"
#include "gpssatsform.h"
#include "histogram.h"
#include "histogramdataform.h"
#include "muondetector_structs.h"
#include "parametermonitorform.h"
#include "logplotswidget.h"
#include "scanform.h"

#include <QThread>
#include <QFile>
#include <QKeyEvent>
#include <QDebug>
#include <QErrorMessage>

#include <iostream>

//#include <gnsssatellite.h>

using namespace std;


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    qRegisterMetaType<TcpMessage>("TcpMessage");
    qRegisterMetaType<GeodeticPos>("GeodeticPos");
    qRegisterMetaType<bool>("bool");
    qRegisterMetaType<I2cDeviceEntry>("I2cDeviceEntry");
    qRegisterMetaType<CalibStruct>("CalibStruct");
    qRegisterMetaType<std::vector<GnssSatellite>>("std::vector<GnssSatellite>");
    qRegisterMetaType<UbxTimePulseStruct>("UbxTimePulseStruct");
    qRegisterMetaType<GPIO_PIN>("GPIO_PIN");
    qRegisterMetaType<GnssMonHwStruct>("GnssMonHwStruct");
    qRegisterMetaType<GnssMonHw2Struct>("GnssMonHw2Struct");
    qRegisterMetaType<UbxTimeMarkStruct>("UbxTimeMarkStruct");
    qRegisterMetaType<int32_t>("int32_t");
    qRegisterMetaType<uint32_t>("uint32_t");
    qRegisterMetaType<uint16_t>("uint16_t");
    qRegisterMetaType<uint8_t>("uint8_t");
    qRegisterMetaType<int8_t>("int8_t");
    qRegisterMetaType<std::vector<GnssConfigStruct>>("std::vector<GnssConfigStruct>");
    qRegisterMetaType<std::chrono::duration<double>>("std::chrono::duration<double>");
    qRegisterMetaType<std::string>("std::string");
//	qRegisterMetaType<LogParameter>("LogParameter");
    qRegisterMetaType<UbxDopStruct>("UbxDopStruct");
    qRegisterMetaType<timespec>("timespec");

    ui->setupUi(this);
    this->setWindowTitle(QString("muondetector-gui  "+QString::fromStdString(MuonPi::Version::software.string())));

    ui->discr1Layout->setAlignment(ui->discr1Slider, Qt::AlignHCenter);
    ui->discr2Layout->setAlignment(ui->discr2Slider, Qt::AlignHCenter); // aligns the slider in their vertical layout centered
    QIcon icon(":/res/muon.ico");
    this->setWindowIcon(icon);
    setMaxThreshVoltage(1.0);

    // setup ipBox and load addresses etc.
    addresses = new QStandardItemModel(this);
    loadSettings(addresses);
    ui->ipBox->setModel(addresses);
    ui->ipBox->setCompleter(new QCompleter{});
    ui->ipBox->setEditable(true);
    //ui->ipBox->installEventFilter(this);

    // setup colors
    ui->ipStatusLabel->setStyleSheet("QLabel {color : darkGray;}");/*
    ui->discr1Hit->setStyleSheet("QLabel {background-color : darkRed;}");
    ui->discr2Hit->setStyleSheet("QLabel {background-color : darkRed;}");*/

    // setup event filter
    ui->ipBox->installEventFilter(this);
    ui->ipButton->installEventFilter(this);

    // setup signal/slots
    connect(ui->ipButton, &QPushButton::pressed, this, &MainWindow::onIpButtonClicked);

    // set timer for and/xor label color change after hit
    int timerInterval = 100; // in msec
    andTimer.setSingleShot(true);
    xorTimer.setSingleShot(true);
    andTimer.setInterval(timerInterval);
    xorTimer.setInterval(timerInterval);
    connect(&andTimer, &QTimer::timeout, this, &MainWindow::resetAndHit);
    connect(&xorTimer, &QTimer::timeout, this, &MainWindow::resetXorHit);

    ui->ANDHit->setFocusPolicy(Qt::NoFocus);
    ui->XORHit->setFocusPolicy(Qt::NoFocus);

    // set timer for automatic rate poll
    if (automaticRatePoll){
        ratePollTimer.setInterval(5000);
        ratePollTimer.setSingleShot(false);
        connect(&ratePollTimer, &QTimer::timeout, this, &MainWindow::sendRequestGpioRates);
        connect(&ratePollTimer, &QTimer::timeout, this, &MainWindow::sendValueUpdateRequests);
        ratePollTimer.start();
    }

    // set mainwindow
    //connect(ui->saveDacButton, &QPushButton, this, &MainWindow::on_saveThresholdsButton_clicked);

    // set all tabs
    ui->tabWidget->removeTab(0);
    Status *status = new Status(this);
    connect(this, &MainWindow::setUiEnabledStates, status, &Status::onUiEnabledStateChange);
    connect(this, &MainWindow::gpioRates, status, &Status::onGpioRatesReceived);
    connect(status, &Status::resetRateClicked, this, [this](){ this->sendRequest(TCP_MSG_KEY::MSG_GPIO_RATE_RESET); } );
    connect(this, &MainWindow::adcSampleReceived, status, &Status::onAdcSampleReceived);
    connect(this, &MainWindow::dacReadbackReceived, status, &Status::onDacReadbackReceived);
    connect(status, &Status::inputSwitchChanged, this, &MainWindow::sendInputSwitch);
    connect(this, &MainWindow::inputSwitchReceived, status, &Status::onInputSwitchReceived);
    connect(this, &MainWindow::biasSwitchReceived, status, &Status::onBiasSwitchReceived);
    connect(status, &Status::biasSwitchChanged, this, &MainWindow::sendSetBiasStatus);
    connect(this, &MainWindow::preampSwitchReceived, status, &Status::onPreampSwitchReceived);
    connect(status, &Status::preamp1SwitchChanged, this, &MainWindow::sendPreamp1Switch);
    connect(status, &Status::preamp2SwitchChanged, this, &MainWindow::sendPreamp2Switch);
    connect(this, &MainWindow::gainSwitchReceived, status, &Status::onGainSwitchReceived);
    connect(status, &Status::gainSwitchChanged, this, &MainWindow::sendGainSwitch);
    connect(this, &MainWindow::temperatureReceived, status, &Status::onTemperatureReceived);
    connect(this, &MainWindow::triggerSelectionReceived, status, &Status::onTriggerSelectionReceived);
    connect(status, &Status::triggerSelectionChanged, this, &MainWindow::onTriggerSelectionChanged);
    connect(this, &MainWindow::timepulseReceived, status, &Status::onTimepulseReceived);
    connect(this, &MainWindow::mqttStatusChanged, status, &Status::onMqttStatusChanged);

    ui->tabWidget->addTab(status,"Overview");

    Settings *settings = new Settings(this);
    connect(this, &MainWindow::setUiEnabledStates, settings, &Settings::onUiEnabledStateChange);
    connect(this, &MainWindow::txBufReceived, settings, &Settings::onTxBufReceived);
    connect(this, &MainWindow::txBufPeakReceived, settings, &Settings::onTxBufPeakReceived);
    connect(this, &MainWindow::rxBufReceived, settings, &Settings::onRxBufReceived);
    connect(this, &MainWindow::rxBufPeakReceived, settings, &Settings::onRxBufPeakReceived);
    connect(this, &MainWindow::addUbxMsgRates, settings, &Settings::addUbxMsgRates);
    connect(settings, &Settings::sendRequestUbxMsgRates, this, &MainWindow::sendRequestUbxMsgRates);
    connect(settings, &Settings::sendSetUbxMsgRateChanges, this, &MainWindow::sendSetUbxMsgRateChanges);
    connect(settings, &Settings::sendUbxReset, this, &MainWindow::onSendUbxReset);
    connect(settings, &Settings::sendUbxConfigDefault, this, [this](){ this->sendRequest(TCP_MSG_KEY::MSG_UBX_CONFIG_DEFAULT); } );
    connect(this, &MainWindow::gnssConfigsReceived, settings, &Settings::onGnssConfigsReceived);
    connect(settings, &Settings::setGnssConfigs, this, &MainWindow::onSetGnssConfigs);
    connect(this, &MainWindow::gpsTP5Received, settings, &Settings::onTP5Received);
    connect(settings, &Settings::setTP5Config, this, &MainWindow::onSetTP5Config);
    connect(settings, &Settings::sendUbxSaveCfg, this, [this](){ this->sendRequest(TCP_MSG_KEY::MSG_UBX_CFG_SAVE); } );

    ui->tabWidget->addTab(settings,"Ublox Settings");

    Map *map = new Map(this);
    ui->tabWidget->addTab(map, "Map");
    connect(this, &MainWindow::geodeticPos, map, &Map::onGeodeticPosReceived);



    I2cForm *i2cTab = new I2cForm(this);
    connect(this, &MainWindow::setUiEnabledStates, i2cTab, &I2cForm::onUiEnabledStateChange);
    connect(this, &MainWindow::i2cStatsReceived, i2cTab, &I2cForm::onI2cStatsReceived);
    connect(i2cTab, &I2cForm::i2cStatsRequest, this, [this]() { this->sendRequest(TCP_MSG_KEY::MSG_I2C_STATS_REQUEST); } );
    connect(i2cTab, &I2cForm::scanI2cBusRequest, this, [this]() { this->sendRequest(TCP_MSG_KEY::MSG_I2C_SCAN_BUS); } );

    ui->tabWidget->addTab(i2cTab,"I2C bus");

    calib = new CalibForm(this);
    connect(this, &MainWindow::setUiEnabledStates, calib, &CalibForm::onUiEnabledStateChange);
    connect(this, &MainWindow::calibReceived, calib, &CalibForm::onCalibReceived);
    connect(calib, &CalibForm::calibRequest, this, [this]() { this->sendRequest(TCP_MSG_KEY::MSG_CALIB_REQUEST); } );
    connect(calib, &CalibForm::writeCalibToEeprom, this, [this]() { this->sendRequest(TCP_MSG_KEY::MSG_CALIB_SAVE); } );
    connect(this, &MainWindow::adcSampleReceived, calib, &CalibForm::onAdcSampleReceived);
    connect(calib, &CalibForm::setBiasDacVoltage, this, &MainWindow::sendSetBiasVoltage);
    connect(calib, &CalibForm::setDacVoltage, this, &MainWindow::sendSetThresh);
    connect(calib, &CalibForm::updatedCalib, this, &MainWindow::onCalibUpdated);
    connect(calib, &CalibForm::setBiasSwitch, this, &MainWindow::sendSetBiasStatus);
    ui->tabWidget->addTab(calib,"Calibration");

    calibscandialog = new CalibScanDialog(this);
    calibscandialog->hide();
    connect(this, &MainWindow::calibReceived, calibscandialog, &CalibScanDialog::onCalibReceived);
//    connect(calib, &CalibForm::calibRequest, this, [this]() { this->sendRequest(calibRequestSig); } );
//    connect(calib, &CalibForm::writeCalibToEeprom, this, [this]() { this->sendRequest(calibWriteEepromSig); } );
    connect(this, &MainWindow::adcSampleReceived, calibscandialog, &CalibScanDialog::onAdcSampleReceived);
    connect(calib, &CalibForm::setBiasDacVoltage, this, &MainWindow::sendSetBiasVoltage);
    connect(calib, &CalibForm::setDacVoltage, this, &MainWindow::sendSetThresh);
    connect(calib, &CalibForm::updatedCalib, this, &MainWindow::onCalibUpdated);

    GpsSatsForm *satsTab = new GpsSatsForm(this);
    connect(this, &MainWindow::setUiEnabledStates, satsTab, &GpsSatsForm::onUiEnabledStateChange);
    connect(this, &MainWindow::satsReceived, satsTab, &GpsSatsForm::onSatsReceived);
    connect(this, &MainWindow::timeAccReceived, satsTab, &GpsSatsForm::onTimeAccReceived);
    connect(this, &MainWindow::freqAccReceived, satsTab, &GpsSatsForm::onFreqAccReceived);
    connect(this, &MainWindow::intCounterReceived, satsTab, &GpsSatsForm::onIntCounterReceived);
    connect(this, &MainWindow::gpsMonHWReceived, satsTab, &GpsSatsForm::onGpsMonHWReceived);
    connect(this, &MainWindow::gpsMonHW2Received, satsTab, &GpsSatsForm::onGpsMonHW2Received);
    connect(this, &MainWindow::gpsVersionReceived, satsTab, &GpsSatsForm::onGpsVersionReceived);
    connect(this, &MainWindow::gpsFixReceived, satsTab, &GpsSatsForm::onGpsFixReceived);
    connect(this, &MainWindow::geodeticPos, satsTab, &GpsSatsForm::onGeodeticPosReceived);
    connect(this, &MainWindow::ubxUptimeReceived, satsTab, &GpsSatsForm::onUbxUptimeReceived);


/*
//    connect(this, &MainWindow::setUiEnabledStates, settings, &Settings::onUiEnabledStateChange);
    connect(this, &MainWindow::calibReceived, calibTab, &CalibForm::onCalibReceived);
    connect(calibTab, &CalibForm::calibRequest, this, [this]() { this->sendRequest(calibRequestSig); } );
    connect(calibTab, &CalibForm::writeCalibToEeprom, this, [this]() { this->sendRequest(calibWriteEepromSig); } );
    connect(this, &MainWindow::adcSampleReceived, calibTab, &CalibForm::onAdcSampleReceived);
*/
    ui->tabWidget->addTab(satsTab,"GNSS Data");


    histogramDataForm *histoTab = new histogramDataForm(this);
    connect(this, &MainWindow::setUiEnabledStates, histoTab, &histogramDataForm::onUiEnabledStateChange);
    connect(this, &MainWindow::histogramReceived, histoTab, &histogramDataForm::onHistogramReceived);
    connect(histoTab, &histogramDataForm::histogramCleared, this, &MainWindow::onHistogramCleared);
    ui->tabWidget->addTab(histoTab,"Statistics");

    ParameterMonitorForm *paramTab = new ParameterMonitorForm(this);
    //connect(this, &MainWindow::setUiEnabledStates, paramTab, &ParameterMonitorForm::onUiEnabledStateChange);
    connect(this, &MainWindow::adcSampleReceived, paramTab, &ParameterMonitorForm::onAdcSampleReceived);
    connect(this, &MainWindow::adcTraceReceived, paramTab, &ParameterMonitorForm::onAdcTraceReceived);
    connect(this, &MainWindow::dacReadbackReceived, paramTab, &ParameterMonitorForm::onDacReadbackReceived);
    connect(this, &MainWindow::inputSwitchReceived, paramTab, &ParameterMonitorForm::onInputSwitchReceived);
    connect(this, &MainWindow::biasSwitchReceived, paramTab, &ParameterMonitorForm::onBiasSwitchReceived);
    connect(this, &MainWindow::preampSwitchReceived, paramTab, &ParameterMonitorForm::onPreampSwitchReceived);
    connect(this, &MainWindow::triggerSelectionReceived, paramTab, &ParameterMonitorForm::onTriggerSelectionReceived);
    connect(this, &MainWindow::temperatureReceived, paramTab, &ParameterMonitorForm::onTemperatureReceived);
    connect(this, &MainWindow::timeAccReceived, paramTab, &ParameterMonitorForm::onTimeAccReceived);
    connect(this, &MainWindow::freqAccReceived, paramTab, &ParameterMonitorForm::onFreqAccReceived);
//    connect(this, &MainWindow::intCounterReceived, paramTab, &ParameterMonitorForm::onIntCounterReceived);
    connect(this, &MainWindow::gainSwitchReceived, paramTab, &ParameterMonitorForm::onGainSwitchReceived);
    connect(this, &MainWindow::calibReceived, paramTab, &ParameterMonitorForm::onCalibReceived);
    connect(this, &MainWindow::timeMarkReceived, paramTab, &ParameterMonitorForm::onTimeMarkReceived);
    connect(paramTab, &ParameterMonitorForm::adcModeChanged, this, &MainWindow::onAdcModeChanged);
    connect(paramTab, &ParameterMonitorForm::setDacVoltage, this, &MainWindow::sendSetThresh);
    connect(paramTab, &ParameterMonitorForm::preamp1EnableChanged, this, &MainWindow::sendPreamp1Switch);
    connect(paramTab, &ParameterMonitorForm::preamp2EnableChanged, this, &MainWindow::sendPreamp2Switch);
    connect(paramTab, &ParameterMonitorForm::biasEnableChanged, this, &MainWindow::sendSetBiasStatus);
    connect(paramTab, &ParameterMonitorForm::polarityChanged, this, &MainWindow::onPolarityChanged);
    connect(paramTab, &ParameterMonitorForm::timingSelectionChanged, this, &MainWindow::sendInputSwitch);
    connect(paramTab, &ParameterMonitorForm::triggerSelectionChanged, this, &MainWindow::onTriggerSelectionChanged);
    connect(paramTab, &ParameterMonitorForm::gpioInhibitChanged, this, &MainWindow::gpioInhibit);
    ui->tabWidget->addTab(paramTab,"Parameters");

    ScanForm *scanTab = new ScanForm(this);
    //connect(this, &MainWindow::setUiEnabledStates, paramTab, &ParameterMonitorForm::onUiEnabledStateChange);
    connect(this, &MainWindow::timeMarkReceived, scanTab, &ScanForm::onTimeMarkReceived);
    connect(scanTab, &ScanForm::setDacVoltage, this, &MainWindow::sendSetThresh);
    connect(scanTab, &ScanForm::gpioInhibitChanged, this, &MainWindow::gpioInhibit);
    ui->tabWidget->addTab(scanTab,"Scans");

    LogPlotsWidget *logTab = new LogPlotsWidget(this);
    connect(this, &MainWindow::temperatureReceived, logTab, &LogPlotsWidget::onTemperatureReceived);
    connect(this, &MainWindow::timeAccReceived, logTab, &LogPlotsWidget::onTimeAccReceived);
    connect(this, &MainWindow::setUiEnabledStates, logTab, &LogPlotsWidget::onUiEnabledStateChange);
    connect(paramTab, &ParameterMonitorForm::biasVoltageCalculated, logTab, &LogPlotsWidget::onBiasVoltageCalculated);
    connect(paramTab, &ParameterMonitorForm::biasCurrentCalculated, logTab, &LogPlotsWidget::onBiasCurrentCalculated);
    connect(this, &MainWindow::gpioRates, logTab, &LogPlotsWidget::onGpioRatesReceived);
    connect(this, &MainWindow::logInfoReceived, logTab, &LogPlotsWidget::onLogInfoReceived);
    ui->tabWidget->addTab(logTab, "Log");


    //sendRequest(calibRequestSig);

    //settings->show();
    // set menu bar actions
    //connect(ui->actionsettings, &QAction::triggered, this, &MainWindow::settings_clicked);

    const QStandardItemModel *model = dynamic_cast<QStandardItemModel*>(ui->biasControlTypeComboBox->model());
    QStandardItem *item = model->item(1);
    item->setEnabled(false);
    // initialise all ui elements that will be inactive at start
    uiSetDisconnectedState();
}

MainWindow::~MainWindow()
{
    emit closeConnection();
    saveSettings(addresses);
    delete ui;
}

void MainWindow::makeConnection(QString ipAddress, quint16 port) {
    // add popup windows for errors!!!
    QThread *tcpThread = new QThread();
    tcpThread->setObjectName("muondetector-gui-tcp");
    if (!tcpConnection) {
        delete(tcpConnection);
    }
    tcpConnection = new TcpConnection(ipAddress, port, verbose);
    tcpConnection->moveToThread(tcpThread);
    connect(tcpThread, &QThread::started, tcpConnection, &TcpConnection::makeConnection);
    connect(tcpThread, &QThread::finished, tcpThread, &QThread::deleteLater);
    connect(tcpThread, &QThread::finished, tcpConnection, &TcpConnection::deleteLater);
    connect(tcpConnection, &TcpConnection::connected, this, &MainWindow::connected);
    connect(this, &MainWindow::closeConnection, tcpConnection, &TcpConnection::closeThisConnection);
    connect(tcpConnection, &TcpConnection::finished, tcpThread, &QThread::quit);
    connect(this, &MainWindow::sendTcpMessage, tcpConnection, &TcpConnection::sendTcpMessage);
    connect(tcpConnection, &TcpConnection::receivedTcpMessage, this, &MainWindow::receivedTcpMessage);
    tcpThread->start();
}

void MainWindow::onTriggerSelectionChanged(GPIO_PIN signal)
{
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_EVENTTRIGGER);
    *(tcpMessage.dStream) << signal;
    emit sendTcpMessage(tcpMessage);
    sendRequest(TCP_MSG_KEY::MSG_EVENTTRIGGER_REQUEST);
}

bool MainWindow::saveSettings(QStandardItemModel *model) {
#if defined(Q_OS_UNIX)
    QFile file(QString(QStandardPaths::writableLocation(QStandardPaths::HomeLocation)+"/.muondetector-gui.save"));
#elif defined(Q_OS_WIN)
    QFile file(QString("ipAddresses.save"));
#else
    QFile file(QString("ipAddresses.save"));
#endif
    if (!file.open(QIODevice::ReadWrite)) {
        qDebug() << "file open failed in 'ReadWrite' mode at location " << file.fileName();
        return false;
    }
    QDataStream stream(&file);
    qint32 n(model->rowCount());
    stream << n;
    for (int i = 0; i < n; i++) {
        model->item(i)->write(stream);
    }
    file.close();
    return true;
}

bool MainWindow::loadSettings(QStandardItemModel* model) {
#if defined(Q_OS_UNIX)
    QFile file(QString(QStandardPaths::writableLocation(QStandardPaths::HomeLocation)+"/.muondetector-gui.save"));
#elif defined(Q_OS_WIN)
    QFile file(QString("ipAddresses.save"));
#else
    QFile file(QString("ipAddresses.save"));
#endif
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "file open failed in 'ReadOnly' mode at location " << file.fileName();
        return false;
    }
    QDataStream stream(&file);
    qint32 n;
    stream >> n;
    for (int i = 0; i < n; i++) {
        model->appendRow(new QStandardItem());
        model->item(i)->read(stream);
    }
    file.close();
    return true;
}

bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Escape) {
            QCoreApplication::quit();
            return true;
        }
        /*
        if (ke->key() == Qt::Key_Escape) {
            if (connectedToDemon){
                onIpButtonClicked();
            }else{
                QCoreApplication::quit();
            }
            return true;
        }
        if (ke->key() == Qt::Key_Enter && !connectedToDemon){
            onIpButtonClicked();
            return true;
        }*/ // crashes when alternating escape and enter ...why?
        auto combobox = dynamic_cast<QComboBox *>(object);
        if (combobox == ui->ipBox) {
            if (ke->key() == Qt::Key_Delete) {
                ui->ipBox->removeItem(ui->ipBox->currentIndex());
            }else if (ke->key() == Qt::Key_Enter || ke->key() == Qt::Key_Return) {
                onIpButtonClicked();
            }else{
                return QObject::eventFilter(object, event);
            }
        }else {
            return QObject::eventFilter(object, event);
        }
        return true;
    }
    else {
        return QObject::eventFilter(object, event);
    }
}

void MainWindow::receivedTcpMessage(TcpMessage tcpMessage) {
//    quint16 msgID = tcpMessage.getMsgID();
    TCP_MSG_KEY msgID = static_cast<TCP_MSG_KEY>(tcpMessage.getMsgID());
    if (msgID == TCP_MSG_KEY::MSG_GPIO_EVENT) {
//	if (msgID == gpioPinSig) {
        unsigned int gpioPin;
        *(tcpMessage.dStream) >> gpioPin;
        receivedGpioRisingEdge((GPIO_PIN)gpioPin);
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_UBX_MSG_RATE) {
    QMap<uint16_t, int> msgRateCfgs;
        *(tcpMessage.dStream) >> msgRateCfgs;
    emit addUbxMsgRates(msgRateCfgs);
    return;
    }
    if (msgID == TCP_MSG_KEY::MSG_THRESHOLD){
    quint8 channel;
    float threshold;
    *(tcpMessage.dStream) >> channel >> threshold;
    if (threshold > maxThreshVoltage){
        sendSetThresh(channel,maxThreshVoltage);
        return;
    }
        sliderValues[channel] = (int)(2000 * threshold);
        updateUiProperties();
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_BIAS_VOLTAGE){
        *(tcpMessage.dStream) >> biasDacVoltage;
        updateUiProperties();
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_BIAS_SWITCH){
        *(tcpMessage.dStream) >> biasON;
        emit biasSwitchReceived(biasON);
        updateUiProperties();
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_PREAMP_SWITCH){
        quint8 channel;
        bool state;
        *(tcpMessage.dStream) >> channel >> state;
        emit preampSwitchReceived(channel, state);
        updateUiProperties();
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_GAIN_SWITCH){
        bool gainSwitch;
        *(tcpMessage.dStream) >> gainSwitch;
        emit gainSwitchReceived(gainSwitch);
        updateUiProperties();
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_PCA_SWITCH){
        *(tcpMessage.dStream) >> pcaPortMask;
        //status->setInputSwitchButtonGroup(pcaPortMask);
        emit inputSwitchReceived(pcaPortMask);
        updateUiProperties();
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_EVENTTRIGGER){
        unsigned int signal;
        *(tcpMessage.dStream) >> signal;
        emit triggerSelectionReceived((GPIO_PIN)signal);
        //updateUiProperties();
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_GPIO_RATE){
        quint8 whichRate;
        QVector<QPointF> rate;
        *(tcpMessage.dStream) >> whichRate >> rate;
        float rateYValue;
        if (!rate.empty()){
            rateYValue = rate.at(rate.size()-1).y();
        }else{
            rateYValue = 0.0;
        }
        if (whichRate == 0){
            ui->rate1->setText(QString::number(rateYValue,'g',3)+"/s");
        }
        if (whichRate == 1){
            ui->rate2->setText(QString::number(rateYValue,'g',3)+"/s");
        }
        emit gpioRates(whichRate, rate);
        updateUiProperties();
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_QUIT_CONNECTION){
        connectedToDemon = false;
        uiSetDisconnectedState();
    }
    if (msgID == TCP_MSG_KEY::MSG_GEO_POS){
        GeodeticPos pos;
        *(tcpMessage.dStream) >> pos.iTOW >> pos.lon >> pos.lat
                >> pos.height >> pos.hMSL >> pos.hAcc >> pos.vAcc;
        emit geodeticPos(pos);
    }
    if (msgID == TCP_MSG_KEY::MSG_ADC_SAMPLE){
        quint8 channel;
        float value;
        *(tcpMessage.dStream) >> channel >> value;
        emit adcSampleReceived(channel, value);
        updateUiProperties();
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_ADC_TRACE){
        quint16 size;
        QVector<float> sampleBuffer;
        *(tcpMessage.dStream) >> size;
        for (int i=0; i<size; i++) {
            float value;
            *(tcpMessage.dStream) >> value;
            sampleBuffer.push_back(value);
        }
        emit adcTraceReceived(sampleBuffer);
        //qDebug()<<"trace received. length="<<sampleBuffer.size();
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_DAC_READBACK){
        quint8 channel;
        float value;
        *(tcpMessage.dStream) >> channel >> value;
        emit dacReadbackReceived(channel, value);
        updateUiProperties();
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_TEMPERATURE){
        float value;
        *(tcpMessage.dStream) >> value;
        emit temperatureReceived(value);
        updateUiProperties();
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_I2C_STATS){
    quint8 nrDevices=0;
    quint32 bytesRead = 0;
    quint32 bytesWritten = 0;
        *(tcpMessage.dStream) >> nrDevices >> bytesRead >> bytesWritten;

    QVector<I2cDeviceEntry> deviceList;
    for (uint8_t i=0; i<nrDevices; i++)
    {
        uint8_t addr = 0;
        QString title = "none";
        uint8_t status = 0;
        *(tcpMessage.dStream) >> addr >> title >> status;
        I2cDeviceEntry entry;
        entry.address=addr;
        entry.name = title;
        entry.status=status;
        deviceList.push_back(entry);
    }
        emit i2cStatsReceived(bytesRead, bytesWritten, deviceList);
        //updateUiProperties();
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_SPI_STATS){
        bool spiPresent;
        *(tcpMessage.dStream) >> spiPresent;
        emit spiStatsReceived(spiPresent);
    }
    if (msgID == TCP_MSG_KEY::MSG_CALIB_SET){
    quint16 nrPars=0;
    quint64 id = 0;
    bool valid = false;
    bool eepromValid = 0;
        *(tcpMessage.dStream) >> valid >> eepromValid >> id >> nrPars;

    QVector<CalibStruct> calibList;
    for (uint8_t i=0; i<nrPars; i++)
    {
        CalibStruct item;
        *(tcpMessage.dStream) >> item;
        calibList.push_back(item);
    }
        emit calibReceived(valid, eepromValid, id, calibList);
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_GNSS_SATS){
    int nrSats=0;
    *(tcpMessage.dStream) >> nrSats;

    QVector<GnssSatellite> satList;
    for (uint8_t i=0; i<nrSats; i++)
    {
        GnssSatellite sat;
        *(tcpMessage.dStream) >> sat;
        satList.push_back(sat);
    }
        emit satsReceived(satList);
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_UBX_GNSS_CONFIG){
        int numTrkCh=0;
        int nrConfigs=0;

        *(tcpMessage.dStream) >> numTrkCh >> nrConfigs;

        QVector<GnssConfigStruct> configList;
        for (int i=0; i<nrConfigs; i++)
        {
            GnssConfigStruct config;
            *(tcpMessage.dStream) >> config.gnssId >> config.resTrkCh >>
                config.maxTrkCh >> config.flags;
            configList.push_back(config);
        }
        emit gnssConfigsReceived(numTrkCh, configList);
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_UBX_TIME_ACCURACY){
        quint32 acc=0;
        *(tcpMessage.dStream) >> acc;
        emit timeAccReceived(acc);
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_UBX_FREQ_ACCURACY){
        quint32 acc=0;
        *(tcpMessage.dStream) >> acc;
        emit freqAccReceived(acc);
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_UBX_EVENTCOUNTER){
        quint32 cnt=0;
        *(tcpMessage.dStream) >> cnt;
        emit intCounterReceived(cnt);
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_UBX_UPTIME){
        quint32 val=0;
        *(tcpMessage.dStream) >> val;
        emit ubxUptimeReceived(val);
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_UBX_TXBUF){
        quint8 val=0;
        *(tcpMessage.dStream) >> val;
        emit txBufReceived(val);
        if (!tcpMessage.dStream->atEnd()) {
            *(tcpMessage.dStream) >> val;
            emit txBufPeakReceived(val);
        }
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_UBX_RXBUF){
        quint8 val=0;
        *(tcpMessage.dStream) >> val;
        emit rxBufReceived(val);
        if (!tcpMessage.dStream->atEnd()) {
            *(tcpMessage.dStream) >> val;
            emit rxBufPeakReceived(val);
        }
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_UBX_TXBUF_PEAK){
        quint8 val=0;
        *(tcpMessage.dStream) >> val;
        emit txBufPeakReceived(val);
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_UBX_RXBUF_PEAK){
        quint8 val=0;
        *(tcpMessage.dStream) >> val;
        emit rxBufPeakReceived(val);
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_UBX_MONHW){
/*        quint16 noise=0;
        quint16 agc=0;
        quint8 antStatus=0;
        quint8 antPower=0;
        quint8 jamInd=0;
        quint8 flags=0;*/
        GnssMonHwStruct hw;
    //*(tcpMessage.dStream) >> noise >> agc >> antStatus >> antPower >> jamInd >> flags;
    *(tcpMessage.dStream) >> hw;
        //emit gpsMonHWReceived(noise,agc,antStatus,antPower,jamInd,flags);
        emit gpsMonHWReceived(hw);
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_UBX_MONHW2){
/*        qint8 ofsI=0, ofsQ=0;
        quint8 magI=0, magQ=0;
        quint8 cfgSrc=0;*/
    GnssMonHw2Struct hw2;
//        *(tcpMessage.dStream) >> hw2.ofsI >> hw2.magI >> hw2.ofsQ >> hw2.magQ >> hw2.cfgSrc;
        *(tcpMessage.dStream) >> hw2;
        //qDebug()<<"ofsI="<<ofsI<<" magI="<<magI<<"ofsQ="<<ofsQ<<" magQ="<<magQ<<" cfgSrc="<<cfgSrc;
//        emit gpsMonHW2Received(ofsI, magI, ofsQ, magQ, cfgSrc);
        emit gpsMonHW2Received(hw2);
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_UBX_VERSION){
        QString sw="";
        QString hw="";
        QString pv="";
        *(tcpMessage.dStream) >> sw >> hw >> pv;
        emit gpsVersionReceived(sw, hw, pv);
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_UBX_FIXSTATUS){
        quint8 val=0;
        *(tcpMessage.dStream) >> val;
        emit gpsFixReceived(val);
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_UBX_CFG_TP5){
        UbxTimePulseStruct tp;
        *(tcpMessage.dStream) >> tp;
        emit gpsTP5Received(tp);
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_HISTOGRAM){
        Histogram h;
        *(tcpMessage.dStream) >> h;
        emit histogramReceived(h);
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_ADC_MODE) {
        quint8 mode;
        *(tcpMessage.dStream) >> mode;
        emit adcModeReceived(mode);
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_LOG_INFO){
        LogInfoStruct lis;
        *(tcpMessage.dStream) >> lis;
        emit logInfoReceived(lis);
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_UBX_TIMEMARK){
        UbxTimeMarkStruct tm;
        *(tcpMessage.dStream) >> tm;
        emit timeMarkReceived(tm);
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_MQTT_STATUS){
        bool connected = false;
        *(tcpMessage.dStream) >> connected;
        emit mqttStatusChanged(connected);
        return;
    }
}

void MainWindow::sendRequest(quint16 requestSig){
    TcpMessage tcpMessage(requestSig);
    emit sendTcpMessage(tcpMessage);
}

void MainWindow::sendRequest(TCP_MSG_KEY requestSig){
    TcpMessage tcpMessage(requestSig);
    emit sendTcpMessage(tcpMessage);
}

void MainWindow::sendRequest(quint16 requestSig, quint8 par){
    TcpMessage tcpMessage(requestSig);
    *(tcpMessage.dStream) << par;
    emit sendTcpMessage(tcpMessage);
}

void MainWindow::sendRequest(TCP_MSG_KEY requestSig, quint8 par){
    TcpMessage tcpMessage(requestSig);
    *(tcpMessage.dStream) << par;
    emit sendTcpMessage(tcpMessage);
}

void MainWindow::sendRequestUbxMsgRates(){
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_UBX_MSG_RATE_REQUEST);
    emit sendTcpMessage(tcpMessage);
}

void MainWindow::sendSetBiasVoltage(float voltage){
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_BIAS_VOLTAGE);
    *(tcpMessage.dStream) << voltage;
    emit sendTcpMessage(tcpMessage);
    emit sendRequest(TCP_MSG_KEY::MSG_DAC_REQUEST, 2);
//    emit sendRequest(adcSampleRequestSig, 2);
//    emit sendRequest(adcSampleRequestSig, 3);
}

void MainWindow::sendSetBiasStatus(bool status){
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_BIAS_SWITCH);
    *(tcpMessage.dStream) << status;
    emit sendTcpMessage(tcpMessage);
}

void MainWindow::sendGainSwitch(bool status){
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_GAIN_SWITCH);
    *(tcpMessage.dStream) << status;
    emit sendTcpMessage(tcpMessage);
}

void MainWindow::sendPreamp1Switch(bool status){
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_PREAMP_SWITCH);
    *(tcpMessage.dStream) << (quint8)0 << status;
    emit sendTcpMessage(tcpMessage);
}

void MainWindow::sendPreamp2Switch(bool status){
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_PREAMP_SWITCH);
    *(tcpMessage.dStream) << (quint8)1 << status;
    emit sendTcpMessage(tcpMessage);
}

void MainWindow::sendSetThresh(uint8_t channel, float value){
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_THRESHOLD);
    *(tcpMessage.dStream) << channel << value;
    emit sendTcpMessage(tcpMessage);
    emit sendRequest(TCP_MSG_KEY::MSG_DAC_REQUEST, channel);
}

void MainWindow::sendSetUbxMsgRateChanges(QMap<uint16_t, int> changes){
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_UBX_MSG_RATE);
    *(tcpMessage.dStream) << changes;
    emit sendTcpMessage(tcpMessage);
}

void MainWindow::onSendUbxReset()
{
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_UBX_RESET);
    emit sendTcpMessage(tcpMessage);
}

void MainWindow::onHistogramCleared(QString histogramName){
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_HISTOGRAM_CLEAR);
    *(tcpMessage.dStream) << histogramName;
    emit sendTcpMessage(tcpMessage);
//	qDebug()<<"received signal in slot MainWindow::onHistogramCleared("<<histogramName<<")";
}

void MainWindow::onAdcModeChanged(quint8 mode){
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_ADC_MODE);
    *(tcpMessage.dStream) << mode;
    emit sendTcpMessage(tcpMessage);
}

void MainWindow::onRateScanStart(uint8_t ch) {
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_RATE_SCAN);
    *(tcpMessage.dStream) << (quint8)ch;
    emit sendTcpMessage(tcpMessage);
}

void MainWindow::onSetGnssConfigs(const QVector<GnssConfigStruct>& configList){
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_UBX_GNSS_CONFIG);
    int N=configList.size();
    *(tcpMessage.dStream) << (int)N;
    for (int i=0; i<N; i++){
        *(tcpMessage.dStream) << configList[i].gnssId<<configList[i].resTrkCh
                              << configList[i].maxTrkCh<<configList[i].flags;
    }
    emit sendTcpMessage(tcpMessage);
}

void MainWindow::onSetTP5Config(const UbxTimePulseStruct &tp)
{
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_UBX_CFG_TP5);
    *(tcpMessage.dStream) << tp;
    emit sendTcpMessage(tcpMessage);
}

void MainWindow::sendRequestGpioRates(){
    TcpMessage xorRateRequest(TCP_MSG_KEY::MSG_GPIO_RATE_REQUEST);
    *(xorRateRequest.dStream) << (quint16)5 << (quint8)0;
    emit sendTcpMessage(xorRateRequest);
    TcpMessage andRateRequest(TCP_MSG_KEY::MSG_GPIO_RATE_REQUEST);
    *(andRateRequest.dStream) << (quint16)5 << (quint8)1;
    emit sendTcpMessage(andRateRequest);
}

void MainWindow::sendRequestGpioRateBuffer(){
    TcpMessage xorRateRequest(TCP_MSG_KEY::MSG_GPIO_RATE_REQUEST);
    *(xorRateRequest.dStream) << (quint16)0 << (quint8)0;
    emit sendTcpMessage(xorRateRequest);
    TcpMessage andRateRequest(TCP_MSG_KEY::MSG_GPIO_RATE_REQUEST);
    *(andRateRequest.dStream) << (quint16)0 << (quint8)1;
    emit sendTcpMessage(andRateRequest);
}

void MainWindow::receivedGpioRisingEdge(GPIO_PIN pin) {
    if (pin == EVT_AND) {
        ui->ANDHit->setStyleSheet("QLabel {background-color: darkGreen;}");
        andTimer.start();
    } else if (pin == EVT_XOR) {
        ui->XORHit->setStyleSheet("QLabel {background-color: darkGreen;}");
        xorTimer.start();
    } else if (pin == TIMEPULSE) {
        emit timepulseReceived();
    }
}

void MainWindow::resetAndHit() {
    ui->ANDHit->setStyleSheet("QLabel {background-color: Window;}");
}
void MainWindow::resetXorHit() {
    ui->XORHit->setStyleSheet("QLabel {background-color: Window;}");
}

void MainWindow::uiSetDisconnectedState() {
    // set button and color of label
    ui->ipStatusLabel->setStyleSheet("QLabel {color: darkGray;}");
    ui->ipStatusLabel->setText("not connected");
    ui->ipButton->setText("connect");
    ui->ipBox->setEnabled(true);
    // disable all relevant objects of mainwindow
    ui->discr1Label->setStyleSheet("QLabel {color: darkGray;}");
    ui->discr2Label->setStyleSheet("QLabel {color: darkGray;}");
    ui->discr1Slider->setValue(0);
    ui->discr1Slider->setDisabled(true);
    ui->discr1Edit->clear();
    ui->discr1Edit->setDisabled(true);
    ui->discr2Slider->setValue(0);
    ui->discr2Slider->setDisabled(true);
    ui->discr2Edit->clear();
    ui->discr2Edit->setDisabled(true);
    ui->ANDHit->setDisabled(true);
    ui->ANDHit->setStyleSheet("QLabel {background-color: Window;}");
    ui->XORHit->setDisabled(true);
    ui->XORHit->setStyleSheet("QLabel {background-color: Window;}");
    ui->rate1->setDisabled(true);
    ui->rate2->setDisabled(true);
    ui->biasPowerLabel->setDisabled(true);
    ui->biasPowerLabel->setStyleSheet("QLabel {color: darkGray;}");
    ui->biasPowerButton->setDisabled(true);
    // disable other widgets
    emit setUiEnabledStates(false);
}

void MainWindow::uiSetConnectedState() {
    // change color and text of labels and buttons
    ui->ipStatusLabel->setStyleSheet("QLabel {color: darkGreen;}");
    ui->ipStatusLabel->setText("connected");
    ui->ipButton->setText("disconnect");
    ui->ipBox->setDisabled(true);
    ui->discr1Label->setStyleSheet("QLabel {color: black;}");
    ui->discr2Label->setStyleSheet("QLabel {color: black;}");
    // enable other widgets
    emit setUiEnabledStates(true);
}

void MainWindow::updateUiProperties() {
    mouseHold = true;

    ui->discr1Slider->setEnabled(true);
    ui->discr1Slider->setValue(sliderValues.at(0));
    ui->discr1Edit->setEnabled(true);
    ui->discr1Edit->setText(QString::number(sliderValues.at(0) / 2.0) + "mV");

    ui->discr2Slider->setEnabled(true);
    ui->discr2Slider->setValue(sliderValues.at(1));
    ui->discr2Edit->setEnabled(true);
    ui->discr2Edit->setText(QString::number(sliderValues.at(1) / 2.0) + "mV");
    double biasVoltage = biasCalibOffset + biasDacVoltage*biasCalibSlope;
    ui->biasVoltageSlider->blockSignals(true);
    ui->biasVoltageDoubleSpinBox->blockSignals(true);
    ui->biasVoltageDoubleSpinBox->setValue(biasVoltage);
    ui->biasVoltageSlider->setValue(100*biasVoltage/maxBiasVoltage);
    ui->biasVoltageSlider->blockSignals(false);
    ui->biasVoltageDoubleSpinBox->blockSignals(false);
    // equation:
    // UBias = c1*UDac + c0
    // (UBias - c0)/c1 = UDac

    ui->ANDHit->setEnabled(true);
    //ui->ANDHit->setStyleSheet("QLabel {background-color: darkRed; color: white;}");
    ui->XORHit->setEnabled(true);
    //ui->XORHit->setStyleSheet("QLabel {background-color: darkRed; color: white;}");
    ui->rate1->setEnabled(true);
    ui->rate2->setEnabled(true);
    ui->biasPowerButton->setEnabled(true);
    ui->biasPowerLabel->setEnabled(true);
    if (biasON) {
        ui->biasPowerButton->setText("Disable Bias");
        ui->biasPowerLabel->setText("Bias ON");
        ui->biasPowerLabel->setStyleSheet("QLabel {background-color: darkGreen; color: white;}");
    }
    else {
        ui->biasPowerButton->setText("Enable Bias");
        ui->biasPowerLabel->setText("Bias OFF");
        ui->biasPowerLabel->setStyleSheet("QLabel {background-color: red; color: white;}");
    }
    mouseHold = false;
}

void MainWindow::connected() {
    connectedToDemon = true;
    saveSettings(addresses);
    uiSetConnectedState();
    sendValueUpdateRequests();
    sendRequest(TCP_MSG_KEY::MSG_PREAMP_SWITCH_REQUEST,0);
    sendRequest(TCP_MSG_KEY::MSG_PREAMP_SWITCH_REQUEST,1);
    sendRequest(TCP_MSG_KEY::MSG_GAIN_SWITCH_REQUEST);
    sendRequest(TCP_MSG_KEY::MSG_THRESHOLD_REQUEST);
    sendRequest(TCP_MSG_KEY::MSG_PCA_SWITCH_REQUEST);
    sendRequestUbxMsgRates();
    sendRequestGpioRateBuffer();
    sendRequest(TCP_MSG_KEY::MSG_CALIB_REQUEST);
    sendRequest(TCP_MSG_KEY::MSG_ADC_MODE_REQUEST);
}


void MainWindow::sendValueUpdateRequests() {
    sendRequest(TCP_MSG_KEY::MSG_BIAS_VOLTAGE_REQUEST);
    sendRequest(TCP_MSG_KEY::MSG_BIAS_SWITCH_REQUEST);
//    sendRequest(preampRequestSig,0);
//    sendRequest(preampRequestSig,1);
//    sendRequest(threshRequestSig);
    for (int i=0; i<4; i++) sendRequest(TCP_MSG_KEY::MSG_DAC_REQUEST,i);
    for (int i=1; i<4; i++) sendRequest(TCP_MSG_KEY::MSG_ADC_SAMPLE_REQUEST,i);
//    sendRequest(pcaChannelRequestSig);
//    sendRequestUbxMsgRates();
//    sendRequestGpioRateBuffer();
    sendRequest(TCP_MSG_KEY::MSG_TEMPERATURE_REQUEST);
    sendRequest(TCP_MSG_KEY::MSG_I2C_STATS_REQUEST);
//    sendRequest(calibRequestSig);
}

void MainWindow::onIpButtonClicked()
{
    if (connectedToDemon) {
        // it is connected and the button shows "disconnect" -> here comes disconnect code
        connectedToDemon = false;
        emit closeConnection();
        andTimer.stop();
        xorTimer.stop();
        uiSetDisconnectedState();
        return;
    }
    QString ipBoxText = ui->ipBox->currentText();
    QStringList ipAndPort = ipBoxText.split(':');
    if (ipAndPort.size() > 2 || ipAndPort.size() < 1) {
        QString errorMess = "error, size of ipAndPort not 1 or 2";
        errorM.showMessage(errorMess);
        return;
    }
    QString ipAddress = ipAndPort.at(0);
    if (ipAddress == "local" || ipAddress == "localhost") {
        ipAddress = "127.0.0.1";
    }
    QString portString;
    if (ipAndPort.size() == 2) {
        portString = ipAndPort.at(1);
    }
    else {
        portString = "51508";
    }
    makeConnection(ipAddress, portString.toUInt());
    if (!ui->ipBox->currentText().isEmpty() && ui->ipBox->findText(ui->ipBox->currentText()) == -1) {
        // if text not already in there, put it in there
        ui->ipBox->addItem(ui->ipBox->currentText());
    }
}

void MainWindow::on_discr1Slider_sliderPressed()
{
    mouseHold = true;
}

void MainWindow::on_discr1Slider_sliderReleased()
{
    mouseHold = false;
    on_discr1Slider_valueChanged(ui->discr1Slider->value());
}

void MainWindow::on_discr1Edit_editingFinished()
{
    float value = parseValue(ui->discr1Edit->text());
    if (value < 0) {
        return;
    }
    ui->discr1Slider->setValue((int)(value * 2 + 0.5));
}

void MainWindow::on_discr1Slider_valueChanged(int value)
{
    float thresh0 = (float)(value / 2000.0);
    ui->discr1Edit->setText(QString::number((float)value / 2.0) + "mV");
    if (!mouseHold) {
        sendSetThresh(0, thresh0);
    }
}

void MainWindow::on_discr2Slider_sliderPressed()
{
    mouseHold = true;
}

void MainWindow::on_discr2Slider_sliderReleased()
{
    mouseHold = false;
    on_discr2Slider_valueChanged(ui->discr2Slider->value());
}

void MainWindow::on_discr2Edit_editingFinished()
{
    float value = parseValue(ui->discr2Edit->text());
    if (value < 0) {
        return;
    }
    ui->discr2Slider->setValue((int)(value * 2 + 0.5));
}

void MainWindow::on_discr2Slider_valueChanged(int value)
{
    float thresh1 =  (float)(value / 2000.0);
    ui->discr2Edit->setText(QString::number((float)(value / 2.0)) + "mV");
    if (!mouseHold) {
        sendSetThresh(1, thresh1);
    }
}
void MainWindow::setMaxThreshVoltage(float voltage){
    // we have 0.5 mV resolution so we have (int)(mVolts)*2 steps on the slider
    // the '+0.5' is to round up or down like in mathematics
    maxThreshVoltage = voltage;
    int maximum = (int)(voltage*2000+0.5);
    ui->discr1Slider->setMaximum(maximum);
    ui->discr2Slider->setMaximum(maximum);
    int bigger = (sliderValues.at(0)>sliderValues.at(1))?0:1;
    if( sliderValues.at(bigger) > maximum){
        sendSetThresh(bigger,voltage);
        if (sliderValues.at(!bigger) > maximum){
            sendSetThresh(!bigger,voltage);
        }
    }
}
float MainWindow::parseValue(QString text) {
    // ignores everything that is not a number or at least most of it
    QRegExp alphabetical = QRegExp("[a-z]+[A-Z]+");
    QRegExp specialCharacters = QRegExp(
        QString::fromUtf8("[\\-`~!@#\\$%\\^\\&\\*()_\\—\\+=|:;<>«»\\?/{}\'\"ß\\\\]+"));
    text = text.simplified();
    text = text.replace(" ", "");
    text = text.remove(alphabetical);
    text = text.replace(",", ".");
    text = text.remove(specialCharacters);
    bool ok;
    float value = text.toFloat(&ok);
    if (!ok) {
        errorM.showMessage("failed to parse discr1Edit to float");
        return -1;
    }
    return value;
}

void MainWindow::on_saveDacButton_clicked()
{
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_DAC_EEPROM_SET);
    emit sendTcpMessage(tcpMessage);
}

void MainWindow::on_biasPowerButton_clicked()
{
    sendSetBiasStatus(!biasON);
}

void MainWindow::sendInputSwitch(uint8_t id) {
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_PCA_SWITCH);
    *(tcpMessage.dStream) << (quint8)id;
    emit sendTcpMessage(tcpMessage);
    sendRequest(TCP_MSG_KEY::MSG_PCA_SWITCH_REQUEST);
}

void MainWindow::on_biasVoltageSlider_sliderReleased()
{
    mouseHold = false;
    on_biasVoltageSlider_valueChanged(ui->biasVoltageSlider->value());
}

void MainWindow::on_biasVoltageSlider_valueChanged(int value)
{
    if (!mouseHold)
    {
        double biasVoltage = (double)value/ui->biasVoltageSlider->maximum()*maxBiasVoltage;
        if (fabs(biasCalibSlope)<1e-5) return;
        double dacVoltage = (biasVoltage-biasCalibOffset)/biasCalibSlope;
        if (dacVoltage<0.) dacVoltage=0.;
        if (dacVoltage>3.3) dacVoltage=3.3;
        sendSetBiasVoltage(dacVoltage);
    }
    // equation:
    // UBias = c1*UDac + c0
    // (UBias - c0)/c1 = UDac
}

void MainWindow::on_biasVoltageSlider_sliderPressed()
{
    mouseHold=true;
}

void MainWindow::onCalibUpdated(const QVector<CalibStruct>& items)
{
    if (calib==nullptr) return;

    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_CALIB_SET);
    if (items.size()) {
        *(tcpMessage.dStream) << (quint8)items.size();
        for (int i=0; i<items.size(); i++) {
            *(tcpMessage.dStream) << items[i];
        }
        emit sendTcpMessage(tcpMessage);
    }

    uint8_t flags = calib->getCalibParameter("CALIB_FLAGS").toUInt();
    bool calibedBias = false;
    if (flags & CalibStruct::CALIBFLAGS_VOLTAGE_COEFFS) calibedBias=true;

    const QStandardItemModel *model = dynamic_cast<QStandardItemModel*>(ui->biasControlTypeComboBox->model());
    QStandardItem *item = model->item(1);

    item->setEnabled(calibedBias);
/*
    item->setFlags(disable ? item->flags() & ~(Qt::ItemIsSelectable|Qt::ItemIsEnabled)
                           : Qt::ItemIsSelectable|Qt::ItemIsEnabled));
    // visually disable by greying out - works only if combobox has been painted already and palette returns the wanted color
    item->setData(disable ? ui->comboBox->palette().color(QPalette::Disabled, QPalette::Text)
                          : QVariant(), // clear item data in order to use default color
                  Qt::TextColorRole);
*/
    ui->biasControlTypeComboBox->setCurrentIndex((calibedBias)?1:0);
//    sendRequest(biasVoltageRequestSig);
}

void MainWindow::on_biasControlTypeComboBox_currentIndexChanged(int index)
{
    if (index==1) {
        if (calib==nullptr) return;
        QString str = calib->getCalibParameter("COEFF0");
        if (!str.size()) return;
        double c0 = str.toDouble();
        str = calib->getCalibParameter("COEFF1");
        if (!str.size()) return;
        double c1 = str.toDouble();
        biasCalibOffset=c0; biasCalibSlope=c1;
        minBiasVoltage=0.; maxBiasVoltage=40.;
        ui->biasVoltageDoubleSpinBox->setMaximum(maxBiasVoltage);
        ui->biasVoltageDoubleSpinBox->setSingleStep(0.1);
    } else {
        biasCalibOffset=0.; biasCalibSlope=1.;
        minBiasVoltage=0.; maxBiasVoltage=3.3;
        ui->biasVoltageDoubleSpinBox->setMaximum(maxBiasVoltage);
        ui->biasVoltageDoubleSpinBox->setSingleStep(0.01);
    }
    sendRequest(TCP_MSG_KEY::MSG_BIAS_VOLTAGE_REQUEST);
}

void MainWindow::on_biasVoltageDoubleSpinBox_valueChanged(double arg1)
{
    double biasVoltage = arg1;
    if (fabs(biasCalibSlope)<1e-5) return;
    double dacVoltage = (biasVoltage-biasCalibOffset)/biasCalibSlope;
    if (dacVoltage<0.) dacVoltage=0.;
    if (dacVoltage>3.3) dacVoltage=3.3;
    sendSetBiasVoltage(dacVoltage);
}

void MainWindow::gpioInhibit(bool inhibit) {
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_GPIO_INHIBIT);
    *(tcpMessage.dStream) << inhibit;
    emit sendTcpMessage(tcpMessage);
}

void MainWindow::onPolarityChanged(bool pol1, bool pol2){
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_POLARITY_SWITCH);
    *(tcpMessage.dStream) << pol1 << pol2;
    emit sendTcpMessage(tcpMessage);
}


