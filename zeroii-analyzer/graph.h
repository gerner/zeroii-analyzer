#ifndef _GRAPH_H
#define _GRAPH_H

#include "log.h"

Logger graph_logger = Logger("graph");

String frequency_formatter(const int32_t fq) {
    int int_part, dec_part;
    char buf[3+1+3+3+1];
    if (fq >= 1ul * 1000ul * 1000ul * 1000ul) {
        int_part = fq / 1000ul / 1000ul / 1000ul;
        dec_part = fq / 1000ul / 1000ul % 1000ul;
        snprintf(buf, sizeof(buf), "%d.%03dGHz", int_part, dec_part);
        return String(buf);
    } else if (fq >= 1ul * 1000ul * 1000ul) {
        int_part = fq / 1000ul / 1000ul;
        dec_part = fq / 1000ul % 1000ul;
        snprintf(buf, sizeof(buf), "%d.%03dMHz", int_part, dec_part);
        return String(buf);
    } else if (fq >= 1ul * 1000ul) {
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

void read_patch(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t* patch) {
    for (int16_t j = 0; j < h; j++, y++) {
        for (int16_t i = 0; i < w; i++) {
            patch[j * w + i] = tft.readPixel(x+i, y);
        }
    }
}

#define POINTER_WIDTH 8
#define POINTER_HEIGHT 8

class GraphContext {
public:
    GraphContext(const AnalysisPoint* results, const size_t results_len, const Analyzer* analyzer) : swr_i_(0), results_(results), results_len_(results_len), analyzer_(analyzer) {}

    void initialize_swr() {
        uint32_t start_fq;
        uint32_t end_fq;
        if(results_len_ == 0) {
            start_fq = MIN_FQ;
            end_fq = MAX_FQ;
        } else if (results_len_ == 1) {
            start_fq = results_[0].fq-100;
            end_fq = results_[0].fq+100;
        } else {
            start_fq = results_[0].fq;
            end_fq = results_[results_len_-1].fq;
        }
        // x ranges from start fq to end fq
        // y ranges from 1 to 5
        x_min_ = start_fq;
        x_max_ = end_fq;
        y_min_ = 5;
        y_max_ = 1;

        x_screen_ = 8*4;
        y_screen_ = 8*TITLE_TEXT_SIZE*2;
        width_ = tft.width()-x_screen_-5*6*LABEL_TEXT_SIZE;
        height_ = tft.height()-y_screen_-8*2;
    }

    void initialize_smith(bool zoom) {
        // gamma range, x is real, y is imag
        x_min_ = -1;
        x_max_ = 1;
        y_min_ = 1;
        y_max_ = -1;

        // zoomed in so plot fills x dimension
        if (zoom) {
            x_screen_ = 8*2;
            y_screen_ = -(tft.width()-x_screen_-tft.height())/2;
            width_ = tft.width()-x_screen_;
            height_ = width_;
        } else {
            // zoomed out so all of plot fits y dimension
            y_screen_ = 8*2;
            x_screen_ = (tft.width()-(tft.height()-y_screen_))/2;
            height_ = tft.height()-y_screen_-1;
            width_ = height_;
        }
    }

    template<class T>
    void draw_swr_label(const T label, uint32_t fq, float swr, const Analyzer* analyzer) {
        int16_t label_xy[2];

        translate_to_screen(fq, swr, label_xy);
        tft.fillCircle(label_xy[0], label_xy[1], 2, WHITE);
        tft.setCursor(label_xy[0]-6*LABEL_TEXT_SIZE/2-4*6*LABEL_TEXT_SIZE, label_xy[1]+8*LABEL_TEXT_SIZE/2);
        tft.print(label);
    }

    void graph_swr() {
        graph_logger.info(String("graphing swr plot with ")+results_len_+" points");

        // set the pointer patch outside the graph area
        pointer_patch_x = tft.width();
        pointer_patch_y = tft.height();

        // area is just below the title
        tft.fillRect(0, 5*2*8, tft.width(), 3*8*3, BLACK);
        tft.fillRect(0, 8*TITLE_TEXT_SIZE, tft.width(), tft.height()-8*TITLE_TEXT_SIZE, BLACK);

        // draw axes
        tft.drawFastHLine(x_screen_, y_screen_, width_, WHITE);
        tft.drawFastHLine(x_screen_, y_screen_+height_, width_, WHITE);
        tft.drawFastVLine(x_screen_, y_screen_, height_, WHITE);
        tft.drawFastVLine(x_screen_+width_, y_screen_, height_, WHITE);

        uint32_t start_fq = results_[0].fq;
        uint32_t end_fq = results_[results_len_-1].fq;

        // add some axes labels fq min/max, swr 1.5, 3
        tft.setTextSize(LABEL_TEXT_SIZE);
        tft.setTextColor(GRAY);
        draw_swr_label(frequency_formatter(start_fq), start_fq, 1.0, analyzer_);
        draw_swr_label(frequency_formatter(end_fq), end_fq, 1.0, analyzer_);
        draw_swr_label(1.5, start_fq, 1.5, analyzer_);
        draw_swr_label(3.0, start_fq, 3.0, analyzer_);
        tft.setTextColor(WHITE);

        int16_t xy_cutoff[2];
        translate_to_screen(0, 3, xy_cutoff);
        tft.drawFastHLine(x_screen_, xy_cutoff[1], width_, RED);
        translate_to_screen(0, 1.5, xy_cutoff);
        tft.drawFastHLine(x_screen_, xy_cutoff[1], width_, MAGENTA);
        // draw all the analysis points
        if (results_len_ == 0) {
            graph_logger.info(F("no results to plot"));
            return;
        }

        //TODO: what do we do if none of the SWR falls into plottable region?

        if (results_len_ == 1) {
            int16_t xy[2];
            translate_to_screen(results_[0].fq, compute_swr(analyzer_->calibrated_gamma(results_[0])), xy);
            tft.fillCircle(xy[0], xy[1], 3, YELLOW);
        } else {
            for (size_t i=0; i<results_len_-1; i++) {
                float swr = compute_swr(analyzer_->calibrated_gamma(results_[i]));
                int16_t xy_start[2];
                translate_to_screen(results_[i].fq, swr, xy_start);
                int16_t xy_end[2];
                translate_to_screen(results_[i+1].fq, compute_swr(analyzer_->calibrated_gamma(results_[i+1])), xy_end);
                graph_logger.debug(String("drawing line ")+xy_start[0]+","+xy_start[1]+" to "+xy_end[0]+","+xy_end[1]);
                tft.drawLine(xy_start[0], xy_start[1], xy_end[0], xy_end[1], YELLOW);
            }
        }
    }

    void draw_swr_pointer() {
        if (results_len_ == 0) {
            return;
        }
        uint32_t start_fq = results_[0].fq;
        uint32_t end_fq = results_[results_len_-1].fq;

        //first clear the old swr pointer
        if(pointer_patch_x < tft.width() && pointer_patch_y < tft.height()) {
            tft.drawRGBBitmap(pointer_patch_x, pointer_patch_y, pointer_patch, POINTER_WIDTH, POINTER_HEIGHT);
        }

        //draw the "pointer"
        int16_t xy_pointer[2];
        translate_to_screen(results_[swr_i_].fq, compute_swr(analyzer_->calibrated_gamma(results_[swr_i_])), xy_pointer);
        pointer_patch_x = xy_pointer[0]-POINTER_WIDTH/2;
        pointer_patch_y = xy_pointer[1];
        read_patch(pointer_patch_x, pointer_patch_y, POINTER_WIDTH, POINTER_HEIGHT, pointer_patch);
        tft.drawTriangle(xy_pointer[0], xy_pointer[1], xy_pointer[0]-POINTER_WIDTH/2, xy_pointer[1]+POINTER_HEIGHT-1, xy_pointer[0]+POINTER_WIDTH/2-1, xy_pointer[1]+POINTER_HEIGHT-1, GREEN);
    }

    size_t draw_swr_title() {
        if (results_len_ == 0) {
            tft.fillRect(0, 0, tft.width()-8*TITLE_TEXT_SIZE*5, 8*TITLE_TEXT_SIZE, BLACK);
            tft.setCursor(0,0);
            tft.setTextSize(TITLE_TEXT_SIZE);
            tft.println("No SWR results");
            return 0;
        }

        size_t min_swr_i = 0;
        float min_swr = compute_swr(analyzer_->calibrated_gamma(results_[0]));
        for (size_t i=0; i<results_len_; i++) {
            float swr = compute_swr(analyzer_->calibrated_gamma(results_[i]));
            if (swr < min_swr) {
                min_swr = swr;
                min_swr_i = i;
            }
        }

        //draw the title
        tft.fillRect(0, 0, tft.width()-8*TITLE_TEXT_SIZE*5, 8*TITLE_TEXT_SIZE*2, BLACK);
        tft.setCursor(0,0);
        tft.setTextSize(TITLE_TEXT_SIZE);

        Complex min_g = analyzer_->calibrated_gamma(results_[min_swr_i]);
        Complex sel_g = analyzer_->calibrated_gamma(results_[swr_i_]);

        tft.println(String("Min @ ")+frequency_formatter(results_[min_swr_i].fq)+" "+compute_swr(min_g)+"SWR");
        tft.println(String("Sel @ ")+frequency_formatter(results_[swr_i_].fq)+" "+compute_swr(sel_g)+"SWR");

        return min_swr_i;
    }

    void draw_smith_coords(size_t min_swr_i) {
        if(results_len_ == 0) {
            return;
        }
        Complex min_g = analyzer_->calibrated_gamma(results_[min_swr_i]);
        Complex min_z = compute_z(min_g, analyzer_->z0_);
        Complex sel_g = analyzer_->calibrated_gamma(results_[swr_i_]);
        Complex sel_z = compute_z(sel_g, analyzer_->z0_);

        tft.fillRect(0, tft.height()-2*8*TITLE_TEXT_SIZE, tft.width(), 8*TITLE_TEXT_SIZE*2, BLACK);
        tft.setCursor(0, tft.height()-2*8*TITLE_TEXT_SIZE);
        tft.setTextSize(TITLE_TEXT_SIZE);
        tft.println(String("Min X: ")+min_z.real()+" R: "+min_z.imag());
        tft.println(String("Sel X: ")+sel_z.real()+" R: "+sel_z.imag());
    }

    void draw_smith_title() {
        size_t min_swr_i = draw_swr_title();
        draw_smith_coords(min_swr_i);
    }

    template<class T>
    void draw_smith_label(const T label, const Complex z) {
        int16_t label_xy[2];
        Complex label_g;

        label_g = compute_gamma(z, analyzer_->z0_);
        translate_to_screen(label_g.real(), label_g.imag(), label_xy);
        tft.fillCircle(label_xy[0], label_xy[1], 2, WHITE);
        tft.setCursor(label_xy[0]+6*LABEL_TEXT_SIZE/2, label_xy[1]+8*LABEL_TEXT_SIZE/2);
        tft.print(label);
    }

    void graph_smith() {
        graph_logger.info(String("graphing swr plot with ")+results_len_+" points");
        pointer_patch_x = tft.width();
        pointer_patch_y = tft.height();

        // area is just below the title
        tft.fillRect(0, 5*2*8, tft.width(), 3*8*3, BLACK);
        tft.fillRect(0, 8*TITLE_TEXT_SIZE, tft.width(), tft.height()-8*TITLE_TEXT_SIZE, BLACK);

        // draw axes
        // horizontal (and arcs) resistance axis
        tft.drawFastHLine(x_screen_, y_screen_+(height_/2), width_, WHITE);
        // circular reactance axis (radius 1, centered at (0.5, 0))
        tft.drawCircle(x_screen_+(width_*3/4), y_screen_+(height_/2), width_/4, WHITE);
        // and draw the outer circle for reference
        tft.drawCircle(x_screen_+(width_/2), y_screen_+(height_/2), width_/2, WHITE);

        // draw some axes labels: z=z0, z=z0*2, z=z0/2, z=1j, z=-1j
        tft.setTextSize(LABEL_TEXT_SIZE);
        tft.setTextColor(GRAY);

        draw_smith_label(analyzer_->z0_, Complex(analyzer_->z0_, 0));
        draw_smith_label(analyzer_->z0_*2, Complex(analyzer_->z0_*2.0, 0));
        draw_smith_label(analyzer_->z0_/2.0, Complex(analyzer_->z0_/2.0, 0));
        draw_smith_label(analyzer_->z0_, Complex(analyzer_->z0_, analyzer_->z0_));
        draw_smith_label(analyzer_->z0_, Complex(analyzer_->z0_, -analyzer_->z0_));

        tft.setTextColor(WHITE);

        //cutoff swr circles
        int16_t center[2];
        translate_to_screen(0, 0, center);

        int16_t swr_3[2];
        translate_to_screen(0.5, 0, swr_3);
        assert(abs(compute_swr(Complex(0.5, 0))-3.0) < 0.001);
        swr_3[0] -= center[0];
        tft.drawCircle(x_screen_+(width_/2), y_screen_+(height_/2), swr_3[0], RED);

        int16_t swr_15[2];
        translate_to_screen(0.2, 0, swr_15);
        assert(abs(compute_swr(Complex(0.2, 0))-1.5) < 0.001);
        swr_15[0] -= center[0];
        tft.drawCircle(x_screen_+width_/2, y_screen_+height_/2, swr_15[0], MAGENTA);

        // draw all the analysis points
        if (results_len_ == 0) {
            graph_logger.info(F("no results to plot"));
            return;
        } else if (results_len_ == 1) {
            int16_t xy[2];
            Complex g = analyzer_->calibrated_gamma(results_[0]);
            translate_to_screen(g.real(), g.imag(), xy);
            tft.fillCircle(xy[0], xy[1], 3, YELLOW);
        } else {
            for (size_t i=0; i<results_len_-1; i++) {
                Complex g_start = analyzer_->calibrated_gamma(results_[i]);
                Complex g_end = analyzer_->calibrated_gamma(results_[i+1]);
                int16_t xy_start[2];
                translate_to_screen(g_start.real(), g_start.imag(), xy_start);
                int16_t xy_end[2];
                translate_to_screen(g_end.real(), g_end.imag(), xy_end);
                graph_logger.debug(String("drawing line ")+xy_start[0]+","+xy_start[1]+" to "+xy_end[0]+","+xy_end[1]);
                tft.drawLine(xy_start[0], xy_start[1], xy_end[0], xy_end[1], YELLOW);
            }
        }
    }

    void draw_smith_pointer() {
        if (results_len_ == 0) {
            return;
        }

        //first clear the old swr pointer
        if(pointer_patch_x < tft.width() && pointer_patch_y < tft.height()) {
            tft.drawRGBBitmap(pointer_patch_x, pointer_patch_y, pointer_patch, POINTER_WIDTH, POINTER_HEIGHT);
        }

        //draw the "pointer"
        int16_t xy_pointer[2];
        Complex gamma_pointer = analyzer_->calibrated_gamma(results_[swr_i_]);
        translate_to_screen(gamma_pointer.real(), gamma_pointer.imag(), xy_pointer);
        pointer_patch_x = xy_pointer[0]-POINTER_WIDTH/2;
        pointer_patch_y = xy_pointer[1];
        read_patch(pointer_patch_x, pointer_patch_y, POINTER_WIDTH, POINTER_HEIGHT, pointer_patch);
        tft.drawTriangle(xy_pointer[0], xy_pointer[1], xy_pointer[0]-POINTER_WIDTH/2, xy_pointer[1]+POINTER_HEIGHT-1, xy_pointer[0]+POINTER_WIDTH/2-1, xy_pointer[1]+POINTER_HEIGHT-1, GREEN);
    }

    void incr_swri(int32_t turn) {
        swr_i_ = constrain((int32_t)swr_i_+turn, 0, results_len_-1);
    }

private:
    int16_t pointer_patch_x, pointer_patch_y;
    uint16_t pointer_patch[POINTER_WIDTH*POINTER_HEIGHT];
    size_t swr_i_;
    const AnalysisPoint* results_;
    size_t results_len_;
    const Analyzer* analyzer_;

    float x_min_;
    float x_max_;
    float y_min_;
    float y_max_;

    int16_t x_screen_;
    int16_t y_screen_;
    int16_t width_;
    int16_t height_;

    void translate_to_screen(float x_in, float y_in, int16_t* xy) {
        float x_range = x_max_ - x_min_;
        float y_range = y_max_ - y_min_;
        xy[0] = (x_in - x_min_) / x_range * width_ + x_screen_;
        xy[1] = (y_in - y_min_) / y_range * height_ + y_screen_;

        graph_logger.debug(String(x_in)+" -> "+xy[0]+" "+y_in+" -> "+xy[1]);
    }
};

#endif //_GRAPH_H
