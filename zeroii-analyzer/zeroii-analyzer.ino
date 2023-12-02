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
#define MAX_STEPS 128

#define ZERO_II_RST 13
#define ZERO_I2C_ADDRESS 0x5B
#define CLK 12
#define DT 11
#define SW 10

#define Z0 50

Analyzer analyzer(Z0);
// holds the most recent set of analysis results, initialize to zero length
// array so we can realloc it below
size_t swr_i = 0;
size_t analysis_results_len = 0;
AnalysisPoint analysis_results[MAX_STEPS];
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
#define MOPT_SWR 10

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
    MenuOption("SWR graph", MOPT_SWR, NULL),
};
Menu root_menu(NULL, root_menu_options, sizeof(root_menu_options)/sizeof(root_menu_options[0]));

MenuManager menu_manager(&root_menu);

uint32_t startFq = 28300000;
uint32_t endFq = 28500000;
uint16_t dotsNumber = 100;

enum CAL_STEP { CAL_START, CAL_S, CAL_O, CAL_L, CAL_END };

class Calibrator {
    public:
    Calibrator(Analyzer* analyzer) {
        analyzer_ = analyzer;
    }
    void initialize(uint32_t target_fq) {
        calibration_state_ = CAL_START;
        target_fq_ = target_fq;
    }
    uint8_t calibration_step() {
        switch(calibration_state_) {
            case CAL_START:
                tft.setTextSize(3);
                tft.fillRect(0, 6*2*8, tft.width(), 2*8*3, BLACK);
                tft.setCursor(0, 7*2*8);
                tft.println("connect short and press knob");
                calibration_state_ = CAL_S;
                break;
            case CAL_S:
                if (click) {
                    tft.setTextSize(3);
                    tft.fillRect(0, 6*2*8, tft.width(), 2*8*3, BLACK);
                    tft.setCursor(0, 6*2*8);
                    tft.println(analyzer_->calibrate_short(target_fq_, Z0));
                    tft.println("connect open and press knob");
                    calibration_state_ = CAL_O;
                }
                break;
            case CAL_O:
                if (click) {
                    tft.setTextSize(3);
                    tft.fillRect(0, 6*2*8, tft.width(), 2*8*3, BLACK);
                    tft.setCursor(0, 6*2*8);
                    tft.println(analyzer_->calibrate_open(target_fq_, Z0));
                    tft.println("connect load and press knob");
                    calibration_state_ = CAL_L;
                }
                break;
            case CAL_L:
                if (click) {
                    tft.setTextSize(3);
                    tft.fillRect(0, 6*2*8, tft.width(), 2*8*3, BLACK);
                    tft.setCursor(0, 6*2*8);
                    tft.println(analyzer_->calibrate_load(target_fq_, Z0));
                    tft.println("done calibrating.");
                    calibration_state_ = CAL_END;
                }
                break;
        }
        return calibration_state_;
    }

    private:
    Analyzer* analyzer_;
    uint8_t calibration_state_;
    uint32_t target_fq_;
};

Calibrator calibrator(&analyzer);

void draw_title() {
    tft.fillRect(0, 0, tft.width(), 8*TITLE_TEXT_SIZE, BLACK);
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
    leave_option(menu_manager.current_option_);
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

String frequency_parts_formatter(const uint32_t fq) {
    uint16_t ghz_part, mhz_part, khz_part, hz_part;
    char buf[1+3*4+3+1];
    ghz_part = fq / 1000 / 1000 / 1000;
    mhz_part = fq / 1000 / 1000 % 1000ul;
    khz_part = fq / 1000 % 1000ul;
    hz_part  = fq % 1000ul;

    snprintf(buf, sizeof(buf), "%d.%03d.%03d.%03d Hz", ghz_part, mhz_part, khz_part, hz_part);
    return String(buf);
}

enum FQ_SETTING_STATE { FQ_SETTING_START, FQ_SETTING_GHZ, FQ_SETTING_MHZ, FQ_SETTING_KHZ, FQ_SETTING_HZ, FQ_SETTING_END };

class FqSetter {
    public:
        void initialize(const uint32_t fq) {
            fq_state_ = FQ_SETTING_START;
            fq_ = fq;
        }

        String frequency_parts_indicator() const {
            switch(fq_state_) {
                case FQ_SETTING_GHZ: return String("    ^");
                case FQ_SETTING_MHZ: return String("      ^^^");
                case FQ_SETTING_KHZ: return String("          ^^^");
                case FQ_SETTING_HZ:  return String("              ^^^");
            }
        }

        void draw_fq_setting(const String label) const {
            tft.setTextSize(3);
            tft.fillRect(0, 6*2*8, tft.width(), 2*8*3, BLACK);
            tft.setCursor(0, 6*2*8);
            tft.print("    ");
            tft.println(frequency_parts_formatter(fq_));
            tft.println(frequency_parts_indicator());
        }

        uint32_t set_fq_value(const uint32_t min_fq, const uint32_t max_fq, const String label) {
            // clicking advances through fields in the fq (GHz, MHz, ...) or sets the value
            if (click) {
                if (fq_state_ == FQ_SETTING_HZ) {
                    tft.fillScreen(BLACK);
                    draw_title();
                    menu_back();
                    fq_state_ = FQ_SETTING_END;
                    return fq_;
                } else {
                    fq_state_++;
                    draw_fq_setting(label);
                    return fq_;
                }
            } else if (fq_state_ == FQ_SETTING_START) {
                fq_state_ = FQ_SETTING_GHZ;
                tft.fillRect(0, 5*2*8, tft.width(), 3*8*3, BLACK);
                tft.setCursor(0, 5*2*8);
                tft.print(label);
                tft.println(":");
                draw_fq_setting(label);
                return fq_;
            } else if (turn != 0) {
                // rotating changes value
                int32_t inc = 1ul;
                switch(fq_state_) {
                    case FQ_SETTING_GHZ: inc = 1ul * 1000 * 1000 * 1000; break;
                    case FQ_SETTING_MHZ: inc = 1ul * 1000 * 1000; break;
                    case FQ_SETTING_KHZ: inc = 1ul * 1000; break;
                }
                if (fq_state_ != FQ_SETTING_GHZ) {
                    // inc scales with how fast you're turning it
                    // inc is direction * 2 ^ speed / 10
                    inc = (uint32_t(1) << (uint32_t(encoder.speed()/2))) * inc;
                } // else just inc by 1GHz to avoid overflow
                fq_ = constrain(fq_ + turn * inc, min_fq, max_fq);
                draw_fq_setting(label);
                return fq_;
            } else {
                return fq_;
            }
        }

        uint32_t fq() const { return fq_; }
    private:
        uint8_t fq_state_;
        uint32_t fq_;
};

FqSetter fq_setter;

void enter_option(int32_t option_id) {
    switch(option_id) {
        case MOPT_FQCENTER: {
            int32_t centerFq = startFq + (endFq-startFq)/2;
            fq_setter.initialize(centerFq);
            break;
        }
        case MOPT_FQWINDOW: {
            int32_t rangeFq = endFq - startFq;
            fq_setter.initialize(rangeFq);
            break;
        }
        case MOPT_FQSTART: fq_setter.initialize(startFq); break;
        case MOPT_FQEND: fq_setter.initialize(endFq); break;
        case MOPT_CALIBRATE: calibrator.initialize((endFq-startFq)/2); break;
        case MOPT_SWR:
            swr_i = 0;
            graph_swr(analysis_results, analysis_results_len);
            draw_swr_pointer(analysis_results, analysis_results_len, swr_i, swr_i);
            draw_swr_title(analysis_results, analysis_results_len, swr_i);
            break;
    }
}

void leave_option(int32_t option_id) {
    switch(option_id) {
        case MOPT_FQCENTER:
            Serial.println(String("setting center fq to: ") + fq_setter.fq());
            // move [startFq, endFq] so it's centered on desired value
            startFq = constrain(fq_setter.fq() - (endFq - startFq)/2, MIN_FQ, MAX_FQ);
            endFq = constrain(fq_setter.fq() + (endFq - startFq)/2, MIN_FQ, MAX_FQ);
            draw_title();
            break;
        case MOPT_FQWINDOW: {
            Serial.println(String("setting window fq to: ") + fq_setter.fq());
            // narrow/expand [startFq, endFq] remaining centered
            int32_t cntFq = startFq + (endFq - startFq)/2;
            startFq = constrain(cntFq - fq_setter.fq()/2, MIN_FQ, MAX_FQ);
            endFq = constrain(cntFq + fq_setter.fq()/2, MIN_FQ, MAX_FQ);
            draw_title();
            break;
        }
        case MOPT_FQSTART:
            Serial.println(String("setting start fq to: ") + fq_setter.fq());
            startFq = fq_setter.fq();
            endFq = constrain(endFq, startFq+1, MAX_FQ);
            draw_title();
            break;
        case MOPT_FQEND:
            Serial.println(String("setting end fq to: ") + fq_setter.fq());
            endFq = fq_setter.fq();
            startFq = constrain(startFq, MIN_FQ, endFq-1);
            draw_title();
            break;
        case MOPT_SWR:
            tft.fillScreen(BLACK);
            draw_title();
            break;
    }
}

void choose_option() {
    clear_menu(menu_manager.current_menu_);
    menu_manager.expand();
    draw_menu(menu_manager.current_menu_, menu_manager.current_option_);
    enter_option(menu_manager.current_option_);
}

void handle_option() {
    switch(menu_manager.current_option_) {
        case -1:
            // clicking does whatever the cursor is on in the menu
            // we'll handle that action on the next loop
            if (click) {
                choose_option();
            } else if (turn != 0) {
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
            analysis_results_len = dotsNumber;
            analyze(startFq, endFq, dotsNumber, analysis_results);
            menu_back();
            menu_manager.select_option(MOPT_SWR);
            choose_option();
            break;
        case MOPT_SWR:
            if (click) {
                menu_back();
            } else if (turn != 0) {
                // move the "pointer" on the swr graph
                size_t old_swr_i = swr_i;

                int32_t inc = (uint32_t(1) << (uint32_t(encoder.speed()/2)));
                swr_i = constrain(swr_i+turn*inc, 0, analysis_results_len);
                draw_swr_pointer(analysis_results, analysis_results_len, swr_i, old_swr_i);
                draw_swr_title(analysis_results, analysis_results_len, swr_i);
            }
            break;
        case MOPT_FQCENTER:
            fq_setter.set_fq_value(MIN_FQ, MAX_FQ, "Center Frequency");
            break;
        case MOPT_FQWINDOW:
            fq_setter.set_fq_value(0, MAX_FQ/2, "Frequency Range");
            break;
        case MOPT_FQSTART:
            fq_setter.set_fq_value(MIN_FQ, MAX_FQ, "Start Frequency");
            break;
        case MOPT_FQEND:
            fq_setter.set_fq_value(MIN_FQ, MAX_FQ, "End Frequency");
            break;
        case MOPT_FQSTEPS:
            dotsNumber = set_user_value(dotsNumber, 1, 128, "Steps");
            break;
        case MOPT_CALIBRATE: {
            uint8_t calibration_state = calibrator.calibration_step();
            if (calibration_state == CAL_END) {
                menu_back();
            }
            break;
        }
        default:
            break;
    }
}

void setup_failed() {
    int led_state = 0;
    while(1) {
        digitalWrite(LED_BUILTIN, led_state);
        delay(1000);
        led_state = !led_state;
    }
}

void setup() {
    Serial.begin(38400);
    Serial.flush();
    tft.println("Initializing...");
    digitalWrite(LED_BUILTIN, 0);

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
    pinMode(ZERO_II_RST, OUTPUT);
    digitalWrite(ZERO_II_RST, LOW);
    delay(50);
    digitalWrite(ZERO_II_RST, HIGH);
    if(!analyzer.zeroii_.startZeroII()) {
        Serial.println("failed to start zeroii");
        tft.println("Failed to start ZeroII. Aborting.");
        setup_failed();
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
    handle_option();
    // TODO: if we're in a measurement, allow cancelling it by clicking
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

void analyze(uint32_t startFq, uint32_t endFq, uint16_t dotsNumber, AnalysisPoint* results) {
    uint32_t fq = startFq;
    uint32_t stepFq = (endFq - startFq)/dotsNumber;

    Serial.println(String("analyzing startFq ")+startFq+" endFq "+endFq+" dotsNumber "+dotsNumber);

    for(size_t i = 0; i <= dotsNumber; ++i, fq+=stepFq)
    {
        Serial.println(String("analyzing fq ")+fq);
        Complex res = analyzer.uncalibrated_measure(fq);
        Serial.println("putting into results array");
        Serial.flush();
        results[i] = AnalysisPoint(fq, res);
    }
    Serial.println("analysis complete");
}

void translate_to_screen(float x_in, float y_in, float x_min, float x_max, float y_min, float y_max, int16_t x_screen, int16_t y_screen, int16_t width, int16_t height, int* xy) {
    float x_range = x_max - x_min;
    float y_range = y_max - y_min;
    xy[0] = (x_in - x_min) / x_range * width + x_screen;
    xy[1] = (y_in - y_min) / y_range * height + y_screen;

    Serial.println(String(x_in)+" -> "+xy[0]+" "+y_in+" -> "+xy[1]);
    Serial.flush();
}

void graph_swr(AnalysisPoint* results, size_t results_len) {
    Serial.println(String("graphing swr plot with ")+results_len+" points");
    Serial.flush();
    // area is just below the title
    tft.fillRect(0, 5*2*8, tft.width(), 3*8*3, BLACK);
    tft.fillRect(0, 8*TITLE_TEXT_SIZE, tft.width(), tft.height()-8*TITLE_TEXT_SIZE, BLACK);

    // draw axes
    // x ranges from start fq to end fq
    // y ranges from 1 to 5
    int16_t x_screen = 8*2;
    int16_t y_screen = 8*TITLE_TEXT_SIZE*2;
    int16_t width = tft.width()-x_screen;
    int16_t height = tft.height()-y_screen-8*2;
    tft.drawFastHLine(x_screen, y_screen+height, width, WHITE);
    tft.drawFastVLine(x_screen, y_screen, height, WHITE);

    int xy_cutoff[2];
    translate_to_screen(0, 3, startFq, endFq, 5, 1, x_screen, y_screen, width, height, xy_cutoff);
    tft.drawFastHLine(x_screen, xy_cutoff[1], width, RED);
    translate_to_screen(0, 1.5, startFq, endFq, 5, 1, x_screen, y_screen, width, height, xy_cutoff);
    tft.drawFastHLine(x_screen, xy_cutoff[1], width, MAGENTA);
    // draw all the analysis points
    if (results_len == 0) {
        Serial.println("no results to plot");
        return;
    } else if (results_len == 1) {
        int xy[2];
        translate_to_screen(results[0].fq, compute_swr(analyzer.calibrated_gamma(results[0].uncal_gamma)), startFq, endFq, 5, 1, x_screen, y_screen, width, height, xy);
        tft.fillCircle(xy[0], xy[1], 3, YELLOW);
    } else {
        for (size_t i=0; i<results_len-1; i++) {
            float swr = compute_swr(analyzer.calibrated_gamma(results[i].uncal_gamma));
            int xy_start[2];
            translate_to_screen(results[i].fq, swr, startFq, endFq, 5, 1, x_screen, y_screen, width, height, xy_start);
            int xy_end[2];
            translate_to_screen(results[i+1].fq, compute_swr(analyzer.calibrated_gamma(results[i+1].uncal_gamma)), startFq, endFq, 5, 1, x_screen, y_screen, width, height, xy_end);
            Serial.println(String("drawing line ")+xy_start[0]+","+xy_start[1]+" to "+xy_end[0]+","+xy_end[1]);
            tft.drawLine(xy_start[0], xy_start[1], xy_end[0], xy_end[1], YELLOW);
        }
    }
}

void draw_swr_pointer(AnalysisPoint* results, size_t analysis_results_len, size_t swr_i, size_t old_swr_i) {
    // x ranges from start fq to end fq
    // y ranges from 1 to 5
    int16_t x_screen = 8*2;
    int16_t y_screen = 8*TITLE_TEXT_SIZE*2;
    int16_t width = tft.width()-x_screen;
    int16_t height = tft.height()-y_screen-8*2;

    if (analysis_results_len == 0) {
        return;
    }
    int16_t pointer_width = 8;
    int16_t pointer_height = 8;

    //first clear the old swr pointer
    int xy_pointer[2];
    translate_to_screen(results[old_swr_i].fq, compute_swr(analyzer.calibrated_gamma(results[old_swr_i].uncal_gamma)), startFq, endFq, 5, 1, x_screen, y_screen, width, height, xy_pointer);
    tft.fillTriangle(xy_pointer[0], xy_pointer[1], xy_pointer[0]-pointer_width/2, xy_pointer[1]+pointer_height, xy_pointer[0]+pointer_width/2, xy_pointer[1]+pointer_height, BLACK);

    //draw the "pointer"
    translate_to_screen(results[swr_i].fq, compute_swr(analyzer.calibrated_gamma(results[swr_i].uncal_gamma)), startFq, endFq, 5, 1, x_screen, y_screen, width, height, xy_pointer);
    tft.drawTriangle(xy_pointer[0], xy_pointer[1], xy_pointer[0]-pointer_width/2, xy_pointer[1]+pointer_height, xy_pointer[0]+pointer_width/2, xy_pointer[1]+pointer_height, GREEN);
}

void draw_swr_title(AnalysisPoint* results, size_t results_len, size_t swr_i) {
    if (analysis_results_len == 0) {
        tft.println("No SWR results");
        return;
    }

    size_t min_swr_i = 0;
    float min_swr = compute_swr(analyzer.calibrated_gamma(results[results_len-1].uncal_gamma));
    for (size_t i=0; i<results_len; i++) {
        float swr = compute_swr(analyzer.calibrated_gamma(results[i].uncal_gamma));
        if (swr < min_swr) {
            min_swr = swr;
            min_swr_i = i;
        }
    }
    //draw the title
    tft.fillRect(0, 0, tft.width(), 8*TITLE_TEXT_SIZE*2, BLACK);
    tft.setCursor(0,0);
    tft.setTextSize(TITLE_TEXT_SIZE);

    tft.println(String("Min SWR: ")+compute_swr(analyzer.calibrated_gamma(results[min_swr_i].uncal_gamma))+" "+frequency_formatter(results[min_swr_i].fq));
    tft.println(String("Sel SWR: ")+compute_swr(analyzer.calibrated_gamma(results[swr_i].uncal_gamma))+" "+frequency_formatter(results[swr_i].fq));
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
