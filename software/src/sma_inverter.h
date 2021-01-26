#ifndef SMA_INVERTER_H
#define SMA_INVERTER_H

#include "inverter.h"
#include <QObject>

class SMAInverter : public Inverter
{
    Q_OBJECT
public:
    SMAInverter(VeQItem *root, const DeviceInfo &deviceInfo, int deviceInstance, int unitId, uint32_t gridCode, QObject *parent = 0);

    void setStatusCode(int condition, int state);

    int unitId() const;

    uint32_t gridCode() const;

    enum OperatingState {
        SMA_OS_Invalid = 0,
        SMA_OS_Stopped = 381,
        SMA_OS_Started = 1467,
        SMA_OS_Derating = 2119,
        SMA_OS_MPP = 295,
        SMA_OS_Shutdown = 1469,
        SMA_OS_Fault = 1392,
        SMA_OS_WaitAC = 1480,
        SMA_OS_WaitPV = 1393,
        SMA_OS_ConstVolt = 443,
        SMA_OS_StandAlone = 1855,
        SMA_OS_Throttled = 8000
    } OperatingState;

    enum OperatingCondition {
        SMA_OC_INVALID = 0,
        SMA_OC_FAULT = 35,
        SMA_OC_OFF = 303,
        SMA_OC_OK = 307,
        SMA_OC_WARN = 455
    } OperatingCondition;

    enum OperatingMode {
        SMA_OM_INVALID = 0,
        SMA_OM_OFF = 303,
        SMA_OM_WATT = 1077,
        SMA_OM_PERCENT = 1078,
        SMA_OM_SYSTEMCTL = 1079
    } OperatingMode;

private:
    uint32_t mGridCode;
    int mUnitId;
};

#endif // SMA_INVERTER_H
