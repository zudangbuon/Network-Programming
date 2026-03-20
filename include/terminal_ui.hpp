#pragma once

#include <chrono>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

namespace ui {

// ─── ANSI Color Codes ───────────────────────────────────────────────

namespace color {
    inline const char* reset    = "\033[0m";
    inline const char* bold     = "\033[1m";
    inline const char* dim      = "\033[2m";

    inline const char* red      = "\033[31m";
    inline const char* green    = "\033[32m";
    inline const char* yellow   = "\033[33m";
    inline const char* blue     = "\033[34m";
    inline const char* magenta  = "\033[35m";
    inline const char* cyan     = "\033[36m";
    inline const char* white    = "\033[37m";

    inline const char* bred     = "\033[1;31m";
    inline const char* bgreen   = "\033[1;32m";
    inline const char* byellow  = "\033[1;33m";
    inline const char* bcyan    = "\033[1;36m";
    inline const char* bwhite   = "\033[1;37m";

    inline const char* bg_green = "\033[42m";
    inline const char* bg_gray  = "\033[100m";
}  // namespace color

// ─── Unicode Characters ─────────────────────────────────────────────

namespace sym {
    inline const char* filled   = "█";
    inline const char* empty    = "░";
    inline const char* arrow    = "➜";
    inline const char* check    = "✔";
    inline const char* cross    = "✘";
    inline const char* dot      = "●";
    inline const char* upload   = "⬆";
    inline const char* download = "⬇";
    inline const char* folder   = "📁";
    inline const char* file_ic  = "📄";
}  // namespace sym

// ─── Spinner ─────────────────────────────────────────────────────────

class Spinner {
  public:
    Spinner() : idx_(0) {}

    char next() {
        static const char frames[] = {'|', '/', '-', '\\'};
        char c = frames[idx_ % 4];
        idx_++;
        return c;
    }

  private:
    int idx_;
};

// ─── Helpers ─────────────────────────────────────────────────────────

inline std::string format_size(uint64_t bytes) {
    std::ostringstream oss;
    if (bytes >= 1024ULL * 1024) {
        oss << std::fixed << std::setprecision(1)
                << (static_cast<double>(bytes) / (1024.0 * 1024.0)) << " MB";
    } else if (bytes >= 1024ULL) {
        oss << std::fixed << std::setprecision(1)
                << (static_cast<double>(bytes) / 1024.0) << " KB";
    } else {
        oss << bytes << " B";
    }
    return oss.str();
}

inline std::string format_speed(double bytes_per_sec) {
    std::ostringstream oss;
    if (bytes_per_sec >= 1024.0 * 1024.0) {
        oss << std::fixed << std::setprecision(1)
                << (bytes_per_sec / (1024.0 * 1024.0)) << " MB/s";
    } else if (bytes_per_sec >= 1024.0) {
        oss << std::fixed << std::setprecision(1)
                << (bytes_per_sec / 1024.0) << " KB/s";
    } else {
        oss << std::fixed << std::setprecision(0)
                << bytes_per_sec << " B/s";
    }
    return oss.str();
}

inline std::string format_eta(double seconds) {
    if (seconds < 0 || seconds > 86400) return "--:--";
    int total = static_cast<int>(seconds + 0.5);
    int h = total / 3600;
    int m = (total % 3600) / 60;
    int s = total % 60;
    std::ostringstream oss;
    if (h > 0) {
        oss << h << "h " << std::setfill('0') << std::setw(2) << m << "m";
    } else if (m > 0) {
        oss << m << "m " << std::setfill('0') << std::setw(2) << s << "s";
    } else {
        oss << s << "s";
    }
    return oss.str();
}

using SteadyClock = std::chrono::steady_clock;
using TimePoint   = std::chrono::steady_clock::time_point;

// ─── Progress Bar ────────────────────────────────────────────────────

static constexpr int kBarWidth = 30;

inline void render_progress(const std::string& label, uint64_t done, uint64_t total,
                                                        const TimePoint& start_time = SteadyClock::now()) {
    if (total == 0) return;

    double ratio = static_cast<double>(done) / static_cast<double>(total);
    if (ratio > 1.0) ratio = 1.0;
    int pct = static_cast<int>(ratio * 100.0);
    int filled = static_cast<int>(ratio * kBarWidth);

    // Calculate speed and ETA
    auto now = SteadyClock::now();
    double elapsed_sec = std::chrono::duration<double>(now - start_time).count();
    double speed_bps = (elapsed_sec > 0.05) ? static_cast<double>(done) / elapsed_sec : 0.0;
    double remaining_bytes = static_cast<double>(total - done);
    double eta_sec = (speed_bps > 0.0) ? (remaining_bytes / speed_bps) : 0.0;

    std::ostringstream bar;

    // Label
    bar << "  " << color::cyan << label << color::reset << " ";

    // Bar opening bracket
    bar << color::dim << "[" << color::reset;

    // Filled portion
    bar << color::bgreen;
    for (int i = 0; i < filled; i++) bar << sym::filled;

    // Empty portion
    bar << color::dim;
    for (int i = filled; i < kBarWidth; i++) bar << sym::empty;

    // Bar closing bracket
    bar << color::reset << color::dim << "]" << color::reset;

    // Percentage
    bar << " " << color::byellow << std::setw(3) << pct << "%" << color::reset;

    // Size info
    bar << "  " << color::white << format_size(done) << " / " << format_size(total) << color::reset;

    // Speed
    if (speed_bps > 0.0 && elapsed_sec > 0.05) {
        bar << "  " << color::dim << format_speed(speed_bps) << color::reset;
    }

    // ETA
    if (pct < 100 && speed_bps > 0.0 && elapsed_sec > 0.05) {
        bar << "  " << color::magenta << "ETA " << format_eta(eta_sec) << color::reset;
    }

    // Overwrite the line
    std::cerr << "\r" << bar.str() << "\033[K" << std::flush;
}

inline void finish_progress(const std::string& label) {
    std::ostringstream bar;

    bar << "  " << color::cyan << label << color::reset << " ";
    bar << color::dim << "[" << color::reset;
    bar << color::bgreen;
    for (int i = 0; i < kBarWidth; i++) bar << sym::filled;
    bar << color::reset << color::dim << "]" << color::reset;
    bar << " " << color::bgreen << "100% " << sym::check << " Done!" << color::reset;

    std::cerr << "\r" << bar.str() << "\033[K" << std::endl;
}

// ─── Styled Output ──────────────────────────────────────────────────

inline void print_banner() {
    std::cout << color::bcyan
                        << "\n"
                        << "  ╔══════════════════════════════════════════╗\n"
                        << "  ║     " << color::bwhite << sym::arrow << " File Transfer Client v2.0" << color::bcyan << "       ║\n"
                        << "  ╚══════════════════════════════════════════╝\n"
                        << color::reset << std::endl;
}

inline void print_prompt() {
    std::cout << color::bgreen << " $ " << color::reset << std::flush;
}

inline void print_success(const std::string& msg) {
    std::cout << "  " << color::bgreen << sym::check << " " << msg << color::reset << std::endl;
}

inline void print_error(const std::string& msg) {
    std::cout << "  " << color::bred << sym::cross << " " << msg << color::reset << std::endl;
}

inline void print_info(const std::string& msg) {
    std::cout << "  " << color::cyan << sym::dot << " " << msg << color::reset << std::endl;
}

inline void print_server_msg(const std::string& msg) {
    std::cout << "  " << color::dim << "Server: " << color::reset << msg << std::endl;
}

inline void print_file_list_header() {
    std::cout << "\n  " << color::bcyan << sym::folder << " Files on server:" << color::reset << "\n";
    std::cout << "  " << color::dim << "─────────────────────────────────────" << color::reset << "\n";
}

inline void print_file_entry(const std::string& name, const std::string& size) {
    std::cout << "  " << color::white << "  " << sym::file_ic << " " << color::bwhite << name
                        << color::reset << color::dim << "  " << size << color::reset << "\n";
}

inline void print_separator() {
    std::cout << "  " << color::dim << "─────────────────────────────────────" << color::reset << "\n";
}

inline void print_help() {
    std::cout << "\n  " << color::bcyan << "Available Commands:" << color::reset << "\n";
    std::cout << "  " << color::dim << "─────────────────────────────────────" << color::reset << "\n";
    std::cout << "  " << color::byellow << "  LIST" << color::reset << color::dim << "                  List files on server" << color::reset << "\n";
    std::cout << "  " << color::byellow << "  UPLOAD" << color::reset << color::cyan << " <path>" << color::reset << color::dim << "         Upload a local file" << color::reset << "\n";
    std::cout << "  " << color::byellow << "  DOWNLOAD" << color::reset << color::cyan << " <filename>" << color::reset << color::dim << "   Download a file" << color::reset << "\n";
    std::cout << "  " << color::byellow << "  QUIT" << color::reset << color::dim << "                  Disconnect" << color::reset << "\n";
    std::cout << "  " << color::dim << "─────────────────────────────────────" << color::reset << "\n\n";
}

}  // namespace ui
