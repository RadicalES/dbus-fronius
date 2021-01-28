#include <QsLog.h>
#include <velib/vecan/products.h>
#include "modbus_tcp_client.h"
#include "modbus_reply.h"
#include "settings.h"
#include "sma_detector.h"

SMADetector::SMADetector(QObject *parent):
    AbstractDetector(parent)
{
}

SMADetector::SMADetector(const Settings *settings, QObject *parent):
    AbstractDetector(parent),
    mSettings(settings)
{
}

DetectorReply *SMADetector::start(const QString &hostName, int timeout)
{
    ModbusTcpClient *client = new ModbusTcpClient(this);
    connect(client, SIGNAL(connected()), this, SLOT(onConnected()));
    connect(client, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
    client->setTimeout(timeout);
    client->connectToServer(hostName);
    Reply *reply = new Reply(this);
    reply->client = client;
    reply->di.networkId = mSettings->unitId();
    reply->di.hostName = hostName;
    mClientToReply[client] = reply;

    QLOG_DEBUG() << "SMADetector start";
    return reply;
}

void SMADetector::onConnected()
{
    ModbusTcpClient *client = static_cast<ModbusTcpClient *>(sender());
    Reply *di = mClientToReply.value(client);
    Q_ASSERT(di != 0);

    QLOG_DEBUG() << "SMADetector connected to " << client->hostName() << " @" << client->portName();

    di->state = Reply::ReadDeviceClass;
    di->currentRegister = 30051;
    startNextReadRequest(di, 2);
}

void SMADetector::onDisconnected()
{
    ModbusTcpClient *client = static_cast<ModbusTcpClient *>(sender());
    Reply *di = mClientToReply.value(client);

    QLOG_DEBUG() << "SMADetector disconnected";

    if (di != 0)
        setDone(di);
}

void SMADetector::onFinished()
{
    ModbusReply *reply = static_cast<ModbusReply *>(sender());
    Reply *di = mModbusReplyToReply.take(reply);
    reply->deleteLater();

    QVector<quint16> values = reply->registers();

    switch (di->state) {
        case Reply::ReadDeviceClass:
        {
            if (values.size() < 2) {
                setDone(di);
                return;
            }

            quint32 devclass = (values[0] << 16) | values[1];
            if (devclass != 8001) {
                QLOG_ERROR() << "SMADetector device class not supported: " << devclass;

                setDone(di);
                return;
            }

            di->currentRegister = 30053;
            di->state = Reply::ReadDeviceType;
            startNextReadRequest(di, 2);
            break;
        }
        case Reply::ReadDeviceType:
        {
            if (values.size() < 2) {
                setDone(di);
                return;
            }

            quint32 modelId = (values[0] << 16) | values[1];
            di->di.deviceType = modelId;
            di->di.phaseCount = 1;
            di->di.retrievalMode = ProtocolSMA;
            di->di.productId = VE_PROD_ID_PV_INVERTER_SMA;

            switch (modelId) {
            case 9074: // SB 3000TL-21
                di->di.productName = QString("SMA %1").arg("SunnyBoy 3000TL-21");
                break;
            case 9075: // SB 4000TL-21
                di->di.productName = QString("SMA %1").arg("SunnyBoy 4000TL-21");
                break;
            case 9076: // SB 5000TL-21
                di->di.productName = QString("SMA %1").arg("SunnyBoy 5000TL-21");
                break;
            case 9165: // SB 3600TL-21
                di->di.productName = QString("SMA %1").arg("SunnyBoy 3600TL-21");
                break;

            default:
                QLOG_ERROR() << "SMADetector model not supported: " << modelId;
                di->di.phaseCount = 0;
                di->di.deviceType = -1;
                di->di.productId = 0;
                setDone(di);
                return;
            }

            QLOG_DEBUG() << "SMADetector found model: " << di->di.productName;
            di->currentRegister = 30057;
            di->state = Reply::ReadSerialNumber;
            startNextReadRequest(di, 2);
            break;
        }
        case Reply::ReadSerialNumber:
        {
            if (values.size() < 2) {
                setDone(di);
                return;
            }
            unsigned long sn = (values[0] << 16) | values[1];
            di->di.uniqueId = di->di.serialNumber.setNum(sn);
            di->di.firmwareVersion = "1.0.4";
            di->di.storageCapacity = 0;

            QLOG_DEBUG() << "SMADetector serial number: " << di->di.serialNumber;

            di->currentRegister = 30059;
            di->state = Reply::ReadSoftwareVersion;
            startNextReadRequest(di, 2);
            break;
        }
        case Reply::ReadSoftwareVersion:
        {
            if (values.size() < 2) {
                setDone(di);
                return;
            }

            quint8 major, minor, build, extra;
            major = BCDtoByte((values[0] >> 8) & 0x00ff);
            minor = BCDtoByte(values[0] & 0x00ff);
            build = BCDtoByte((values[1] >> 8) & 0x00ff);
            extra = BCDtoByte(values[1] & 0x00ff);
            QChar t[6] = { 'N', 'E', 'A', 'B', 'R', 'S' };
            QString vstr;

            if(extra < 6) {
                vstr = QString("%1.%2.%3.%4").arg(major).arg(minor).arg(build).arg(t[extra]);
            }
            else {
                vstr = QString("%1.%2.%3.%4").arg(major).arg(minor).arg(build).arg(extra);
            }


            QLOG_DEBUG() << "SMADetector software version: " << vstr;
            di->di.firmwareVersion = vstr;

            di->currentRegister = 30231;
            di->state = Reply::ReadMaxPower;
            startNextReadRequest(di, 2);
            break;
        }       
        case Reply::ReadMaxPower:
        {
            if (values.size() < 2) {
                setDone(di);
                return;
            }

            di->di.maxPower = values[1];
            QLOG_DEBUG() << "SMADetector Max Power: " << di->di.maxPower << " W";

            di->currentRegister = 30837;
            di->state = Reply::ReadPowerLimit;
            startNextReadRequest(di, 2);
            break;
        }
        case Reply::ReadPowerLimit:
        {
            if (values.size() < 2) {
                setDone(di);
                return;
            }

            // save our actual value here
            di->di.powerLimitScale = values[1];
            QLOG_DEBUG() << "SMADetector power limit: " << values[1] << " W";

            // finish the process
            di->setResult();
            setDone(di);
            break;
        }

        default:
            setDone(di);
    }
}

void SMADetector::startNextReadRequest(Reply *di, quint16 regCount)
{
    ModbusReply *reply = di->client->readHoldingRegisters(di->di.networkId, di->currentRegister,
                                                          regCount);
    mModbusReplyToReply[reply] = di;
    connect(reply, SIGNAL(finished()), this, SLOT(onFinished()));
}

void SMADetector::setDone(Reply *di)
{
    if (!mClientToReply.contains(di->client))
        return;
    di->setFinished();
    disconnect(di->client);
    mClientToReply.remove(di->client);
    di->client->deleteLater();
}

SMADetector::Reply::Reply(QObject *parent):
    DetectorReply(parent),
    client(0),
    state(ReadDeviceClass),
    currentRegister(0)
{
}

quint8 SMADetector::BCDtoByte(quint8 bcd)
{
    quint8 d = bcd & 0x0f;
    bcd >>= 4;
    d += bcd * 10;
    return d;
}

SMADetector::Reply::~Reply()
{
}
