#include "RigExpertZeroII_I2C.h"
#include "Wire.h"
#include "Complex.h"

#include "analyzer.h"
#include "rotary_encoder.h"

#define ZEROII_Reset_Pin  6
#define ZERO_I2C_ADDRESS 0x5B
#define CLK 2
#define DT 3
#define SW 5
#define Z0 50

Analyzer analyzer(Z0);
RotaryEncoder encoder(CLK, DT, SW);

void setup() {
    Serial.begin(38400);
    Serial.flush();

    Serial.println("resetting ZEROII");
    pinMode(ZEROII_Reset_Pin, OUTPUT);
    digitalWrite(ZEROII_Reset_Pin, LOW);
    delay(50);
    digitalWrite(ZEROII_Reset_Pin, HIGH);

    if(!analyzer.zeroii_.startZeroII()) {
        Serial.println("failed to start zeroii");
        return;
    }

    String str = "Version: ";
    Serial.println(str + analyzer.zeroii_.getMajorVersion() + "." + analyzer.zeroii_.getMinorVersion() +
            ", HW Revision: " + analyzer.zeroii_.getHwRevision() +
            ", SN: " + analyzer.zeroii_.getSerialNumber()
    );

    encoder.initialize();

    Serial.println("done");
}

#define STATE_CALIBRATION 0
#define STATE_CALIBRATION_S 1
#define STATE_CALIBRATION_O 2
#define STATE_CALIBRATION_L 3
#define STATE_WAITING 4
#define STATE_MEASURING 5

int state = STATE_CALIBRATION;
int clicks = 0;
int32_t centerFq = 28400000; //300000000;// 148 000 000 Hz
void loop() {
    encoder.update();
    switch(state) {
        case STATE_CALIBRATION:
            Serial.println("connect short and press knob");
            state = STATE_CALIBRATION_S;
            break;
        case STATE_CALIBRATION_S:
            if (encoder.click_) {
                Serial.println(analyzer.calibrate_short(centerFq, Z0));
                state = STATE_CALIBRATION_O;
                Serial.println("connect open and press knob");
            }
            break;
        case STATE_CALIBRATION_O:
            if (encoder.click_) {
                Serial.println(analyzer.calibrate_open(centerFq, Z0));
                state = STATE_CALIBRATION_L;
                Serial.println("connect load and press knob");
            }
            break;
        case STATE_CALIBRATION_L:
            if (encoder.click_) {
                Serial.println(analyzer.calibrate_load(centerFq, Z0));
                state = STATE_WAITING;
                Serial.println("done calibrating.");
            }
            break;
        case STATE_WAITING:
            // rotating changes frequency
            if (encoder.turn_ != 0) {
                if(encoder.turn_ < 0) {
                    centerFq--;
                } else if(encoder.turn_ > 0) {
                    centerFq++;
                }
                Serial.print("Center Frequency: ");
                Serial.println(centerFq);
            }
            // clicking starts a measurement
            if (encoder.click_) {
                Serial.println("Analyzing...");
                state = STATE_MEASURING;
                analyze(centerFq);
                state = STATE_WAITING;
            }
            break;
    }
    if (encoder.click_) {
        Serial.print("click: ");
        Serial.println(++clicks);
    }
    // TODO: if we're in a measurement, allow cancelling it by clicking
    delay(1);
}

void analyze_frequency(int32_t fq) {
    Complex uncal_z = analyzer.uncalibrated_measure(fq);
    Complex cal_gamma = analyzer.calibrated_gamma(uncal_z);
    float SWR = compute_swr(cal_gamma);

    Serial.print("Fq: ");
    Serial.print(fq);
    Serial.print(", Gamma: ");
    Serial.print(cal_gamma);
    Serial.print(", Z: ");
    Serial.print(compute_z(cal_gamma, Z0));
    Serial.print(", SWR: ");
    Serial.print(SWR);
    Serial.print(", Zuncal: ");
    Serial.print(uncal_z);
    Serial.print("\r\n");
}

void analyze(int32_t centerFq) {
    int32_t rangeFq = 800000;//400000000;// 10 000 000 Hz
    int32_t dotsNumber = 100;

    int32_t startFq = centerFq - (rangeFq/2);
    int32_t endFq = centerFq + (rangeFq/2);
    int32_t stepFq = (endFq - startFq)/dotsNumber;

    for(int i = 0; i <= dotsNumber; ++i)
    {
        analyze_frequency(startFq + (stepFq*i));
    }
    Serial.print("------------------------\r\n");
}

/*
Complex compute_gamma(float r, float x, float z0_real) {
    // gamma = (z - z0) / (z + z0)
    // z = r + xj

    Complex z(r, x);
    Complex z0(z0_real);

    return (z - z0) / (z + z0);
}

Complex calibrate_reflection(Complex sc, Complex oc, Complex load, Complex reflection) {
    // inspired by NanoVNA
    Complex a=load, b=oc, c=sc, d = reflection;
    return -(a - d)*(b - c)/(a*(b - c) + Complex(2.f)*c*(a - b) + d*(Complex(-2.f)*a + b + c));
}

float resistance(Complex v) {
    float re = v.real(), im = v.imag();
    float z0 = 50;
    float d = z0 / ((1-re)*(1-re)+im*im);
    float zr = ((1+re)*(1-re) - im*im) * d;
    return zr;
}

float reactance(Complex v) {
    float re = v.real(), im = v.imag();
    float z0 = 50;
    float d = z0 / ((1-re)*(1-re)+im*im);
    float zi = 2*im * d;
    return zi;
}
*/

/*
// some thoughts on getting capacitance/inductance
// X = - 1/(2*pi*f*C)
// X = 2*pi*f*L
// courtesy nanovna-v2/NanoVNA-QT
inline double capacitance_inductance(double freq, double Z) {
    if(Z>0) return Z/(2*M_PI*freq);
    return 1./(2*Z*M_PI*freq);
}

case SParamViewSource::TYPE_Z_CAP: // capacitance in pF
    y = -capacitance_inductance(freqHz, Z.imag()) * 1e12;
    break;
case SParamViewSource::TYPE_Z_IND: // inductance in nH
    y = capacitance_inductance(freqHz, Z.imag()) * 1e9;
    break;
*/
