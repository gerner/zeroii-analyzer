#ifndef _ANALYZER_H
#define _ANALYZER_H

#include "Complex.h"

Complex compute_gamma(Complex z, float z0_real) {
    // gamma = (z - z0) / (z + z0)
    // z = r + xj

    Complex z0(z0_real);

    return (z - z0) / (z + z0);
}

Complex compute_z(Complex gamma, float z0_real) {
    Complex z0(z0_real);
    return z0*((one+gamma) / (one-gamma));
}

/*Complex calibrate_reflection(Complex sc, Complex oc, Complex load, Complex reflection) {
    // inspired by NanoVNA
    Complex a=load, b=oc, c=sc, d = reflection;
    return -(a - d)*(b - c)/(a*(b - c) + Complex(2.f)*c*(a - b) + d*(Complex(-2.f)*a + b + c));
}*/

Complex calibrate_reflection(Complex sc, Complex oc, Complex load, Complex reflection) {
    // via: https://hsinjulit.com/sol-calibration/
    // e01 * e10 = 2 * (oc - load)(load - sc)/(oc - sc)
    // e11 = (sc + oc - 2*load)/(oc - sc)
    // cal_gamma = (uncal_gamma - load)/(e01*e10 + e11(uncal_gamma - load))
    Complex two = Complex(2);
    Complex uncal_gamma = reflection;
    Complex e01e10 = two * (oc - load)*(load - sc)/(oc - sc);
    Complex e11 = (sc + oc - two*load)/(oc - sc);
    Complex cal_gamma = (uncal_gamma - load)/(e01e10 + e11*(uncal_gamma - load));
    return cal_gamma;
}

float compute_swr(Complex gamma) {
    float mag = gamma.modulus();
    if (mag > 1) {
        return INFINITY;
    } else {
        return (1 + mag) / (1 - mag);
    }
}

class Analyzer {
    public:
        Analyzer(float z0) {
            z0_ = z0;
            cal_short_ = Complex(-1);
            cal_open_ = Complex(1);
            cal_load_ = Complex(0);
        }

        Complex calibrate_short(int32_t fq, float z0) {
            cal_short_ = compute_gamma(uncalibrated_measure(fq), z0);
            return cal_short_;
        }

        Complex calibrate_open(int32_t fq, float z0) {
            cal_open_ = compute_gamma(uncalibrated_measure(fq), z0);
            return cal_open_;
        }

        Complex calibrate_load(int32_t fq, float z0) {
            cal_load_ = compute_gamma(uncalibrated_measure(fq), z0);
            return cal_load_;
        }

        void calibrate(Complex cal_short, Complex cal_open, Complex cal_load) {
            cal_short_ = cal_short;
            cal_open_ = cal_open;
            cal_load_ = cal_load;
        }

        Complex uncalibrated_measure(int32_t fq) {
            zeroii_.startMeasure(fq);

            return Complex(zeroii_.getR(), zeroii_.getX());
        }

        Complex calibrated_gamma(Complex uncalibrated_z) {
            return calibrate_reflection(cal_short_, cal_open_, cal_load_, compute_gamma(uncalibrated_z, z0_));
        }

        RigExpertZeroII_I2C zeroii_;

    private:
        float z0_;
        Complex cal_short_;
        Complex cal_open_;
        Complex cal_load_;
};

#endif