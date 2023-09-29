#ifndef _MENU_MANAGER_H
#define _MENU_MANAGER_H

#include<assert.h>

struct Menu;

struct MenuOption {
    MenuOption(String l, int oid, Menu* sm) {
        label = l;
        option_id = oid;
        sub_menu = sm;
    }
    String label;
    int option_id;
    Menu* sub_menu;
};

struct Menu {
    Menu(Menu* p, MenuOption* o, size_t o_count) {
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
    MenuOption* options;
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

        void select_up() {
            select(current_menu_->selected_option+1);
        }

        void select_down() {
            select(current_menu_->selected_option-1);
        }

        void expand() {
            assert(current_menu_->selected_option < current_menu_->option_count);
            MenuOption* current_option = &(current_menu_->options[current_menu_->selected_option]);
            if (current_option_ >= 0) {
                return;
            }
            if (current_option->sub_menu == NULL) {
                current_option_ = current_option->option_id;
            } else {
                current_menu_ = current_option->sub_menu;
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
        int current_option_;

    private:
};

#endif
