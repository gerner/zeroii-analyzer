#ifndef _ANALYZER_H
#define _ANALYZER_H

#include <algorithm>
#include "Complex.h"

#include "log.h"

Logger analysis_logger("analysis");

Complex compute_gamma(const Complex z, const float z0_real) {
    // gamma = (z - z0) / (z + z0)
    // z = r + xj

    Complex z0(z0_real);

    return (z - z0) / (z + z0);
}

Complex compute_z(const Complex gamma, const float z0_real) {
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

struct AnalysisPoint {
    uint32_t fq;
    Complex uncal_z;

    AnalysisPoint(): fq(0) {}
    AnalysisPoint(const AnalysisPoint &p): fq(p.fq), uncal_z(p.uncal_z) {}
    AnalysisPoint(uint32_t a_fq, Complex a_uncal_z) : fq(a_fq), uncal_z(a_uncal_z) {}

    static const size_t data_size = sizeof(uint32_t)+sizeof(Complex);

    static AnalysisPoint from_bytes(const uint8_t* data) {
        // assume data has enough elements
        uint32_t fq = *(uint32_t*)data;
        Complex c = *(Complex*)(data+sizeof(uint32_t));

        return AnalysisPoint(fq, c);
    }

    static void to_bytes(AnalysisPoint point, uint8_t* data) {
        *(uint32_t*)data = point.fq;
        *(Complex *)(data+sizeof(uint32_t)) = point.uncal_z;
    }
};

struct CalibrationPoint {
    CalibrationPoint() {
    }

    CalibrationPoint(const CalibrationPoint &p) {
        fq = p.fq;
        cal_short = p.cal_short;
        cal_open = p.cal_open;
        cal_load = p.cal_load;
    }

    CalibrationPoint(uint32_t f, Complex s, Complex o, Complex l) {
        fq = f;
        cal_short = s;
        cal_open = o;
        cal_load = l;
    }

    uint32_t fq;
    Complex cal_short;
    Complex cal_open;
    Complex cal_load;
};

bool CalibrationCmp(const CalibrationPoint &lhs, const uint32_t &fq) {
    return lhs.fq < fq;
}

CalibrationPoint uncalibrated_point = CalibrationPoint(0, Complex(-1), Complex(1), Complex(0));

class Analyzer {
    public:
        Analyzer(float z0, CalibrationPoint* calibration_results) {
            z0_ = z0;
            calibration_len_ = 0;
            calibration_results_ = calibration_results;
        }

        Complex uncalibrated_measure(uint32_t fq) {
            zeroii_.startMeasure(fq);

            float R = zeroii_.getR();
            float X = zeroii_.getX();

            analysis_logger.debug(String("")+R+" + "+X+" i");

            return Complex(R, X);
        }

        Complex calibrated_gamma(const AnalysisPoint &p) const {
            return calibrated_gamma(p.fq, p.uncal_z);
        }

        Complex calibrated_gamma(uint32_t fq, Complex uncalibrated_z) const {
            CalibrationPoint* cal = find_calibration(fq);
            return calibrate_reflection(cal->cal_short, cal->cal_open, cal->cal_load, compute_gamma(uncalibrated_z, z0_));
        }

        CalibrationPoint* find_calibration(uint32_t fq) const {
            if(calibration_len_ == 0) {
                return &uncalibrated_point;
            } else {
                size_t i;
                return std::lower_bound(calibration_results_, &calibration_results_[calibration_len_], fq, CalibrationCmp);
            }
        }

        RigExpertZeroII_I2C zeroii_;

        float z0_;
        size_t calibration_len_;
        CalibrationPoint *calibration_results_;
};

#endif
