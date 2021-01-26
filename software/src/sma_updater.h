#ifndef SMA_UPDATER_H
#define SMA_UPDATER_H

#include <QObject>
#include <QAbstractSocket>
#include "froniussolar_api.h"
#include "sma_inverter.h"

class DataProcessor;
class Inverter;
class InverterSettings;
class ModbusReply;
class ModbusTcpClient;
class QTimer;

class SMAUpdater : public QObject
{
    Q_OBJECT
public:
    explicit SMAUpdater(Inverter *inverter, InverterSettings *settings, QObject *parent = 0);

signals:
    void connectionLost();

    void inverterModelChanged();

public slots:
    void onReadCompleted();

    void onWriteCompleted();

    void onPowerLimitRequested(double value);

    void onConnected();

    void onDisconnected();

    void onTimer();

   // void onPhaseChanged();

private:
    enum ModbusState {
        CheckCondition,
        CheckState,
        CheckLogin,
        DoLogin,
        CheckOpMode,
        SetOpMode,

        ReadPowerYield,
        ReadACFrequency,
        ReadACCurrent,
        ReadACPowerAndVoltage,
        ReadDCData1,
        ReadDCData2,

        ReadPowerLimit,
        WritePowerLimit,
        Idle
    };

    void connectModbusClient();

    void startNextAction(ModbusState state);

    void startIdleTimer();

    void setInverterState(int state);

    void readHoldingRegisters(quint16 startRegister, quint16 count);

    void writeMultipleHoldingRegisters(quint16 startReg, const QVector<quint16> &values);

    bool handleModbusError(ModbusReply *reply);

    void handleError();

    ModbusState getInitState() const;

    uint64_t getDoubleULong(const QVector<quint16> &values, int offset);
    uint32_t getULong(const QVector<quint16> &values, int offset);

    SMAInverter *mInverter;
    InverterSettings *mSettings;
    ModbusTcpClient *mModbusClient;
    QTimer *mTimer;
    DataProcessor *mDataProcessor;
    ModbusState mCurrentState;
    uint16_t mPowerLimitWatt;
    int mRetryCount;
    int mWriteCount;
    int mState;
    int mCondition;
    bool mWritePowerLimitRequested;
    bool mLoggedIn;
    CommonInverterData mInverterData;

};

#endif // SMA_UPDATER_H
