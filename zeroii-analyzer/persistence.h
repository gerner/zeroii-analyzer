#ifndef _SETTINGS_H
#define _SETTINGS_H

#include <SdFat.h>
#include <ArduinoJson.h>
#include <Complex.h>

#include "analyzer.h"

namespace ARDUINOJSON_NAMESPACE {
template <>
struct Converter<Complex> {
  static bool toJson(const Complex& src, JsonVariant dst) {
    dst[0] = src.real();
    dst[1] = src.imag();
    return true;
  }

  static Complex fromJson(JsonVariantConst src) {
    return Complex(src[0], src[1]);
  }

  static bool checkJson(JsonVariantConst src) {
    return src[0].is<double>() && src[1].is<double>();
  }
};
}

struct FsFileReader {
    FsFileReader(FsFile* entry) : entry_(entry) {}

    int read() {
        return entry_->read();
    }

    size_t readBytes(char* buffer, size_t length) {
        return entry_->read(buffer, length);
    }

    FsFile* entry_;
};

// Tools for managing settings and results persistence

#define DEFAULT_ANALYZER_PERSISTENCE_NAME "zeroii-analyzer"
#define SETTINGS_PREFIX "settings_"
#define RESULTS_PREFIX "results_"

class AnalyzerPersistence {
    public:
    // save named settings
    bool save_settings(const char* name, const Analyzer* analyzer) {
        DynamicJsonDocument settings_doc(8192);
        settings_doc["z0"] = analyzer->z0_;
        settings_doc["cal_open"] = analyzer->cal_open_;
        settings_doc["cal_short"] = analyzer->cal_short_;
        settings_doc["cal_load"] = analyzer->cal_load_;


        FsFile entry;
        if(!entry.open(&settings_dir_, name, O_WRONLY | O_CREAT | O_TRUNC)) {
            Serial.println(String("could not open ")+name);
            return false;
        }
        serializeJson(settings_doc, entry);
        entry.close();
        return true;
    }

    bool load_settings(FsFile* entry, Analyzer* analyzer) {
        DynamicJsonDocument settings_doc(8192);
        FsFileReader reader(entry);
        DeserializationError err = deserializeJson(settings_doc, reader);
        if(err != DeserializationError::Ok) {
            Serial.println(String("error deserializing settings: ")+err.c_str());
            return false;
        }
        analyzer->z0_ = settings_doc["z0"].as<float>();
        analyzer->cal_short_ = settings_doc["cal_short"].as<Complex>();
        analyzer->cal_open_ = settings_doc["cal_open"].as<Complex>();
        analyzer->cal_load_ = settings_doc["cal_load"].as<Complex>();
        Serial.println("loaded settings");
        return true;
    }

    // load named settings
    bool load_settings(const char* name, Analyzer* analyzer) {
        FsFile entry;
        if(!entry.open(&settings_dir_, name, O_RDONLY)){
            return false;
        }
        return load_settings(&entry, analyzer) && entry.close();
    }

    // save settings to automatically named file
    bool save_settings(const Analyzer* analyzer) {
        FsFile entry;
        char filename[128];
        if(find_latest_file(&settings_dir_, &entry, SETTINGS_PREFIX)) {
            size_t filename_len = entry.getName(filename, sizeof(filename));;
            entry.close();
            int file_number = str2int(filename+sizeof(SETTINGS_PREFIX), filename_len-sizeof(SETTINGS_PREFIX));
            (String(SETTINGS_PREFIX)+(file_number+1)+".json").toCharArray(filename, sizeof(filename));
            Serial.println(String("saving settings to additional settings file ")+filename);
            return save_settings(filename, analyzer);
        } else {
            (String(SETTINGS_PREFIX)+"0.json").toCharArray(filename, sizeof(filename));
            Serial.println(String("saving settings to first settings file ")+filename);
            return save_settings(filename, analyzer);
        }
    }

    // load most recent settings
    bool load_settings(Analyzer* analyzer) {
        FsFile entry;
        if(find_latest_file(&settings_dir_, &entry, SETTINGS_PREFIX)) {
            return load_settings(&entry, analyzer) && entry.close();
        } else {
            Serial.println("no settings found");
            return false;
        }
    }

    // save named results
    bool save_results(const char* name, const AnalysisPoint* results, const size_t results_len) {
        DynamicJsonDocument results_doc(8192);
        JsonArray results_array = results_doc.to<JsonArray>();
        for(size_t i; i<results_len; i++) {
            JsonObject point = results_array.createNestedObject();
            point["fq"] = results[i].fq;
            point["uncal_z"] = results[i].uncal_z;
        }

        FsFile entry;
        if(!entry.open(&results_dir_, name, O_WRONLY | O_CREAT | O_TRUNC)) {
            Serial.println(String("could not open ") + name);
            return false;
        }
        serializeJson(results_doc, entry);
        entry.close();
        return true;
    }

    bool load_results(FsFile* entry, AnalysisPoint* results, size_t *results_len, size_t max_len) {
        DynamicJsonDocument results_doc(8192);
        FsFileReader reader(entry);
        DeserializationError err = deserializeJson(results_doc, reader);
        if(err != DeserializationError::Ok) {
            Serial.println(String("error deserializing results: ")+err.c_str());
            return false;
        }

        if (results_doc.size() > max_len) {
            Serial.println("truncating results");
        }
        size_t results_to_read = min(results_doc.size(), max_len);
        for (size_t i=0; i<results_to_read; i++) {
            results[i].fq = results_doc[i]["fq"].as<uint32_t>();
            results[i].uncal_z = results_doc[i]["uncal_z"].as<Complex>();
        }

        *results_len = results_to_read;
        Serial.println(String("loaded ")+results_to_read+" results");
        return true;
    }

    // load named results
    bool load_results(const char* name, AnalysisPoint* results, size_t *results_len, const size_t max_len) {
        FsFile entry;
        if(!entry.open(&results_dir_, name, O_RDONLY)) {
            Serial.println(String("could not open results file ") + name);
            return false;
        }
        return load_results(&entry, results, results_len, max_len) && entry.close();
    }

    // save automatically named results file
    bool save_results(const AnalysisPoint* results, const size_t results_len) {
        FsFile entry;
        char filename[128];
        if(find_latest_file(&results_dir_, &entry, RESULTS_PREFIX)) {
            size_t filename_len = entry.getName(filename, sizeof(filename));
            entry.close();
            int file_number = str2int(filename+sizeof(RESULTS_PREFIX), filename_len-sizeof(RESULTS_PREFIX));
            (String(RESULTS_PREFIX)+(file_number+1)+".json").toCharArray(filename, sizeof(filename));
            Serial.println(String("saving results to additional results file ")+filename);
            return save_results(filename, results, results_len);
        } else {
            (String(RESULTS_PREFIX)+"0.json").toCharArray(filename, sizeof(filename));
            Serial.println(String("saving results to first results file ")+filename);
            return save_results(filename, results, results_len);
        }
    }

    // load most recent results
    bool load_results(AnalysisPoint* results, size_t *results_len, const size_t max_len) {
        FsFile entry;
        if(find_latest_file(&results_dir_, &entry, RESULTS_PREFIX)) {
            return load_results(&entry, results, results_len, max_len) && entry.close();
        } else {
            Serial.println("no results found");
            return false;
        }
    }

    bool begin(const char* directory_name=DEFAULT_ANALYZER_PERSISTENCE_NAME) {
        // check for directory structure
        FsFile root;
        if(!root.open("/")) {
            Serial.println("could not open root!");
            return false;
        }
        if(!persistence_root_.open(&root, directory_name)) {
            if(!persistence_root_.mkdir(&root, directory_name)) {
                root.close();
                Serial.println("could not create persistence directory");
                return false;
            }
        } else if(!persistence_root_.isDirectory()) {
            root.close();
            persistence_root_.close();
            Serial.println(String("persistence root ")+directory_name+" exists, but is not a directory!");
            return false;
        }
        root.close();

        if(!settings_dir_.open(&persistence_root_, "settings")) {
            if(!settings_dir_.mkdir(&persistence_root_, "settings")) {
                Serial.println("could not create settings dir!");
                return false;
            }
        } else if(!settings_dir_.isDirectory()) {
            settings_dir_.close();
            Serial.println("settings dir exists, but is not a directory!");
            return false;
        }

        if(!results_dir_.open(&persistence_root_, "results")) {
            if(!results_dir_.mkdir(&persistence_root_, "results")) {
                Serial.println("could not create results dir!");
                return false;
            }
        } else if(!results_dir_.isDirectory()) {
            results_dir_.close();
            Serial.println("results dir exists, but is not a directory!");
            return false;
        }

        if(!persistence_root_.isOpen()) {
            Serial.println("persistence root is not open");
        }
        if(!settings_dir_.isOpen()) {
            Serial.println("settings dir is not open");
        }
        if(!results_dir_.isOpen()) {
            Serial.println("results dir is not open");
        }

        return true;
    }


    FsFile persistence_root_;
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
                Serial.println(String("could not open ")+max_filename+" in "+dirname);
                return false;
            }
            return true;
        } else {
            return false;
        }
    }

};

#endif //_SETTINGS_H
