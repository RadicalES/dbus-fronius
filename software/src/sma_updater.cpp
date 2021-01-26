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

SMAUpdater::SMAUpdater(Inverter *inverter, InverterSettings *settings, QObject *parent) :
    QObject(parent),
      mInverter(inverter),
      mSettings(settings),
      mModbusClient(new ModbusTcpClient(this)),
      mTimer(new QTimer(this)),
      mDataProcessor(new DataProcessor(inverter, settings, this)),
      mCurrentState(Idle),
      mPowerLimitPct(100),
      mRetryCount(0),
      mWritePowerLimitRequested(false)
{
    Q_ASSERT(inverter != 0);
    connectModbusClient();
    mModbusClient->setTimeout(5000);
    mModbusClient->connectToServer(inverter->hostName());
    connect(
        mInverter, SIGNAL(powerLimitRequested(double)),
        this, SLOT(onPowerLimitRequested(double)));
    mTimer->setSingleShot(true);
    connect(mTimer, SIGNAL(timeout()), this, SLOT(onTimer()));
    connect(mSettings, SIGNAL(phaseChanged()), this, SLOT(onPhaseChanged()));
}

void SMAUpdater::startNextAction(ModbusState state)
{
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
        readHoldingRegisters(30795, 10);
        break;

    case ReadDCData1:
        readHoldingRegisters(30769, 6);
        break;

    case ReadDCData2:
        readHoldingRegisters(30957, 6);
        break;

    case Idle:
        startIdleTimer();
        break;

    default:
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
        if (values.size() != 2)
            break;

        mCondition = values[1];
        nextState = CheckState;

        // make sure the inverter is operational
        if((mCondition != SMAInverter::SMA_OC_OK) || (mCondition != SMAInverter::SMA_OC_WARN)) {
            mInverter->setStatusCode(mCondition, SMAInverter::SMA_OS_Fault);
            nextState = Idle;
        }
        break;
    }

    case CheckState:
    {
        if (values.size() != 2)
            break;

        // save state
        mState = values[1];

        // set default state
        nextState = CheckLogin;
        mWriteCount = 0;

        // invalid grid code, we operate in readonly mode
        if(mInverter->gridCode() == 0) {
            nextState = ReadPowerYield;
        }

        if(mState != SMAInverter::SMA_OS_MPP) {
            mInverter->setStatusCode(mCondition, mState);
            // inverter DC side is down, read yield and go idle
            // the other states will not work
            nextState = ReadPowerYield;
        }
        break;
    }

    case CheckLogin:
    {
        if (values.size() != 2)
            break;

        // save login state
        mLoggedIn = values[1] == 1;

        // set default next state
        nextState = CheckControlType;

        if((!mLoggedIn) && (mWriteCount < 3)) {
            mWriteCount++;
            nextState = DoLogin;
        }
        else if(mWriteCount >= 3) {
            // we are readonly mode, grid code does not work
            nextState = ReadPowerYield;
        }

        break;
    }

    case ReadPowerYield:
    {
        if (values.size() != 8)
            break;

        uint64_t uul = getDoubleULong(values, 0);
        mInverterData.totalEnergy = (double)uul;
        uint64_t uul = getDoubleULong(values, 4);
        mInverterData.dayEnergy = (double)uul;

        // set default next state
        nextState = ReadACFrequency;

        // if DC is off, go idle
        if(mState != SMAInverter::SMA_OS_MPP) {
            nextState = Idle;
        }
        break;
    }

    case ReadACFrequency:
    {
        if (values.size() != 2)
            break;

        uint32_t ul = getULong(values, 0);
        mInverterData.acFrequency = ((double)ul) / 100.0;

        nextState = ReadACCurrent;
        break;
    }

    case ReadACCurrent:
    {
        if (values.size() != 2)
            break;

        uint32_t ul = getULong(values, 0);
        mInverterData.acCurrent = ((double)ul) / 1000.0;

        nextState = ReadACPowerAndVoltage;
        break;
    }

    case ReadACPowerAndVoltage:
    {
        if (values.size() != 10)
            break;

        uint32_t ul = getULong(values, 0);
        mInverterData.acPower = (double)ul;
        ul = getULong(values, 8);
        mInverterData.acVoltage = ((double)ul) / 100.0;

        nextState = ReadDCData1;
        break;
    }

    case ReadDCData1:
    {
        if (values.size() != 6)
            break;

        ul = getULong(values, 0);
        mInverterData.dcCurrent = ((double)ul) / 1000.0;
        ul = getULong(values, 2);
        mInverterData.dcVoltage = ((double)ul) / 100.0;

        nextState = ReadDCData2;
        break;
    }

    case ReadDCData2:
    {
        if (values.size() != 6)
            break;

        ul = getULong(values, 0);
        mInverterData.dcCurrent += ((double)ul) / 1000.0;
        mInverterData.dcCurrent /= 2.0;
        ul = getULong(values, 2);
        mInverterData.dcVoltage += ((double)ul) / 100.0;
        mInverterData.dcVoltage /= 2.0;

        nextState = ReadPowerLimit;
        break;
    }

    case ReadPowerLimit:
       if (values.size() == 2) {
            double powerLimit = values[1];
            mInverter->setPowerLimit(powerLimit);
            nextState = Idle;
        }
        break;

    default:
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
        nextState = CheckLogin;
    }
    case SetOpMode:
    {
        nextState = CheckOpMode;
    }
    case WritePowerLimit:
    {
        nextState = CheckCondition;
    }

    default:
        Q_ASSERT(false);
        nextState = CheckCondition;
        break;
    }

    startNextAction(nextState);
}

void SMAUpdater::onPowerLimitRequested(double value)
{
    const DeviceInfo &deviceInfo = mInverter->deviceInfo();
    double powerLimitScale = deviceInfo.powerLimitScale;
    //if (powerLimitScale < PowerLimitScale)
      //  return;
    // An invalid power limit means that power limiting is not supported. So we ignore the request.
    // See comment in the getInitState function.

    if (!qIsFinite(mInverter->powerLimit()))
        return;

    mPowerLimitPct = qBound(0.0, value / deviceInfo.maxPower, 100.0);
    if (mTimer->isActive()) {
        mTimer->stop();
        if (mCurrentState == Idle) {
            startNextAction(WritePowerLimit);
            return; // Skip setting of mWritePowerLimitRequested
        }
        startNextAction(mCurrentState);
    }
    mWritePowerLimitRequested = true;
}

void SMAUpdater::onConnected()
{
    startNextAction(getInitState());
}

void SMAUpdater::onDisconnected()
{
    mCurrentState = getInitState();
    handleError();
}

void SMAUpdater::onTimer()
{
    Q_ASSERT(!mTimer->isActive());
    if (mModbusClient->isConnected())
        startNextAction(mCurrentState == Idle ? getInitState() : mCurrentState);
    else
        mModbusClient->connectToServer(mInverter->hostName());
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

