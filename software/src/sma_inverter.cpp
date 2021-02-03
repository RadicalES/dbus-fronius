
#include <QsLog.h>
#include "ve_service.h"
#include "sma_inverter.h"

SMAInverter::SMAInverter(VeQItem *root, const DeviceInfo &deviceInfo,
                         int deviceInstance, int unitId, uint32_t gridCode,
                         QObject *parent) :
    Inverter(root, deviceInfo, deviceInstance, parent),
    mPvInfo1(new PvInfo(root->itemGetOrCreate("Pv/0", false), this)),
    mPvInfo2(new PvInfo(root->itemGetOrCreate("Pv/1", false), this)),
    mTemperature(createItem("Temperature")),
    mFrequency(createItem("Ac/Frequency")),
    mOperatingMode(createItem("OperatingMode")),
    mOperatingState(createItem("OperatingState")),
    mOperatingCondition(createItem("OperatingCondition")),
    mLoggedOn(createItem("LoggedIn")),
    mGridCode(gridCode),
    mUnitId(unitId),
    mStatus(SMA_SC_START0),
    mOpMode(SMA_OM_INVALID),
    mOpCondition(SMA_OC_INVALID),
    mOpState(SMA_OS_Invalid),
    mLoggedIn(false),
    mFreqValue(0.0)
{
    produceValue(createItem("SMADeviceType"), deviceInfo.deviceType);
    mFirmware = mDeviceInfo.firmwareVersion;
}

PvInfo *SMAInverter::pvInfo1()
{
    return mPvInfo1;
}

PvInfo *SMAInverter::pvInfo2()
{
    return mPvInfo2;
}

int SMAInverter::unitId() const
{
    return mUnitId;
}

uint32_t SMAInverter::gridCode() const
{
    return mGridCode;
}

bool SMAInverter::isLoggedIn() const
{
    return mLoggedIn;
}

int SMAInverter::status() const
{
    return mStatus;
}

void SMAInverter::setStatus(StatusCode_t code)
{
    // only update velib when need to
    if(mStatus != code) {
        mStatus = code;

        QLOG_DEBUG() << "SMAInverter Status Code changed to " << code;

        setStatusCode(code);
    }
}

void SMAInverter::setError(int error)
{
    // only update velib when need to
    if(mError != error) {
        mError = error;
        setErrorCode(error);
    }
}

void SMAInverter::setTemperature(double degC)
{
    produceDouble(mTemperature, degC, 1, "deg C");
}

void SMAInverter::setLoggedOn(bool status)
{
    // only update velib when need to
    if(mLoggedIn != status) {
        mLoggedIn = status;

        if(mLoggedIn) {
            produceValue(mLoggedOn, 1, "LOGGED ON");
            QLOG_DEBUG() << "SMAInverter LOGGED IN";
        }
        else {
            produceValue(mLoggedOn, 0, "LOGGED OFF");
            QLOG_DEBUG() << "SMAInverter LOGGED OFF";
        }
    }
}

void SMAInverter::setFrequency(double value)
{
    if(value != mFreqValue) {
        mFreqValue = value;
        produceDouble(mFrequency, value, 1, "Hz");
    }
}

SMAInverter::OperatingState_t SMAInverter::opState() const
{
    return mOpState;
}

void SMAInverter::setOperatingState(OperatingState_t state)
{
    // only update on change of state
    if(mOpState == state) {
        return;
    }

    mOpState = state;
    QString text;

    switch (state) {
    case SMA_OS_MPP:
        text = "RUNNING (MPPT)";
        break;
    case SMA_OS_Started:
        text = "STARTED";
        break;
    case SMA_OS_Stopped:
        text = "STOPPED";
        break;
    case SMA_OS_Derating:
        text = "DERATING";
        break;
    case SMA_OS_Shutdown:
        text = "SHUTDOWN";
        break;
    case SMA_OS_Fault:
        text = "FAULT";
        break;
    case SMA_OS_WaitAC:
        text = "WAITING FOR AC";
        break;
    case SMA_OS_WaitPV:
        text = "WAITING FOR PV";
        break;
    case SMA_OS_ConstVolt:
        text = "CONSTANT VOLTAGE";
        break;
    case SMA_OS_StandAlone:
        text = "STANDBY ALONE OPERATION";
        break;

    default:
        text = "UNKNOWN (" + QString::number(state) + ")";
        break;
    }

    QLOG_DEBUG() << "SMAInverter Operating State Changed to " << text << " (" << state << ")";

    produceValue(mOperatingState, state, text);
}

SMAInverter::OperatingCondition_t SMAInverter::opCondition() const
{
    return mOpCondition;
}

void SMAInverter::setOperatingCondition(OperatingCondition_t condition)
{
    // only update on change of state
    if(mOpCondition == condition) {
        return;
    }

    mOpCondition = condition;
    QString text;

    switch (condition) {
    case SMA_OC_INVALID:
        text = "INVALID";
        break;
    case SMA_OC_FAULT:
        text = "FAULT";
        break;
    case SMA_OC_OFF:
        text = "OFF";
        break;
    case SMA_OC_OK:
        text = "OK";
        break;
    case SMA_OC_WARN:
        text = "WARNING";
        break;

    default:
        text = "UNKNOWN (" + QString::number(condition) + ")";
        break;
    }

    QLOG_DEBUG() << "SMAInverter Operating Condition changed to " << text << " (" << condition << ")";

    produceValue(mOperatingCondition, condition, text);
}

SMAInverter::OperatingMode_t SMAInverter::opMode() const
{
    return mOpMode;
}

void SMAInverter::setOperatingMode(OperatingMode_t mode)
{
    // only update on change of state
    if(mOpMode == mode) {
        return;
    }

    mOpMode = mode;
    QString text;

    switch (mode) {
    case SMA_OM_INVALID:
        text = "INVALID";
        break;
    case SMA_OM_OFF:
        text = "OFF";
        break;
    case SMA_OM_WATT:
        text = "WATT LIMITED";
        break;
    case SMA_OM_PERCENT:
        text = "PERCENTAGE LIMITED";
        break;
    case SMA_OM_SYSTEMCTL:
        text = "SYSTEM CONTROL";
        break;

    default:
        text = "UNKNOWN (" + QString::number(mode) + ")";
        break;
    }

    QLOG_DEBUG() << "SMAInverter Operating Mode changed to " << text << " (" << mode << ")";

    produceValue(mOperatingMode, mode, text);
}

