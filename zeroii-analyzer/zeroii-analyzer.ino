#include "RigExpertZeroII_I2C.h"
#include "Wire.h"
#include "Complex.h"

#define ZEROII_Reset_Pin  6
#define ZERO_I2C_ADDRESS 0x5B

RigExpertZeroII_I2C zeroii = RigExpertZeroII_I2C();

void setup() {
    Serial.begin(38400);
    Serial.flush();

    Serial.println("resetting ZEROII");
    pinMode(ZEROII_Reset_Pin, OUTPUT);
    digitalWrite(ZEROII_Reset_Pin, LOW);
    delay(50);
    digitalWrite(ZEROII_Reset_Pin, HIGH);

    if(!zeroii.startZeroII()) {
        Serial.println("failed to start zeroii");
        return;
    }

    String str = "Version: ";
    Serial.println(str + zeroii.getMajorVersion() + "." + zeroii.getMinorVersion() + 
            ", HW Revision: " + zeroii.getHwRevision() + 
            ", SN: " + zeroii.getSerialNumber()
    );

    analyze();

    Serial.println("done");
}

void loop() {
    delay(500);
}

void analyze() {
    double Z0 = 50;
    int32_t centerFq = 28400000; //300000000;// 148 000 000 Hz
    int32_t rangeFq = 800000;//400000000;// 10 000 000 Hz
    int32_t dotsNumber = 100;

    int32_t startFq = centerFq - (rangeFq/2);
    int32_t endFq = centerFq + (rangeFq/2);
    int32_t stepFq = (endFq - startFq)/dotsNumber;

    zeroii.setZ0(50);

    for(int i = 0; i <= dotsNumber; ++i)
    {
        int32_t temp = startFq + (stepFq*i);
        zeroii.startMeasure(temp);
        Serial.print("Fq: ");
        Serial.print(temp);
        Serial.print(", R: ");
        Serial.print(zeroii.getR());
        Serial.print(", Rp: ");
        Serial.print(zeroii.getRp());
        Serial.print(", X: ");
        Serial.print(zeroii.getX());
        Serial.print(", Xp: ");
        Serial.print(zeroii.getXp());
        Serial.print(", SWR: ");
        Serial.print(zeroii.getSWR());
        Serial.print(", RL: ");
        Serial.print(zeroii.getRL());
        Serial.print(", Z: ");
        Serial.print(zeroii.getZ());
        Serial.print(", Phase: ");
        Serial.print(zeroii.getPhase());
        Serial.print(", Rho: ");
        Serial.print(zeroii.getRho());
        Serial.print(", Gamma: ");
        Serial.print(compute_gamma(zeroii.getR(), zeroii.getX(), 50));
        Serial.print("\r\n");
    }
    Serial.print("------------------------\r\n");
}

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

/*
// some thoughts on getting capacitance/inductance
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
