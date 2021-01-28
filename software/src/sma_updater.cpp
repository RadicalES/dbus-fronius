#include <qnumeric.h>
#include <QsLog.h>
#include <QTimer>
#include <velib/vecan/products.h>
#include "data_processor.h"
#include "inverter.h"
#include "sma_updater.h"
#include "inverter_settings.h"
#include "modbus_tcp_client.h"
#include "modbus_reply.h"
#include "power_info.h"

// The PV inverter will reset the power limit to maximum after this interval. The reset will cause
// the power of the inverter to increase (or stay at its current value), so a large value for the
// timeout is pretty safe.
static const int PowerLimitTimeout = 120;

SMAUpdater::SMAUpdater(SMAInverter *inverter, InverterSettings *settings, QObject *parent) :
    QObject(parent),
      mInverter(inverter),
      mSettings(settings),
      mModbusClient(new ModbusTcpClient(this)),
      mTimer(new QTimer(this)),
      mDataProcessor(new DataProcessor(inverter, settings, this)),
      mCurrentState(Idle),
      mRetryCount(0),
      mOpMode(SMAInverter::SMA_OM_INVALID)
{
    Q_ASSERT(inverter != 0);
    connectModbusClient();
    mModbusClient->setTimeout(5000);
    mModbusClient->connectToServer(inverter->hostName());
    connect(mInverter, SIGNAL(powerLimitRequested(double)), this, SLOT(onPowerLimitRequested(double)));
    mTimer->setSingleShot(true);
    connect(mTimer, SIGNAL(timeout()), this, SLOT(onTimer()));
    connect(mSettings, SIGNAL(phaseChanged()), this, SLOT(onPhaseChanged()));
    mPowerLimitWatt = (quint16)(mInverter->deviceInfo().powerLimitScale);
}

void SMAUpdater::startNextAction(ModbusState state)
{
    ModbusState prvstate = mCurrentState;
    mCurrentState = state;

    switch (mCurrentState) {

    case CheckCondition:
        readHoldingRegisters(30201, 2);
        break;

    case CheckState:
        readHoldingRegisters(40029, 2);
        break;

    case CheckLogin:
        readHoldingRegisters(43090, 2);
        break;

    case DoLogin:
    {
        QVector<quint16> values;
        uint32_t gc = mInverter->gridCode();
        uint16_t gc0,gc1;
        gc0 = (gc >> 16) & 0xffff;
        gc1 = gc & 0xffff;
        values.append(gc0);
        values.append(gc1);
        writeMultipleHoldingRegisters(40210, values);
        break;
    }

    case CheckOpMode:
        readHoldingRegisters(40210, 2);
        break;

    case SetOpMode:
    {
        QVector<quint16> values;
        values.append(0);
        values.append(SMAInverter::SMA_OM_WATT);
        writeMultipleHoldingRegisters(40210, values);
        break;
    }

    case ReadPowerLimit:
        readHoldingRegisters(40212, 2);
        break;

    case WritePowerLimit:
    {
        QVector<quint16> values;
        values.append(0);
        values.append(mPowerLimitWatt);
        writeMultipleHoldingRegisters(40212, values);
        break;
    }

    case ReadPowerYield:
        readHoldingRegisters(30513, 8);
        break;

    case ReadACFrequency:
        readHoldingRegisters(40135, 2);
        break;

    case ReadACCurrent:
        readHoldingRegisters(30795, 2);
        break;

    case ReadACPowerAndVoltage:
        readHoldingRegisters(30775, 10);
        break;

    case ReadTemperature:
        readHoldingRegisters(34113, 2);
        break;

    case ReadPVData1:
        readHoldingRegisters(30769, 6);
        break;

    case ReadPVData2:
        readHoldingRegisters(30957, 6);
        break;

    case Idle:
        startIdleTimer();
        break;

    case Error:
        QLOG_ERROR() << "SMAUpdate in error state! Old state = " << prvstate;
        break;

    default:
        QLOG_ERROR() << "SMAUpdater::startNextAction ERROR";
        Q_ASSERT(false);
        break;
    }
}

void SMAUpdater::startIdleTimer()
{
    mTimer->setInterval(mCurrentState == Idle ? 1000 : 5000);
    mTimer->start();
}

void SMAUpdater::readHoldingRegisters(quint16 startRegister, quint16 count)
{
    const DeviceInfo &deviceInfo = mInverter->deviceInfo();
    ModbusReply *reply = mModbusClient->readHoldingRegisters(deviceInfo.networkId, startRegister, count);
    connect(reply, SIGNAL(finished()), this, SLOT(onReadCompleted()));
}

void SMAUpdater::writeMultipleHoldingRegisters(quint16 startReg, const QVector<quint16> &values)
{
    const DeviceInfo &deviceInfo = mInverter->deviceInfo();
    ModbusReply *reply = mModbusClient->writeMultipleHoldingRegisters(deviceInfo.networkId, startReg, values);
    connect(reply, SIGNAL(finished()), this, SLOT(onWriteCompleted()));
}

bool SMAUpdater::handleModbusError(ModbusReply *reply)
{
    if (reply->error() == ModbusReply::NoException) {
        mRetryCount = 0;
        return true;
    }
    handleError();
    return false;
}

void SMAUpdater::handleError()
{
    ++mRetryCount;
    if (mRetryCount > 5) {
        mRetryCount = 0;
        emit connectionLost();
    }
    startIdleTimer();
}

void SMAUpdater::onReadCompleted()
{
    ModbusReply *reply = static_cast<ModbusReply *>(sender());
    reply->deleteLater();
    if (!handleModbusError(reply))
        return;

    QVector<quint16> values = reply->registers();

    mRetryCount = 0;

    ModbusState nextState = mCurrentState;
    switch (mCurrentState) {
    case CheckCondition:
    {
        if (values.size() != 2) {
            nextState = Error;
            break;
        }

        // reset values
        mInverterData.acFrequency = 0.0;
        mInverterData.acCurrent = 0.0;
        mInverterData.acPower = 0.0;
        mInverterData.acVoltage = 0.0;

        // save condition
        mCondition = values[1];
        nextState = CheckState;

        QLOG_DEBUG() << "SMAUpdater Condition: " << mCondition;

        // make sure the inverter is operational
        if((mCondition != SMAInverter::SMA_OC_OK) && (mCondition != SMAInverter::SMA_OC_WARN)) {
            QLOG_INFO() << "SMAUpdater Condition SLEEP/FAULT";
            mInverter->setStatusCode(mCondition, SMAInverter::SMA_OS_Fault);
            mInverter->setErrorCode(mCondition);
            nextState = Idle;
        }

        // no error
        mInverter->setErrorCode(0);
        break;
    }

    case CheckState:
    {
        if (values.size() != 2) {
            nextState = Error;
            break;
        }

        // save state
        mState = values[1];

        // set default state
        nextState = CheckLogin;
        mWriteCount = 0;

        QLOG_DEBUG() << "SMAUpdater State: " << mState;

        // invalid grid code, we operate in readonly mode
        if(mInverter->gridCode() == 0) {
            QLOG_DEBUG() << "SMAUpdater no valid Grid Code, in report only mode";
            nextState = ReadPowerYield;
        }

        // report inverter state
        mInverter->setStatusCode(mCondition, mState);

        if(mState != SMAInverter::SMA_OS_MPP) {
            QLOG_DEBUG() << "SMAUpdater MPPT down, limited information is available";
            // inverter DC side is down, read yield and go idle
            // the other states will not work
            nextState = ReadPowerYield;
        }
        break;
    }

    case CheckLogin:
    {
        if (values.size() != 2) {
            nextState = Error;
            break;
        }

        // save login state
        mLoggedIn = values[1] == 1;

        // set default next state
        nextState = CheckOpMode;

        QLOG_DEBUG() << "SMAUpdater Login Status: " << mLoggedIn;

        if((!mLoggedIn) && (mWriteCount < 3)) {
            QLOG_INFO() << "SMAUpdater Login RETRY: count=" << mWriteCount;
            mWriteCount++;
            nextState = DoLogin;
        }
        else if(mWriteCount >= 3) {
            QLOG_INFO() << "SMAUpdater Login failed, REPORT ONLY";
            // we are readonly mode, grid code does not work
            nextState = ReadPowerYield;
        }

        if(nextState == CheckOpMode) {
            mWriteCount = 0;
        }

        break;
    }

    case CheckOpMode:
    {
        if (values.size() != 2) {
            nextState = Error;
            break;
        }

        // default next state
        nextState = ReadPowerYield;

        mOpMode = values[1];
        QLOG_DEBUG() << "SMAUpdater Operating Mode: " << mOpMode;

        if((mOpMode != SMAInverter::SMA_OM_WATT) && (mWriteCount < 3)) {
            QLOG_INFO() << "SMAUpdater update operating mode (retry=" << mWriteCount << ")";
            mWriteCount++;
            nextState = SetOpMode;
        }

        break;
    }

    case ReadPowerYield:
    {
        if (values.size() != 8) {
            nextState = Error;
            break;
        }

        uint64_t uul = getDoubleULong(values, 0);
        mInverterData.totalEnergy = (double)uul;
        uul = getDoubleULong(values, 4);
        mInverterData.dayEnergy = (double)uul;

        // set default next state
        nextState = ReadACFrequency;

        QLOG_DEBUG() << "SMAUpdater Power Yield: " << mInverterData.totalEnergy << " WH / " << mInverterData.dayEnergy << " WH";

        // if DC is off, go idle
        if(mState != SMAInverter::SMA_OS_MPP) {
            QLOG_DEBUG() << "SMAUpdater PV OFF, GO IDLE";
            mDataProcessor->process(mInverterData);
            nextState = Idle;
        }
        break;
    }

    case ReadACFrequency:
    {
        if (values.size() != 2) {
            nextState = Error;
            break;
        }

        uint32_t ul = getULong(values, 0);
        mInverterData.acFrequency = ((double)ul) / 100.0;

        QLOG_DEBUG() << "SMAUpdater AC Frequency: " << mInverterData.acFrequency;

        nextState = ReadACCurrent;
        break;
    }

    case ReadACCurrent:
    {
        if (values.size() != 2) {
            nextState = Error;
            break;
        }

        uint32_t ul = getULong(values, 0);
        mInverterData.acCurrent = ((double)ul) / 1000.0;

        QLOG_DEBUG() << "SMAUpdater AC Current: " << mInverterData.acCurrent;

        nextState = ReadACPowerAndVoltage;
        break;
    }

    case ReadACPowerAndVoltage:
    {
        if (values.size() != 10) {
            nextState = Error;
            break;
        }

        uint32_t ul = getULong(values, 0);
        mInverterData.acPower = (double)ul;
        ul = getULong(values, 8);
        mInverterData.acVoltage = ((double)ul) / 100.0;

       QLOG_DEBUG() << "SMAUpdater AC Power And Voltage: " << mInverterData.acPower << " W / " << mInverterData.acVoltage << " V";

        nextState = ReadTemperature;
        break;
    }

    case ReadTemperature:
    {
        if (values.size() != 2) {
            nextState = Error;
            break;
        }

        uint32_t ul = getULong(values, 0);
        double t = ((double)ul) / 10.0;
        mInverter->setTemperature(t);

        QLOG_DEBUG() << "SMAUpdater Temperature" <<  t << " deg C";

        nextState = ReadPVData1;
        break;
    }

    case ReadPVData1:
    {
        if (values.size() != 6) {
            nextState = Error;
            break;
        }

        PvInfo *pvi = mInverter->pvInfo1();
        uint32_t ul = getULong(values, 0);
        double pvc = ((double)ul) / 1000.0;
        pvi->setCurrent(pvc);
        mInverterData.dcCurrent = pvc;
        ul = getULong(values, 2);
        double pvv = ((double)ul) / 100.0;
        pvi->setVoltage(pvv);
        mInverterData.dcVoltage = pvv;
        ul = getULong(values, 4);
        double pvp = ((double)ul);
        pvi->setPower(pvp);

        QLOG_DEBUG() << "SMAUpdater PV Data 1: " << pvp << " W / " << pvv << " V / " << pvc << " A";

        nextState = ReadPVData2;
        break;
    }

    case ReadPVData2:
    {
        if (values.size() != 6) {
            nextState = Error;
            break;
        }

        PvInfo *pvi = mInverter->pvInfo2();
        uint32_t ul = getULong(values, 0);
        double pvc = ((double)ul) / 1000.0;
        pvi->setCurrent(pvc);
        mInverterData.dcCurrent += pvc;
        ul = getULong(values, 2);
        double pvv = ((double)ul) / 100.0;
        pvi->setVoltage(pvv);
        mInverterData.dcVoltage += pvv;
        mInverterData.dcVoltage /= 2.0; // average
        ul = getULong(values, 4);
        double pvp = ((double)ul);
        pvi->setPower(pvp);

        QLOG_DEBUG() << "SMAUpdater PV Data 2: " << pvp << " W / " << pvv << " V / " << pvc << " A";

        nextState = ReadPowerLimit;
        break;
    }

    case ReadPowerLimit:
    {
       if (values.size() != 2) {
           nextState = Error;
           break;
        }
        double powerLimit = values[1];
        mInverter->setPowerLimit(powerLimit);

        QLOG_DEBUG() << "SMAUpdater Power Limit: " << powerLimit << " W";
        mDataProcessor->process(mInverterData);
        nextState = Idle;

        // if we are logged in and mode correct, we can control the inverter output
        if((mLoggedIn) && (mOpMode == SMAInverter::SMA_OM_WATT)) {
            nextState = WritePowerLimit;
        }

        break;
    }

    default:
        QLOG_ERROR() << "SMAUpdater::onReadCompleted ERROR";
        Q_ASSERT(false);
        nextState = CheckCondition;
        break;
    }

    startNextAction(nextState);
}

void SMAUpdater::onWriteCompleted()
{
    ModbusReply *reply = static_cast<ModbusReply *>(sender());
    reply->deleteLater();
    mWritePowerLimitRequested = false;

    ModbusState nextState = mCurrentState;
    switch (mCurrentState) {
    case DoLogin:
    {
        QLOG_DEBUG() << "SMAUpdater DoLogin Complete";
        nextState = CheckLogin;
        break;
    }
    case SetOpMode:
    {
        QLOG_DEBUG() << "SMAUpdater SetOpMode Complete";
        nextState = CheckOpMode;
        break;
    }
    case WritePowerLimit:
    {
        QLOG_DEBUG() << "SMAUpdater WritePowerLimit Complete";
        nextState = Idle;
        break;
    }

    default:
        QLOG_ERROR() << "SMAUpdater::onWriteCompleted ERROR";
        Q_ASSERT(false);
        nextState = CheckCondition;
        break;
    }

    startNextAction(nextState);
}

void SMAUpdater::onPowerLimitRequested(double value)
{
    const DeviceInfo &deviceInfo = mInverter->deviceInfo();
    double maxpower = deviceInfo.maxPower;

    if(value > maxpower) {
        value = maxpower;
        QLOG_WARN() << "SMAUpdater::onPowerLimitRequested Invalid Requested = " << value << " Max Value" << maxpower;
    }

    QLOG_INFO() << "SMAUpdater::onPowerLimitRequested" << value;
    mPowerLimitWatt = value;
}

void SMAUpdater::onConnected()
{
    QLOG_DEBUG() << "SMAUpdater Connected";
    startNextAction(CheckCondition);
}

void SMAUpdater::onDisconnected()
{
    QLOG_DEBUG() << "SMAUpdater Disconnected";
    mCurrentState = CheckCondition;
    handleError();
}

void SMAUpdater::onTimer()
{
    Q_ASSERT(!mTimer->isActive());
    if (mModbusClient->isConnected())
        startNextAction(mCurrentState == Idle ? CheckCondition : mCurrentState);
    else
        mModbusClient->connectToServer(mInverter->hostName());
}

void SMAUpdater::onPhaseChanged()
{
    mInverter->l1PowerInfo()->resetValues();
    mInverter->l2PowerInfo()->resetValues();
    mInverter->l3PowerInfo()->resetValues();
}

void SMAUpdater::connectModbusClient()
{
    connect(mModbusClient, SIGNAL(connected()), this, SLOT(onConnected()));
    connect(mModbusClient, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
}

/* Utilities */
uint64_t SMAUpdater::getDoubleULong(const QVector<quint16> &values, int offset)
{
    uint32_t v0 = static_cast<quint32>((values[offset] << 16) | values[offset + 1]);
    uint32_t v1 = static_cast<quint32>((values[offset + 2] << 16) | values[offset + 3]);
    uint64_t u64 = v0;
    u64 <<= 32;
    u64 |= v1;
    return u64;
}

uint32_t SMAUpdater::getULong(const QVector<quint16> &values, int offset)
{
    uint32_t v = static_cast<quint32>((values[offset] << 16) | values[offset + 1]);
    return v;
}

