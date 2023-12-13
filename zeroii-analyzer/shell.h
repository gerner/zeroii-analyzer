#ifndef _SHELL_H
#define _SHELL_H

#include <SdFat.h>
#include <ArduinoJson.h>

#define MAX_SERIAL_COMMAND 128
char serial_command[MAX_SERIAL_COMMAND];
size_t serial_command_len = 0;
bool read_serial_command() {
    int c;
    while(serial_command_len < MAX_SERIAL_COMMAND && ((c = Serial.read()) > 0)) {
        if(c == '\n') {
            return true;
        }
        serial_command[serial_command_len++] = c;
    }
    if(serial_command_len == MAX_SERIAL_COMMAND) {
        serial_command_len = 0;
    }
    return false;
}

void wait_for_serial() {
    char c;
    while(((c = Serial.read()) == 0) || (c != '\n')) {
    }
}

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

void print_directory(FsBaseFile* dir, int numTabs=0) {
  FsFile entry;
  size_t filename_max_len = 128;
  char filename[filename_max_len];
  while (entry.openNext(dir, O_RDONLY)) {
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    entry.getName(filename, filename_max_len);
    Serial.print(filename);
    if (entry.isDir()) {
      Serial.println("/");
      print_directory(&entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

void handle_serial_command() {
    if(serial_command_len == 0) {
        return;
    }
    if(strncmp(serial_command, "reset", serial_command_len) == 0) {
        Serial.println("resetting");
        NVIC_SystemReset();
    } else if(strncmp(serial_command, "eeprom ", min(serial_command_len, 7)) == 0) {
        int idx = str2int(serial_command+7, serial_command_len-7);
        Serial.println(String("eeprom idx\t")+idx+":\t0x"+String(EEPROM.read(idx), HEX));
    } else if(strncmp(serial_command, "result ", min(serial_command_len, 7)) == 0) {
        int idx = str2int(serial_command+7, serial_command_len-7);
        if(idx >= analysis_results_len) {
            Serial.println(String("idx ")+idx+" >= "+analysis_results_len);
        } else {
            Serial.println(String("result idx\t")+idx);
            Serial.print("Raw:\t");
            Serial.println(analysis_results[idx].uncal_z);
            Serial.print("Uncal gamma:\t");
            Serial.println(compute_gamma(analysis_results[idx].uncal_z, 50));
            Serial.print("Cal gamma:\t");
            Serial.println(analyzer.calibrated_gamma(analysis_results[idx].uncal_z));
            Serial.print("SWR:\t");
            Serial.println(compute_swr(analyzer.calibrated_gamma(analysis_results[idx].uncal_z)));
        }
    } else if(strncmp(serial_command, "results", serial_command_len) == 0) {
        for (size_t i=0; i<analysis_results_len; i++) {
            Serial.print(analysis_results[i].fq);
            Serial.print("\t");
            Serial.print(analysis_results[i].uncal_z);
            Serial.print("\t");
            Serial.print(compute_gamma(analysis_results[i].uncal_z, 50));
            Serial.print("\t");
            Serial.print(analyzer.calibrated_gamma(analysis_results[i].uncal_z));
            Serial.print("\t");
            Serial.println(compute_swr(analyzer.calibrated_gamma(analysis_results[i].uncal_z)));
        }
    } else if(strncmp(serial_command, "batt", serial_command_len) == 0) {
        uint16_t batt_raw = analogRead(A3);
        Serial.print(batt_raw);
        Serial.print("\t");
        Serial.print(batt_raw*5.0/1023.0*2);
        Serial.print("\t");
        Serial.print(batt_raw*analogReference()/1023.0*2);
        Serial.print("\t");
        Serial.print(measure_vbatt());
        Serial.print("\t");
        Serial.println(vbatt);
    } else if(strncmp(serial_command, "aref", serial_command_len) == 0) {
        Serial.print(analogRead(AVCC_MEASURE_PIN));
        Serial.print("\t");
        Serial.print(analogReference());
        Serial.print("\n");
    } else if(strncmp(serial_command, "load_settings", serial_command_len) == 0) {
        Serial.println("loading settings from file...");
        uint32_t start_time = millis();
        FsFile settings_file;
        if(!settings_file.open("settings.json")) {
            Serial.println("could not open settings.json");
        } else {
            DynamicJsonDocument settings_doc(8192);
            deserializeJson(settings_doc, settings_file);
            settings_file.close();
            if (!settings_doc.containsKey("version") || settings_doc["version"] != 1) {
                Serial.println("version not found or bad value");
            } else {
                Serial.println("settings look ok");
            }
        }
        Serial.println(String("loaded settings in ")+(millis()-start_time)+"ms");
    } else if(strncmp(serial_command, "load_results", serial_command_len) == 0) {
        Serial.println("loading results from file...");
        uint32_t start_time = millis();
        FsFile results_file;
        if(!results_file.open("results.json")) {
            Serial.println("could not open results.json");
        } else {
            DynamicJsonDocument results_doc(8192);
            deserializeJson(results_doc, results_file);
            results_file.close();

            Serial.print("results list of size ");
            Serial.println(results_doc.size());
            for (size_t i=0; i<results_doc.size(); i++) {
                uint32_t fq = results_doc[i]["fq"];
                Complex uncal_z(results_doc[i]["uncal_z"][0], results_doc[i]["uncal_z"][1]);

                Serial.print(fq);
                Serial.print("\t");
                Serial.print(uncal_z);
                Serial.print("\n");
            }
        }
        Serial.println(String("loaded results in ")+(millis()-start_time)+"ms");
    } else if(strncmp(serial_command, "dir", serial_command_len) == 0) {
        FsFile root;
        if (!root.open("/")) {
            Serial.println("could not open root!");
        } else {
            print_directory(&root);
        }
    } else {
        char* buf = (char*)malloc(serial_command_len+1);
        memcpy(buf, serial_command, serial_command_len);
        buf[serial_command_len] = 0;
        Serial.println(String("unknown command of length ")+serial_command_len+": '"+buf+"'");
        free(buf);
    }
}

#endif //_SHELL_H
