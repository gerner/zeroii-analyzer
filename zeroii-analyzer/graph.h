#ifndef _GRAPH_H
#define _GRAPH_H

#define POINTER_MOVES_REDRAW 32
uint8_t pointer_moves = 0;
size_t swr_i = 0;

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
    //draw the title
    tft.fillRect(0, 0, tft.width(), 8*TITLE_TEXT_SIZE*2, BLACK);
    tft.setCursor(0,0);
    tft.setTextSize(TITLE_TEXT_SIZE);

    if (analysis_results_len == 0) {
        tft.println("No SWR results");
        return;
    }

    size_t min_swr_i = 0;
    float min_swr = compute_swr(analyzer.calibrated_gamma(results[0].uncal_gamma));
    for (size_t i=0; i<results_len; i++) {
        float swr = compute_swr(analyzer.calibrated_gamma(results[i].uncal_gamma));
        if (swr < min_swr) {
            min_swr = swr;
            min_swr_i = i;
        }
    }

    tft.println(String("Min SWR: ")+compute_swr(analyzer.calibrated_gamma(results[min_swr_i].uncal_gamma))+" "+frequency_formatter(results[min_swr_i].fq));
    tft.println(String("Sel SWR: ")+compute_swr(analyzer.calibrated_gamma(results[swr_i].uncal_gamma))+" "+frequency_formatter(results[swr_i].fq));
}

#endif //_GRAPH_H
