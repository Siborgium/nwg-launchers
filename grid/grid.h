/* GTK-based application grid
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <iostream>
#include <fstream>
#include <filesystem>
#include <optional>

#include <gtkmm.h>
#include <glibmm/ustring.h>

#include <nlohmann/json.hpp>

#include "nwgconfig.h"
#include "nwg_classes.h"

namespace fs = std::filesystem;
namespace ns = nlohmann;

extern bool pins;
extern std::string wm;

extern int num_col;

extern std::filesystem::path    pinned_file;
extern std::vector<std::string> pinned;
extern ns::json                 cache;
extern std::filesystem::path    cache_file;

class GridBox : public AppBox {
public:
    enum FavTag: bool {
        Common = 0,
        Favorite = 1,
    };
    enum PinTag: bool {
        Unpinned = 0,
        Pinned = 1,
    };

    /* name, exec, comment, favorite, pinned */
    GridBox(Glib::ustring, Glib::ustring, Glib::ustring, FavTag, PinTag);
    bool on_button_press_event(GdkEventButton*) override;
    bool on_focus_in_event(GdkEventFocus*) override;
    void on_enter() override;
    void on_activate() override;


    FavTag favorite;
    PinTag pinned;
};

class MainWindow : public CommonWindow {
    public:
        MainWindow();

        Gtk::SearchEntry searchbox;              // Search apps
        Gtk::Label description;                  // To display .desktop entry Comment field at the bottom
        Gtk::FlowBox apps_grid;                  // All application buttons grid
        Gtk::FlowBox favs_grid;                  // Favourites grid above
        Gtk::FlowBox pinned_grid;                // Pinned entries grid above
        Gtk::Separator separator;                // between favs and all apps
        Gtk::Separator separator1;               // below pinned
        Gtk::VBox outer_vbox;
        Gtk::VBox inner_vbox;
        Gtk::HBox hbox_header;
        Gtk::HBox pinned_hbox;
        Gtk::HBox favs_hbox;
        Gtk::HBox apps_hbox;
        Gtk::ScrolledWindow scrolled_window;

        template <typename ... Args>
        GridBox& emplace_box(Args&& ... args);      // emplace box

        bool has_fav_with_exec(const std::string&) const;
        void build_grids();
        void toggle_pinned(GridBox& box);
        void set_description(const Glib::ustring&);
    protected:
        //Override default signal handler:
        bool on_key_press_event(GdkEventKey*) override;
        bool on_delete_event(GdkEventAny*) override;
        bool on_button_press_event(GdkEventButton*) override;
    private:
        std::list<GridBox> all_boxes {};         // stores all applications buttons
        std::vector<GridBox*> apps_boxes {};     // boxes attached to apps_grid
        std::vector<GridBox*> filtered_boxes {}; // filtered boxes from
        std::vector<GridBox*> fav_boxes {};      // attached to favs_grid
        std::vector<GridBox*> pinned_boxes {};   // attached to pinned_grid
        bool pins_changed = false;
        bool is_filtered = false;

        void focus_first_box();
        void filter_view();
        void refresh_separators();
};

template <typename ... Args>
GridBox& MainWindow::emplace_box(Args&& ... args) {
    auto& ab = this -> all_boxes.emplace_back(std::forward<Args>(args)...);
    if (ab.pinned) {
        pinned_boxes.push_back(&ab);
    } else if (ab.favorite) {
        fav_boxes.push_back(&ab);
    } else {
        apps_boxes.push_back(&ab);
    }
    return ab;
}

struct CacheEntry {
    std::string exec;
    int clicks;
    CacheEntry(std::string, int);
};

/*
 * Function declarations
 * */
ns::json                    get_cache(fs::path&);
fs::path                    get_cache_path(void);
fs::path                    get_pinned_path(void);
std::vector<std::string>    get_pinned(fs::path&);
std::vector<CacheEntry>     get_favourites(ns::json&&, int);
std::optional<DesktopEntry> desktop_entry(std::string&&, const std::string&);
std::vector<fs::path>       get_app_dirs(void);
std::vector<fs::path>       list_entries(const std::vector<fs::path>&);
