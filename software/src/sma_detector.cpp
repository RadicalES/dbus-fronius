#include <QsLog.h>
#include <velib/vecan/products.h>
#include "modbus_tcp_client.h"
#include "modbus_reply.h"
#include "settings.h"
#include "sma_detector.h"
//#include "sma_tools.h"

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

    QLOG_INFO() << "SMADetector::start";
    return reply;
}

void SMADetector::onConnected()
{
    ModbusTcpClient *client = static_cast<ModbusTcpClient *>(sender());
    Reply *di = mClientToReply.value(client);
    Q_ASSERT(di != 0);
    QLOG_INFO() << "SMADetector::onConnected to " << client->hostName() << " @" << client->portName();
    di->state = Reply::ReadDeviceClass;
    di->currentRegister = 30051;
    startNextReadRequest(di, 2);
}

void SMADetector::onDisconnected()
{
    ModbusTcpClient *client = static_cast<ModbusTcpClient *>(sender());
    Reply *di = mClientToReply.value(client);
    QLOG_INFO() << "SMADetector::onDisconnected";
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
                QLOG_INFO() << "SMADetector::OnFinnished device class not supported: " << devclass;
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
            di->di.retrievalMode = ProtocolSMAPVReadOnly;
            di->di.productId = VE_PROD_ID_PV_INVERTER_SMA;

            switch (modelId) {
            case 9074: // SB 3000TL-21
                di->di.productName = QString("%1 %2").arg("SMA").arg("SB 3000TL-21");
                break;
            case 9075: // SB 4000TL-21
                di->di.productName = QString("%1 %2").arg("SMA").arg("SB 4000TL-21");
                break;
            case 9076: // SB 5000TL-21
                di->di.productName = QString("%1 %2").arg("SMA").arg("SB 5000TL-21");
                break;
            case 9165: // SB 3600TL-21
                di->di.productName = QString("%1 %2").arg("SMA").arg("SB 3600TL-21");
                break;

            default:
                QLOG_INFO() << "SMADetector::OnFinnished ReadDeviceType mode not supported: " << modelId;
                di->di.phaseCount = 0;
                di->di.deviceType = -1;
                di->di.productId = 0;
                setDone(di);
                return;
            }

            QLOG_INFO() << "SMADetector::OnFinnished model" << di->di.productName;

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
            di->di.serialNumber.setNum(sn);

            QLOG_INFO() << "SMADetector::OnFinnished serial number" << di->di.serialNumber;

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
            unsigned long softwarePackage = (values[0] << 16) | values[1];

            QLOG_INFO() << "SMADetector::OnFinnished software version: " << softwarePackage;

            di->currentRegister = 40029;
            di->state = Reply::ReadStatus;
            startNextReadRequest(di, 2);
            break;
        }
        case Reply::ReadStatus:
        {
            if (values.size() < 2) {
                setDone(di);
                return;
            }
            //di->di.softwarePackage = (values[0] << 16) | values[1];

            QLOG_INFO() << "SMADetector::OnFinnished inverter status: " << values[1];

            di->currentRegister = 40133;
            di->state = Reply::ReadGridVoltageFrequency;
            startNextReadRequest(di, 4);
            break;
        }
        case Reply::ReadGridVoltageFrequency:
        {
            if (values.size() < 4) {
                setDone(di);
                return;
            }
            //di->di.softwarePackage = (values[0] << 16) | values[1];

            QLOG_INFO() << "SMADetector::OnFinnished grid voltage: " << values[1] << " - grid frequency: " << values[3];

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
            QLOG_INFO() << "SMADetector::OnFinnished Max Power" << di->di.maxPower;

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

            // set dummy value, not used
            di->di.powerLimitScale = 10000;
            QLOG_INFO() << "SMADetector::OnFinnished current power limit" << values[1];

            di->currentRegister = 30835;
            di->state = Reply::ReadOpMode;
            startNextReadRequest(di, 2);
            break;
}
        case Reply::ReadOpMode:
        {
            if (values.size() < 2) {
                setDone(di);
                return;
            }

            unsigned int opMode = values[1];

            QLOG_INFO() << "SMADetector::OnFinnished operation mode" << opMode;

            di->state = Reply::WriteGridCode;
            di->retries = 0;
            uint32_t gc = mSettings->gridCode();
            QVector<quint16> vals;
            quint16 gc0 = (gc >> 16) & 0xffff;
            quint16 gc1 = gc & 0xffff;
            vals.clear();
            vals.append(gc0);
            vals.append(gc1);

            di->currentRegister = 43090;
            startNextWriteRequest(di, vals);
            break;
        }
        case Reply::WriteGridCode:
        {
            QLOG_INFO() << "SMADetector::OnFinnished grid code written" << mSettings->gridCode();
            di->state = Reply::CheckGridCode;
            startNextReadRequest(di, 2);
            break;
        }
        case Reply::CheckGridCode:
        {
            if (values.size() < 2) {
                setDone(di);
                return;
            }
            QLOG_INFO() << "SMADetector::OnFinnished read grid code status" << values[0] << " lsb " << values[1];

            if(values[1] != 1) {

                if(di->retries < 3) {
                    QLOG_INFO() << "SMADetector::OnFinnished write grid code retry";
                    di->retries++;
                    di->state = Reply::WriteGridCode;
                    uint32_t gc = mSettings->gridCode();
                    QVector<quint16> vals;
                    quint16 gc0 = (gc >> 16) & 0xffff;
                    quint16 gc1 = gc & 0xffff;
                    vals.clear();
                    vals.append(gc0);
                    vals.append(gc1);

                    di->currentRegister = 43090;
                    startNextWriteRequest(di, vals);
                    return;
                }

                QLOG_INFO() << "SMADetector::OnFinnished failed to log in, register in readonly mode";
                di->setResult();
                setDone(di);
                return;
            }

            // we have full control over the inverter
            QLOG_INFO() << "SMADetector::OnFinnished Gride Code is SET";
            di->state = Reply::WriteOpMode;
            QVector<quint16> values;
            values.append(0);
            values.append(1077);
            di->currentRegister = 40210;
            startNextWriteRequest(di, values);
            break;
        }
        case Reply::WriteOpMode:
        {
            di->currentRegister = 40210;
            di->state = Reply::CheckOpMode;
            startNextReadRequest(di, 2);
            break;
        }
        case Reply::CheckOpMode:
        {
            if (values.size() < 2) {
                setDone(di);
                return;
            }
            unsigned int opMode = values[1];
            QLOG_INFO() << "SMADetector::OnFinnished checking op mode" << opMode;

            if(opMode == 1077) {
                QLOG_INFO() << "SMADetector::OnFinnished Op Mode OK!";
                di->di.retrievalMode = ProtocolSMAPVReadWrite;
            }
            else {
                QLOG_INFO() << "SMADetector::OnFinnished failed to set op mode!";
            }
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

void SMADetector::startNextWriteRequest(Reply *di, const QVector<quint16> &values)
{
    ModbusReply *reply = di->client->writeMultipleHoldingRegisters(di->di.networkId, di->currentRegister,
                                                          values);
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
    QLOG_INFO() << "SMADetector Done!";
}

SMADetector::Reply::Reply(QObject *parent):
    DetectorReply(parent),
    client(0),
    state(ReadDeviceClass),
    currentRegister(0)
{
}

SMADetector::Reply::~Reply()
{
}
