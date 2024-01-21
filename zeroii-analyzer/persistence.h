#ifndef _SETTINGS_H
#define _SETTINGS_H

#include <SdFat.h>
#include <Complex.h>
#include <JsonListener.h>
#include <JsonStreamingParser.h>

#include "log.h"
#include "analyzer.h"

Logger persistence_logger("persistence");

// Tools for managing settings and results persistence

enum SettingsListenerState { SETTINGS_START, SETTINGS_Z0, SETTINGS_CAL, SETTINGS_CAL_POINT, SETTINGS_CAL_FQ, SETTINGS_CAL_S, SETTINGS_CAL_S_R, SETTINGS_CAL_S_I, SETTINGS_CAL_O, SETTINGS_CAL_O_R, SETTINGS_CAL_O_I, SETTINGS_CAL_L, SETTINGS_CAL_L_R, SETTINGS_CAL_L_I };

class SettingsJsonListener : public JsonListener {
public:
    SettingsJsonListener(size_t max_steps) {
        max_steps_ = max_steps;
        calibration_results_ = new CalibrationPoint[max_steps];
    }

    ~SettingsJsonListener() {
        delete [] calibration_results_;
    }

    void initialize() {
        state_ = SETTINGS_START;
        has_error_ = false;
        calibration_len_ = 0;
        saw_z0_ = false;
    }

    void key(String k) {
        if (has_error_) {
            return;
        }
        switch(state_) {
            case SETTINGS_START:
                if (k == "z0") {
                    state_ = SETTINGS_Z0;
                } else if(k == "calibration") {
                    state_ = SETTINGS_CAL;
                }
                break;
            case SETTINGS_CAL_POINT:
                if (k == "fq") {
                    state_ = SETTINGS_CAL_FQ;
                } else if (k == "cal_short") {
                    state_ = SETTINGS_CAL_S;
                } else if (k == "cal_open") {
                    state_ = SETTINGS_CAL_O;
                } else if (k == "cal_load") {
                    state_ = SETTINGS_CAL_L;
                }
                break;
            default:
                // only allow keys in the root or inside a calibration point
                has_error_ = true;
                persistence_logger.warn(String("key \"")+k+"\" found in state "+state_);
        }
    }

    void value(String v) {
        if (has_error_) {
            return;
        }
        switch(state_) {
            case SETTINGS_START:
            case SETTINGS_CAL_POINT:
                break;
            case SETTINGS_Z0:
                z0_ = atof(v.c_str());
                state_ = SETTINGS_START;
                saw_z0_ = true;
                break;
            case SETTINGS_CAL_FQ:
                calibration_results_[calibration_len_].fq = atoi(v.c_str());
                state_ = SETTINGS_CAL_POINT;
                saw_fq_ = true;
                break;
            case SETTINGS_CAL_S_R:
            case SETTINGS_CAL_O_R:
            case SETTINGS_CAL_L_R:
                cal_val_.setReal(atof(v.c_str()));
                state_ += 1;
                break;
            case SETTINGS_CAL_S_I:
            case SETTINGS_CAL_O_I:
            case SETTINGS_CAL_L_I:
                cal_val_.setImag(atof(v.c_str()));
                break;
            default:
                has_error_ = true;
                persistence_logger.warn(String("value \"")+v+"\" found in state "+state_);
        }
    }

    void startArray() {
        if (has_error_) {
            return;
        }
        switch(state_) {
            case SETTINGS_START:
            case SETTINGS_CAL:
                break;
            case SETTINGS_CAL_S:
            case SETTINGS_CAL_O:
            case SETTINGS_CAL_L:
                state_++;
                break;
            default:
                has_error_ = true;
                persistence_logger.warn(String("start array found in state ")+state_);
        }
    }

    void endArray() {
        if (has_error_) {
            return;
        }
        switch(state_) {
            case SETTINGS_START:
                break;
            case SETTINGS_CAL:
                state_ = SETTINGS_START;
                break;
            case SETTINGS_CAL_S_I:
                calibration_results_[calibration_len_].cal_short = cal_val_;
                saw_cal_short_ = true;
                state_ = SETTINGS_CAL_POINT;
                break;
            case SETTINGS_CAL_O_I:
                calibration_results_[calibration_len_].cal_open = cal_val_;
                saw_cal_open_ = true;
                state_ = SETTINGS_CAL_POINT;
                break;
            case SETTINGS_CAL_L_I:
                calibration_results_[calibration_len_].cal_load = cal_val_;
                saw_cal_load_ = true;
                state_ = SETTINGS_CAL_POINT;
                break;
            default:
                has_error_ = true;
                persistence_logger.warn(String("end array found in state ")+state_);
        }

    }

    void startObject() {
        if (has_error_) {
            return;
        }
        switch(state_) {
            case SETTINGS_START:
                break;
            case SETTINGS_CAL:
                if (calibration_len_ == max_steps_) {
                    has_error_ = true;
                    persistence_logger.warn("too many calibration points");
                } else {
                    state_ = SETTINGS_CAL_POINT;
                    saw_fq_ = false;
                    saw_cal_short_ = false;
                    saw_cal_open_ = false;
                    saw_cal_load_ = false;
                }
                break;
            default:
                has_error_ = true;
                persistence_logger.warn(String("start object found in state ")+state_);
        }
    }

    void endObject() {
        if (has_error_) {
            return;
        }
        switch(state_) {
            case SETTINGS_START:
                break;
            case SETTINGS_CAL_POINT:
                if (!(saw_fq_ && saw_cal_short_ && saw_cal_open_ && saw_cal_load_)) {
                    has_error_ = true;
                    persistence_logger.warn(F("didn't see all required elements in calibration point"));
                    persistence_logger.warn(String("point: ")+calibration_len_);
                } else {
                    state_ = SETTINGS_CAL;
                    calibration_len_++;
                }
                break;
            default:
                has_error_ = true;
                persistence_logger.warn(String("end object found in state ")+state_);
        }
    }

    void startDocument() {
    }

    void endDocument() {
        if (has_error_) {
            return;
        }
        if (!saw_z0_) {
            has_error_ = true;
            persistence_logger.warn("didn't see z0");
        }
    }

    void whitespace(char c) {
    }

    bool has_error_;
    float z0_;
    size_t calibration_len_;
    CalibrationPoint* calibration_results_;
private:
    uint8_t state_;
    size_t max_steps_;

    Complex cal_val_;
    bool saw_z0_;
    bool saw_fq_;
    bool saw_cal_short_;
    bool saw_cal_open_;
    bool saw_cal_load_;
};

enum ResultsListenerState { RESULTS_START, RESULTS_POINT, RESULTS_FQ, RESULTS_Z, RESULTS_Z_R, RESULTS_Z_I };

class ResultsJsonListener : public JsonListener {
public:
    ResultsJsonListener(size_t max_steps) {
        max_steps_ = max_steps;
        results_ = new AnalysisPoint[max_steps];
    }

    ~ResultsJsonListener() {
        delete [] results_;
    }

    void initialize() {
        results_len_ = 0;
        state_ = RESULTS_START;
        has_error_ = false;
    }

    void key(String k) {
        if(has_error_) {
            return;
        }
        switch(state_) {
            case RESULTS_POINT:
                if (k == "fq") {
                    state_ = RESULTS_FQ;
                } else if (k =="uncal_z") {
                    state_ = RESULTS_Z;
                }
                break;
            default:
                has_error_ = true;
                persistence_logger.warn(String("key \"")+k+"\" found in state "+state_);
        }
    }

    void value(String v) {
        if(has_error_) {
            return;
        }
        switch(state_) {
            case RESULTS_FQ:
                results_[results_len_].fq = atoi(v.c_str());
                state_ = RESULTS_POINT;
                saw_fq_ = true;
                break;
            case RESULTS_Z_R:
                results_[results_len_].uncal_z.setReal(atof(v.c_str()));
                state_ = RESULTS_Z_I;
                break;
            case RESULTS_Z_I:
                results_[results_len_].uncal_z.setImag(atof(v.c_str()));
                break;
            case RESULTS_POINT:
                break;
            default:
                has_error_ = true;
                persistence_logger.warn(String("value \"")+v+"\" found in state "+state_);
        }
    }

    void startArray() {
        if(has_error_) {
            return;
        }
        switch(state_) {
            case RESULTS_START:
                break;
            case RESULTS_Z:
                state_ = RESULTS_Z_R;
                break;
            default:
                has_error_ = true;
                persistence_logger.warn(String("start array found in state ")+state_);
        }
    }

    void endArray() {
        if(has_error_) {
            return;
        }
        switch(state_) {
            case RESULTS_START:
                break;
            case RESULTS_Z_I:
                state_ = RESULTS_POINT;
                saw_z_ = true;
                break;
            default:
                has_error_ = true;
                persistence_logger.warn(String("end array found in state ")+state_);
        }
    }

    void startObject() {
        if(has_error_) {
            return;
        }
        switch(state_) {
            case RESULTS_START:
                if (results_len_ == max_steps_) {
                    has_error_ = true;
                    persistence_logger.warn("too many result points");
                } else {
                    state_ = RESULTS_POINT;
                    saw_fq_ = false;
                    saw_z_ = false;
                }
                break;
            case RESULTS_POINT:
                break;
            default:
                has_error_ = true;
                persistence_logger.warn(String("start object found in state ")+state_);
        }
    }

    void endObject() {
        if(has_error_) {
            return;
        }
        switch(state_) {
            case RESULTS_POINT:
                if(!(saw_fq_ && saw_z_)) {
                    has_error_ = true;
                    persistence_logger.warn("didn't see all required fields in result point");
                } else {
                    results_len_++;
                }
                break;
            default:
                has_error_ = true;
                persistence_logger.warn(String("end object found in state ")+state_);
        }
    }

    void startDocument() {
    }

    void endDocument() {
    }

    void whitespace(char c) {
    }

    size_t results_len_;
    AnalysisPoint* results_;

    bool has_error_;
private:
    uint8_t state_;
    size_t max_steps_;
    bool saw_fq_;
    bool saw_z_;
};

#define DEFAULT_ANALYZER_PERSISTENCE_NAME "zeroii-analyzer"
#define SETTINGS_PREFIX "settings_"
#define RESULTS_PREFIX "results_"

class AnalyzerPersistence {
    public:
    // save named settings
    bool save_settings(const char* name, const Analyzer* analyzer) {
        FsFile entry;
        if(!entry.open(&settings_dir_, name, O_WRONLY | O_CREAT | O_TRUNC)) {
            persistence_logger.error(String("settings saving could not open ")+name);
            return false;
        }

        char buf[32];
        entry.write("{\"z0\":");
        entry.write(dtostrf(analyzer->z0_, 1, 6, buf));

        entry.write(",\"calibration\":[");
        bool is_first = true;
        for(size_t i; i<analyzer->calibration_len_; i++) {
            if(!is_first) {
                entry.write(",");
            } else {
                is_first = false;
            }
            entry.write("{\"fq\":");
            entry.write(itoa(analyzer->calibration_results_[i].fq, buf, 10));

            entry.write(",\"cal_short\":[");
            entry.write(dtostrf(analyzer->calibration_results_[i].cal_short.real(), 1, 6, buf));
            entry.write(",");
            entry.write(dtostrf(analyzer->calibration_results_[i].cal_short.imag(), 1, 6, buf));
            entry.write("]");

            entry.write(",\"cal_open\":[");
            entry.write(dtostrf(analyzer->calibration_results_[i].cal_open.real(), 1, 6, buf));
            entry.write(",");
            entry.write(dtostrf(analyzer->calibration_results_[i].cal_open.imag(), 1, 6, buf));
            entry.write("]");

            entry.write(",\"cal_load\":[");
            entry.write(dtostrf(analyzer->calibration_results_[i].cal_load.real(), 1, 6, buf));
            entry.write(",");
            entry.write(dtostrf(analyzer->calibration_results_[i].cal_load.imag(), 1, 6, buf));
            entry.write("]");

            entry.write("}");
        }
        entry.write("]");
        entry.write("}");

        entry.close();
        persistence_logger.info(String("saved settings to ")+name);
        return true;
    }

    bool load_settings(FsFile* entry, Analyzer* analyzer, size_t max_cal_len) {
        JsonStreamingParser parser;
        SettingsJsonListener listener(max_cal_len);
        listener.initialize();
        parser.setListener(&listener);

        int c;
        while((c = entry->read()) >= 0) {
            parser.parse(c);
        }

        if(entry->available() > 0) {
            persistence_logger.error(String("failed to read settings file error ")+entry->getError());
            return false;
        }


        if (listener.has_error_) {
            persistence_logger.error("failed to load settings");
            return false;
        }

        analyzer->z0_ = listener.z0_;
        for(size_t i; i<listener.calibration_len_; i++) {
            analyzer->calibration_results_[i] = listener.calibration_results_[i];
        }
        analyzer->calibration_len_ = listener.calibration_len_;

        persistence_logger.info("loaded settings");
        return true;
    }

    // load named settings
    bool load_settings(const char* name, Analyzer* analyzer, size_t max_cal_len) {
        FsFile entry;
        if(!entry.open(&settings_dir_, name, O_RDONLY)){
            return false;
        }
        return load_settings(&entry, analyzer, max_cal_len) && entry.close();
    }

    // save settings to automatically named file
    bool save_settings(const Analyzer* analyzer) {
        FsFile entry;
        char filename[128];
        if(find_latest_file(&settings_dir_, &entry, SETTINGS_PREFIX)) {
            size_t filename_len = entry.getName(filename, sizeof(filename));;
            entry.close();
            persistence_logger.info(String("latest file is ")+filename);
            persistence_logger.info(String("suffix is \"")+(filename+sizeof(SETTINGS_PREFIX))+("\""));
            int file_number = str2int(filename+sizeof(SETTINGS_PREFIX)-1, filename_len-sizeof(SETTINGS_PREFIX)+1);
            persistence_logger.info(String("number is ")+file_number);
            (String(SETTINGS_PREFIX)+(file_number+1)+".json").toCharArray(filename, sizeof(filename));
            persistence_logger.info(String("saving settings to additional settings file ")+filename);
            return save_settings(filename, analyzer);
        } else {
            (String(SETTINGS_PREFIX)+"0.json").toCharArray(filename, sizeof(filename));
            persistence_logger.info(String("saving settings to first settings file ")+filename);
            return save_settings(filename, analyzer);
        }
    }

    // load most recent settings
    bool load_settings(Analyzer* analyzer, size_t max_cal_len) {
        FsFile entry;
        if(find_latest_file(&settings_dir_, &entry, SETTINGS_PREFIX)) {
            return load_settings(&entry, analyzer, max_cal_len) && entry.close();
        } else {
            persistence_logger.warn("no settings found");
            return false;
        }
    }

    // save named results
    bool save_results(const char* name, const AnalysisPoint* results, const size_t results_len) {
        FsFile entry;
        if(!entry.open(&results_dir_, name, O_WRONLY | O_CREAT | O_TRUNC)) {
            persistence_logger.error(String("could not open ") + name);
            return false;
        }

        entry.write("[");
        bool is_first = true;
        char buf[32];
        for(size_t i; i<results_len; i++) {
            if(!is_first) {
                entry.write(",");
            } else {
                is_first = false;
            }
            entry.write("{\"fq\":");
            entry.write(itoa(results[i].fq, buf, 10));

            entry.write(",\"uncal_z\":[");
            entry.write(dtostrf(results[i].uncal_z.real(), 1, 6, buf));
            entry.write(",");
            entry.write(dtostrf(results[i].uncal_z.imag(), 1, 6, buf));
            entry.write("]");
        }
        entry.write("]");

        entry.close();
        return true;
    }

    bool load_results(FsFile* entry, AnalysisPoint* results, size_t *results_len, size_t max_len) {
        JsonStreamingParser parser;
        ResultsJsonListener listener(max_len);
        listener.initialize();
        parser.setListener(&listener);

        int c;
        while((c = entry->read()) >= 0) {
            parser.parse(c);
        }

        if(entry->available() > 0) {
            persistence_logger.error(String("failed to read results file error ")+entry->getError());
            return false;
        }

        if(listener.has_error_) {
            persistence_logger.error("failed to load results");
            return false;
        }

        for(size_t i=0; i<listener.results_len_; i++) {
            results[i] = listener.results_[i];
        }
        *results_len = listener.results_len_;
        persistence_logger.info(String("loaded ")+listener.results_len_+" results");
        return true;
    }

    // load named results
    bool load_results(const char* name, AnalysisPoint* results, size_t *results_len, const size_t max_len) {
        FsFile entry;
        if(!entry.open(&results_dir_, name, O_RDONLY)) {
            persistence_logger.error(String("could not open results file ") + name);
            return false;
        }
        persistence_logger.info(String("loaded results from ") + name);
        return load_results(&entry, results, results_len, max_len) && entry.close();
    }

    // save automatically named results file
    bool save_results(const AnalysisPoint* results, const size_t results_len) {
        FsFile entry;
        char filename[128];
        if(find_latest_file(&results_dir_, &entry, RESULTS_PREFIX)) {
            size_t filename_len = entry.getName(filename, sizeof(filename));
            entry.close();
            int file_number = str2int(filename+sizeof(RESULTS_PREFIX)-1, filename_len-sizeof(RESULTS_PREFIX)+1);
            (String(RESULTS_PREFIX)+(file_number+1)+".json").toCharArray(filename, sizeof(filename));
            persistence_logger.info(String("saving results to additional results file ")+filename);
            return save_results(filename, results, results_len);
        } else {
            (String(RESULTS_PREFIX)+"0.json").toCharArray(filename, sizeof(filename));
            persistence_logger.info(String("saving results to first results file ")+filename);
            return save_results(filename, results, results_len);
        }
    }

    // load most recent results
    bool load_results(AnalysisPoint* results, size_t *results_len, const size_t max_len) {
        FsFile entry;
        if(find_latest_file(&results_dir_, &entry, RESULTS_PREFIX)) {
            return load_results(&entry, results, results_len, max_len) && entry.close();
        } else {
            persistence_logger.warn("no results found");
            return false;
        }
    }

    bool begin(const char* directory_name=DEFAULT_ANALYZER_PERSISTENCE_NAME) {
        // check for directory structure
        FsFile root;
        if(!root.open("/")) {
            persistence_logger.error("could not open root!");
            return false;
        }
        FsFile persistence_root;
        if(!persistence_root.open(&root, directory_name)) {
            if(!persistence_root.mkdir(&root, directory_name)) {
                root.close();
                persistence_logger.error("could not create persistence directory");
                return false;
            }
        } else if(!persistence_root.isDirectory()) {
            root.close();
            persistence_root.close();
            persistence_logger.error(String("persistence root ")+directory_name+" exists, but is not a directory!");
            return false;
        }
        root.close();

        if(!settings_dir_.open(&persistence_root, "settings")) {
            if(!settings_dir_.mkdir(&persistence_root, "settings")) {
                persistence_logger.error("could not create settings dir!");
                return false;
            }
        } else if(!settings_dir_.isDirectory()) {
            settings_dir_.close();
            persistence_logger.error("settings dir exists, but is not a directory!");
            return false;
        }

        if(!results_dir_.open(&persistence_root, "results")) {
            if(!results_dir_.mkdir(&persistence_root, "results")) {
                persistence_logger.error("could not create results dir!");
                return false;
            }
        } else if(!results_dir_.isDirectory()) {
            results_dir_.close();
            persistence_logger.error("results dir exists, but is not a directory!");
            return false;
        }

        persistence_root.close();
        if(!settings_dir_.isOpen()) {
            persistence_logger.error("settings dir is not open");
        }
        if(!results_dir_.isOpen()) {
            persistence_logger.error("results dir is not open");
        }

        return true;
    }

    FsFile settings_dir_;
    FsFile results_dir_;

    private:
    int str2int(const char* str, int len)
    {
        int i;
        int ret = 0;
        for(i = 0; i < len; ++i)
        {
            if(str[i] < '0' || str [i] > '9') {
                return ret;
            } else {
                ret = ret * 10 + (str[i] - '0');
            }
        }
        return ret;
    }

    bool find_latest_file(FsFile* directory, FsFile* entry, const char* prefix) {
        size_t prefix_len = strlen(prefix);
        // iterate through directory
        char filename[128];
        char max_filename[128];
        int max_file_number = -1;
        directory->rewindDirectory();
        while(entry->openNext(directory, O_RDONLY)) {
            // check for an int at the end of each file name after prefix
            size_t name_len = entry->getName(filename, sizeof(filename));
            if (strncmp(filename, prefix, min(name_len, prefix_len)) != 0) {
                entry->close();
                continue;
            }

            int file_number = str2int(filename+prefix_len, name_len-prefix_len);
            if (file_number > max_file_number) {
                max_file_number = file_number;
                strcpy(max_filename, filename);
            }

            entry->close();
        }

        if (max_file_number >= 0) {
            if(!entry->open(directory, max_filename, O_RDONLY)) {
                char dirname[128];
                directory->getName(dirname, 128);
                persistence_logger.error(String("could not open ")+max_filename+" in "+dirname);
                return false;
            }
            return true;
        } else {
            return false;
        }
    }
};

#endif //_SETTINGS_H
