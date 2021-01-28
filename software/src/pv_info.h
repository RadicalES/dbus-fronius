#ifndef PV_INFO_H
#define PV_INFO_H

#include "ve_service.h"

class PvInfo : public VeService
{
    Q_OBJECT
public:
    explicit PvInfo(VeQItem *root, QObject *parent = 0);

    double current() const;

    void setCurrent(double c);

    double voltage() const;

    void setVoltage(double v);

    double power() const;

    void setPower(double p);

    /*!
     * @brief Reset all measured values to NaN
     */
    void resetValues();

private:
    VeQItem *mCurrent;
    VeQItem *mVoltage;
    VeQItem *mPower;
};

#endif // PV_INFO_H
