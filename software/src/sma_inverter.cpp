
#include "ve_service.h"
#include "sma_inverter.h"

SMAInverter::SMAInverter(VeQItem *root, const DeviceInfo &deviceInfo,
                         int deviceInstance, int unitId, uint32_t gridCode,
                         QObject *parent) :
    Inverter(root, deviceInfo, deviceInstance, parent),
    mGridCode(gridCode),
    mUnitId(unitId)
{
    produceValue(createItem("SMADeviceType"), deviceInfo.deviceType);
}

int SMAInverter::unitId() const
{
    return mUnitId;
}

uint32_t SMAInverter::gridCode() const
{
    return mGridCode;
}

void SMAInverter::setStatusCode(int condition, int state)
{
    QString text;
    int code = condition;

    //text = QString("Startup %1/6").arg(code);

    if(condition == 35) { // FAULT
        text = "FAULT STATE";
    }
    else if(condition == 303) { // OFF
        text = "OFF STATE";
    }
    else if((condition == 307) || (condition == 455)) { // OK or WARNING
        code = state;
        switch (state) {
        case SMAMPP:
            text = "RUNNING (MPPT)";
            break;
        case SMAThrottled:
            text = "RUNNING (THROTTLED)";
            break;
        case SMAStarted:
            text = "STARTED";
            break;
        case SMAStopped:
            text = "STOPPED";
            break;
        case SMADerating:
            text = "DERATING";
            break;
        case SMAShutdown:
            text = "SHUTDOWN";
            break;
        case SMAFault:
            text = "FAULT";
            break;
        case SMAWaitAC:
            text = "WAITING FOR AC";
            break;
        case SMAWaitPV:
            text = "WAITING FOR PV";
            break;
        case SMAConstVolt:
            text = "CONSTANT VOLTAGE";
            break;
        case SMAStandAlone:
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
