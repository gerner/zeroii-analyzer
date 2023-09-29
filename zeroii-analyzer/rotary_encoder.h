#ifndef _ROTARY_ENCODER_H
#define _ROTARY_ENCODER_H

#define ROTARY_ENCODER_ACCEL_BASE 1.005

#define POWERS_OF_TEN_MAX 6
const int32_t POWERS_OF_TEN[] = {1, 10, 100, 1000, 10000, 100000, 1000000};//, 10000000, 100000000};

class RotaryEncoder {
    public:
        RotaryEncoder(int CLK, int DT, int SW) {
            CLK_ = CLK;
            DT_ = DT;
            SW_ = SW;
            last_update_ = 0;
            last_button_press_ = 0;
            turns_so_far_ = 0;
            last_turn_ = 0;

            turn_ = 0;
            click_ = false;
            press_ = false;
        }

        void initialize() {
            pinMode(CLK_, INPUT);   // Set encoder pins as inputs
            pinMode(DT_, INPUT);
            pinMode(SW_, INPUT_PULLUP);
            last_state_CLK_ = digitalRead(CLK_);  // Read the initial state of CLK
        }

        void update() {
            unsigned long now = millis();
            // handle encoder turning
            current_state_CLK_ = digitalRead(CLK_);  // Read the current state of CLK
            if (current_state_CLK_ != last_state_CLK_ && current_state_CLK_ == 1) {
                if (now - last_turn_ > 500) {
                    // stop for half a second and acceleration resets
                    turns_so_far_ = 1;
                } else if(now - last_turn_ < 100) {
                    // spin really fast and we accelerate turn rate
                    turns_so_far_ += 1;
                } // otherwise we keep turn rate constant
                if (digitalRead(DT_) != current_state_CLK_) {
                    turn_ = 1;
                } else {
                    turn_ = -1;
                }
                last_turn_ = now;
            } else {
                turn_ = 0;
            }
            last_state_CLK_ = current_state_CLK_;

            // handle encoder clicking
            click_ = false;
            int button_state = digitalRead(SW_);
            if (button_state == LOW) {
                if (!press_ && now - last_button_press_ > 100) {
                    click_ = true;
                }
                press_ = true;
                last_button_press_ = now;
            } else {
                press_ = false;
            }
            last_update_ = now;
        }

        int32_t acceleration() {
            size_t bucket = ceil(turns_so_far_/20.0)-1;
            if (bucket > POWERS_OF_TEN_MAX) {
                bucket = POWERS_OF_TEN_MAX;
            }
            return int32_t(turn_) * POWERS_OF_TEN[bucket];
        }

        int turn_;
        bool click_;
        bool press_;

    private:
        int CLK_, DT_, SW_;
        int current_state_CLK_;
        int last_state_CLK_;
        unsigned long last_update_;
        unsigned long last_button_press_;
        unsigned long turns_so_far_;
        unsigned long last_turn_;
};

#endif
