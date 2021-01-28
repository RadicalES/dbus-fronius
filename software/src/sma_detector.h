#ifndef SMA_DETECTOR_H
#define SMA_DETECTOR_H

#include <QAbstractSocket>
#include "abstract_detector.h"
#include "defines.h"

class Settings;
class ModbusReply;
class ModbusTcpClient;

class SMADetector : public AbstractDetector
{
    Q_OBJECT
public:
    SMADetector(QObject *parent = 0);

    SMADetector(const Settings *settings, QObject *parent = 0);

    virtual DetectorReply *start(const QString &hostName, int timeout);


private slots:
    void onConnected();

    void onDisconnected();

    void onFinished();

private:
    class Reply : public DetectorReply
    {
    public:
        Reply(QObject *parent = 0);

        virtual ~Reply();

        virtual QString hostName() const
        {
            return di.hostName;
        }

        void setResult()
        {
            emit deviceFound(di);
        }

        void setFinished()
        {
            emit finished();
        }

        enum State {
            ReadDeviceClass,
            ReadDeviceType,
            ReadSerialNumber,
            ReadSoftwareVersion,
            ReadMaxPower,
            ReadPowerLimit,
        };

        DeviceInfo di;
        ModbusTcpClient *client;
        State state;
        quint16 currentRegister;
    };

    void startNextReadRequest(Reply *di, quint16 regCount);
    void setDone(Reply *di);
    quint8 BCDtoByte(quint8 bcd);

    QHash<ModbusTcpClient *, Reply *> mClientToReply;
    QHash<ModbusReply *, Reply *> mModbusReplyToReply;
    const Settings *mSettings;
};

#endif // SMA_DETECTOR_H
