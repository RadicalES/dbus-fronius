#include <qnumeric.h>
#include "pv_info.h"

PvInfo::PvInfo(VeQItem *root, QObject *parent) :
    VeService(root, parent),
    mCurrent(createItem("Current")),
    mVoltage(createItem("Voltage")),
    mPower(createItem("Power"))
{
}

double PvInfo::current() const
{
    return getDouble(mCurrent);
}

void PvInfo::setCurrent(double c)
{
    produceDouble(mCurrent, c, 3, "A");
}

double PvInfo::voltage() const
{
    return getDouble(mVoltage);
}

void PvInfo::setVoltage(double v)
{
    produceDouble(mVoltage, v, 2, "V");
}

double PvInfo::power() const
{
    return getDouble(mPower);
}

void PvInfo::setPower(double v)
{
    produceDouble(mPower, v, 0, "W");
}

void PvInfo::resetValues()
{
    setCurrent(qQNaN());
    setVoltage(qQNaN());
    setPower(qQNaN());
}
