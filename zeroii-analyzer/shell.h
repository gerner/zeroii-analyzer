#ifndef _SHELL_H
#define _SHELL_H

#include <ctime>
#include <EEPROM.h>

#include <RTClib.h>
#include <SdFat.h>
#include <ArduinoJson.h>

#define MAX_SERIAL_COMMAND 128
char serial_command[MAX_SERIAL_COMMAND+1];
size_t serial_command_len = 0;
bool read_serial_command() {
    int c;
    while(serial_command_len < MAX_SERIAL_COMMAND && ((c = Serial.read()) > 0)) {
        if(c == '\n') {
            serial_command[serial_command_len] = '\0';
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

int str2int(const char* str, int len, char** ptr=NULL)
{
    int i;
    int ret = 0;
    for(i = 0; i < len; ++i)
    {
        if(str[i] < '0' || str [i] > '9') {
            break;
        } else {
            ret = ret * 10 + (str[i] - '0');
        }
    }
    if(ptr) {
        *ptr = (char *)((void *)str+i);
    }
    return ret;
}

size_t lstrip(const char* str, size_t len) {
    size_t i;
    for(i=0; i<len && str[i]; i++) {
        if (str[i] != ' ' && str[i] != '\t' && str[i] != '\n' && str[i] != '\r') {
            break;
        }
    }
    return i;
}

void fsdate_to_ymd(uint16_t fs_date, uint16_t* y, uint8_t* m, uint8_t* d) {
    // fs_date packs 7-bit year shifted up 9, 4 bit month up 5, then 5 bit day
    // year 0 starts at 1980
    *y = (fs_date >> 9) + 1980;
    *m = (fs_date >> 5) & 15;
    *d = fs_date & 31;
}

void fstime_to_hms(uint16_t fs_time, uint8_t* H, uint8_t* M, uint8_t* S) {
    // fs_time packs 5 bit hour shifted up 11, 6-bit minute shifted up 5, 5-bit second shifted down 1 (2 second resolution)
    *H = fs_time >> 11;
    *M = (fs_time >> 5) & 0X3F;
    *S = 2 * (fs_time & 0X1F);
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
            Serial.print(entry.size(), DEC);
            Serial.print("\t");

            uint16_t fs_date;
            uint16_t fs_time;
            entry.getModifyDateTime(&fs_date, &fs_time);

            uint16_t y;
            uint8_t m, d, H, M, S;
            fsdate_to_ymd(fs_date, &y, &m, &d);
            fstime_to_hms(fs_time, &H, &M, &S);


            DateTime modify_time(y, m, d, H, M, S);
            time_t t(modify_time.unixtime());
            char formatted_date[20];
            strftime(formatted_date, sizeof(formatted_date), "%Y-%m-%dT%H:%M:%S", localtime(&t));

            Serial.println(formatted_date);

        }
        entry.close();
    }
}

void shellfn_reset(const char* serial_command, size_t serial_command_len) {
    Serial.println("resetting");
    NVIC_SystemReset();
}

void shellfn_eeprom(const char* serial_command, size_t serial_command_len) {
    if(serial_command_len < 7) {
        Serial.println("usage: `eeprom IDX` where IDX is an index into eeprom");
        return;
    }
    int idx = str2int(serial_command+7, serial_command_len-7);
    Serial.println(String("eeprom idx\t")+idx+":\t0x"+String(EEPROM.read(idx), HEX));
}

void shellfn_result(const char* serial_command, size_t serial_command_len) {
    if(serial_command_len < 7) {
        Serial.println("usage: `results IDX` where IDX is a index into result array");
        return;
    }

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
}

void shellfn_results(const char* serial_command, size_t serial_command_len) {
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
}

void shellfn_batt(const char* serial_command, size_t serial_command_len) {
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
}

void shellfn_aref(const char* serial_command, size_t serial_command_len) {
    Serial.print(analogRead(AVCC_MEASURE_PIN));
    Serial.print("\t");
    Serial.print(analogReference());
    Serial.print("\n");
}

void shellfn_dir(const char* serial_command, size_t serial_command_len) {
    FsFile root;
    if (!root.open("/")) {
        Serial.println("could not open root!");
    } else {
        print_directory(&root);
        root.close();
    }
}

void shellfn_df(const char* serial_command, size_t serial_command_len) {

    uint32_t blocks_per_cluster = sd.vol()->sectorsPerCluster();
    uint32_t cluster_count = sd.vol()->clusterCount();
    uint32_t free_cluster_count = sd.vol()->freeClusterCount();
    Serial.println("Filesystem\t512 byte blocks\tUsed\tAvailable\tUse%");
    Serial.print("/");
    Serial.print("\t");
    Serial.print((uint64_t)cluster_count * (uint64_t)blocks_per_cluster);
    Serial.print("\t");
    Serial.print(((uint64_t)(cluster_count-free_cluster_count)*(uint64_t)blocks_per_cluster));
    Serial.print("\t");
    Serial.print((uint64_t)(free_cluster_count)*(uint64_t)blocks_per_cluster);
    Serial.print("\t");
    Serial.print(((float)free_cluster_count)/((float)cluster_count));
    Serial.print("%\n");
}

void shellfn_fstype(const char* serial_command, size_t serial_command_len) {
    uint8_t fat_type = sd.fatType();
    switch(fat_type) {
        case FAT_TYPE_EXFAT:
            Serial.println("exFat");
            break;
        case FAT_TYPE_FAT32:
            Serial.println("Fat32");
            break;
        case FAT_TYPE_FAT16:
            Serial.println("Fat16");
            break;
        default:
            Serial.println(String("unknown fat type: ")+fat_type);
        }
}

void shellfn_rm(const char* serial_command, size_t serial_command_len) {

    size_t ws_len = lstrip(serial_command+2, serial_command_len-2);
    const char* target_name = serial_command+2+ws_len;
    if (!target_name) {
        Serial.println("remove what file?");
        return;
    }
    if(strcmp(target_name, "/") == 0) {
        Serial.println("can't remove root");
        return;
    }

    if(!sd.exists(target_name)) {
        Serial.println(String("no such file ")+target_name);
        return;
    }

    FsFile target;
    if(!target.open(target_name)) {
        Serial.println(String("could not open")+target_name);
        return;
    }
    if(target.isDirectory()) {
        FsFile child;
        if(target.openNext(&child)) {
            Serial.println(String(target_name)+" is a non-empty directory");
            return;
        }
    }
    target.close();

    if(!sd.remove(target_name)) {
        Serial.println("remove failed");
        return;
    }
    Serial.println(String("removed ")+target_name);
}

void shellfn_touch(const char* serial_command, size_t serial_command_len) {
    size_t ws_len = lstrip(serial_command+5, serial_command_len-5);
    const char* target_name = serial_command+5+ws_len;
    if (!target_name) {
        Serial.println("touch what file?");
        return;
    }

    FsFile target;
    if(!target.open(target_name, O_RDWR | O_CREAT | O_AT_END)) {
        Serial.println(String("could not open ")+target_name+" for append");
        return;
    }
    target.sync();
    target.close();

    Serial.println("touched");
}

void shellfn_cat(const char* serial_command, size_t serial_command_len) {
    size_t ws_len = lstrip(serial_command+3, serial_command_len-3);
    const char* target_name = serial_command+3+ws_len;
    if (!target_name) {
        Serial.println("cat what file?");
        return;
    }

    FsFile target;
    if(!target.open(target_name, O_RDONLY)) {
        Serial.println(String("could not open ")+target_name+" for append");
        return;
    }

    while(target.available()) {
        Serial.write(target.read());
    }

    target.close();
}

void shellfn_pixel(const char* serial_command, size_t serial_command_len) {
    //pixel x y [color]
    //without color, print the color of pixel x y
    //with color set pixel x y to color

    char* ptr;
    ptr = strchr(serial_command, ' ');
    if (!ptr) {
        Serial.println("usage: pixel x y [color]");
        return;
    }
    int x = str2int(ptr+1, serial_command_len-(serial_command-ptr)-1, &ptr);
    ptr = strchr(ptr, ' ');
    if (!ptr) {
        Serial.println("usage: pixel x y [color]");
        return;
    }
    int y = str2int(ptr+1, serial_command_len-(serial_command-ptr)-1, &ptr);
    if (ptr < serial_command+serial_command_len) {
        ptr = strchr(ptr, ' ');
        if (!ptr) {
            Serial.println("usage: pixel x y [color]");
            return;
        }
        int color = str2int(ptr+1, serial_command_len-(serial_command-ptr)-1, &ptr);
        Serial.println(String("drawing pixel at ")+x+","+y+" "+color);
        tft.drawPixel(x, y, color);
    } else {
        Serial.println(String("reading at ")+x+","+y);
        Serial.println(tft.readPixel(x, y));
    }
}

void shellfn_date(const char* serial_command, size_t serial_command_len) {
    if(serial_command_len == 4) {
        DateTime now = rtc.now();
        Serial.print(now.year(), DEC);
        Serial.print('-');
        Serial.print(now.month(), DEC);
        Serial.print('-');
        Serial.print(now.day(), DEC);
        Serial.print('T');
        Serial.print(now.hour(), DEC);
        Serial.print(':');
        Serial.print(now.minute(), DEC);
        Serial.print(':');
        Serial.print(now.second(), DEC);
        Serial.print('Z');
        Serial.print("\t");
        Serial.print(rtc.getTemperature());
        Serial.print("C");
        Serial.println();
    } else {
        char* ptr = strchr(serial_command, ' ');
        if(ptr && ptr - serial_command < serial_command_len) {
            ptr++;
            //assume ptr is 8601
            DateTime new_now(ptr);
            rtc.adjust(new_now);
            Serial.println("set time");
        } else {
            Serial.println("expected iso8601 datetime");
        }
    }
}

const char* SHELL_COMMANDS[] = {
    "help",
    "reset",
    "eeprom",
    "result",
    "results",
    "batt",
    "aref",
    "dir",
    "df",
    "fstype",
    "rm",
    "touch",
    "cat",
    "pixel",
    "date"
};

void shellfn_help(const char* serial_command, size_t serial_command_len) {
    Serial.println(String("available commands (")+sizeof(SHELL_COMMANDS)/sizeof(SHELL_COMMANDS[0])+"):");
    for(size_t i=0; i<sizeof(SHELL_COMMANDS)/sizeof(SHELL_COMMANDS[0]); i++) {
        Serial.println(String("\t")+SHELL_COMMANDS[i]);
    }
}

typedef void(*shell_command_t)(const char*, size_t);

const shell_command_t SHELL_FUNCTIONS[] = {
    shellfn_help,
    shellfn_reset,
    shellfn_eeprom,
    shellfn_result,
    shellfn_results,
    shellfn_batt,
    shellfn_aref,
    shellfn_dir,
    shellfn_df,
    shellfn_fstype,
    shellfn_rm,
    shellfn_touch,
    shellfn_cat,
    shellfn_pixel,
    shellfn_date
};

void handle_serial_command() {
    if(serial_command_len == 0) {
        return;
    }
    const char *param_list = strchr(serial_command, ' ');
    size_t command_name_len = serial_command_len;
    if(param_list) {
        command_name_len = param_list - serial_command;
    }

    for(size_t i=0; i<sizeof(SHELL_COMMANDS)/sizeof(SHELL_COMMANDS[0]); i++) {
        if(command_name_len < strlen(SHELL_COMMANDS[i])) {
            continue;
        }
        if(strncmp(serial_command, SHELL_COMMANDS[i], command_name_len) == 0) {
            SHELL_FUNCTIONS[i](serial_command, serial_command_len);
            return;
        }
    }
    char* buf = (char*)malloc(serial_command_len+1);
    memcpy(buf, serial_command, serial_command_len);
    buf[serial_command_len] = 0;
    Serial.println(String("unknown command of length ")+command_name_len+" ("+serial_command_len+"): '"+buf+"'");
    free(buf);
}

#endif //_SHELL_H
