
#include "ve_service.h"
#include "sma_inverter.h"

SMAInverter::SMAInverter(VeQItem *root, const DeviceInfo &deviceInfo,
                         int deviceInstance, int unitId, uint32_t gridCode,
                         QObject *parent) :
    Inverter(root, deviceInfo, deviceInstance, parent),
    mPvInfo1(new PvInfo(root->itemGetOrCreate("Pv/0", false), this)),
    mPvInfo2(new PvInfo(root->itemGetOrCreate("Pv/1", false), this)),
    mTemperature(createItem("Temperature")),
    mGridCode(gridCode),
    mUnitId(unitId)
{
    produceValue(createItem("SMADeviceType"), deviceInfo.deviceType);
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

void SMAInverter::setTemperature(double degC)
{
    produceDouble(mTemperature, degC, 1, "deg C");
}


void SMAInverter::setStatusCode(int condition, int state)
{
    QString text;
    int code = condition;

    if(condition == 35) { // FAULT
        text = "FAULT STATE";
    }
    else if(condition == 303) { // OFF
        text = "OFF STATE";
    }
    else if((condition == 307) || (condition == 455)) { // OK or WARNING
        code = state;
        switch (state) {
        case SMA_OS_MPP:
            text = "RUNNING (MPPT)";
            break;
        case SMA_OS_Throttled:
            text = "RUNNING (THROTTLED)";
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
            //text = QString::number(code);
            text = "UNKNOWN (" + QString::number(code) + ")";
            break;
        }
    }

    produceValue(mStatusCode, code, text);
}
