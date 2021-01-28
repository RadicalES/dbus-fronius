#ifndef SMA_INVERTER_H
#define SMA_INVERTER_H

#include "inverter.h"
#include "pv_info.h"
#include <QObject>

class SMAInverter : public Inverter
{
    Q_OBJECT
public:
    SMAInverter(VeQItem *root, const DeviceInfo &deviceInfo, int deviceInstance, int unitId, uint32_t gridCode, QObject *parent = 0);

    typedef enum OperatingState {
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
    } OperatingState_t;

    typedef enum OperatingCondition {
        SMA_OC_INVALID = 0,
        SMA_OC_FAULT = 35,
        SMA_OC_OFF = 303,
        SMA_OC_OK = 307,
        SMA_OC_WARN = 455
    } OperatingCondition_t;

    typedef enum OperatingMode {
        SMA_OM_INVALID = 0,
        SMA_OM_OFF = 303,
        SMA_OM_WATT = 1077,
        SMA_OM_PERCENT = 1078,
        SMA_OM_SYSTEMCTL = 1079
    } OperatingMode_t;

    typedef enum StatusCode {
        SMA_SC_START0 = 0,
        SMA_SC_START1 = 1,
        SMA_SC_START2 = 2,
        SMA_SC_START3 = 3,
        SMA_SC_START4 = 4,
        SMA_SC_START5 = 5,
        SMA_SC_START6 = 6,
        SMA_SC_RUNNING = 7,
        SMA_SC_STANDBY = 8,
        SMA_SC_BOOT = 9,
        SMA_SC_ERROR = 10,
        SMA_SC_MPPT = 11,
        SMA_SC_THROTTLED = 12
    } StatusCode_t;

    int status() const;
    OperatingState_t opState() const;
    OperatingCondition_t opCondition() const;
    OperatingMode_t opMode() const;
    bool isLoggedIn() const;

    PvInfo *pvInfo1();
    PvInfo *pvInfo2();
    void setTemperature(double degC);
    void setOperatingMode(OperatingMode_t mode);
    void setOperatingCondition(OperatingCondition_t condition);
    void setOperatingState(OperatingState_t state);
    void setStatus(StatusCode_t code);
    void setError(int error);
    void setLoggedOn(bool status);
    int unitId() const;

    uint32_t gridCode() const;

private:

    PvInfo *mPvInfo1;
    PvInfo *mPvInfo2;
    VeQItem *mTemperature;
    VeQItem *mOperatingMode;
    VeQItem *mOperatingState;
    VeQItem *mOperatingCondition;
    VeQItem *mLoggedOn;

    QString mFirmware;
    uint32_t mGridCode;
    int mUnitId;
    StatusCode_t mStatus;
    OperatingMode_t mOpMode;
    OperatingCondition_t mOpCondition;
    OperatingState_t mOpState;
    int mError;
    bool mLoggedIn;

};

#endif // SMA_INVERTER_H
