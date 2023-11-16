#include "RigExpertZeroII_I2C.h"
#include "Complex.h"
#include "MD_REncoder.h"

#include <Adafruit_GFX.h>    // Core graphics library
#include <SPI.h>
#include "Adafruit_TFTLCD.h"
#include "TouchScreen.h"

#include "analyzer.h"
#include "menu_manager.h"
#include "button.h"

// These are the four touchscreen analog pins
#define YP A2  // must be an analog pin, use "An" notation!
#define XM A3  // must be an analog pin, use "An" notation!
#define YM 7   // can be a digital pin
#define XP 8   // can be a digital pin

// This is calibration data for the raw touch data to the screen coordinates
#define TS_MINX 110
#define TS_MINY 80
#define TS_MAXX 900
#define TS_MAXY 940

#define MINPRESSURE 10
#define MAXPRESSURE 1000

 // The control pins for the LCD can be assigned to any digital or
// analog pins...but we'll use the analog pins as this allows us to
// double up the pins with the touch screen (see the TFT paint example).
#define LCD_CS A3 // Chip Select goes to Analog 3
#define LCD_CD A2 // Command/Data goes to Analog 2
#define LCD_WR A1 // LCD Write goes to Analog 1
#define LCD_RD A0 // LCD Read goes to Analog 0
#define LCD_RESET -1

#define TFT_ROTATION 3
#define	BLACK   0x0000
#define	BLUE    0x001F
#define	RED     0xF800
#define	GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

//Adafruit_HX8357 tft = Adafruit_HX8357(TFT_CS, TFT_DC, TFT_RST);
Adafruit_TFTLCD tft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET);

// Some display configs
#define TITLE_TEXT_SIZE 2
#define MENU_TEXT_SIZE 2
#define MENU_ORIG_X 0
#define MENU_ORIG_Y TITLE_TEXT_SIZE*8*2

// For better pressure precision, we need to know the resistance
// between X+ and X- Use any multimeter to read it
// For the one we're using, its 300 ohms across the X plate
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

//100kHz
#define MIN_FQ 100000
//1GHz
#define MAX_FQ 1000000000

#define ZERO_I2C_ADDRESS 0x5B
#define CLK 12
#define DT 11
#define SW 10

#define Z0 50

Analyzer analyzer(Z0);
MD_REncoder encoder(CLK, DT);
Button button(SW);

int8_t turn = 0;
bool click = false;

#define MOPT_ANALYZE 1
#define MOPT_FQ 2
#define MOPT_CALIBRATE 3
#define MOPT_FQCENTER 4
#define MOPT_FQWINDOW 5
#define MOPT_FQSTART 6
#define MOPT_FQEND 7
#define MOPT_FQSTEPS 8
#define MOPT_BACK 9

MenuOption fq_menu_options[] = {
    MenuOption("Fq Start", MOPT_FQSTART, NULL),
    MenuOption("Fq End", MOPT_FQEND, NULL),
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

uint32_t startFq = 28300000;
uint32_t endFq = 28500000;
uint16_t dotsNumber = 100;

#define CAL_START 0
#define CAL_S 1
#define CAL_O 2
#define CAL_L 3
#define CAL_END 4
uint8_t calibration_state = CAL_START;

/*void draw_menu(Menu* current_menu, int current_option) {
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
}*/

void draw_title() {
    tft.setCursor(0,0);
    tft.setTextSize(TITLE_TEXT_SIZE);

    tft.print(frequency_formatter(startFq) + " to " + frequency_formatter(endFq));
    tft.print(" Steps: ");
    tft.print(dotsNumber);
}

void clear_menu(Menu* current_menu) {
    int16_t w = 1;
    int16_t h = 8*current_menu->option_count*MENU_TEXT_SIZE;
    for(int i=0; i<current_menu->option_count; i++) {
        if(current_menu->options[i].label.length()+1 > w) {
            w = current_menu->options[i].label.length()+1;
        }
    }
    w = 6*MENU_TEXT_SIZE*w;
    tft.fillRect(MENU_ORIG_X, MENU_ORIG_Y, w, h, BLACK);
}

// draws menu on the tft
void draw_menu(Menu* current_menu, int current_option, bool fresh=true) {
    if (fresh) {
        clear_menu(current_menu);
    } else {
        // just blank the cursor area
        tft.fillRect(MENU_ORIG_X, MENU_ORIG_Y, 6*MENU_TEXT_SIZE, 8*current_menu->option_count*MENU_TEXT_SIZE, BLACK);
    }
    tft.setCursor(MENU_ORIG_X, MENU_ORIG_Y);
    tft.setTextSize(MENU_TEXT_SIZE);
    for(int i=0; i<current_menu->option_count; i++) {
        // either:
        // this is the current option
        // this is the selected option
        // this is just an option
        if (current_option >= 0 && current_menu->options[i].option_id == current_option) {
            tft.print("+");
        } else if(current_menu->selected_option == i) {
            tft.print(">");
        } else {
            tft.print(" ");
        }
        tft.println(current_menu->options[i].label);
    }
}


void menu_back() {
    clear_menu(menu_manager.current_menu_);
    menu_manager.collapse();
    draw_menu(menu_manager.current_menu_, menu_manager.current_option_);
}

typedef String (*int_formatter) (const int32_t);
String decimal_int_formatter(const int32_t v) {
    return String(v);
}

int32_t set_user_value(int32_t current_value, int32_t min_value, int32_t max_value, String label, int32_t multiplier=1, int_formatter formatter=&decimal_int_formatter) {
    // clicking backs out of this option
    if (click) {
        tft.fillScreen(BLACK);
        draw_title();
        menu_back();
        return current_value;
    }
    // rotating changes value
    if (turn != 0) {
        // inc scales with how fast you're turning it
        // inc is direction * 2 ^ speed / 10
        int32_t inc = turn * (uint32_t(1) << (uint32_t(encoder.speed()/2))) * multiplier;
        int32_t updated_value = constrain(current_value + inc, min_value, max_value);
        tft.setTextSize(3);
        tft.fillRect(0, 5*2*8, tft.width(), 2*8*3, BLACK);
        tft.setCursor(0, 5*2*8);
        tft.print(label);
        tft.println(":");
        tft.print("    ");
        tft.println(formatter(updated_value));
        return updated_value;
    }
    return current_value;
}

uint16_t frequency_step(uint32_t fq) {
    if (fq > 1 * 1000 * 1000 * 1000) {
        return 1 * 1000 * 1000;
    } else if (fq > 1 * 1000 * 1000) {
        return 1 * 1000;
    } else if (fq > 1 * 1000) {
        return 1;
    } else {
        return 1;
    }
}

String frequency_formatter(const int32_t fq) {
    int int_part, dec_part;
    char buf[3+1+3+3+1];
    if (fq > 1ul * 1000ul * 1000ul * 1000ul) {
        int_part = fq / 1000ul / 1000ul / 1000ul;
        dec_part = fq / 1000ul / 1000ul % 1000ul;
        snprintf(buf, sizeof(buf), "%d.%03dGHz", int_part, dec_part);
        return String(buf);
    } else if (fq > 1ul * 1000ul * 1000ul) {
        int_part = fq / 1000ul / 1000ul;
        dec_part = fq / 1000ul % 1000ul;
        snprintf(buf, sizeof(buf), "%d.%03dMHz", int_part, dec_part);
        return String(buf);
    } else if (fq > 1ul * 1000ul) {
        int_part = fq / 1000ul;
        dec_part = fq % 1000ul;
        snprintf(buf, sizeof(buf), "%d.%03dkHz", int_part, dec_part);
        return String(buf);
    } else {
        int_part = fq;
        dec_part = 0;
        snprintf(buf, sizeof(buf), "%d.%03dHz", int_part, dec_part);
        return String(buf);
    }
}

void handle_waiting() {
    switch(menu_manager.current_option_) {
        case -1:
            // clicking does whatever the cursor is on in the menu
            // we'll handle that action on the next loop
            if (click) {
                clear_menu(menu_manager.current_menu_);
                menu_manager.expand();
                draw_menu(menu_manager.current_menu_, menu_manager.current_option_);
            }
            if (turn != 0) {
                if(turn < 0) {
                    menu_manager.select_down();
                    draw_menu(menu_manager.current_menu_, menu_manager.current_option_, false);
                } else if(turn > 0) {
                    menu_manager.select_up();
                    draw_menu(menu_manager.current_menu_, menu_manager.current_option_, false);
                }
            }
            break;
        case MOPT_BACK:
            // need to back out of being in BACK and back out of parent menu
            clear_menu(menu_manager.current_menu_);
            menu_manager.collapse();
            menu_back();
            break;
        case MOPT_ANALYZE:
            Serial.println("Analyzing...");
            analyze(startFq, endFq);
            menu_back();
            break;
        case MOPT_FQCENTER: {
            // move [startFq, endFq] so it's centered on desired value
            int32_t centerFq = startFq + (endFq-startFq)/2;
            centerFq = set_user_value(centerFq, MIN_FQ, MAX_FQ, "Center Frequency", frequency_step(centerFq), &frequency_formatter);
            startFq = constrain(centerFq - (endFq - startFq)/2, MIN_FQ, MAX_FQ);
            endFq = constrain(centerFq + (endFq - startFq)/2, MIN_FQ, MAX_FQ);
            break;
        }
        case MOPT_FQWINDOW: {
            // narrow/expand [startFq, endFq] remaining centered
            int32_t rangeFq = endFq - startFq;
            rangeFq = set_user_value(rangeFq, 0, MAX_FQ/2, "Frequency Range", frequency_step(rangeFq), &frequency_formatter);
            int32_t cntFq = startFq + (endFq - startFq)/2;
            startFq = constrain(cntFq - rangeFq/2, MIN_FQ, MAX_FQ);
            endFq = constrain(cntFq + rangeFq/2, MIN_FQ, MAX_FQ);
            break;
        }
        case MOPT_FQSTART:
            startFq = set_user_value(startFq, MIN_FQ, MAX_FQ, "Start Frequency", frequency_step(startFq), &frequency_formatter);
            endFq = constrain(endFq, startFq+frequency_step(startFq), MAX_FQ);
            break;
        case MOPT_FQEND:
            endFq = set_user_value(endFq, MIN_FQ, MAX_FQ, "End Frequency", frequency_step(endFq), &frequency_formatter);
            startFq = constrain(startFq, MIN_FQ, endFq-frequency_step(endFq));
            break;
        case MOPT_FQSTEPS:
            dotsNumber = set_user_value(dotsNumber, 1, 128, "Steps");
            break;
        case MOPT_CALIBRATE:
            calibration_state = calibration_step(calibration_state);
            if (calibration_state == CAL_END) {
                menu_back();
                calibration_state = CAL_START;
            }
            break;
        default:
            break;
    }
}


void setup() {
    Serial.begin(38400);
    Serial.flush();
    tft.println("Initializing...");

    Serial.println("starting TFT...");
    tft.begin(tft.readID());
    tft.fillScreen(BLACK);
    tft.setRotation(TFT_ROTATION);
    tft.setCursor(0, 0);
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    Serial.println("TFT started.");

    //Serial.println("resetting ZEROII");
    //pinMode(ZEROII_Reset_Pin, OUTPUT);
    //digitalWrite(ZEROII_Reset_Pin, LOW);
    //delay(50);
    //digitalWrite(ZEROII_Reset_Pin, HIGH);

    Serial.println("starting ZEROII...");
    if(!analyzer.zeroii_.startZeroII()) {
        Serial.println("failed to start zeroii");
        tft.println("Failed to start ZeroII. Aborting.");
        return;
    }
    Serial.println("ZEROII started.");

    String str = "Version: ";
    Serial.println(str + analyzer.zeroii_.getMajorVersion() + "." + analyzer.zeroii_.getMinorVersion() +
            ", HW Revision: " + analyzer.zeroii_.getHwRevision() +
            ", SN: " + analyzer.zeroii_.getSerialNumber()
    );

    Serial.println("Starting encoder/button...");
    encoder.setPeriod(200);
    encoder.begin();
    button.begin();
    Serial.println("Encoder/button started.");


    Serial.println("Initialization complete.");

    tft.fillScreen(BLACK);
    draw_title();
    draw_menu(menu_manager.current_menu_, menu_manager.current_option_);
}

void loop() {
    uint8_t encoder_state = encoder.read();
    if (encoder_state == DIR_CW) {
        turn = -1;
    } else if(encoder_state == DIR_CCW) {
        turn = 1;
    } else {
        turn = 0;
    }
    click = button.read();
    handle_waiting();
    // TODO: if we're in a measurement, allow cancelling it by clicking
    delay(1);
}

void analyze_frequency(uint32_t fq) {
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

void analyze(uint32_t startFq, uint32_t endFq) {

    uint32_t stepFq = (endFq - startFq)/dotsNumber;

    for(uint16_t i = 0; i <= dotsNumber; ++i)
    {
        analyze_frequency(startFq + (stepFq*i));
    }
    Serial.print("------------------------\r\n");
}

uint8_t calibration_step(uint8_t calibration_state) {
    switch(calibration_state) {
        case CAL_START:
            Serial.println("connect short and press knob");
            return CAL_S;
            break;
        case CAL_S:
            if (click) {
                Serial.println(analyzer.calibrate_short((endFq-startFq)/2, Z0));
                Serial.println("connect open and press knob");
                return CAL_O;
            }
            break;
        case CAL_O:
            if (click) {
                Serial.println(analyzer.calibrate_open((endFq-startFq)/2, Z0));
                Serial.println("connect load and press knob");
                return CAL_L;
            }
            break;
        case CAL_L:
            if (click) {
                Serial.println(analyzer.calibrate_load((endFq-startFq)/2, Z0));
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
