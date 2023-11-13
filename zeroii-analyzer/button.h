#ifndef _BUTTON_H
#define _BUTTON_H

class Button {
    public:
        Button(const int SW) {
            SW_ = SW;
            press_ = false;
            last_button_press_ = 0;
        }
        void begin() {
            pinMode(SW_, INPUT_PULLUP);
        }
        bool read() {
            bool click = false;
            if (digitalRead(SW_) == LOW) {
                uint32_t now = millis();
                if (!press_ && now - last_button_press_ > 100) {
                    click = true;
                }
                press_ = true;
                last_button_press_ = now;
            } else {
                press_ = false;
            }
            return click;
        }
    private:
        int SW_;
        bool press_;
        uint32_t last_button_press_;

};

#endif //_BUTTON_H
