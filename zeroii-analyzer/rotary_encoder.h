#ifndef _ROTARY_ENCODER_H
#define _ROTARY_ENCODER_H

class RotaryEncoder {
    public:
        RotaryEncoder(int CLK, int DT, int SW) {
            CLK_ = CLK;
            DT_ = DT;
            SW_ = SW;
            last_update_ = 0;
            last_button_press_ = 0;

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
            // handle encoder turning
            current_state_CLK_ = digitalRead(CLK_);  // Read the current state of CLK
            if (current_state_CLK_ != last_state_CLK_ && current_state_CLK_ == 1) {
                if (digitalRead(DT_) != current_state_CLK_) {
                    turn_ = -1;
                } else {
                    turn_ = 1;
                }
            } else {
                turn_ = 0;
            }
            last_state_CLK_ = current_state_CLK_;

            // handle encoder clicking
            click_ = false;
            int button_state = digitalRead(SW_);
            if (button_state == LOW) {
                if (!press_ && millis() - last_button_press_ > 100) {
                    click_ = true;
                }
                press_ = true;
                last_button_press_ = millis();
            } else {
                press_ = false;
            }
            last_update_ = millis();
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
};

#endif
