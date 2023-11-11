#ifndef _ROTARY_ENCODER_H
#define _ROTARY_ENCODER_H

#define ROTARY_ENCODER_ACCEL_BASE 1.005

class RotaryEncoder {
    public:
        RotaryEncoder(int CLK, int DT, int SW) {
            CLK_ = CLK;
            DT_ = DT;
            SW_ = SW;
            period_ = 500;

            last_update_ = 0;
            last_button_press_ = 0;
            turns_so_far_ = 0;
            speed_ = 0;
            time_last_ = 0;

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

            if (now - time_last_ > 500) {
                speed_ = turns_so_far_ / (1000 / period_);
                turns_so_far_ = 0;
                time_last_ = now;
            }
            // handle encoder turning
            current_state_CLK_ = digitalRead(CLK_);  // Read the current state of CLK
            if (current_state_CLK_ != last_state_CLK_ && current_state_CLK_ == 1) {
                turns_so_far_++;
                if (digitalRead(DT_) != current_state_CLK_) {
                    if (turn_ == 0) { //debounce?
                        turn_ = 1;
                    }
                } else {
                    if (turn_ == 0) { //debounce?
                        turn_ = -1;
                    }
                }
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

        uint16_t speed() {
            Serial.println(turns_so_far_);
            return speed_;
        }

        int turn_;
        bool click_;
        bool press_;

    private:
        int CLK_, DT_, SW_;
        uint16_t period_;
        int current_state_CLK_;
        int last_state_CLK_;
        uint16_t last_update_;
        uint16_t last_button_press_;
        uint16_t turns_so_far_;
        uint16_t speed_;
        uint32_t time_last_;
};

#endif
