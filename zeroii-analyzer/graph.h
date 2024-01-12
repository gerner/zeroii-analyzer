#ifndef _GRAPH_H
#define _GRAPH_H

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

void read_patch(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t* patch) {
    for (int16_t j = 0; j < h; j++, y++) {
        for (int16_t i = 0; i < w; i++) {
            patch[j * w + i] = tft.readPixel(x+i, y);
        }
    }
}

#define POINTER_WIDTH 8
#define POINTER_HEIGHT 8
int16_t pointer_patch_x, pointer_patch_y;
uint16_t pointer_patch[POINTER_WIDTH*POINTER_HEIGHT];

template<class T>
void draw_swr_label(const T label, uint32_t fq, float swr, float x_min, float x_max, float y_min, float y_max, int16_t x_screen, int16_t y_screen, int16_t width, int16_t height, const Analyzer* analyzer) {
    int16_t label_xy[2];

    translate_to_screen(fq, swr, x_min, x_max, y_min, y_max, x_screen, y_screen, width, height, label_xy);
    tft.fillCircle(label_xy[0], label_xy[1], 2, WHITE);
    tft.setCursor(label_xy[0]-6*LABEL_TEXT_SIZE/2-4*6*LABEL_TEXT_SIZE, label_xy[1]+8*LABEL_TEXT_SIZE/2);
    tft.print(label);
}

void graph_swr(const AnalysisPoint* results, size_t results_len, const Analyzer* analyzer) {
    Serial.println(String("graphing swr plot with ")+results_len+" points");
    Serial.flush();

    // set the pointer patch outside the graph area
    pointer_patch_x = tft.width();
    pointer_patch_y = tft.height();

    // area is just below the title
    tft.fillRect(0, 5*2*8, tft.width(), 3*8*3, BLACK);
    tft.fillRect(0, 8*TITLE_TEXT_SIZE, tft.width(), tft.height()-8*TITLE_TEXT_SIZE, BLACK);

    // draw axes
    // x ranges from start fq to end fq
    // y ranges from 1 to 5
    int16_t x_screen = 8*4;
    int16_t y_screen = 8*TITLE_TEXT_SIZE*2;
    int16_t width = tft.width()-x_screen-5*6*LABEL_TEXT_SIZE;
    int16_t height = tft.height()-y_screen-8*2;
    tft.drawFastHLine(x_screen, y_screen, width, WHITE);
    tft.drawFastHLine(x_screen, y_screen+height, width, WHITE);
    tft.drawFastVLine(x_screen, y_screen, height, WHITE);
    tft.drawFastVLine(x_screen+width, y_screen, height, WHITE);

    uint32_t start_fq = results[0].fq;
    uint32_t end_fq = results[results_len-1].fq;

    // add some axes labels fq min/max, swr 1.5, 3
    tft.setTextSize(LABEL_TEXT_SIZE);
    tft.setTextColor(GRAY);
    draw_swr_label(frequency_formatter(start_fq), start_fq, 1.0, start_fq, end_fq, 5, 1, x_screen, y_screen, width, height, analyzer);
    draw_swr_label(frequency_formatter(end_fq), end_fq, 1.0, start_fq, end_fq, 5, 1, x_screen, y_screen, width, height, analyzer);
    draw_swr_label(1.5, start_fq, 1.5, start_fq, end_fq, 5, 1, x_screen, y_screen, width, height, analyzer);
    draw_swr_label(3.0, start_fq, 3.0, start_fq, end_fq, 5, 1, x_screen, y_screen, width, height, analyzer);
    tft.setTextColor(WHITE);

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

    //TODO: what do we do if none of the SWR falls into plottable region?

    if (results_len == 1) {
        int16_t xy[2];
        translate_to_screen(results[0].fq, compute_swr(analyzer->calibrated_gamma(results[0])), start_fq, end_fq, 5, 1, x_screen, y_screen, width, height, xy);
        tft.fillCircle(xy[0], xy[1], 3, YELLOW);
    } else {
        for (size_t i=0; i<results_len-1; i++) {
            float swr = compute_swr(analyzer->calibrated_gamma(results[i]));
            int16_t xy_start[2];
            translate_to_screen(results[i].fq, swr, start_fq, end_fq, 5, 1, x_screen, y_screen, width, height, xy_start);
            int16_t xy_end[2];
            translate_to_screen(results[i+1].fq, compute_swr(analyzer->calibrated_gamma(results[i+1])), start_fq, end_fq, 5, 1, x_screen, y_screen, width, height, xy_end);
            Serial.println(String("drawing line ")+xy_start[0]+","+xy_start[1]+" to "+xy_end[0]+","+xy_end[1]);
            tft.drawLine(xy_start[0], xy_start[1], xy_end[0], xy_end[1], YELLOW);
        }
    }
}

void draw_swr_pointer(const AnalysisPoint* results, size_t results_len, size_t swr_i, const Analyzer* analyzer) {
    // x ranges from start fq to end fq
    // y ranges from 1 to 5
    int16_t x_screen = 8*4;
    int16_t y_screen = 8*TITLE_TEXT_SIZE*2;
    int16_t width = tft.width()-x_screen-5*6*LABEL_TEXT_SIZE;
    int16_t height = tft.height()-y_screen-8*2;

    if (results_len == 0) {
        return;
    }
    uint32_t start_fq = results[0].fq;
    uint32_t end_fq = results[results_len-1].fq;

    //first clear the old swr pointer
    if(pointer_patch_x < tft.width() && pointer_patch_y < tft.height()) {
        tft.drawRGBBitmap(pointer_patch_x, pointer_patch_y, pointer_patch, POINTER_WIDTH, POINTER_HEIGHT);
    }

    //draw the "pointer"
    int16_t xy_pointer[2];
    translate_to_screen(results[swr_i].fq, compute_swr(analyzer->calibrated_gamma(results[swr_i])), start_fq, end_fq, 5, 1, x_screen, y_screen, width, height, xy_pointer);
    pointer_patch_x = xy_pointer[0]-POINTER_WIDTH/2;
    pointer_patch_y = xy_pointer[1];
    read_patch(pointer_patch_x, pointer_patch_y, POINTER_WIDTH, POINTER_HEIGHT, pointer_patch);
    tft.drawTriangle(xy_pointer[0], xy_pointer[1], xy_pointer[0]-POINTER_WIDTH/2, xy_pointer[1]+POINTER_HEIGHT-1, xy_pointer[0]+POINTER_WIDTH/2-1, xy_pointer[1]+POINTER_HEIGHT-1, GREEN);
}

size_t draw_swr_title(const AnalysisPoint* results, size_t results_len, size_t swr_i, const Analyzer* analyzer) {
    if (results_len == 0) {
        tft.println("No SWR results");
        return 0;
    }

    size_t min_swr_i = 0;
    float min_swr = compute_swr(analyzer->calibrated_gamma(results[0]));
    for (size_t i=0; i<results_len; i++) {
        float swr = compute_swr(analyzer->calibrated_gamma(results[i]));
        if (swr < min_swr) {
            min_swr = swr;
            min_swr_i = i;
        }
    }

    //draw the title
    tft.fillRect(0, 0, tft.width()-8*TITLE_TEXT_SIZE*5, 8*TITLE_TEXT_SIZE*2, BLACK);
    tft.setCursor(0,0);
    tft.setTextSize(TITLE_TEXT_SIZE);

    Complex min_g = analyzer->calibrated_gamma(results[min_swr_i]);
    Complex sel_g = analyzer->calibrated_gamma(results[swr_i]);

    tft.println(String("Min @ ")+frequency_formatter(results[min_swr_i].fq)+" "+compute_swr(min_g)+"SWR");
    tft.println(String("Sel @ ")+frequency_formatter(results[swr_i].fq)+" "+compute_swr(sel_g)+"SWR");

    return min_swr_i;
}

void draw_smith_coords(const AnalysisPoint* results, size_t results_len, size_t swr_i, const Analyzer* analyzer, size_t min_swr_i) {
    if(results_len == 0) {
        return;
    }
    Complex min_g = analyzer->calibrated_gamma(results[min_swr_i]);
    Complex min_z = compute_z(min_g, analyzer->z0_);
    Complex sel_g = analyzer->calibrated_gamma(results[swr_i]);
    Complex sel_z = compute_z(sel_g, analyzer->z0_);

    tft.fillRect(0, tft.height()-2*8*TITLE_TEXT_SIZE, tft.width(), 8*TITLE_TEXT_SIZE*2, BLACK);
    tft.setCursor(0, tft.height()-2*8*TITLE_TEXT_SIZE);
    tft.setTextSize(TITLE_TEXT_SIZE);
    tft.println(String("Min X: ")+min_z.real()+" R: "+min_z.imag());
    tft.println(String("Sel X: ")+sel_z.real()+" R: "+sel_z.imag());
}

void draw_smith_title(const AnalysisPoint* results, size_t results_len, size_t swr_i, const Analyzer* analyzer) {
    size_t min_swr_i = draw_swr_title(results, results_len, swr_i, analyzer);
    draw_smith_coords(results, results_len, swr_i, analyzer, min_swr_i);
}

template<class T>
void draw_smith_label(const T label, const Complex z, float x_min, float x_max, float y_min, float y_max, int16_t x_screen, int16_t y_screen, int16_t width, int16_t height, const Analyzer* analyzer) {
    int16_t label_xy[2];
    Complex label_g;

    label_g = compute_gamma(z, analyzer->z0_);
    translate_to_screen(label_g.real(), label_g.imag(), x_min, x_max, y_min, y_max, x_screen, y_screen, width, height, label_xy);
    tft.fillCircle(label_xy[0], label_xy[1], 2, WHITE);
    tft.setCursor(label_xy[0]+6*LABEL_TEXT_SIZE/2, label_xy[1]+8*LABEL_TEXT_SIZE/2);
    tft.print(label);
}

void graph_smith(const AnalysisPoint* results, size_t results_len, const Analyzer* analyzer) {
    Serial.println(String("graphing swr plot with ")+results_len+" points");
    Serial.flush();
    pointer_patch_x = tft.width();
    pointer_patch_y = tft.height();

    // area is just below the title
    tft.fillRect(0, 5*2*8, tft.width(), 3*8*3, BLACK);
    tft.fillRect(0, 8*TITLE_TEXT_SIZE, tft.width(), tft.height()-8*TITLE_TEXT_SIZE, BLACK);

    // draw axes
    int16_t x_screen = 8*2;
    int16_t y_screen = -(tft.width()-x_screen-tft.height())/2;//8*TITLE_TEXT_SIZE*2;
    int16_t width = tft.width()-x_screen;
    int16_t height = width;//tft.height()-y_screen-8*2;

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

    // draw some axes labels: z=z0, z=z0*2, z=z0/2, z=1j, z=-1j
    tft.setTextSize(LABEL_TEXT_SIZE);
    tft.setTextColor(GRAY);

    draw_smith_label(analyzer->z0_, Complex(analyzer->z0_, 0), x_min, x_max, y_min, y_max, x_screen, y_screen, width, height, analyzer);
    draw_smith_label(analyzer->z0_*2, Complex(analyzer->z0_*2.0, 0), x_min, x_max, y_min, y_max, x_screen, y_screen, width, height, analyzer);
    draw_smith_label(analyzer->z0_/2.0, Complex(analyzer->z0_/2.0, 0), x_min, x_max, y_min, y_max, x_screen, y_screen, width, height, analyzer);
    draw_smith_label(analyzer->z0_, Complex(analyzer->z0_, analyzer->z0_), x_min, x_max, y_min, y_max, x_screen, y_screen, width, height, analyzer);
    draw_smith_label(analyzer->z0_, Complex(analyzer->z0_, -analyzer->z0_), x_min, x_max, y_min, y_max, x_screen, y_screen, width, height, analyzer);

    tft.setTextColor(WHITE);

    //cutoff swr circles
    int16_t center[2];
    translate_to_screen(0, 0, x_min, x_max, y_min, y_max, x_screen, y_screen, width, height, center);

    int16_t swr_3[2];
    translate_to_screen(0.5, 0, x_min, x_max, y_min, y_max, x_screen, y_screen, width, height, swr_3);
    assert(abs(compute_swr(Complex(0.5, 0))-3.0) < 0.001);
    swr_3[0] -= center[0];
    tft.drawCircle(x_screen+(width/2), y_screen+(height/2), swr_3[0], RED);

    int16_t swr_15[2];
    translate_to_screen(0.2, 0, x_min, x_max, y_min, y_max, x_screen, y_screen, width, height, swr_15);
    assert(abs(compute_swr(Complex(0.2, 0))-1.5) < 0.001);
    swr_15[0] -= center[0];
    tft.drawCircle(x_screen+width/2, y_screen+height/2, swr_15[0], MAGENTA);

    // draw all the analysis points
    if (results_len == 0) {
        Serial.println("no results to plot");
        return;
    } else if (results_len == 1) {
        int16_t xy[2];
        Complex g = analyzer->calibrated_gamma(results[0]);
        translate_to_screen(g.real(), g.imag(), x_min, x_max, y_min, y_max, x_screen, y_screen, width, height, xy);
        tft.fillCircle(xy[0], xy[1], 3, YELLOW);
    } else {
        for (size_t i=0; i<results_len-1; i++) {
            Complex g_start = analyzer->calibrated_gamma(results[i]);
            Complex g_end = analyzer->calibrated_gamma(results[i+1]);
            int16_t xy_start[2];
            translate_to_screen(g_start.real(), g_start.imag(), x_min, x_max, y_min, y_max, x_screen, y_screen, width, height, xy_start);
            int16_t xy_end[2];
            translate_to_screen(g_end.real(), g_end.imag(), x_min, x_max, y_min, y_max, x_screen, y_screen, width, height, xy_end);
            Serial.println(String("drawing line ")+xy_start[0]+","+xy_start[1]+" to "+xy_end[0]+","+xy_end[1]);
            tft.drawLine(xy_start[0], xy_start[1], xy_end[0], xy_end[1], YELLOW);
        }
    }
}

void draw_smith_pointer(const AnalysisPoint* results, size_t results_len, size_t swr_i, const Analyzer* analyzer) {
    int16_t x_screen = 8*2;
    int16_t y_screen = -(tft.width()-x_screen-tft.height())/2;//8*TITLE_TEXT_SIZE*2;
    int16_t width = tft.width()-x_screen;
    int16_t height = width;//tft.height()-y_screen-8*2;

    // gamma range, x is real, y is imag
    float x_min = -1;
    float x_max = 1;
    float y_min = 1;
    float y_max = -1;

    if (results_len == 0) {
        return;
    }
    uint32_t start_fq = results[0].fq;
    uint32_t end_fq = results[results_len-1].fq;

    //first clear the old swr pointer
    if(pointer_patch_x < tft.width() && pointer_patch_y < tft.height()) {
        tft.drawRGBBitmap(pointer_patch_x, pointer_patch_y, pointer_patch, POINTER_WIDTH, POINTER_HEIGHT);
    }

    //draw the "pointer"
    int16_t xy_pointer[2];
    Complex gamma_pointer = analyzer->calibrated_gamma(results[swr_i]);
    translate_to_screen(gamma_pointer.real(), gamma_pointer.imag(), x_min, x_max, y_min, y_max, x_screen, y_screen, width, height, xy_pointer);
    pointer_patch_x = xy_pointer[0]-POINTER_WIDTH/2;
    pointer_patch_y = xy_pointer[1];
    read_patch(pointer_patch_x, pointer_patch_y, POINTER_WIDTH, POINTER_HEIGHT, pointer_patch);
    tft.drawTriangle(xy_pointer[0], xy_pointer[1], xy_pointer[0]-POINTER_WIDTH/2, xy_pointer[1]+POINTER_HEIGHT-1, xy_pointer[0]+POINTER_WIDTH/2-1, xy_pointer[1]+POINTER_HEIGHT-1, GREEN);
}

#endif //_GRAPH_H
