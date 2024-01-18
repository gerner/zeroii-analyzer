#ifndef _PROCESS_H
#define _PROCESS_H

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
                    tft.fillRect(0, 7*2*8, tft.width(), 2*8, BLACK);
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
                    results_[result_idx_].cal_short = compute_gamma(analyzer_->uncalibrated_measure(fq_), analyzer_->z0_);
                    results_[result_idx_].fq = fq_;
                    result_idx_++;
                    fq_ += step_fq_;
                    draw_progress_meter(steps_, result_idx_);
                } else {
                    Serial.println("done calibrating short.");
                    tft.setTextSize(2);
                    tft.fillRect(0, 6*2*8, tft.width(), 2*8*3, BLACK);
                    tft.setCursor(0, 7*2*8);
                    tft.println("connect open and press knob");
                    calibration_state_ = CAL_O_START;
                }
                break;
            case CAL_O:
                if(fq_ <= end_fq_) {
                    results_[result_idx_].cal_open = compute_gamma(analyzer_->uncalibrated_measure(fq_), analyzer_->z0_);
                    result_idx_++;
                    fq_ += step_fq_;
                    draw_progress_meter(steps_, result_idx_);
                } else {
                    Serial.println("done calibrating open.");
                    tft.setTextSize(2);
                    tft.fillRect(0, 6*2*8, tft.width(), 2*8*3, BLACK);
                    tft.setCursor(0, 7*2*8);
                    tft.println(F("connect load and press knob"));
                    calibration_state_ = CAL_L_START;
                }
                break;
            case CAL_L:
                if(fq_ <= end_fq_) {
                    results_[result_idx_].cal_load = compute_gamma(analyzer_->uncalibrated_measure(fq_), analyzer_->z0_);
                    result_idx_++;
                    fq_ += step_fq_;
                    draw_progress_meter(steps_, result_idx_);
                } else {
                    Serial.println("done calibrating load.");
                    tft.setTextSize(2);
                    tft.fillRect(0, 6*2*8, tft.width(), 2*8*3, BLACK);
                    tft.setCursor(0, 6*2*8);
                    tft.println(F("done calibrating."));
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

#define NUM_BANDS 18
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
            band_idx_ = constrain((int32_t)band_idx_+turn, 0, NUM_BANDS-1);
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
        const uint32_t band_fqs[NUM_BANDS][2] = { {135700, 137800}, {472000, 479000}, {1800000, 2000000}, {3500000, 4000000}, {5330500, 5406400}, {7000000, 7300000}, {10100000, 10150000}, {14000000, 14350000}, {18068000, 18168000}, {2100000, 21450000}, {24890000, 24990000}, {28000000, 29700000}, {50000000, 54000000}, {144000000, 148000000}, {219000000, 225000000}, {420000000, 450000000}, {902000000, 928000000}, {100000, 600000000} };
        const char* band_names[NUM_BANDS] = {F("2200m"), F("630m"), F("160m"), F("80m"), F("60m"), F("40m"), F("30m"), F("20m"), F("17m"), F("15m"), F("12m"), F("10m"), F("6m"), F("VHF"), F("1.25m"), F("UHF"), F("33cm"), F("Reference RF")};
};

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
            file_menu_ = NULL;
            file_options_ = NULL;
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

typedef bool (*ProgressFn)(void);

class ConfirmDialog {
    public:
    void initialize(ProgressFn progress_fn) {
        confirmation_menu.selected_option = 0;
        progress_fn_ = progress_fn;
        progress_ = true;
    }

    bool progress() {
        if (progress_) {
            if(progress_fn_()) {
                Serial.println("inner progress fn returned true, proceeding with confirmation.");
                progress_ = false;
                tft.fillScreen(BLACK);
                tft.setCursor(CONFIRM_ORIG_X-6*TITLE_TEXT_SIZE, CONFIRM_ORIG_Y-8*TITLE_TEXT_SIZE);
                tft.print("Are you sure?");
                draw_title();
                draw_menu(&confirmation_menu, -1, true, CONFIRM_ORIG_X, CONFIRM_ORIG_Y);
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
    MenuOption confirmation_menu_options[2] = {
        MenuOption("Yes", 0, NULL),
        MenuOption("Cancel", 0, NULL),
    };
    Menu confirmation_menu = Menu(NULL, confirmation_menu_options, sizeof(confirmation_menu_options)/sizeof(confirmation_menu_options[0]));

    bool progress_;
    ProgressFn progress_fn_;
};

#endif