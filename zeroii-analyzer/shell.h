#ifndef _SHELL_H
#define _SHELL_H

#include <ctime>
#include <EEPROM.h>

#include <RTClib.h>
#include <SdFat.h>

#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif  // __arm__

int freeMemory() {
  char top;
#ifdef __arm__
  return &top - reinterpret_cast<char*>(sbrk(0));
#elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
  return &top - __brkval;
#else  // __arm__
  return __brkval ? &top - __brkval : &top - __malloc_heap_start;
#endif  // __arm__
}

size_t total_memory;
void shell_begin() {
#ifndef RAMEND
    total_memory = freeMemory();
#else
    total_memory = RAMEND;
#endif
}

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
        delay(100);
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

void shellfn_reset(size_t argc, char* argv[]) {
    Serial.println("resetting");
    NVIC_SystemReset();
}

void shellfn_eeprom(size_t argc, char* argv[]) {
    if(argc < 2) {
        Serial.println("usage: `eeprom IDX` where IDX is an index into eeprom");
        return;
    }
    int idx = atoi(argv[1]);
    Serial.println(String("eeprom idx\t")+idx+":\t0x"+String(EEPROM.read(idx), HEX));
}

void shellfn_result(size_t argc, char* argv[]) {
    if(argc < 2) {
        Serial.println("usage: `results IDX` where IDX is a index into result array");
        return;
    }

    int idx = atoi(argv[1]);
    if(idx >= analysis_results_len) {
        Serial.println(String("idx ")+idx+" >= "+analysis_results_len);
    } else {
        Serial.println(String("result idx\t")+idx);
        Serial.print("Raw:\t");
        Serial.println(analysis_results[idx].uncal_z);
        Serial.print("Uncal gamma:\t");
        Serial.println(compute_gamma(analysis_results[idx].uncal_z, 50));
        Serial.print("Cal gamma:\t");
        Serial.println(analyzer.calibrated_gamma(analysis_results[idx]));
        Serial.print("SWR:\t");
        Serial.println(compute_swr(analyzer.calibrated_gamma(analysis_results[idx])));
    }
}

void shellfn_results(size_t argc, char* argv[]) {
    for (size_t i=0; i<analysis_results_len; i++) {
        Serial.print(analysis_results[i].fq);
        Serial.print("\t");
        Serial.print(analysis_results[i].uncal_z);
        Serial.print("\t");
        Serial.print(compute_gamma(analysis_results[i].uncal_z, 50));
        Serial.print("\t");
        Serial.print(analyzer.calibrated_gamma(analysis_results[i]));
        Serial.print("\t");
        Serial.println(compute_swr(analyzer.calibrated_gamma(analysis_results[i])));
    }
}

void shellfn_menu_state(size_t argc, char* argv[]) {
    Serial.println(menu_manager.current_option_);
}


void shellfn_batt(size_t argc, char* argv[]) {
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

void shellfn_aref(size_t argc, char* argv[]) {
    Serial.print(analogRead(AVCC_MEASURE_PIN));
    Serial.print("\t");
    Serial.print(analogReference());
    Serial.print("\n");
}

#include <malloc.h>
extern uint32_t __StackTop;
void shellfn_free(size_t argc, char* argv[]) {
    size_t free = freeMemory();
    size_t used = mallinfo().arena + ((size_t*)__StackTop - &used);
    Serial.println("\ttotal\tused\tfree");
    Serial.println(String("Mem:\t")+(free+used)+"\t"+used+"\t"+free);
}

void shellfn_dir(size_t argc, char* argv[]) {
    FsFile root;
    if (!root.open("/")) {
        Serial.println("could not open root!");
    } else {
        print_directory(&root);
        root.close();
    }
}

void shellfn_df(size_t argc, char* argv[]) {

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

void shellfn_fstype(size_t argc, char* argv[]) {
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

void shellfn_rm(size_t argc, char* argv[]) {

    if (argc < 2) {
        Serial.println("remove what file?");
        return;
    }
    const char* target_name = argv[1];
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

void shellfn_mv(size_t argc, char* argv[]) {

    if (argc < 2) {
        Serial.println("move what file?");
        return;
    }
    const char* target_name = argv[1];
    if(strcmp(target_name, "/") == 0) {
        Serial.println("can't move root");
        return;
    }

    if(argc < 3) {
        Serial.println("move it to what new name?");
        return;
    }
    const char* new_name = argv[2];

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

    if(!sd.rename(target_name, new_name)) {
        Serial.println("move failed");
        return;
    }
    Serial.println(String("moved ")+target_name+" to "+new_name);
}

void shellfn_touch(size_t argc, char* argv[]) {
    if (argc < 2) {
        Serial.println("touch what file?");
        return;
    }
    const char* target_name = argv[1];

    FsFile target;
    if(!target.open(target_name, O_RDWR | O_CREAT | O_AT_END)) {
        Serial.println(String("could not open ")+target_name+" for append");
        return;
    }
    target.sync();
    target.close();

    Serial.println("touched");
}

void shellfn_cat(size_t argc, char* argv[]) {
    if (argc < 2) {
        Serial.println("cat what file?");
        return;
    }
    const char* target_name = argv[1];

    FsFile target;
    if(!target.open(target_name, O_RDONLY)) {
        Serial.println(String("could not open ")+target_name+" for read-only");
        return;
    }

    while(target.available()) {
        Serial.write(target.read());
    }

    target.close();
}

void shellfn_mkdir(size_t argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        Serial.println("usage: mkdir [-p] dirpath");
        return;
    }

    const char* target_name = argv[1];
    bool mk_parents = false;
    if (argc > 2) {
        if(strcmp("-p", argv[1]) == 0) {
            mk_parents = true;
            target_name = argv[2];
        } else if(strcmp("-p", argv[2]) == 0) {
            mk_parents = true;
        } else {
            Serial.println("usage: mkdir [-p] dirpath");
            return;
        }
    }

    FsFile root;
    if (!root.open("/")) {
        Serial.println("could not open root!");
    }

    FsFile new_dir;
    if(!new_dir.mkdir(&root, target_name, mk_parents)) {
        Serial.println("error creating directory");
    }

    root.close();
}

void shellfn_pixel(size_t argc, char* argv[]) {
    //pixel x y [color]
    //without color, print the color of pixel x y
    //with color set pixel x y to color

    if (argc < 3) {
        Serial.println("usage: pixel x y [color]");
        return;
    }
    int x = atoi(argv[1]);
    int y = atoi(argv[2]);
    if (argc > 3) {
        int color = atoi(argv[3]);
        Serial.println(String("drawing pixel at ")+x+","+y+" "+color);
        tft.drawPixel(x, y, color);
    } else {
        Serial.println(String("reading at ")+x+","+y);
        Serial.println(tft.readPixel(x, y));
    }
}

template<class T>
void write16(T &writer, const uint16_t v) {
    writer.write(((uint8_t *)&v)[0]);
    writer.write(((uint8_t *)&v)[1]);
}

template<class T>
void write32(T &writer, const uint32_t v) {
    writer.write(((uint8_t *)&v)[0]);
    writer.write(((uint8_t *)&v)[1]);
    writer.write(((uint8_t *)&v)[2]);
    writer.write(((uint8_t *)&v)[3]);
}

template<class T>
void write_color16_as_24(T &writer, uint16_t pixel) {
    // per AdafruitTFT lib color565 function (24bit color -> 16bit color):
    // ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    // pixel is 565 format
    uint8_t r = (pixel >> 8) & 0xF8;
    uint8_t g = (pixel >> 3) & 0xFC;
    uint8_t b = (pixel << 3) & 0xFF;

    writer.write(r);
    writer.write(g);
    writer.write(b);
}

void shellfn_screenshot(size_t argc, char* argv[]) {
    if (argc < 2) {
        Serial.println("usage: screenshot filepath");
        return;
    }

    const char* target_name = argv[1];

    const uint16_t bmp_depth = 24;
    const uint32_t width = tft.width();
    const uint32_t height = tft.height();
    const uint32_t image_offset = 14 + 40;
    // row_size has some padding
    // this calculation taken from Adafruit TFT bmp example
    // this assumes 24 bits per pixel (compare to bmp_depth above)
    const uint32_t row_size = (width * 3 + 3) & ~3;
    const uint32_t bitmap_size = row_size * height;
    const uint32_t file_size = bitmap_size + image_offset;
    const uint8_t padding = row_size - width*bmp_depth/8;

    FsFile target;
    if(!target.open(target_name, O_RDWR | O_CREAT | O_TRUNC)) {
        Serial.println(String("could not open ")+target_name+" for append");
        return;
    }

    Serial.println("writing header");
    // BMP header (14 bytes)
    // magic bytes
    target.write(0x42);
    target.write(0x4D);
    //file size
    write32(target, file_size);
    // creator bytes
    write32(target, 0u);
    // image offset
    write32(target, image_offset);

    // DIB header (Windows BITMAPINFO format) (40 bytes)
    // header size
    write32(target, 40u);
    // width/height
    write32(target, width);
    write32(target, height);
    // planes == 1
    write16(target, 1u);
    // bits per pixel
    write16(target, bmp_depth);
    // compression == 0 no compression
    write32(target, 0u);
    // raw bitmap size
    write32(target, bitmap_size);
    // horizontal/vertical resolutions 2835 ~72dpi
    write32(target, 2835);
    write32(target, 2835);
    // colors and important colors in palette, 0 is default
    write32(target, 0u);
    write32(target, 0u);

    Serial.println(String("file size: ")+file_size);
    Serial.println(String("image offset: ")+image_offset);

    Serial.println(String("rows: ")+height);
    Serial.println(String("cols: ")+width);
    Serial.println(String("bitmap size: ")+bitmap_size);

    Serial.println(String("rowsize: ")+row_size);
    Serial.println(String("padding: ")+padding);

    // pixel array (at last)
    // rows are padded to multiple of 32 bits (row_size)
    // rows are stored bottom to top
    // tft pixels are 16-bit 565 format and we need to explode that into 24-bit
    Serial.print("writing pixel data");

    // unsigned value will wrap around to a large number after row 0,
    // so we check that row < height for the loop condition
    for(size_t row = height-1; row < height; row--) {
        for(size_t col = 0; col<width; col++) {
            uint16_t pixel = tft.readPixel(col, row);
            write_color16_as_24(target, pixel);
        }
        //fill out the padding
        for(size_t i=0; i<padding; i++) {
            target.write((uint8_t)0u);
        }
        Serial.print(".");
    }
    Serial.println("done");

    target.close();

    Serial.println(String("screenshot saved to ")+target_name);
}

void shellfn_date(size_t argc, char* argv[]) {
    if(argc == 1) {
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
    } else if(argc > 1){
        //assume argv[1] is 8601
        DateTime new_now(argv[1]);
        rtc.adjust(new_now);
        Serial.println("set time");
    }
}

const char* SHELL_COMMANDS[] = {
    "help",
    "reset",
    "eeprom",
    "batt",
    "aref",
    "free",

    //fs stuff
    "dir",
    "df",
    "fstype",
    "rm",
    "mv",
    "touch",
    "cat",
    "mkdir",

    //tft stuff
    "pixel",
    "screenshot",

    //rtc stuff
    "date",

    //stuff specific to antenna analyzer
    "result",
    "results",
    "menu_state",
};



void shellfn_help(size_t argc, char* argv[]) {
    Serial.println(String("available commands (")+sizeof(SHELL_COMMANDS)/sizeof(SHELL_COMMANDS[0])+"):");
    for(size_t i=0; i<sizeof(SHELL_COMMANDS)/sizeof(SHELL_COMMANDS[0]); i++) {
        Serial.println(String("\t")+SHELL_COMMANDS[i]);
    }
}

typedef void(*shell_command_t)(size_t, char**);

const shell_command_t SHELL_FUNCTIONS[] = {
    shellfn_help,
    shellfn_reset,
    shellfn_eeprom,
    shellfn_batt,
    shellfn_aref,
    shellfn_free,

    shellfn_dir,
    shellfn_df,
    shellfn_fstype,
    shellfn_rm,
    shellfn_mv,
    shellfn_touch,
    shellfn_cat,
    shellfn_mkdir,

    shellfn_pixel,
    shellfn_screenshot,

    shellfn_date,

    shellfn_result,
    shellfn_results,
    shellfn_menu_state
};

typedef char CHECK_SHELL_COMMANDS[sizeof(SHELL_COMMANDS) / sizeof(SHELL_COMMANDS[0]) == sizeof(SHELL_FUNCTIONS)/sizeof(SHELL_FUNCTIONS[0]) ? 1 : -1];

size_t split_args(char* command, size_t command_len, char** argv) {
    size_t len_left = command_len;
    char* current = command;
    size_t i = 0;
    while(current) {
        size_t ws_len = lstrip(current, len_left);
        if(ws_len == len_left) {
            break;
        }
        argv[i] = current+ws_len;
        char* next = (char*)memchr(argv[i], ' ', len_left-ws_len);
        i++;
        if(!next) {
            break;
        }
        //null terminate argv[i], the next starts after the space we found
        *next = 0;
        len_left -= next+1 - current;
        current = next+1;
    }
    return i;
}

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
            char* shell_argv[MAX_SERIAL_COMMAND];
            size_t shell_argc = split_args(serial_command, serial_command_len, shell_argv);

            SHELL_FUNCTIONS[i](shell_argc, shell_argv);
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
