/*
 * Tools for nwg-launchers
 * Copyright (c) 2020 Érico Nogueira
 * e-mail: ericonr@disroot.org
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <stdlib.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#include <charconv>
#include <iostream>
#include <fstream>

#include "nwgconfig.h"
#include "nwg_tools.h"

// extern variables from nwg_tools.h
int image_size = 72;

// stores the name of the pid_file, for use in atexit
static std::filesystem::path pid_file;

/*
 * Returns config dir
 * */
std::filesystem::path get_config_dir(std::string_view app) {
    std::filesystem::path path = getenv("XDG_CONFIG_HOME");
    if (path.empty()) {
        path = getenv("HOME");
        if (path.empty()) {
            std::cerr << "ERROR: Couldn't find config directory, $HOME not set!\n";
            std::exit(EXIT_FAILURE);
        }
        path /= ".config";
    }
    path /= "nwg-launchers";
    path /= app;

    return path;
}
/*
 * Return runtime dir
 * */
std::filesystem::path get_runtime_dir() {
    char* xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!xdg_runtime_dir) {
        std::array<char, 64> myuid;
        auto myuid_ = getuid();
        auto [p, ec] = std::to_chars(myuid.begin(), myuid.end(), myuid_);
        if (ec != std::errc()) {
            std::cerr << "ERROR: Failed to convert UID to chars\n";
            std::exit(EXIT_FAILURE);
        }
        std::string_view myuid_view(myuid.data(), p - myuid.data());
        std::filesystem::path path = "/var/run/user";
        path /= myuid_view;
        return path;
    }
    return xdg_runtime_dir;
}
/*
 * Returns window manager name
 * */
std::string detect_wm() {
    /* Actually we only need to check if we're on sway, i3 or other WM,
     * but let's try to find a WM name if possible. If not, let it be just "other" */
    const char *env_var[3] = {"DESKTOP_SESSION", "SWAYSOCK", "I3SOCK"};
    char *env_val;
    std::string wm_name{"other"};

    for(int i=0; i<3; i++) {
        // get environment values
        env_val = getenv(env_var[i]);
        if (env_val != NULL) {
            std::string s(env_val);
            if (s.find("sway") != std::string::npos) {
                wm_name = "sway";
                break;
            } else if (s.find("i3") != std::string::npos) {
                wm_name = "i3";
                break;
            } else {
                // is the value a full path or just a name?
                if (s.find("/") == std::string::npos) {
                    // full value is the name
                    wm_name = s;
                    break;
                } else {
                    // path given
                    int idx = s.find_last_of("/") + 1;
                    wm_name = s.substr(idx);
                    break;
                }
            }
        }
    }
    return wm_name;
}

/*
 * Returns x, y, width, hight of focused display
 * */
Geometry display_geometry(const std::string& wm, Glib::RefPtr<Gdk::Display> display, Glib::RefPtr<Gdk::Window> window) {
    Geometry geo = {0, 0, 0, 0};
    if (wm == "sway") {
        try {
            auto jsonString = get_output("swaymsg -t get_outputs");
            auto jsonObj = string_to_json(jsonString);
            for (auto&& entry : jsonObj) {
                if (entry.at("focused")) {
                    auto&& rect = entry.at("rect");
                    geo.x = rect.at("x");
                    geo.y = rect.at("y");
                    geo.width = rect.at("width");
                    geo.height = rect.at("height");
                    break;
                }
            }
            return geo;
        }
        catch (...) { }
    }

    // it's going to fail until the window is actually open
    int retry = 0;
    while (geo.width == 0 || geo.height == 0) {
        Gdk::Rectangle rect;
        auto monitor = display->get_monitor_at_window(window);
        if (monitor) {
            monitor->get_geometry(rect);
            geo.x = rect.get_x();
            geo.y = rect.get_y();
            geo.width = rect.get_width();
            geo.height = rect.get_height();
        }
        retry++;
        if (retry > 100) {
            std::cerr << "\nERROR: Failed checking display geometry, tries: " << retry << "\n\n";
            break;
        }
    }
    return geo;
}

/*
 * Returns Gtk::Image out of the icon name of file path
 * TODO: Move it to the constructor or somewhere else
 *  */
Gtk::Image* app_image(const Gtk::IconTheme& icon_theme, const std::string& icon) {
    Glib::RefPtr<Gdk::Pixbuf> pixbuf;

    try {
        if (icon.find_first_of("/") == std::string::npos) {
            pixbuf = icon_theme.load_icon(icon, image_size, Gtk::ICON_LOOKUP_FORCE_SIZE);
        } else {
            pixbuf = Gdk::Pixbuf::create_from_file(icon, image_size, image_size, true);
        }
    } catch (...) {
        pixbuf = Gdk::Pixbuf::create_from_file(DATA_DIR_STR "/nwgbar/icon-missing.svg", image_size, image_size, true);
    }
    auto image = Gtk::manage(new Gtk::Image(pixbuf));

    return image;
}

/*
 * Returns current locale
 * */
std::string get_locale() {
    std::string loc = getenv("LANG");
    if (!loc.empty()) {
        auto idx = loc.find_first_of("_");
        if (idx != std::string::npos) {
            loc.resize(idx);
        }
        return loc;
    }
    return "en";
}

/*
 * Returns file content as a string
 * */
std::string read_file_to_string(const std::string& filename) {
    std::ifstream input(filename);
    std::stringstream sstr;

    while(input >> sstr.rdbuf());

    return sstr.str();
}

/*
 * Saves a string to a file
 * */
void save_string_to_file(const std::string& s, const std::string& filename) {
    std::ofstream file(filename);
    file << s;
}

/*
 * Splits string into vector of strings by delimiter
 * */
std::vector<std::string_view> split_string(std::string_view str, std::string_view delimiter) {
    std::vector<std::string_view> result;
    std::size_t current, previous = 0;
    current = str.find_first_of(delimiter);
    while (current != std::string_view::npos) {
        result.emplace_back(str.substr(previous, current - previous));
        previous = current + 1;
        current = str.find_first_of(delimiter, previous);
    }
    result.emplace_back(str.substr(previous, current - previous));

    return result;
}

/*
 * Splits string by delimiter and takes the last piece
 * */
std::string_view take_last_by(std::string_view str, std::string_view delimiter) {
    auto pos = str.find_last_of(delimiter);
    if (pos != std::string_view::npos) {
        return str.substr(pos + 1);
    }
    return str;
}

/*
 * Converts json string into a json object
 * */
ns::json string_to_json(const std::string& jsonString) {
    ns::json jsonObj;
    std::stringstream(jsonString) >> jsonObj;

    return jsonObj;
}

/*
 * Saves json into file
 * */
void save_json(const ns::json& json_obj, const std::string& filename) {
    std::ofstream o(filename);
    o << std::setw(2) << json_obj << std::endl;
}

/*
 * Sets RGBA background according to hex strings
 * Note: if `string` is #RRGGBB, alpha will not be changed
 * */
void decode_color(std::string_view string, RGBA& color) {
    std::string hex_string {"0x"};
    unsigned long int rgba;
    std::stringstream ss;
    try {
        if (string.find("#") == 0) {
            hex_string += string.substr(1);
        } else {
            hex_string += string;
        }
        ss << std::hex << hex_string;
        ss >> rgba;
        if (hex_string.size() == 8) {
            color.red = ((rgba >> 16) & 0xff) / 255.0;
            color.green = ((rgba >> 8) & 0xff) / 255.0;
            color.blue = ((rgba) & 0xff) / 255.0;
        } else if (hex_string.size() == 10) {
            color.red = ((rgba >> 24) & 0xff) / 255.0;
            color.green = ((rgba >> 16) & 0xff) / 255.0;
            color.blue = ((rgba >> 8) & 0xff) / 255.0;
            color.alpha = ((rgba) & 0xff) / 255.0;
        } else {
            std::cerr << "ERROR: invalid color value. Should be RRGGBB or RRGGBBAA\n";
        }
    }
    catch (...) {
        std::cerr << "Error parsing RGB(A) value\n";
    }
}

/*
 * Returns output of a command as string
 * */
std::string get_output(const std::string& cmd) {
    const char *command = cmd.c_str();
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}


/*
 * Remove pid_file created by create_pid_file_or_kill_pid.
 * This function will be run before exiting.
 * */
static void clean_pid_file(void) {
    unlink(pid_file.c_str());
}

/*
 * Signal handler to exit normally
 * */
static void exit_normal(int sig) {
    auto signal = strsignal(sig);
    std::cerr << "Received " << signal << ", exiting...\n";
    clean_pid_file();
    std::exit(128 + sig); // https://unix.stackexchange.com/a/99117
}

/*
 * Prints message and exits with 0 upon receving SIGTERM
 * On failure exits with EXIT_FAILURE
 * */
void set_default_sigterm_handler() {
    if (set_signal_handler(exit_normal, SIGTERM) < 0) {
        std::cerr << "ERROR: Failed to set SIGTERM handler\n";
        std::exit(EXIT_FAILURE);
    }
}

/*
 * Creates PID file for the new instance,
 * killing other instance if needed.
 *
 * This file will be removed on exit. 
 * To achieve this, this procedure sets `atexit`, SIGINT and SIGTERM handlers
 *
 * This allows for behavior where using the shortcut to open one
 * of the launchers closes the currently running one.
 * */
void register_instance(std::string_view cmd) {
    pid_file = get_runtime_dir();
    pid_file /= cmd;
    pid_file += ".pid";

    auto pid_read = std::ifstream(pid_file);
    // set to not throw exceptions
    pid_read.exceptions(std::ifstream::goodbit);
    // another instance is running, close it
    if (pid_read.is_open()) {
        pid_t saved_pid;
        pid_read >> saved_pid;
        if (saved_pid <= 0) {
            std::cerr << "ERROR: Bad pid\n";
            std::exit(EXIT_FAILURE);
        }
        if (kill(saved_pid, 0) != 0) {
            std::cerr << "ERROR: `kill` check failed\n";
            std::exit(EXIT_FAILURE);
        }
        if (kill(saved_pid, SIGTERM) != 0) {
            std::cerr << "ERROR: Failed to send SIGTERM to another instance\n";
            std::exit(EXIT_FAILURE);
        }
    }
    auto pid = getpid();
    auto pid_write = std::ofstream(pid_file);
    pid_write << pid;

    // TODO: if received a signal from other instance, do not unlink file (?)
    atexit(clean_pid_file);
    int err = 0;
    err += set_signal_handler(exit_normal, SIGTERM);
    err += set_signal_handler(exit_normal, SIGINT);
    if (err < 0) {
        std::cerr << "ERROR: Failed to set signal handlers\n";
        std::exit(EXIT_FAILURE);
    }
}

/*
 * Calls `handler` upon receiving Unix signal `signal`
 * */
int set_signal_handler(void (*sa_restorer)(int), int sig) {
    struct sigaction action;
    action.sa_flags = 0;
    action.sa_handler = sa_restorer;
    return sigaction(sig, &action, nullptr);
}
