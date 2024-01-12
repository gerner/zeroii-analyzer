#include "RigExpertZeroII_I2C.h"
#include "Complex.h"

#include <Adafruit_GFX.h>    // Core graphics library
#include "Adafruit_TFTLCD.h"
#include "TouchScreen.h"

#include <SPI.h>
#include <SdFat.h>
#include <ArduinoJson.h>

#include "RTClib.h"

#include <SerialWombat.h>

#include "analyzer.h"
#include "menu_manager.h"
#include "persistence.h"

#define WAIT_FOR_SERIAL 0

#define VBATT_ALPHA 0.05
#define BATT_SENSE_PIN A3
#define BATT_SENSE_PERIOD 3000
float vbatt = 8.0;
uint32_t last_vbatt = 0;

float measure_vbatt() {
    // experimentally calibrated using a linear transformation
    uint16_t batt_raw = analogRead(BATT_SENSE_PIN);
    float vbatt = batt_raw * 0.0113 - 0.434;
    return vbatt;
}
void init_vbatt() {
    vbatt = measure_vbatt();
    last_vbatt = millis();
}
void update_vbatt() {
    vbatt = ((1.0-VBATT_ALPHA) * vbatt) + ((VBATT_ALPHA) * measure_vbatt());
    last_vbatt = millis();
}

// These are the four touchscreen analog pin
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
#define LCD_CS 1  //A3 // Chip Select goes to Analog 3
#define LCD_CD 0 // Command/Data goes to Analog 2
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
#define GRAY    0xBDF7

//Adafruit_HX8357 tft = Adafruit_HX8357(TFT_CS, TFT_DC, TFT_RST);
Adafruit_TFTLCD tft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET);

// Some display configs
#define TITLE_TEXT_SIZE 2
#define LABEL_TEXT_SIZE 1
#define MENU_TEXT_SIZE 2
#define MENU_ORIG_X 0
#define MENU_ORIG_Y TITLE_TEXT_SIZE*8*2
#define CONFIRM_ORIG_X TITLE_TEXT_SIZE*6
#define CONFIRM_ORIG_Y TITLE_TEXT_SIZE*8*3

#define ERROR_MESSAGE_DWELL_TIME 7000

// For better pressure precision, we need to know the resistance
// between X+ and X- Use any multimeter to read it
// For the one we're using, its 300 ohms across the X plate
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

//TODO: cleanup graph.h so it doesn't have to be included here
#include "graph.h"

//100kHz
#define MIN_FQ 100000
//1GHz
#define MAX_FQ 1000000000
#define MAX_STEPS 128

#define ZERO_I2C_ADDRESS 0x5B
#define CLK A2
#define DT A4
#define SW A5

#define Z0 50

#define PROGRESS_METER_X 8*2*4
#define PROGRESS_METER_Y 8*2*4
#define PROGRESS_METER_WIDTH (tft.width()-PROGRESS_METER_Y*2)

// holds the most recent set of analysis results, initialize to zero length
// array so we can realloc it below
size_t analysis_results_len = 0;
AnalysisPoint analysis_results[MAX_STEPS];

size_t calibration_len = 0;
CalibrationPoint calibration_results[MAX_STEPS];

Analyzer analyzer(Z0, calibration_results);

AnalyzerPersistence persistence;

void initialize_progress_meter(String label) {
    tft.fillRect(PROGRESS_METER_X, PROGRESS_METER_Y, PROGRESS_METER_WIDTH, 8*2*2, BLACK);
    tft.setTextSize(2);
    tft.setCursor(PROGRESS_METER_X, PROGRESS_METER_Y);
    tft.print(label);
    tft.drawRect(PROGRESS_METER_X, PROGRESS_METER_Y+8*2, PROGRESS_METER_WIDTH, 7*2, WHITE);
}

void draw_progress_meter(size_t total, size_t current) {
    tft.fillRect(PROGRESS_METER_X, PROGRESS_METER_Y+8*2, PROGRESS_METER_WIDTH*current/total, 7*2, WHITE);
    tft.fillRect(PROGRESS_METER_X, PROGRESS_METER_Y+8*2*2, PROGRESS_METER_WIDTH, 8*2, BLACK);
    tft.setCursor(PROGRESS_METER_X, PROGRESS_METER_Y+8*2*2);
    tft.print(current);
    tft.print("/");
    tft.print(total);
}

class AnalysisProcessor {
    public:
    void initialize(uint32_t start_fq, uint32_t end_fq, uint16_t steps, AnalysisPoint* results) {
        fq_ = start_fq;
        steps_ = steps;
        step_fq_ = (end_fq - start_fq)/steps;
        results_ = results;
        result_idx_ = 0;

        Serial.println(String("analyzing startFq ")+start_fq+" endFq "+end_fq+" steps "+steps);
        tft.fillScreen(BLACK);
        draw_title();
        initialize_progress_meter("Analyzing...");
    }

    bool analyze() {
        if (result_idx_ >= steps_) {
            return true;
        }

        Serial.println(String("analyzing fq ")+fq_);
        Complex z = analyzer.uncalibrated_measure(fq_);
        Serial.println("putting into results array");
        Serial.flush();
        results_[result_idx_] = AnalysisPoint(fq_, z);
        fq_ += step_fq_;
        result_idx_++;

        // update progress meter
        draw_progress_meter(steps_, result_idx_);

        return false;
    }


    private:
    uint32_t fq_;
    uint32_t step_fq_;
    AnalysisPoint* results_;
    size_t steps_;
    size_t result_idx_;
};

AnalysisProcessor analysis_processor;

RTC_DS3231 rtc;

// Call back for file timestamps.  Only called for file create and sync().
void date_callback(uint16_t* date, uint16_t* time) {
    DateTime now = rtc.now();
    // Return date using FS_DATE macro to format fields.
    *date = FS_DATE(now.year(), now.month(), now.day());

    // Return time using FS_TIME macro to format fields.
    *time = FS_TIME(now.hour(), now.minute(), now.second());
}

SdFs sd;
SerialWombatChip sw;

SerialWombatQuadEnc quad_enc(sw);
SerialWombatDebouncedInput debounced_input(sw);

int32_t turn = 0;
uint16_t last_quad_enc = 32768;
bool click = false;

enum MOPT {
    MOPT_ANALYZE,
    MOPT_FQ,
    MOPT_RESULTS,
    MOPT_SETTINGS,

    MOPT_FQCENTER,
    MOPT_FQWINDOW,
    MOPT_FQSTART,
    MOPT_FQEND,
    MOPT_FQBAND,
    MOPT_FQSTEPS,

    MOPT_SWR,
    MOPT_SMITH,
    MOPT_SAVE_RESULTS,
    MOPT_LOAD_RESULTS,

    MOPT_CALIBRATE,
    MOPT_Z0,
    MOPT_SAVE_SETTINGS,
    MOPT_LOAD_SETTINGS,

    MOPT_BACK,
};

MenuOption fq_menu_options[] = {
    MenuOption("Fq Start", MOPT_FQSTART, NULL),
    MenuOption("Fq End", MOPT_FQEND, NULL),
    MenuOption("Fq Center", MOPT_FQCENTER, NULL),
    MenuOption("Fq Range", MOPT_FQWINDOW, NULL),
    MenuOption("Fq Band", MOPT_FQBAND, NULL),
    MenuOption("Steps", MOPT_FQSTEPS, NULL),
    MenuOption("Back", MOPT_BACK, NULL),
};
Menu fq_menu(NULL, fq_menu_options, sizeof(fq_menu_options)/sizeof(fq_menu_options[0]));

MenuOption results_menu_options[] {
    MenuOption("SWR graph", MOPT_SWR, NULL),
    MenuOption("Smith chart", MOPT_SMITH, NULL),
    MenuOption("Save Results", MOPT_SAVE_RESULTS, NULL),
    MenuOption("Load Results", MOPT_LOAD_RESULTS, NULL),
    MenuOption("Back", MOPT_BACK, NULL),
};
Menu results_menu(NULL, results_menu_options, sizeof(results_menu_options)/sizeof(results_menu_options[0]));

MenuOption settings_menu_options[] = {
    MenuOption("Calibration", MOPT_CALIBRATE, NULL),
    MenuOption("Z0", MOPT_Z0, NULL),
    MenuOption("Save Settings", MOPT_SAVE_SETTINGS, NULL),
    MenuOption("Load Settings", MOPT_LOAD_SETTINGS, NULL),
    MenuOption("Back", MOPT_BACK, NULL),
};
Menu settings_menu(NULL, settings_menu_options, sizeof(settings_menu_options)/sizeof(settings_menu_options[0]));

MenuOption root_menu_options[] = {
    MenuOption("Analyze", MOPT_ANALYZE, NULL),
    MenuOption("Frequencies", MOPT_FQ, &fq_menu),
    MenuOption("Results", MOPT_RESULTS, &results_menu),
    MenuOption("Settings", MOPT_SETTINGS, &settings_menu),
};
Menu root_menu(NULL, root_menu_options, sizeof(root_menu_options)/sizeof(root_menu_options[0]));

MenuManager menu_manager(&root_menu);

uint32_t startFq = 28300000;
uint32_t endFq = 28500000;
uint16_t dotsNumber = 100;


enum CAL_STEP { CAL_START, CAL_S_START, CAL_S, CAL_O_START, CAL_O, CAL_L_START, CAL_L, CAL_END };

class Calibrator {
    public:
    Calibrator(Analyzer* analyzer) {
        analyzer_ = analyzer;
    }
    void initialize(uint32_t start_fq, uint32_t end_fq, uint16_t steps, CalibrationPoint* results) {
        calibration_state_ = CAL_START;
        start_fq_ = start_fq;
        end_fq_ = end_fq;
        fq_ = start_fq;
        steps_ = steps;
        step_fq_ = (end_fq - start_fq)/steps;
        results_ = results;
        result_idx_ = 0;

        Serial.println(String("calibrating startFq ")+start_fq+" endFq "+end_fq+" steps "+steps+" step_fq "+step_fq_);
        tft.fillScreen(BLACK);
        draw_title();
    }
    bool calibration_step() {
        switch(calibration_state_) {
            case CAL_START:
                tft.setTextSize(2);
                tft.fillRect(0, 6*2*8, tft.width(), 2*8*3, BLACK);
                tft.setCursor(0, 7*2*8);
                tft.println("connect short and press knob");
                calibration_state_ = CAL_S_START;
                break;
            // all three "start" cases are the same
            // just increment the state on click
            case CAL_S_START:
            case CAL_O_START:
            case CAL_L_START:
                if (click) {
                    Serial.print("calibration start state ");
                    Serial.println(calibration_state_);
                    calibration_state_++;
                    fq_ = start_fq_;
                    result_idx_ = 0;
                    initialize_progress_meter("Calibrating...");
                }
                break;
            case CAL_S:
                if(fq_ <= end_fq_) {
                    Serial.print("calibrating short fq: ");
                    Serial.println(fq_);
                    results_[result_idx_].cal_short = compute_gamma(analyzer_->uncalibrated_measure(fq_), analyzer_->z0_);
                    results_[result_idx_].fq = fq_;
                    result_idx_++;
                    fq_ += step_fq_;
                    draw_progress_meter(steps_, result_idx_);
                } else {
                    Serial.println("done calibrating short.");
                    tft.setTextSize(2);
                    tft.fillRect(0, 6*2*8, tft.width(), 2*8*3, BLACK);
                    tft.setCursor(0, 6*2*8);
                    tft.println("connect open and press knob");
                    calibration_state_ = CAL_O_START;
                }
                break;
            case CAL_O:
                if(fq_ <= end_fq_) {
                    Serial.print("calibrating open fq: ");
                    Serial.println(fq_);
                    results_[result_idx_].cal_open = compute_gamma(analyzer_->uncalibrated_measure(fq_), analyzer_->z0_);
                    result_idx_++;
                    fq_ += step_fq_;
                    draw_progress_meter(steps_, result_idx_);
                } else {
                    Serial.println("done calibrating open.");
                    tft.setTextSize(2);
                    tft.fillRect(0, 6*2*8, tft.width(), 2*8*3, BLACK);
                    tft.setCursor(0, 6*2*8);
                    tft.println("connect load and press knob");
                    calibration_state_ = CAL_L_START;
                }
                break;
            case CAL_L:
                if(fq_ <= end_fq_) {
                    Serial.print("calibrating load fq: ");
                    Serial.println(fq_);
                    results_[result_idx_].cal_load = compute_gamma(analyzer_->uncalibrated_measure(fq_), analyzer_->z0_);
                    result_idx_++;
                    fq_ += step_fq_;
                    draw_progress_meter(steps_, result_idx_);
                } else {
                    Serial.println("done calibrating load.");
                    tft.setTextSize(2);
                    tft.fillRect(0, 6*2*8, tft.width(), 2*8*3, BLACK);
                    tft.setCursor(0, 6*2*8);
                    tft.println("done calibrating.");
                    calibration_state_ = CAL_END;
                    // we're done!
                    return true;
                }
                break;
        }
        return false;
    }

    private:
    Analyzer* analyzer_;
    uint8_t calibration_state_;

    uint32_t start_fq_;
    uint32_t end_fq_;

    uint32_t fq_;
    uint32_t step_fq_;
    CalibrationPoint* results_;
    size_t steps_;
    size_t result_idx_;
};

Calibrator calibrator(&analyzer);

void draw_title() {
    tft.fillRect(0, 0, tft.width(), 8*TITLE_TEXT_SIZE, BLACK);
    tft.setCursor(0,0);
    tft.setTextSize(TITLE_TEXT_SIZE);

    tft.print(frequency_formatter(startFq) + "-" + frequency_formatter(endFq));
    tft.print(" St:");
    tft.print(dotsNumber);
    tft.print(" Z0:");
    tft.print((uint32_t)analyzer.z0_);

    draw_error();
}

uint32_t last_error_print = 0;
uint32_t last_error_time = 0;
char error_message[128];

void clear_error_display() {
    current_error("");
    tft.fillRect(0, tft.height()-8*TITLE_TEXT_SIZE, tft.width(), 8*TITLE_TEXT_SIZE, BLACK);
}

void current_error(const char* error_msg) {
    strncpy(error_message, error_msg, sizeof(error_message));
    last_error_time = millis();
    draw_error();
}

void draw_error() {
    if (error_message[0] && millis() - last_error_time < ERROR_MESSAGE_DWELL_TIME) {
        tft.fillRect(0, tft.height()-8*TITLE_TEXT_SIZE, tft.width(), 8*TITLE_TEXT_SIZE, BLACK);
        tft.setCursor(0, tft.height()-8*TITLE_TEXT_SIZE);
        tft.setTextSize(TITLE_TEXT_SIZE);
        tft.setTextColor(RED);

        tft.print(error_message);

        tft.setTextColor(WHITE);
    }
}

void draw_vbatt() {
    if (vbatt >= 100 || vbatt < 0) {
        //something weird is going on
        return;
    }
    tft.fillRect(tft.width() - 6*TITLE_TEXT_SIZE*5, 0, 6*TITLE_TEXT_SIZE*5, 8*TITLE_TEXT_SIZE, BLACK);
    tft.setCursor(tft.width() - 6*TITLE_TEXT_SIZE*5, 0);
    tft.setTextSize(TITLE_TEXT_SIZE);
    if (vbatt >= 10) {
        tft.print((int)vbatt);
        tft.print(".");
        tft.print((int)vbatt*10%10);
    } else {
        tft.print(vbatt);
    }
    tft.print("v");
}

void clear_menu(Menu* current_menu, int16_t menu_x=MENU_ORIG_X, int16_t menu_y=MENU_ORIG_Y) {
    int16_t w = 1;
    int16_t h = 8*current_menu->option_count*MENU_TEXT_SIZE;
    for(int i=0; i<current_menu->option_count; i++) {
        if(current_menu->options[i].label.length()+1 > w) {
            w = current_menu->options[i].label.length()+1;
        }
    }
    w = 6*MENU_TEXT_SIZE*w;
    tft.fillRect(menu_x, menu_y, w, h, BLACK);
}

// draws menu on the tft
void draw_menu(Menu* current_menu, int current_option, bool fresh=true, int16_t menu_x=MENU_ORIG_X, int16_t menu_y=MENU_ORIG_Y) {
    if (fresh) {
        clear_menu(current_menu, menu_x, menu_y);
    } else {
        // just blank the cursor area
        tft.fillRect(menu_x, menu_y, 6*MENU_TEXT_SIZE, 8*current_menu->option_count*MENU_TEXT_SIZE, BLACK);
    }
    tft.setTextSize(MENU_TEXT_SIZE);
    for(int i=0; i<current_menu->option_count; i++) {
        tft.setCursor(menu_x, menu_y+i*8*MENU_TEXT_SIZE);
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
        tft.print(current_menu->options[i].label);
    }
}


void menu_back() {
    leave_option(menu_manager.current_option_);
    clear_menu(menu_manager.current_menu_);
    menu_manager.collapse();
    draw_menu(menu_manager.current_menu_, menu_manager.current_option_, true);
    draw_title();
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
        int32_t inc = turn * multiplier;// * (uint32_t(1) << (uint32_t(encoder.speed()/2))) * multiplier;
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
                    //inc = (uint32_t(1) << (uint32_t(encoder.speed()/2))) * inc;
                } //k else just inc by 1GHz to avoid overflow
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

uint32_t band_fqs[][2] = { {135700, 137800}, {472000, 479000}, {1800000, 2000000}, {3500000, 4000000}, {5330500, 5406400}, {7000000, 7300000}, {10100000, 10150000}, {14000000, 14350000}, {18068000, 18168000}, {2100000, 21450000}, {24890000, 24990000}, {28000000, 29700000}, {50000000, 54000000}, {144000000, 148000000}, {219000000, 225000000}, {420000000, 450000000}, {902000000, 928000000}, {100000, 600000000} };
String band_names[] = {"2200m", "630m", "160m", "80m", "60m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m", "VHF", "1.25m", "UHF", "33cm", "Reference RF"};
class BandSetter {
    public:
    void initialize() {
        band_idx_ = 0;
        tft.fillScreen(BLACK);
        draw_title();
        tft.setCursor(0, 5*2*8);
        tft.println("Band:");
        draw_band_setting();
    }
    bool set_band() {
        if (click) {
            return true;
        } else if (turn != 0) {
            band_idx_ = constrain((int32_t)band_idx_+turn, 0, sizeof(band_names)/sizeof(band_names[0])-1);
            draw_band_setting();
        }
        return false;
    }
    void draw_band_setting() {
        tft.setTextSize(3);
        tft.fillRect(0, 6*2*8, tft.width(), 2*8*3, BLACK);
        tft.setCursor(0, 6*2*8);
        tft.print("    ");
        tft.println(band_names[band_idx_]);
    }

    void band(uint32_t* start_fq, uint32_t* end_fq) {
        *start_fq = band_fqs[band_idx_][0];
        *end_fq = band_fqs[band_idx_][1];
    }
    private:
        size_t band_idx_;
};

BandSetter band_setter;

class FileBrowser {
    public:
    FileBrowser() : file_menu_(NULL), file_options_(NULL) {}

    ~FileBrowser() {
        if (file_menu_) {
            delete file_menu_;
            delete [] file_options_;
        }
    }

    bool initialize(FsFile* directory, bool with_new) {
        Serial.println("initializing file browser");
        Serial.flush();

        tft.fillScreen(BLACK);
        draw_title();

        if(!directory->isOpen() || !directory->isDirectory()) {
            return false;
        }
        if (file_menu_) {
            delete file_menu_;
            delete [] file_options_;
        }

        with_new_ = with_new;

        Serial.println("counting files in directory");
        Serial.flush();

        // awkward to iterate through directory once to get count and a second
        // time to actually get the names into the array. not sure how else to
        // do it with an array of MenuOptions (no stl shenanigans)
        size_t file_count = 0;
        directory->rewindDirectory();
        FsFile entry;
        while(entry.openNext(directory, O_RDONLY)) {
            file_count++;
        }

        Serial.println("allocating file options");
        Serial.flush();

        size_t idx;
        if (with_new_) {
            file_count++;
            file_options_ = new MenuOption[file_count];
            file_options_[0].label = String("New File");
            idx = 1;
        } else {
            file_options_ = new MenuOption[file_count];
            idx = 0;
        }

        Serial.println("iterating through directory");
        Serial.flush();

        directory->rewindDirectory();
        while(entry.openNext(directory, O_RDONLY) && idx < file_count) {
            char filename[128];
            entry.getName(filename, sizeof(filename));
            file_options_[idx++].label = String(filename);
        }

        file_menu_ = new Menu(NULL, file_options_, file_count);

        Serial.println("drawing menu");
        Serial.flush();

        draw_menu(file_menu_, -1, true);
        return true;
    }

    bool choose_file() {
        if (click) {
            return true;
        } else if (turn != 0) {
            file_menu_->selected_option = constrain((int32_t)file_menu_->selected_option+turn, 0, file_menu_->option_count);
            draw_menu(file_menu_, -1, false);
            return false;
        } else {
            return false;
        }
    }

    bool is_new() {
        return with_new_ && file_menu_->selected_option == 0;
    }

    void file(char* filename, size_t max_len) {
        file_options_[file_menu_->selected_option].label.toCharArray(filename, max_len);
    }

    private:
    bool with_new_;
    MenuOption* file_options_;
    Menu* file_menu_;
};
FileBrowser file_browser;

bool browse_progress() {
    return file_browser.choose_file();
}

typedef bool (*ProgressFn)(void);

MenuOption confirmation_menu_options[] = {
    MenuOption("Yes", 0, NULL),
    MenuOption("Cancel", 0, NULL),
};
Menu confirmation_menu(NULL, confirmation_menu_options, sizeof(confirmation_menu_options)/sizeof(confirmation_menu_options[0]));

class ConfirmDialog {
    public:
    void initialize(ProgressFn progress_fn) {
        confirmation_menu.selected_option = 0;
        progress_fn_ = progress_fn;
        progress_ = true;
    }

    bool progress() {
        if (progress_) {
            Serial.println("calling inner progress fn");
            if (click) {
                Serial.println("click");
            }
            if(progress_fn_()) {
                Serial.println("inner progress fn returned true, proceeding with confirmation.");
                progress_ = false;
                tft.fillScreen(BLACK);
                tft.setCursor(CONFIRM_ORIG_X-6*TITLE_TEXT_SIZE, CONFIRM_ORIG_Y-8*TITLE_TEXT_SIZE);
                tft.print("Are you sure?");
                draw_title();
                draw_menu(&confirmation_menu, -1, true, CONFIRM_ORIG_X, CONFIRM_ORIG_Y);
            } else {
                Serial.println("inner progress fn returned false");
            }
        } else {
            if(click) {
                return true;
            } else if(turn != 0) {
                confirmation_menu.selected_option = constrain((int32_t)confirmation_menu.selected_option+turn, 0, confirmation_menu.option_count);
                draw_menu(&confirmation_menu, -1, false, CONFIRM_ORIG_X, CONFIRM_ORIG_Y);
            }
        }
        return false;
    }

    bool confirm() {
        return !progress_ && confirmation_menu.selected_option == 0;
    }

    private:
    bool progress_;
    ProgressFn progress_fn_;
};

ConfirmDialog confirm_dialog;

void analyze(uint32_t startFq, uint32_t endFq, uint16_t dotsNumber, AnalysisPoint* results) {
    uint32_t fq = startFq;
    uint32_t stepFq = (endFq - startFq)/dotsNumber;

    Serial.println(String("analyzing startFq ")+startFq+" endFq "+endFq+" dotsNumber "+dotsNumber);

    for(size_t i = 0; i <= dotsNumber; ++i, fq+=stepFq)
    {
        Serial.println(String("analyzing fq ")+fq);
        Complex z = analyzer.uncalibrated_measure(fq);
        Serial.println("putting into results array");
        Serial.flush();
        results[i] = AnalysisPoint(fq, z);
    }
    Serial.println("analysis complete");
}

void enter_option(int32_t option_id) {
    switch(option_id) {
        case MOPT_ANALYZE:
            analysis_results_len = dotsNumber;
            analysis_processor.initialize(startFq, endFq, dotsNumber, analysis_results);
            break;
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
        case MOPT_FQBAND: band_setter.initialize(); break;
        case MOPT_CALIBRATE:
            calibration_len = dotsNumber;
            calibrator.initialize(startFq, endFq, dotsNumber, calibration_results);
            break;
        case MOPT_SWR: {
            swr_i = 0;
            graph_swr(analysis_results, analysis_results_len, &analyzer);
            draw_swr_pointer(analysis_results, analysis_results_len, swr_i, &analyzer);
            draw_swr_title(analysis_results, analysis_results_len, swr_i, &analyzer);
            break;
        }
        case MOPT_SMITH: {
            swr_i = 0;
            graph_smith(analysis_results, analysis_results_len, &analyzer);
            draw_smith_pointer(analysis_results, analysis_results_len, swr_i, &analyzer);
            draw_smith_title(analysis_results, analysis_results_len, swr_i, &analyzer);
            break;
        }
        case MOPT_SAVE_RESULTS:
            file_browser.initialize(&persistence.results_dir_, true);
            confirm_dialog.initialize(&browse_progress);
            break;
        case MOPT_LOAD_RESULTS:
            file_browser.initialize(&persistence.results_dir_, false);
            confirm_dialog.initialize(&browse_progress);
            break;
        case MOPT_SAVE_SETTINGS:
            file_browser.initialize(&persistence.settings_dir_, true);
            confirm_dialog.initialize(&browse_progress);
            break;
        case MOPT_LOAD_SETTINGS:
            file_browser.initialize(&persistence.settings_dir_, false);
            confirm_dialog.initialize(&browse_progress);
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
            tft.fillScreen(BLACK);
            draw_title();
            break;
        case MOPT_FQWINDOW: {
            Serial.println(String("setting window fq to: ") + fq_setter.fq());
            // narrow/expand [startFq, endFq] remaining centered
            int32_t cntFq = startFq + (endFq - startFq)/2;
            startFq = constrain(cntFq - fq_setter.fq()/2, MIN_FQ, MAX_FQ);
            endFq = constrain(cntFq + fq_setter.fq()/2, MIN_FQ, MAX_FQ);
            tft.fillScreen(BLACK);
            draw_title();
            break;
        }
        case MOPT_FQSTART:
            Serial.println(String("setting start fq to: ") + fq_setter.fq());
            startFq = fq_setter.fq();
            endFq = constrain(endFq, startFq+1, MAX_FQ);
            tft.fillScreen(BLACK);
            draw_title();
            break;
        case MOPT_FQEND:
            Serial.println(String("setting end fq to: ") + fq_setter.fq());
            endFq = fq_setter.fq();
            startFq = constrain(startFq, MIN_FQ, endFq-1);
            tft.fillScreen(BLACK);
            draw_title();
            break;
        case MOPT_FQBAND:
            band_setter.band(&startFq, &endFq);
            Serial.println(String("setting start/end to: ") + startFq + "/" + endFq);
            tft.fillScreen(BLACK);
            draw_title();
            break;
        case MOPT_ANALYZE:
            tft.fillScreen(BLACK);
            draw_title();
            break;
        case MOPT_CALIBRATE:
            tft.fillScreen(BLACK);
            draw_title();
            analyzer.calibration_len_ = calibration_len;
            break;
        case MOPT_SAVE_RESULTS:
            if(confirm_dialog.confirm()) {
                if(file_browser.is_new()) {
                    if(!persistence.save_results(analysis_results, analysis_results_len)) {
                        Serial.println("could not save results");
                        current_error("could not save results");
                    }
                } else {
                    char filename[128];
                    file_browser.file(filename, sizeof(filename));
                    if(!persistence.save_results(filename, analysis_results, analysis_results_len)) {
                        Serial.println("could not save results");
                        current_error("could not save results");
                    }
                }
            } else {
                Serial.println("cancelled saving results");
                current_error("cancelled saving results");
            }
            tft.fillScreen(BLACK);
            draw_title();
            break;
        case MOPT_LOAD_RESULTS:
            if(confirm_dialog.confirm()) {
                char filename[128];
                file_browser.file(filename, sizeof(filename));
                if(!persistence.load_results(filename, analysis_results, &analysis_results_len, MAX_STEPS)) {
                    Serial.println("could not load results");
                    current_error("could not load results");
                }
            } else {
                Serial.println("cancelled loading results");
                current_error("cancelled loading results");
            }
            tft.fillScreen(BLACK);
            draw_title();
            break;
        case MOPT_SAVE_SETTINGS:
            if(confirm_dialog.confirm()) {
                if(file_browser.is_new()) {
                    if(!persistence.save_settings(&analyzer)) {
                        Serial.println("could not save settings");
                        current_error("could not save settings");
                    }
                } else {
                    char filename[128];
                    file_browser.file(filename, sizeof(filename));
                    if(!persistence.save_settings(filename, &analyzer)) {
                        Serial.println("could not save settings");
                        current_error("could not save settings");
                    }
                }
            } else {
                Serial.println("cancelled saving settings");
                current_error("cancelled saving settings");
            }
            tft.fillScreen(BLACK);
            draw_title();
            break;
        case MOPT_LOAD_SETTINGS:
            if(confirm_dialog.confirm()) {
                char filename[128];
                file_browser.file(filename, sizeof(filename));
                if(!persistence.load_settings(filename, &analyzer, MAX_STEPS)) {
                    Serial.println("could not load settings");
                    current_error("could not load settings");
                }
            } else {
                Serial.println("cancelled loading settings");
                current_error("cancelled loading settings");
            }
            tft.fillScreen(BLACK);
            draw_title();
            break;
        case MOPT_SWR:
            tft.fillScreen(BLACK);
            draw_title();
            break;
        case MOPT_SMITH:
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
                menu_manager.select_rel(turn);
                draw_menu(menu_manager.current_menu_, menu_manager.current_option_, false);
            }
            break;
        case MOPT_BACK:
            // need to back out of being in BACK and back out of parent menu
            clear_menu(menu_manager.current_menu_);
            menu_manager.collapse();
            menu_back();
            break;
        case MOPT_ANALYZE:
            if (analysis_processor.analyze()) {
                menu_back();
                if(!menu_manager.select_option(MOPT_SWR)) {
                    Serial.println("could not find SWR option");
                } else {
                    choose_option();
                }
            }
            break;
        case MOPT_SWR:
            if (click) {
                menu_back();
            } else if (turn != 0) {
                // move the "pointer" on the swr graph
                swr_i = constrain((int32_t)swr_i+turn, 0, analysis_results_len-1);
                assert(swr_i < analysis_results_len);
                draw_swr_pointer(analysis_results, analysis_results_len, swr_i, &analyzer);
                draw_swr_title(analysis_results, analysis_results_len, swr_i, &analyzer);
            }
            break;
        case MOPT_SMITH:
            if (click) {
                menu_back();
            } else if (turn != 0) {
                // move the "pointer" on the smith chart
                swr_i = constrain((int32_t)swr_i+turn, 0, analysis_results_len-1);
                assert(swr_i < analysis_results_len);
                draw_smith_pointer(analysis_results, analysis_results_len, swr_i, &analyzer);
                draw_smith_title(analysis_results, analysis_results_len, swr_i, &analyzer);
            }
            break;
        case MOPT_SAVE_RESULTS:
        case MOPT_LOAD_RESULTS:
        case MOPT_SAVE_SETTINGS:
        case MOPT_LOAD_SETTINGS:
            if(confirm_dialog.progress()) {
                menu_back();
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
        case MOPT_FQBAND:
            if (band_setter.set_band()) {
                menu_back();
                draw_title();
            }
            break;
        case MOPT_FQSTEPS:
            dotsNumber = set_user_value(dotsNumber, 1, 128, "Steps");
            break;
        case MOPT_CALIBRATE: {
            if(calibrator.calibration_step()) {
                menu_back();
            }
            break;
        }
        case MOPT_Z0:
            analyzer.z0_ = set_user_value(analyzer.z0_, 1, 999, "Z0");
            break;
        default:
            Serial.println(String("don't know what to do with option ")+menu_manager.current_option_);
            menu_back();
            break;
    }
}

int digitalReadOutputPin(uint8_t pin)
{
  uint8_t bit = digitalPinToBitMask(pin);
  uint8_t port = digitalPinToPort(pin);
  //if (port == NOT_A_PIN)
  //  return LOW;

  return (*portOutputRegister(port) & bit) ? HIGH : LOW;
}

#define UNKNOWN_PIN 0xFF

uint8_t getPinMode(uint8_t pin)
{
  uint16_t bit = digitalPinToBitMask(pin);
  uint16_t port = digitalPinToPort(pin);

  // I don't see an option for mega to return this, but whatever...
  //if (NOT_A_PIN == port) return UNKNOWN_PIN;

  // Is there a bit we can check?
  if (0 == bit) return UNKNOWN_PIN;

  // Is there only a single bit set?
  if (bit & bit - 1) return UNKNOWN_PIN;

  volatile uint16_t *reg, *out;
  reg = portModeRegister(port);
  out = portOutputRegister(port);

  if (*reg & bit)
    return OUTPUT;
  else if (*out & bit)
    return INPUT_PULLUP;
  else
    return INPUT;
}

void setPinInput(uint8_t pin)
{
  uint16_t bit = digitalPinToBitMask(pin);
  uint16_t port = digitalPinToPort(pin);
  volatile uint16_t *reg;
  reg = portModeRegister(port);
  *reg &= ~bit;
}

void setPinOutput(uint8_t pin)
{
  uint16_t bit = digitalPinToBitMask(pin);
  uint16_t port = digitalPinToPort(pin);
  volatile uint16_t *reg;
  reg = portModeRegister(port);
  *reg |= bit;
}

//TODO: refactor things so we don't have to include shell.h here
#include "shell.h"

void setup_failed() {
    int led_state = 0;
    while(1) {
        delay(1000);
        led_state = !led_state;
    }
}

void setup() {
    Serial.begin(38400);
    Serial.flush();

    if (WAIT_FOR_SERIAL) {
        wait_for_serial();
    }

    init_vbatt();
    current_error("");

    Serial.println("starting TFT...");

    uint16_t tft_id = tft.readID();
    if (tft_id != 0x8357) {
        Serial.print("got unexpected tft id 0x");
        Serial.println(tft_id, HEX);
        setup_failed();
    }
    tft.begin(tft_id);
    tft.fillScreen(BLACK);
    tft.setRotation(TFT_ROTATION);
    tft.setCursor(0, 0);
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    Serial.println("TFT started.");
    tft.println("Initializing...");

    Serial.println("starting RTC...");
    tft.println("starting RTC...");
    if (!rtc.begin()) {
        Serial.println("RTC failed to begin");
        tft.println("RTC failed to begin");
        setup_failed();
    }

    if (rtc.lostPower()) {
        Serial.println("RTC lost power, let's set the time!");
        // following line sets the RTC to the date & time this sketch was compiled
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    Serial.println("starting SD...");
    tft.println("starting SD...");
    if(!sd.begin(SdSpiConfig(10, DEDICATED_SPI, SPI_HALF_SPEED))) {
        Serial.println("SD failed to begin");
        tft.println("SD failed to start");
        setup_failed();
    }
    FsDateTime::setCallback(date_callback);

    Serial.println("starting ZEROII...");
    tft.println("Starting ZEROII...");
    if(!analyzer.zeroii_.startZeroII()) {
        Serial.println("failed to start zeroii");
        tft.println("Failed to start ZeroII. Aborting.");
        setup_failed();
        return;
    }
    String str = "Version: ";
    Serial.println(str + analyzer.zeroii_.getMajorVersion() + "." + analyzer.zeroii_.getMinorVersion() +
            ", HW Revision: " + analyzer.zeroii_.getHwRevision() +
            ", SN: " + analyzer.zeroii_.getSerialNumber()
    );
    Serial.println("ZEROII started.");

    Serial.println("starting serial wombat...");
    if(!sw.begin(Wire, 0x6C)) {
        Serial.println("serial wombat failed to begin");
        tft.println("serial wombat failed to start");
        setup_failed();
    }
    uint32_t sw_version = sw.readVersion_uint32();
    if (sw_version == 0) {
        Serial.println(String("serial wombat version was bad: ")+sw_version);
    }
    Serial.println(String("serial wombat version: ")+sw_version+" '"+sw.readVersion()+"'");
    quad_enc.begin(2, 1, 10, false, QE_ONLOW_POLL);
    quad_enc.read(32768);
    debounced_input.begin(0, 30, false, false);
    debounced_input.readTransitionsState();

    Serial.println("checking for settings...");
    if(!persistence.begin()) {
        Serial.println("persistence failed to begin");
        tft.println("persistence failed to start");
        setup_failed();
    }
    if(!persistence.load_settings(&analyzer, MAX_STEPS)) {
        Serial.println("could not load existing settings");
    } else {
        Serial.println("loaded settings");
    }
    if(!persistence.load_results(analysis_results, &analysis_results_len, MAX_STEPS)) {
        Serial.println("could not load existing results");
    } else {
        Serial.println("loaded results");
    }

    Serial.println("Initialization complete.");
    tft.println("Initializing complete.");

    tft.fillScreen(BLACK);
    draw_title();
    draw_menu(menu_manager.current_menu_, menu_manager.current_option_);
}

void loop() {
    uint32_t now = millis();
    if (last_vbatt + BATT_SENSE_PERIOD < now) {
        update_vbatt();
        draw_vbatt();
    }

    if (error_message[0] && now - last_error_time > ERROR_MESSAGE_DWELL_TIME) {
        clear_error_display();
    }

    debounced_input.readTransitionsState();
    click = debounced_input.transitions > 0 && !debounced_input.digitalRead();

    if(click) {
        Serial.println("got a click");
    }

    if(click && error_message[0]) {
        //clear errors on positive user interaction
        clear_error_display();
    }

    uint16_t next_quad_enc = quad_enc.read(32768);
    turn = next_quad_enc - last_quad_enc;

    if(read_serial_command()) {
        handle_serial_command();
        serial_command_len = 0;
    }

    handle_option();
    // TODO: if we're in a measurement, allow cancelling it by clicking
    //delay(50);
}

void analyze_frequency(uint32_t fq) {
    Complex uncal_z = analyzer.uncalibrated_measure(fq);
    Complex cal_gamma = analyzer.calibrated_gamma(fq, uncal_z);
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
