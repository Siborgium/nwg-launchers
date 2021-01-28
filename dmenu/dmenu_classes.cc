/* GTK-based dmenu
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * *
 * Credits for window transparency go to AthanasiusOfAlex at https://stackoverflow.com/a/21460337
 * transparent.cpp
 * Code adapted from 'alphademo.c' by Mike
 * (http://plan99.net/~mike/blog--now a dead link--unable to find it.)
 * as modified by karlphillip for StackExchange:
 * (https://stackoverflow.com/questions/3908565/how-to-make-gtk-window-background-transparent)
 * Re-worked for Gtkmm 3.0 by Louis Melahn, L.C. January 31, 2014.
 * */

#include "dmenu.h"

Anchor::Anchor(DMenu *menu) : menu{menu} {}

bool Anchor::on_focus_in_event(GdkEventFocus* focus_event) {
    (void) focus_event; // suppress warning

    Gdk::Gravity gravity_widget {Gdk::GRAVITY_CENTER};
    Gdk::Gravity gravity_menu {Gdk::GRAVITY_CENTER};
    if (wm == "sway" || wm == "i3") {
        if (v_align == "t") {
            gravity_widget = Gdk::GRAVITY_NORTH;
            gravity_menu = Gdk::GRAVITY_NORTH;
        } else if (v_align == "b") {
            gravity_widget = Gdk::GRAVITY_SOUTH;
            gravity_menu = Gdk::GRAVITY_SOUTH;
        }
    } else {
        if (v_align == "t") {
            gravity_widget = Gdk::GRAVITY_SOUTH;
            gravity_menu = Gdk::GRAVITY_SOUTH;
        } else if (v_align == "b") {
            gravity_widget = Gdk::GRAVITY_NORTH;
            gravity_menu = Gdk::GRAVITY_NORTH;
        }
    }
    menu->popup_at_widget(this, gravity_widget, gravity_menu, nullptr);

    return true;
}

inline auto set_searchbox_placeholder = [](auto && searchbox, auto case_sensitive) {
    constexpr std::array placeholders { "TYPE TO SEARCH", "Type to Search" };
    searchbox.set_placeholder_text(placeholders[case_sensitive]);
};

inline auto build_commands_list = [](auto && dmenu, auto && commands, auto max) {
    decltype(max) count{ 0 };
    for (auto && command: commands) {
        dmenu.emplace_back(command);
        count++;
        if (count == max) {
            break;
        }
    }
};

/*
 * 
 */
DMenu::DMenu(Gtk::Window& main): main{main} {
    set_searchbox_placeholder(searchbox, case_sensitive);
    searchbox.set_sensitive(true);
    searchbox.set_name("searchbox");
    searchbox.signal_search_changed()
        .connect(sigc::mem_fun(*this, &DMenu::filter_view));
    if (show_searchbox) {
        auto search_item = Gtk::manage(new Gtk::MenuItem{ searchbox });
        search_item->set_name("search_item");
        append(*search_item);
    }
    build_commands_list(*this, all_commands, rows);
    fix_selection();
}

DMenu::~DMenu() {
    using namespace std::string_view_literals;
    if (case_sensitivity_changed) {
        std::ofstream file{ settings_file, std::ios::trunc };
        constexpr std::array values { "case_insensitive"sv, "case_sensitive"sv };
        file << values[case_sensitive];
    }
}

void DMenu::emplace_back(const Glib::ustring& cmd) {
    auto item = Gtk::manage(new Gtk::MenuItem{ cmd });
    item->signal_activate()
        .connect(sigc::bind<Glib::ustring>(sigc::mem_fun(*this, &DMenu::on_item_clicked), cmd));
    append(*item);
    if (!first_item) {
        first_item = item;
    }
}


void DMenu::switch_case_sensitivity() {
    case_sensitivity_changed = true;
    case_sensitive = !case_sensitive;
    searchbox.set_text("");
    set_searchbox_placeholder(searchbox, case_sensitive);
}

void DMenu::fix_selection() {
    if (first_item) {
        this->select_item(*first_item);
        first_item->grab_focus();
    }
}

bool DMenu::on_key_press_event(GdkEventKey* key_event) {
    if (show_searchbox) {
        switch (key_event->keyval) {
            case GDK_KEY_Escape:
                main.close();
                break;
            case GDK_KEY_Delete:
                searchbox.set_text("");
                break;
            case GDK_KEY_Insert:
                case_sensitive = !case_sensitive;
                switch_case_sensitivity();
                searchbox.set_text("");
                break;
            case GDK_KEY_Left:
            case GDK_KEY_Right:
            case GDK_KEY_Up:
            case GDK_KEY_Down:
                // seem to work fine as is
                break;
            case GDK_KEY_Return:
                fix_selection();
                break;
            default:
                searchbox.grab_focus();
                searchbox.select_region(0, 0);
                searchbox.set_position(-1);
        }
    }
    //if the event has not been handled, call the base class
    return Gtk::Menu::on_key_press_event(key_event);
}

void DMenu::on_item_clicked(Glib::ustring cmd) {
    if (dmenu_run) {
        cmd = cmd + " &";
        const char *command = cmd.c_str();
        std::system(command);
    } else {
        std::cout << cmd;
    }
    main.close();
}

/* Rebuild menu to match the search phrase */
void DMenu::filter_view() {
    auto clear_children = [this]() {
        this->foreach([this](auto && child) {
            if (child.get_name() != "search_item") {
                this->remove(child);
            }
        });
        this->first_item = nullptr;
    };
    auto search_phrase = searchbox.get_text();
    if (search_phrase.size() > 0) {
        // remove all items except searchbox
        clear_children();
        int cnt = 0;
        bool limit_exhausted = false;
        for (Glib::ustring command : all_commands) {
            auto sf = search_phrase;
            auto cm = command;
            if (!case_sensitive) {
                sf = sf.uppercase();
                cm = cm.uppercase();
            }
            if (cm.find(sf) == 0) {
                emplace_back(command);
                // This will highlight 1st menu item, still it won't start on Enter.
                // See workaround in on_key_press_event.
                //if (cnt == 0) {
                //    item -> select();
                //}
                cnt++;
                if (cnt > rows - 1) {
                    limit_exhausted = true;
                    break;
                }
            }
        }
        if (!limit_exhausted) {
            for (Glib::ustring command : all_commands) {
                auto sf = search_phrase;
                auto cm = command;
                if (!case_sensitive) {
                    sf = sf.uppercase();
                    cm = cm.uppercase();
                }
                if (cm.find(sf) != cm.npos && cm.find(sf) != 0) {
                    emplace_back(command);
                    cnt++;
                    if (cnt > rows - 1) {
                        break;
                    }
                }
            }
        }
        this -> show_all();

    } else {
        set_searchbox_placeholder(searchbox, case_sensitive);
        // remove all items except searchbox
        clear_children();
        build_commands_list(*this, all_commands, rows);
        this -> show_all();
    }
    fix_selection();

}

MainWindow::MainWindow() : CommonWindow("~nwgdmenu", "~nwgdmenu"), menu(nullptr) {
    if (wm == "dwm" || wm == "bspwm" || wm == "qtile" || wm == "bspwm" || wm == "tiling") {
        fullscreen();
    } else if (wm == "sway" || wm == "i3") {
        set_type_hint(Gdk::WINDOW_TYPE_HINT_SPLASHSCREEN);
    }
    set_decorated(false);
}

bool MainWindow::on_button_press_event(GdkEventButton* button_event) {
    if((button_event->type == GDK_BUTTON_PRESS) && (button_event->button == 3)) {
        if(!menu->get_attach_widget()) {
          menu->attach_to_widget(*this);
        }
        if(menu) {
            menu -> popup_at_widget(this->anchor, Gdk::GRAVITY_CENTER, Gdk::GRAVITY_CENTER, nullptr);
        }
        return true; //It has been handled.
    } else {
        return false;
    }
}
