#ifndef _MENU_MANAGER_H
#define _MENU_MANAGER_H

#include<assert.h>

struct Menu;

struct MenuOption {
    MenuOption() : sub_menu(NULL) { }
    MenuOption(String l, uint16_t oid, Menu* sm) {
        label = l;
        option_id = oid;
        sub_menu = sm;
    }
    String label;
    uint8_t option_id;
    Menu* sub_menu;
};

struct Menu {
    Menu(Menu* p, const MenuOption* o, size_t o_count) {
        parent = p;
        options = o;
        option_count = o_count;
        selected_option = 0;

        //set us as parent
        for (size_t i=0; i<option_count; i++) {
            if (options[i].sub_menu != NULL) {
                options[i].sub_menu->parent = this;
            }
        }
    }

    Menu* parent;
    const MenuOption* options;
    size_t option_count;
    size_t selected_option;
};

class MenuManager {
    public:
        MenuManager(Menu* root_menu) {
            root_menu_ = root_menu;
            current_menu_ = root_menu;
            current_option_ = -1;
        }

        void select(size_t i) {
            if (i < current_menu_->option_count) {
                current_menu_->selected_option = i;
            }
        }

        void select_rel(int32_t delta) {
            if (current_menu_->option_count > 0) {
                select(constrain((int32_t)current_menu_->selected_option+delta, 0, current_menu_->option_count-1));
            }
        }

        void select_up() {
            select(current_menu_->selected_option+1);
        }

        void select_down() {
            select(current_menu_->selected_option-1);
        }

        bool select_option(uint16_t option_id) {
            return select_option(root_menu_, option_id);
        }

        void expand() {
            assert(current_menu_->selected_option < current_menu_->option_count);
            const MenuOption* current_option = &(current_menu_->options[current_menu_->selected_option]);
            if (current_option_ >= 0) {
                return;
            }
            if (current_option->sub_menu == NULL) {
                current_option_ = current_option->option_id;
            } else {
                current_menu_ = current_option->sub_menu;
                assert(current_option_ == -1);
            }
        }

        void collapse() {
            if (current_option_ >= 0) {
                current_option_ = -1;
            } else {
                if (current_menu_ == root_menu_) {
                    return;
                }
                assert(current_menu_->parent != NULL);
                current_menu_->selected_option = 0;
                current_menu_ = current_menu_->parent;
            }
        }

        Menu* root_menu_;
        Menu* current_menu_;
        int16_t current_option_;

    private:
        bool select_option(Menu* m, uint16_t option_id) {
            for(size_t i; i<m->option_count; i++) {
                if(m->options[i].option_id == option_id) {
                    current_menu_ = m;
                    select(i);
                    return true;
                } else if(m->options[i].sub_menu) {
                    if(select_option(m->options[i].sub_menu, option_id)) {
                        return true;
                    }
                }
            }
            return false;
        }
};

#endif
