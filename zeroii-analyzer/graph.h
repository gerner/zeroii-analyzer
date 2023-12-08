#ifndef _GRAPH_H
#define _GRAPH_H

#define POINTER_MOVES_REDRAW 32
uint8_t pointer_moves = 0;
size_t swr_i = 0;

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

void translate_to_screen(float x_in, float y_in, float x_min, float x_max, float y_min, float y_max, int16_t x_screen, int16_t y_screen, int16_t width, int16_t height, int16_t* xy) {
    float x_range = x_max - x_min;
    float y_range = y_max - y_min;
    xy[0] = (x_in - x_min) / x_range * width + x_screen;
    xy[1] = (y_in - y_min) / y_range * height + y_screen;

    Serial.println(String(x_in)+" -> "+xy[0]+" "+y_in+" -> "+xy[1]);
    Serial.flush();
}

void graph_swr(const AnalysisPoint* results, size_t results_len, const Analyzer* analyzer) {
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

    int16_t xy_cutoff[2];
    translate_to_screen(0, 3, 0, 1, 5, 1, x_screen, y_screen, width, height, xy_cutoff);
    tft.drawFastHLine(x_screen, xy_cutoff[1], width, RED);
    translate_to_screen(0, 1.5, 0, 1, 5, 1, x_screen, y_screen, width, height, xy_cutoff);
    tft.drawFastHLine(x_screen, xy_cutoff[1], width, MAGENTA);
    // draw all the analysis points
    if (results_len == 0) {
        Serial.println("no results to plot");
        return;
    }

    uint32_t start_fq = results[0].fq;
    uint32_t end_fq = results[results_len-1].fq;

    if (results_len == 1) {
        int16_t xy[2];
        translate_to_screen(results[0].fq, compute_swr(analyzer->calibrated_gamma(results[0].uncal_z)), start_fq, end_fq, 5, 1, x_screen, y_screen, width, height, xy);
        tft.fillCircle(xy[0], xy[1], 3, YELLOW);
    } else {
        for (size_t i=0; i<results_len-1; i++) {
            float swr = compute_swr(analyzer->calibrated_gamma(results[i].uncal_z));
            int16_t xy_start[2];
            translate_to_screen(results[i].fq, swr, start_fq, end_fq, 5, 1, x_screen, y_screen, width, height, xy_start);
            int16_t xy_end[2];
            translate_to_screen(results[i+1].fq, compute_swr(analyzer->calibrated_gamma(results[i+1].uncal_z)), start_fq, end_fq, 5, 1, x_screen, y_screen, width, height, xy_end);
            Serial.println(String("drawing line ")+xy_start[0]+","+xy_start[1]+" to "+xy_end[0]+","+xy_end[1]);
            tft.drawLine(xy_start[0], xy_start[1], xy_end[0], xy_end[1], YELLOW);
        }
    }
}

void draw_swr_pointer(const AnalysisPoint* results, size_t results_len, size_t swr_i, size_t old_swr_i, const Analyzer* analyzer) {
    // x ranges from start fq to end fq
    // y ranges from 1 to 5
    int16_t x_screen = 8*2;
    int16_t y_screen = 8*TITLE_TEXT_SIZE*2;
    int16_t width = tft.width()-x_screen;
    int16_t height = tft.height()-y_screen-8*2;

    if (results_len == 0) {
        return;
    }
    uint32_t start_fq = results[0].fq;
    uint32_t end_fq = results[results_len-1].fq;
    int16_t pointer_width = 8;
    int16_t pointer_height = 8;

    //first clear the old swr pointer
    int16_t xy_pointer[2];
    translate_to_screen(results[old_swr_i].fq, compute_swr(analyzer->calibrated_gamma(results[old_swr_i].uncal_z)), start_fq, end_fq, 5, 1, x_screen, y_screen, width, height, xy_pointer);
    tft.fillTriangle(xy_pointer[0], xy_pointer[1], xy_pointer[0]-pointer_width/2, xy_pointer[1]+pointer_height, xy_pointer[0]+pointer_width/2, xy_pointer[1]+pointer_height, BLACK);

    //draw the "pointer"
    translate_to_screen(results[swr_i].fq, compute_swr(analyzer->calibrated_gamma(results[swr_i].uncal_z)), start_fq, end_fq, 5, 1, x_screen, y_screen, width, height, xy_pointer);
    tft.drawTriangle(xy_pointer[0], xy_pointer[1], xy_pointer[0]-pointer_width/2, xy_pointer[1]+pointer_height, xy_pointer[0]+pointer_width/2, xy_pointer[1]+pointer_height, GREEN);
}

void draw_swr_title(const AnalysisPoint* results, size_t results_len, size_t swr_i, const Analyzer* analyzer) {
    //draw the title
    tft.fillRect(0, 0, tft.width(), 8*TITLE_TEXT_SIZE*2, BLACK);
    tft.setCursor(0,0);
    tft.setTextSize(TITLE_TEXT_SIZE);

    if (results_len == 0) {
        tft.println("No SWR results");
        return;
    }

    size_t min_swr_i = 0;
    float min_swr = compute_swr(analyzer->calibrated_gamma(results[0].uncal_z));
    for (size_t i=0; i<results_len; i++) {
        float swr = compute_swr(analyzer->calibrated_gamma(results[i].uncal_z));
        if (swr < min_swr) {
            min_swr = swr;
            min_swr_i = i;
        }
    }

    tft.println(String("Min SWR: ")+compute_swr(analyzer->calibrated_gamma(results[min_swr_i].uncal_z))+" "+frequency_formatter(results[min_swr_i].fq));
    tft.println(String("Sel SWR: ")+compute_swr(analyzer->calibrated_gamma(results[swr_i].uncal_z))+" "+frequency_formatter(results[swr_i].fq));
}

void graph_smith(const AnalysisPoint* results, size_t results_len, const Analyzer* analyzer) {
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

    // gamma range, x is real, y is imag
    float x_min = -1;
    float x_max = 1;
    float y_min = 1;
    float y_max = -1;

    // horizontal (and arcs) resistance axis
    tft.drawFastHLine(x_screen, y_screen+(height/2), width, WHITE);
    // circular reactance axis (radius 1, centered at (0.5, 0))
    tft.drawCircle(x_screen+(width*3/4), y_screen+(height/2), width/4, WHITE);
    // and draw the outer circle for reference
    tft.drawCircle(x_screen+(width/2), y_screen+(height/2), width/2, WHITE);


    //cutoff swr circles
    int16_t center[2];
    translate_to_screen(0, 0, x_min, x_max, y_min, y_max, x_screen, y_screen, width, height, center);
    int16_t swr_3[2];
    translate_to_screen(0.5, 0, x_min, x_max, y_min, y_max, x_screen, y_screen, width, height, swr_3);
    swr_3[0] -= center[0];
    tft.drawCircle(x_screen+(width/2), y_screen+(height/2), swr_3[0], RED);
    int16_t swr_15[2];
    translate_to_screen(0.2, 0, x_min, x_max, y_min, y_max, x_screen, y_screen, width, height, swr_15);
    swr_15[0] -= center[0];
    tft.drawCircle(x_screen+width/2, y_screen+height/2, swr_15[0], MAGENTA);

    int16_t test_point[2];
    Complex test_g = compute_gamma(Complex(41.961, 16.353), 50);
    translate_to_screen(test_g.real(), test_g.imag(), x_min, x_max, y_min, y_max, x_screen, y_screen, width, height, test_point);

    // draw all the analysis points
    if (results_len == 0) {
        Serial.println("no results to plot");
        return;
    } else if (results_len == 1) {
        int16_t xy[2];
        Complex g = analyzer->calibrated_gamma(results[0].uncal_z);
        translate_to_screen(g.real(), g.imag(), x_min, x_max, y_min, y_max, x_screen, y_screen, width, height, xy);
        tft.fillCircle(xy[0], xy[1], 3, YELLOW);
    } else {
        for (size_t i=0; i<results_len-1; i++) {
            Complex g_start = analyzer->calibrated_gamma(results[i].uncal_z);
            Complex g_end = analyzer->calibrated_gamma(results[i+1].uncal_z);
            int16_t xy_start[2];
            translate_to_screen(g_start.real(), g_start.imag(), x_min, x_max, y_min, y_max, x_screen, y_screen, width, height, xy_start);
            int16_t xy_end[2];
            translate_to_screen(g_end.real(), g_end.imag(), x_min, x_max, y_min, y_max, x_screen, y_screen, width, height, xy_end);
            Serial.println(String("drawing line ")+xy_start[0]+","+xy_start[1]+" to "+xy_end[0]+","+xy_end[1]);
            tft.drawLine(xy_start[0], xy_start[1], xy_end[0], xy_end[1], YELLOW);
        }
    }
}

#endif //_GRAPH_H
