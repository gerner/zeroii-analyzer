#include "RigExpertZeroII_I2C.h"
#include "Wire.h"
#include "Complex.h"

#include "analyzer.h"
#include "rotary_encoder.h"
#include "menu_manager.h"

#define ZEROII_Reset_Pin  6
#define ZERO_I2C_ADDRESS 0x5B
#define CLK 2
#define DT 3
#define SW 5
#define Z0 50

Analyzer analyzer(Z0);
RotaryEncoder encoder(CLK, DT, SW);

#define MOPT_ANALYZE 1
#define MOPT_FQ 2
#define MOPT_CALIBRATE 3
#define MOPT_FQCENTER 4
#define MOPT_FQWINDOW 5
#define MOPT_FQSTEPS 6
#define MOPT_BACK 7

MenuOption fq_menu_options[] = {
    MenuOption("Fq Center", MOPT_FQCENTER, NULL),
    MenuOption("Fq Range", MOPT_FQWINDOW, NULL),
    MenuOption("Steps", MOPT_FQSTEPS, NULL),
    MenuOption("Back", MOPT_BACK, NULL),
};
Menu fq_menu(NULL, fq_menu_options, sizeof(fq_menu_options)/sizeof(fq_menu_options[0]));

MenuOption root_menu_options[] = {
    MenuOption("Analyze", MOPT_ANALYZE, NULL),
    MenuOption("Fq Options", MOPT_FQ, &fq_menu),
    MenuOption("Calibration", MOPT_CALIBRATE, NULL),
};
Menu root_menu(NULL, root_menu_options, sizeof(root_menu_options)/sizeof(root_menu_options[0]));

MenuManager menu_manager(&root_menu);

int32_t centerFq = 28400000; //300000000;// 148 000 000 Hz
int32_t rangeFq = 800000;//400000000;// 10 000 000 Hz
int32_t dotsNumber = 100;

#define CAL_START 0
#define CAL_S 1
#define CAL_O 2
#define CAL_L 3
#define CAL_END 4
int calibration_state = CAL_START;

void draw_menu(Menu* current_menu, int current_option) {
    Serial.println(menu_manager.current_option_);
    for(int i=0; i<current_menu->option_count; i++) {
        // either:
        // this is the current option
        // this is the selected option
        // this is just an option
        if (current_option >= 0 && current_menu->options[i].option_id == current_option) {
            Serial.print(" +");
        } else if(current_menu->selected_option == i) {
            Serial.print(" >");
        } else {
            Serial.print("  ");
        }
        Serial.print("[");
        Serial.print(current_menu->options[i].label);
        Serial.print("]");
    }
    Serial.println();
}

void menu_back() {
    menu_manager.collapse();
    draw_menu(menu_manager.current_menu_, menu_manager.current_option_);
}

void handle_waiting() {
    switch(menu_manager.current_option_) {
        case -1:
            // clicking does whatever the cursor is on in the menu
            // we'll handle that action on the next loop
            if (encoder.click_) {
                menu_manager.expand();
                draw_menu(menu_manager.current_menu_, menu_manager.current_option_);
            }
            if (encoder.turn_ != 0) {
                if(encoder.turn_ < 0) {
                    menu_manager.select_down();
                    draw_menu(menu_manager.current_menu_, menu_manager.current_option_);
                } else if(encoder.turn_ > 0) {
                    menu_manager.select_up();
                    draw_menu(menu_manager.current_menu_, menu_manager.current_option_);
                }
            }
            break;
        case MOPT_BACK:
            // need to back out of being in BACK and back out of parent menu
            menu_manager.collapse();
            menu_back();
            break;
        case MOPT_ANALYZE:
            Serial.println("Analyzing...");
            analyze(centerFq);
            menu_back();
            break;
        case MOPT_FQCENTER:
            // clicking backs out of this option
            if (encoder.click_) {
                menu_back();
            }
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
            break;
        case MOPT_FQWINDOW:
            // clicking backs out of this option
            if (encoder.click_) {
                menu_back();
            }
            // rotating changes window size
            if (encoder.turn_ != 0) {
                if(encoder.turn_ < 0) {
                    rangeFq--;
                } else if(encoder.turn_ > 0) {
                    rangeFq++;
                }
                Serial.print("Frequency Range: ");
                Serial.println(rangeFq);
            }
            break;
        case MOPT_FQSTEPS:
            // clicking backs out of this option
            if (encoder.click_) {
                menu_back();
            }
            // rotating changes window size
            if (encoder.turn_ != 0) {
                if(encoder.turn_ < 0) {
                    dotsNumber--;
                } else if(encoder.turn_ > 0) {
                    dotsNumber++;
                }
                Serial.print("Steps: ");
                Serial.println(dotsNumber);
            }
            break;
        case MOPT_CALIBRATE:
            calibration_state = calibration_step(calibration_state);
            if (calibration_state == CAL_END) {
                menu_back();
                calibration_state = CAL_START;
            }
            break;
    }
}


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

    draw_menu(menu_manager.current_menu_, menu_manager.current_option_);
}

void loop() {
    encoder.update();
    handle_waiting();
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

    int32_t startFq = centerFq - (rangeFq/2);
    int32_t endFq = centerFq + (rangeFq/2);
    int32_t stepFq = (endFq - startFq)/dotsNumber;

    for(int i = 0; i <= dotsNumber; ++i)
    {
        analyze_frequency(startFq + (stepFq*i));
    }
    Serial.print("------------------------\r\n");
}

int calibration_step(int calibration_state) {
    switch(calibration_state) {
        case CAL_START:
            Serial.println("connect short and press knob");
            return CAL_S;
            break;
        case CAL_S:
            if (encoder.click_) {
                Serial.println(analyzer.calibrate_short(centerFq, Z0));
                Serial.println("connect open and press knob");
                return CAL_O;
            }
            break;
        case CAL_O:
            if (encoder.click_) {
                Serial.println(analyzer.calibrate_open(centerFq, Z0));
                Serial.println("connect load and press knob");
                return CAL_L;
            }
            break;
        case CAL_L:
            if (encoder.click_) {
                Serial.println(analyzer.calibrate_load(centerFq, Z0));
                Serial.println("done calibrating.");
                return CAL_END;
            }
            break;
    }
    return calibration_state;
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
