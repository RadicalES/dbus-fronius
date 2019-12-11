#ifndef INVERTER_H
#define INVERTER_H

#include "defines.h"
#include "ve_service.h"

class PowerInfo;
class BasicPowerInfo;

enum InverterQuirk {
	QuirkIgnoreNullFrames = 1  // Fronius inverters sometimes send runt null
							   // frames with an error
};

class Inverter : public VeService
{
	Q_OBJECT
public:
	Inverter(VeQItem *root, const DeviceInfo &deviceInfo, int deviceInstance, QObject *parent = 0);

	/*!
	 * Error code as returned by the fronius inverter
	 */
	int errorCode() const;

	void setErrorCode(int code);

	/*!
	 * Status as returned by the fronius inverter
	 * - 0-6: Startup
	 * - 7: Running
	 * - 8: Standby
	 * - 9: Boot loading
	 * - 10: Error
	 */
	int statusCode() const;

	void setStatusCode(int code);

	void invalidateStatusCode();

	QString productName() const;

	QString customName() const;

	void setCustomName(const QString &name);

	QString hostName() const;

	void setHostName(const QString &h);

	int port() const;

	void setPort(int p);

	InverterPosition position() const;

	void setPosition(InverterPosition p);

	const DeviceInfo &deviceInfo() const
	{
		return mDeviceInfo;
	}

	BasicPowerInfo *meanPowerInfo();

	PowerInfo *l1PowerInfo();

	PowerInfo *l2PowerInfo();

	PowerInfo *l3PowerInfo();

	PowerInfo *getPowerInfo(InverterPhase phase);

	double powerLimit() const;

	void setPowerLimit(double p);

	virtual int handleSetValue(VeQItem *item, const QVariant &variant);

	/// Returns a string describing the location ('<serial>@<ip-address>:<port>').
	QString location() const;

	int quirks() const
	{
		return mQuirks;
	}


protected:
	void setQuirks(int quirks);

signals:
	void isConnectedChanged();

	void customNameChanged();

	void hostNameChanged();

	void portChanged();

	void powerLimitRequested(double value);

private:
	void updateConnectionItem();

	DeviceInfo mDeviceInfo;
	VeQItem *mErrorCode;
	VeQItem *mStatusCode;
	VeQItem *mPowerLimit;
	VeQItem *mPosition;
	VeQItem *mDeviceInstance;
	VeQItem *mCustomName;
	VeQItem *mProductName;
	VeQItem *mConnection;

	BasicPowerInfo *mMeanPowerInfo;
	PowerInfo *mL1PowerInfo;
	PowerInfo *mL2PowerInfo;
	PowerInfo *mL3PowerInfo;

	int mQuirks;
};

#endif // INVERTER_H
