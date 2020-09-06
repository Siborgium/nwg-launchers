
#include <sys/types.h>
#include <signal.h>

#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

#include <nwg_tools.h>

int main() {
    auto pid_path = get_runtime_dir();
    pid_path /= "nwggrid.pid";
    auto pid_file = std::ifstream(pid_path);
    pid_file.exceptions(std::ifstream::goodbit);
    if (!pid_file.is_open()) {
        std::cerr << "ERROR: Unimplemented\n";
        std::cerr << "ERROR: Daemon is not active\n";
        return EXIT_FAILURE;
    }
    pid_t daemon_pid;
    pid_file >> daemon_pid;

    if (daemon_pid <= 0) {
        std::cerr << "ERROR: Invalid pid\n";
        return EXIT_FAILURE;
    }

    if (kill(daemon_pid, 0) != 0) {
        std::cerr << "ERROR: `kill` check failed\n";
        return EXIT_FAILURE;
    }
    
    return kill(daemon_pid, SIGUSR1);
}
