#include "RigExpertZeroII_I2C.h"
#include "Complex.h"

#include <Adafruit_GFX.h>    // Core graphics library
#include "Adafruit_TFTLCD.h"

const __FlashStringHelper* test = F("foobar");

#include <SdFat.h>

#include "RTClib.h"

#include <SerialWombat.h>

#include "log.h"
#include "analyzer.h"
#include "menu_manager.h"
#include "persistence.h"

Logger loop_logger("loop");

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
//TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

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

const MenuOption fq_menu_options[] = {
    MenuOption(F("Fq Start"), MOPT_FQSTART, NULL),
    MenuOption(F("Fq End"), MOPT_FQEND, NULL),
    MenuOption(F("Fq Center"), MOPT_FQCENTER, NULL),
    MenuOption(F("Fq Range"), MOPT_FQWINDOW, NULL),
    MenuOption(F("Fq Band"), MOPT_FQBAND, NULL),
    MenuOption(F("Steps"), MOPT_FQSTEPS, NULL),
    MenuOption(F("Back"), MOPT_BACK, NULL),
};
Menu fq_menu(NULL, fq_menu_options, sizeof(fq_menu_options)/sizeof(fq_menu_options[0]));

const MenuOption results_menu_options[] {
    MenuOption(F("SWR graph"), MOPT_SWR, NULL),
    MenuOption(F("Smith chart"), MOPT_SMITH, NULL),
    MenuOption(F("Save Results"), MOPT_SAVE_RESULTS, NULL),
    MenuOption(F("Load Results"), MOPT_LOAD_RESULTS, NULL),
    MenuOption(F("Back"), MOPT_BACK, NULL),
};
Menu results_menu(NULL, results_menu_options, sizeof(results_menu_options)/sizeof(results_menu_options[0]));

const MenuOption settings_menu_options[] = {
    MenuOption(F("Calibration"), MOPT_CALIBRATE, NULL),
    MenuOption(F("Z0"), MOPT_Z0, NULL),
    MenuOption(F("Save Settings"), MOPT_SAVE_SETTINGS, NULL),
    MenuOption(F("Load Settings"), MOPT_LOAD_SETTINGS, NULL),
    MenuOption(F("Back"), MOPT_BACK, NULL),
};
Menu settings_menu(NULL, settings_menu_options, sizeof(settings_menu_options)/sizeof(settings_menu_options[0]));

const MenuOption root_menu_options[] = {
    MenuOption(F("Analyze"), MOPT_ANALYZE, NULL),
    MenuOption(F("Frequencies"), MOPT_FQ, &fq_menu),
    MenuOption(F("Results"), MOPT_RESULTS, &results_menu),
    MenuOption(F("Settings"), MOPT_SETTINGS, &settings_menu),
};
Menu root_menu(NULL, root_menu_options, sizeof(root_menu_options)/sizeof(root_menu_options[0]));

MenuManager menu_manager(&root_menu);

uint32_t start_fq = MIN_FQ;
uint32_t end_fq = MAX_FQ;
uint16_t step_count = MAX_STEPS/2;

bool set_analysis_from_calibration() {
    if(analyzer.calibration_len_ == 0) {
        return false;
    }

    start_fq = constrain(analyzer.calibration_results_[0].fq, MIN_FQ, MAX_FQ);
    end_fq = constrain(analyzer.calibration_results_[analyzer.calibration_len_-1].fq, MIN_FQ, MAX_FQ);
    step_count = constrain(analyzer.calibration_len_, 1, MAX_STEPS);
    return true;
}

void draw_title() {
    tft.fillRect(0, 0, tft.width(), 8*TITLE_TEXT_SIZE, BLACK);
    tft.setCursor(0,0);
    tft.setTextSize(TITLE_TEXT_SIZE);

    tft.print(frequency_formatter(start_fq) + "-" + frequency_formatter(end_fq));
    tft.print(" St:");
    tft.print(step_count);
    tft.print(" Z0:");
    tft.print((uint32_t)analyzer.z0_);

    draw_error();
}

uint32_t last_error_print = 0;
uint32_t last_error_time = 0;
char error_message[64];

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
    loop_logger.info(F("menu back"));
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

/*************
 * A set of state machines to handle different kinds of activities that cross
 * loop invocations.
 ************/
#include "process.h"
AnalysisProcessor* analysis_processor = NULL;
Calibrator* calibrator = NULL;
FqSetter* fq_setter = NULL;
BandSetter* band_setter = NULL;
FileBrowser* file_browser = NULL;
ConfirmDialog* confirm_dialog = NULL;

GraphContext* graph_context = NULL;

bool browse_progress() {
    return file_browser->choose_file();
}

void enter_option(int32_t option_id) {
    loop_logger.debug(String("entering ")+option_id);
    switch(option_id) {
        case MOPT_ANALYZE:
            analysis_results_len = step_count;
            analysis_processor = new AnalysisProcessor();
            if(analysis_processor == NULL) {
                loop_logger.error(F("could not make an AnalysisProcessor"));
            }
            analysis_processor->initialize(start_fq, end_fq, step_count, analysis_results);
            break;
        case MOPT_FQCENTER: {
            int32_t centerFq = start_fq + (end_fq-start_fq)/2;
            fq_setter = new FqSetter();
            if(fq_setter == NULL) {
                loop_logger.error("could not make an FqSetter");
            }
            fq_setter->initialize(centerFq);
            break;
        }
        case MOPT_FQWINDOW: {
            int32_t rangeFq = end_fq - start_fq;
            fq_setter = new FqSetter();
            if(fq_setter == NULL) {
                loop_logger.error("could not make an FqSetter");
            }
            fq_setter->initialize(rangeFq);
            break;
        }
        case MOPT_FQSTART: {
            assert(fq_setter == NULL);
            fq_setter = new FqSetter();
            if(fq_setter == NULL) {
                loop_logger.error("could not make an FqSetter");
            }
            fq_setter->initialize(start_fq);
            break;
        }
        case MOPT_FQEND: {
            fq_setter = new FqSetter();
            if(fq_setter == NULL) {
                loop_logger.error("could not make an FqSetter");
            }
            fq_setter->initialize(end_fq);
            break;
        }
        case MOPT_FQBAND:
            band_setter = new BandSetter();
            if(band_setter == NULL) {
                loop_logger.error("could not make an BandSetter");
            }
            band_setter->initialize();
            break;
        case MOPT_FQSTEPS:
            tft.fillScreen(BLACK);
            draw_title();
            break;
        case MOPT_Z0:
            tft.fillScreen(BLACK);
            draw_title();
            break;
        case MOPT_CALIBRATE:
            calibration_len = step_count;
            calibrator = new Calibrator(&analyzer);
            if(calibrator == NULL) {
                loop_logger.error("could not make a Calibrator");
            }
            calibrator->initialize(start_fq, end_fq, step_count, calibration_results);
            break;
        case MOPT_SWR: {
            graph_context = new GraphContext(analysis_results, analysis_results_len, &analyzer);
            if(graph_context == NULL) {
                loop_logger.error("could not make a GraphContext");
            }
            graph_context->graph_swr();
            graph_context->draw_swr_pointer();
            graph_context->draw_swr_title();
            break;
        }
        case MOPT_SMITH: {
            graph_context = new GraphContext(analysis_results, analysis_results_len, &analyzer);
            if(graph_context == NULL) {
                loop_logger.error("could not make a GraphContext");
            }
            graph_context->graph_smith();
            graph_context->draw_smith_pointer();
            graph_context->draw_smith_title();
            break;
        }
        case MOPT_SAVE_RESULTS:
            file_browser = new FileBrowser();
            if(file_browser == NULL) {
                loop_logger.error("could not make a FileBrowser");
            }
            file_browser->initialize(&persistence.results_dir_, true);
            confirm_dialog = new ConfirmDialog();
            if(confirm_dialog == NULL) {
                loop_logger.error("could not make a ConfirmDialog");
            }
            confirm_dialog->initialize(&browse_progress);
            break;
        case MOPT_LOAD_RESULTS:
            file_browser = new FileBrowser();
            if(file_browser == NULL) {
                loop_logger.error("could not make a FileBrowser");
            }
            file_browser->initialize(&persistence.results_dir_, false);
            confirm_dialog = new ConfirmDialog();
            if(confirm_dialog == NULL) {
                loop_logger.error("could not make a ConfirmDialog");
            }
            confirm_dialog->initialize(&browse_progress);
            break;
        case MOPT_SAVE_SETTINGS:
            file_browser = new FileBrowser();
            if(file_browser == NULL) {
                loop_logger.error("could not make a FileBrowser");
            }
            file_browser->initialize(&persistence.settings_dir_, true);
            confirm_dialog = new ConfirmDialog();
            if(confirm_dialog == NULL) {
                loop_logger.error("could not make a ConfirmDialog");
            }
            confirm_dialog->initialize(&browse_progress);
            break;
        case MOPT_LOAD_SETTINGS:
            file_browser = new FileBrowser();
            if(file_browser == NULL) {
                loop_logger.error("could not make a FileBrowser");
            }
            file_browser->initialize(&persistence.settings_dir_, false);
            confirm_dialog = new ConfirmDialog();
            if(confirm_dialog == NULL) {
                loop_logger.error("could not make a ConfirmDialog");
            }
            confirm_dialog->initialize(&browse_progress);
            break;
    }
}

void leave_option(int32_t option_id) {
    loop_logger.debug(String("leaving ")+option_id);
    switch(option_id) {
        case MOPT_FQCENTER:
            loop_logger.info(String("setting center fq to: ") + fq_setter->fq());
            // move [start_fq, end_fq] so it's centered on desired value
            /*start_fq = constrain(fq_setter->fq() - (end_fq - start_fq)/2, MIN_FQ, MAX_FQ);
            end_fq = constrain(fq_setter->fq() + (end_fq - start_fq)/2, MIN_FQ, MAX_FQ);*/
            delete fq_setter;
            fq_setter = NULL;
            tft.fillScreen(BLACK);
            draw_title();
            break;
        case MOPT_FQWINDOW: {
            loop_logger.info(String("setting window fq to: ") + fq_setter->fq());
            // narrow/expand [start_fq, end_fq] remaining centered
            int32_t cntFq = start_fq + (end_fq - start_fq)/2;
            start_fq = constrain(cntFq - fq_setter->fq()/2, MIN_FQ, MAX_FQ);
            end_fq = constrain(cntFq + fq_setter->fq()/2, MIN_FQ, MAX_FQ);
            delete fq_setter;
            fq_setter = NULL;
            tft.fillScreen(BLACK);
            draw_title();
            break;
        }
        case MOPT_FQSTART:
            loop_logger.info(String("setting start fq to: ") + fq_setter->fq());
            start_fq = fq_setter->fq();
            end_fq = constrain(end_fq, start_fq+1, MAX_FQ);
            delete fq_setter;
            fq_setter = NULL;
            tft.fillScreen(BLACK);
            draw_title();
            break;
        case MOPT_FQEND:
            loop_logger.info(String("setting end fq to: ") + fq_setter->fq());
            end_fq = fq_setter->fq();
            start_fq = constrain(start_fq, MIN_FQ, end_fq-1);
            delete fq_setter;
            fq_setter = NULL;
            tft.fillScreen(BLACK);
            draw_title();
            break;
        case MOPT_FQBAND:
            band_setter->band(&start_fq, &end_fq);
            loop_logger.info(String("setting start/end to: ") + start_fq + "/" + end_fq);
            delete band_setter;
            band_setter = NULL;
            tft.fillScreen(BLACK);
            draw_title();
            break;
        case MOPT_ANALYZE:
            delete analysis_processor;
            analysis_processor = NULL;
            tft.fillScreen(BLACK);
            draw_title();
            break;
        case MOPT_CALIBRATE:
            delete calibrator;
            calibrator = NULL;
            tft.fillScreen(BLACK);
            draw_title();
            analyzer.calibration_len_ = calibration_len;
            break;
        case MOPT_SAVE_RESULTS:
            if(confirm_dialog->confirm()) {
                if(file_browser->is_new()) {
                    if(!persistence.save_results(analysis_results, analysis_results_len)) {
                        loop_logger.error(F("could not save results"));
                        current_error("could not save results");
                    }
                } else {
                    char filename[128];
                    file_browser->file(filename, sizeof(filename));
                    if(!persistence.save_results(filename, analysis_results, analysis_results_len)) {
                        loop_logger.error(F("could not save results"));
                        current_error("could not save results");
                    }
                }
            } else {
                loop_logger.info(F("cancelled saving results"));
                current_error("cancelled saving results");
            }
            delete file_browser;
            file_browser = NULL;
            delete confirm_dialog;
            confirm_dialog = NULL;
            tft.fillScreen(BLACK);
            draw_title();
            break;
        case MOPT_LOAD_RESULTS:
            if(confirm_dialog->confirm()) {
                char filename[128];
                file_browser->file(filename, sizeof(filename));
                if(!persistence.load_results(filename, analysis_results, &analysis_results_len, MAX_STEPS)) {
                    loop_logger.error(F("could not load results"));
                    current_error("could not load results");
                }
            } else {
                loop_logger.info(F("cancelled loading results"));
                current_error("cancelled loading results");
            }
            delete file_browser;
            file_browser = NULL;
            delete confirm_dialog;
            confirm_dialog = NULL;
            tft.fillScreen(BLACK);
            draw_title();
            break;
        case MOPT_SAVE_SETTINGS:
            if(confirm_dialog->confirm()) {
                if(file_browser->is_new()) {
                    if(!persistence.save_settings(&analyzer)) {
                        loop_logger.error(F("could not save settings"));
                        current_error("could not save settings");
                    }
                } else {
                    char filename[128];
                    file_browser->file(filename, sizeof(filename));
                    if(!persistence.save_settings(filename, &analyzer)) {
                        loop_logger.error(F("could not save settings"));
                        current_error("could not save settings");
                    }
                }
            } else {
                loop_logger.info(F("cancelled saving settings"));
                current_error("cancelled saving settings");
            }
            delete file_browser;
            file_browser = NULL;
            delete confirm_dialog;
            confirm_dialog = NULL;
            tft.fillScreen(BLACK);
            draw_title();
            break;
        case MOPT_LOAD_SETTINGS:
            if(confirm_dialog->confirm()) {
                char filename[128];
                file_browser->file(filename, sizeof(filename));
                if(!persistence.load_settings(filename, &analyzer, MAX_STEPS)) {
                    loop_logger.error(F("could not load settings"));
                    current_error("could not load settings");
                } else {
                    set_analysis_from_calibration();
                }
            } else {
                loop_logger.info(F("cancelled loading settings"));
                current_error("cancelled loading settings");
            }
            delete file_browser;
            file_browser = NULL;
            delete confirm_dialog;
            confirm_dialog = NULL;
            tft.fillScreen(BLACK);
            draw_title();
            break;
        case MOPT_SWR:
            delete graph_context;
            graph_context = NULL;
            tft.fillScreen(BLACK);
            draw_title();
            break;
        case MOPT_SMITH:
            delete graph_context;
            graph_context = NULL;
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
            if (analysis_processor->analyze()) {
                menu_back();
                if(!menu_manager.select_option(MOPT_SWR)) {
                    loop_logger.error(F("could not find SWR option"));
                } else {
                    choose_option();
                }
            }
            break;
        case MOPT_SWR:
            if (click) {
                menu_back();
            } else if (turn != 0 && analysis_results_len > 0) {
                // move the "pointer" on the swr graph
                graph_context->incr_swri(turn);
                graph_context->draw_swr_pointer();
                graph_context->draw_swr_title();
            }
            break;
        case MOPT_SMITH:
            if (click) {
                menu_back();
            } else if (turn != 0 && analysis_results_len > 0) {
                // move the "pointer" on the smith chart
                graph_context->incr_swri(turn);
                graph_context->draw_smith_pointer();
                graph_context->draw_smith_title();
            }
            break;
        case MOPT_SAVE_RESULTS:
        case MOPT_LOAD_RESULTS:
        case MOPT_SAVE_SETTINGS:
        case MOPT_LOAD_SETTINGS:
            if(confirm_dialog->progress()) {
                menu_back();
            }
            break;
        case MOPT_FQCENTER:
            if(fq_setter->set_fq_value(MIN_FQ, MAX_FQ, "Center Frequency")) {
                menu_back();
            }
            break;
        case MOPT_FQWINDOW:
            if(fq_setter->set_fq_value(0, MAX_FQ/2, "Frequency Range")) {
                menu_back();
            }
            break;
        case MOPT_FQSTART:
            if(fq_setter->set_fq_value(MIN_FQ, MAX_FQ, "Start Frequency")) {
                menu_back();
            }
            break;
        case MOPT_FQEND:
            if(fq_setter->set_fq_value(MIN_FQ, MAX_FQ, "End Frequency")) {
                menu_back();
            }
            break;
        case MOPT_FQBAND:
            if (band_setter->set_band()) {
                menu_back();
                draw_title();
            }
            break;
        case MOPT_FQSTEPS:
            step_count = set_user_value(step_count, 1, 128, "Steps");
            break;
        case MOPT_CALIBRATE: {
            if(calibrator->calibration_step()) {
                menu_back();
            }
            break;
        }
        case MOPT_Z0:
            analyzer.z0_ = set_user_value(analyzer.z0_, 1, 999, "Z0");
            break;
        default:
            loop_logger.error(String("don't know what to do with option ")+menu_manager.current_option_);
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

    loop_logger.info(F("starting TFT..."));

    uint16_t tft_id = tft.readID();
    if (tft_id != 0x8357) {
        loop_logger.error(F("got unexpected tft id 0x"));
        Serial.println(tft_id, HEX);
        setup_failed();
    }
    tft.begin(tft_id);
    tft.fillScreen(BLACK);
    tft.setRotation(TFT_ROTATION);
    tft.setCursor(0, 0);
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    loop_logger.info(F("TFT started."));
    tft.println(F("Initializing..."));

    loop_logger.info(F("starting RTC..."));
    tft.println(F("starting RTC..."));
    if (!rtc.begin()) {
        delay(100);
        if(!rtc.begin()) {
            loop_logger.error(F("RTC failed to begin"));
            tft.println(F("RTC failed to begin"));
            setup_failed();
        }
    }

    if (rtc.lostPower()) {
        loop_logger.warn(F("RTC lost power, let's set the time!"));
        // following line sets the RTC to the date & time this sketch was compiled
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    loop_logger.info(F("starting SD..."));
    tft.println(F("starting SD..."));
    if(!sd.begin(SdSpiConfig(10, DEDICATED_SPI, SPI_HALF_SPEED))) {
        loop_logger.error(F("SD failed to begin"));
        tft.println(F("SD failed to start"));
        setup_failed();
    }
    FsDateTime::setCallback(date_callback);

    loop_logger.info(F("starting ZEROII..."));
    tft.println(F("Starting ZEROII..."));
    if(!analyzer.zeroii_.startZeroII()) {
        loop_logger.error(F("failed to start zeroii"));
        tft.println(F("Failed to start ZeroII. Aborting."));
        setup_failed();
        return;
    }
    String str = "Version: ";
    loop_logger.info(str + analyzer.zeroii_.getMajorVersion() + "." + analyzer.zeroii_.getMinorVersion() +
            ", HW Revision: " + analyzer.zeroii_.getHwRevision() +
            ", SN: " + analyzer.zeroii_.getSerialNumber()
    );
    loop_logger.info(F("ZEROII started."));

    loop_logger.info(F("starting serial wombat..."));
    if(!sw.begin(Wire, 0x6C)) {
        loop_logger.error(F("serial wombat failed to begin"));
        tft.println(F("serial wombat failed to start"));
        setup_failed();
    }
    uint32_t sw_version = sw.readVersion_uint32();
    if (sw_version == 0) {
        loop_logger.error(String("serial wombat version was bad: ")+sw_version);
    }
    loop_logger.info(String("serial wombat version: ")+sw_version+" '"+sw.readVersion()+"'");
    quad_enc.begin(2, 1, 10, false, QE_ONLOW_POLL);
    quad_enc.read(32768);
    debounced_input.begin(0, 30, false, false);
    debounced_input.readTransitionsState();

    loop_logger.info(F("setting some initial start/end fq"));
    {
        BandSetter bs;
        start_fq = bs.band_fqs[BAND_10M][0];
        end_fq = bs.band_fqs[BAND_10M][1];
    }

    loop_logger.info(F("checking for settings..."));
    if(!persistence.begin()) {
        loop_logger.error(F("persistence failed to begin"));
        tft.println(F("persistence failed to start"));
        setup_failed();
    }
    if(!persistence.load_settings(&analyzer, MAX_STEPS)) {
        loop_logger.error(F("could not load existing settings"));
    } else {
        set_analysis_from_calibration();
        loop_logger.info(F("loaded settings"));
    }
    if(!persistence.load_results(analysis_results, &analysis_results_len, MAX_STEPS)) {
        loop_logger.error(F("could not load existing results"));
    } else {
        loop_logger.info(F("loaded results"));
    }

    loop_logger.info(F("Initialization complete."));
    tft.println(F("Initializing complete."));

    tft.fillScreen(BLACK);
    draw_title();
    draw_menu(menu_manager.current_menu_, menu_manager.current_option_);
}

void loop() {
    loop_logger.debug(F("entering loop"));
    uint32_t now = millis();
    if (last_vbatt + BATT_SENSE_PERIOD < now) {
        //loop_logger.debug("updating battery measurement");
        update_vbatt();
        draw_vbatt();
        //loop_logger.debug("battery measurement updated");
    }

    if (error_message[0] && now - last_error_time > ERROR_MESSAGE_DWELL_TIME) {
        clear_error_display();
    }

    debounced_input.readTransitionsState();
    click = debounced_input.transitions > 0 && !debounced_input.digitalRead();

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
