#pragma once

#include <chrono>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <sys/ioctl.h>
#include <unistd.h>
#include <algorithm>

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
    inline const char* img_ic   = "🖼️ ";
    inline const char* music_ic = "🎵";
    inline const char* video_ic = "🎬";
    inline const char* arch_ic  = "📦";
    inline const char* code_ic  = "📝";
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

// ─── Terminal Width ─────────────────────────────────────────────────

inline int get_raw_terminal_width() {
    struct winsize w{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0)
        return static_cast<int>(w.ws_col);
    if (ioctl(STDERR_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0)
        return static_cast<int>(w.ws_col);
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0)
        return static_cast<int>(w.ws_col);
    return 80;
}

inline int get_terminal_width() {
    return get_raw_terminal_width();
}

inline int get_content_width() {
    return std::max(20, get_terminal_width() - 4);
}

// ─── Display Width (ANSI-aware) ─────────────────────────────────────

inline int str_display_width(const std::string& s) {
    int w = 0;
    bool in_esc = false;
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == 0x1B) { in_esc = true; i++; continue; }
        if (in_esc) {
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
                in_esc = false;
            i++;
            continue;
        }
        uint32_t cp = 0;
        int bytes = 1;
        if      (c < 0x80)               { cp = c;        bytes = 1; }
        else if ((c & 0xE0) == 0xC0)     { cp = c & 0x1F; bytes = 2; }
        else if ((c & 0xF0) == 0xE0)     { cp = c & 0x0F; bytes = 3; }
        else if ((c & 0xF8) == 0xF0)     { cp = c & 0x07; bytes = 4; }
        else                             { i++; continue; }
        for (int j = 1; j < bytes && (i + j) < s.size(); j++)
            cp = (cp << 6) | (static_cast<unsigned char>(s[i + j]) & 0x3F);
        i += bytes;
        if (cp < 0x20) continue;
        if ((cp >= 0x1F000 && cp <= 0x1FFFF) ||
            (cp >= 0x2B00  && cp <= 0x2BFF)  ||
            (cp >= 0x1100  && cp <= 0x115F)  ||
            (cp >= 0x2E80  && cp <= 0x303E)  ||
            (cp >= 0x3040  && cp <= 0x33BF)  ||
            (cp >= 0x4E00  && cp <= 0x9FFF)  ||
            (cp >= 0xAC00  && cp <= 0xD7AF)  ||
            (cp >= 0xF900  && cp <= 0xFAFF)  ||
            (cp >= 0x20000 && cp <= 0x2FA1F))
        {
            w += 2;
        } else {
            w += 1;
        }
    }
    return w;
}

// ─── Helpers ─────────────────────────────────────────────────────────

inline std::string get_margin() { return "  "; }

inline std::string make_line(const std::string& pattern, int count) {
    std::string s;
    for (int i = 0; i < count; ++i) s += pattern;
    return s;
}

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

// ─── Progress State ─────────────────────────────────────────────────

namespace detail {
    inline bool& wrap_disabled() {
        static bool v = false;
        return v;
    }
    inline SteadyClock::time_point& last_render_time() {
        static SteadyClock::time_point t = SteadyClock::now();
        return t;
    }
}  // namespace detail

// ─── Progress Bar ────────────────────────────────────────────────────

inline void render_progress(const std::string& label, uint64_t done, uint64_t total,
                            const TimePoint& start_time = SteadyClock::now()) {
    if (total == 0) return;

    // ── Throttle: only render every ~80ms to avoid flooding ──
    auto now = SteadyClock::now();
    double since_last = std::chrono::duration<double>(now - detail::last_render_time()).count();
    if (since_last < 0.08 && done < total) return;
    detail::last_render_time() = now;

    // ── Disable line wrapping on first render ──
    // This prevents the terminal from breaking our line into multiple rows
    // when the user resizes. \r will always overwrite the same single line.
    if (!detail::wrap_disabled()) {
        std::cerr << "\033[?7l"; // Disable line wrap (DECRST RM mode 7)
        detail::wrap_disabled() = true;
    }

    int raw_cols = get_raw_terminal_width();

    // ── Compute transfer metrics ──
    double elapsed = std::chrono::duration<double>(now - start_time).count();
    double ratio = std::min(1.0, static_cast<double>(done) / static_cast<double>(total));
    int pct = static_cast<int>(ratio * 100.0);
    double speed = (elapsed > 0.05) ? static_cast<double>(done) / elapsed : 0.0;
    double eta = (speed > 0.0) ? static_cast<double>(total - done) / speed : 0.0;

    // ── Pre-build text fragments ──
    std::string pct_str = std::to_string(pct) + "%";
    while (pct_str.size() < 4) pct_str = " " + pct_str;
    std::string size_str = format_size(done) + " / " + format_size(total);
    std::string speed_str = format_speed(speed);
    std::string eta_str = "ETA " + format_eta(eta);

    int label_w = str_display_width(label);
    bool show_speed = (speed > 0.0 && elapsed > 0.05);
    bool show_eta = (pct < 100 && speed > 0.0 && elapsed > 0.05);

    // overhead = margin(2) + label_w + " "(1) + "["(1) + "]"(1) + " "(1) + pct(4) + "  "(2) + size_str
    int overhead = 12 + label_w + static_cast<int>(size_str.size());
    int speed_w = show_speed ? (2 + static_cast<int>(speed_str.size())) : 0;
    int eta_w = show_eta ? (2 + static_cast<int>(eta_str.size())) : 0;

    int bar_w = raw_cols - overhead - speed_w - eta_w - 3; // -3 safety margin

    // Graceful degradation
    if (bar_w < 8 && show_eta)   { show_eta = false;   bar_w += eta_w; }
    if (bar_w < 8 && show_speed) { show_speed = false;  bar_w += speed_w; }
    bar_w = std::max(5, bar_w);

    int filled = static_cast<int>(ratio * bar_w);

    // ── Build output ──
    std::ostringstream out;
    out << get_margin() << color::cyan << label << color::reset << " ";
    out << color::dim << "[" << color::reset;
    out << color::bgreen;
    for (int i = 0; i < filled; i++) out << sym::filled;
    out << color::dim;
    for (int i = filled; i < bar_w; i++) out << sym::empty;
    out << color::reset << color::dim << "]" << color::reset;
    out << " " << color::byellow << pct_str << color::reset;
    out << "  " << color::white << size_str << color::reset;
    if (show_speed)
        out << "  " << color::dim << speed_str << color::reset;
    if (show_eta)
        out << "  " << color::magenta << eta_str << color::reset;

    // ── Output: \r to beginning, write bar, \033[K to clear any trailing junk ──
    std::cerr << "\r" << out.str() << "\033[K" << std::flush;
}

inline void finish_progress(const std::string& label) {
    int raw_cols = get_raw_terminal_width();
    int label_w = str_display_width(label);
    int overhead = 12 + label_w + 14; // "100% ✔ Done!" is ~14 visible chars
    int bar_w = std::max(5, raw_cols - overhead - 3);

    std::ostringstream out;
    out << get_margin() << color::cyan << label << color::reset << " ";
    out << color::dim << "[" << color::reset;
    out << color::bgreen;
    for (int i = 0; i < bar_w; i++) out << sym::filled;
    out << color::reset << color::dim << "]" << color::reset;
    out << " " << color::bgreen << "100% " << sym::check << " Done!" << color::reset;

    std::cerr << "\r" << out.str() << "\033[K" << std::endl;

    // Re-enable line wrapping now that we're done
    if (detail::wrap_disabled()) {
        std::cerr << "\033[?7h"; // Re-enable line wrap (DECSET SM mode 7)
        detail::wrap_disabled() = false;
    }
}

// ─── Styled Output ──────────────────────────────────────────────────

inline void print_banner() {
    int cw = std::min(60, get_content_width());
    std::string margin = get_margin();

    // Build plain title to measure its display width
    std::string title = std::string(sym::arrow) + " File Transfer Client";
    int title_w = str_display_width(title);

    std::string top_edge = "╔" + make_line("═", cw - 2) + "╗";
    std::string bot_edge = "╚" + make_line("═", cw - 2) + "╝";

    int inner = cw - 2;
    int left_pad = (inner - title_w) / 2;
    int right_pad = inner - title_w - left_pad;
    if (left_pad < 0)  left_pad = 0;
    if (right_pad < 0) right_pad = 0;

    std::cout << color::bcyan << "\n"
              << margin << top_edge << "\n"
              << margin << "║" << std::string(left_pad, ' ')
              << color::bwhite << title
              << color::bcyan << std::string(right_pad, ' ') << "║\n"
              << margin << bot_edge << "\n"
              << color::reset << std::endl;
}
inline void print_server_banner() {
    int cw = std::min(60, get_content_width());
    std::string margin = get_margin();

    // Build plain title to measure its display width
    std::string title = std::string(sym::arrow) + " File Transfer Server";
    int title_w = str_display_width(title);

    std::string top_edge = "╔" + make_line("═", cw - 2) + "╗";
    std::string bot_edge = "╚" + make_line("═", cw - 2) + "╝";

    int inner = cw - 2;
    int left_pad = (inner - title_w) / 2;
    int right_pad = inner - title_w - left_pad;
    if (left_pad < 0)  left_pad = 0;
    if (right_pad < 0) right_pad = 0;

    std::cout << color::bgreen << "\n"
              << margin << top_edge << "\n"
              << margin << "║" << std::string(left_pad, ' ')
              << color::byellow << title
              << color::bgreen << std::string(right_pad, ' ') << "║\n"
              << margin << bot_edge << "\n"
              << color::reset << std::endl;
}

inline void print_prompt() {
    std::cout << color::bgreen << " $ " << color::reset << std::flush;
}

inline void print_success(const std::string& msg) {
    std::cout << get_margin() << color::bgreen << sym::check << " " << msg << color::reset << std::endl;
}

inline void print_error(const std::string& msg) {
    std::cout << get_margin() << color::bred << sym::cross << " " << msg << color::reset << std::endl;
}

inline void print_info(const std::string& msg) {
    std::cout << get_margin() << color::cyan << sym::dot << " " << msg << color::reset << std::endl;
}

inline void print_server_msg(const std::string& msg) {
    std::cout << get_margin() << color::dim << "Server: " << color::reset << msg << std::endl;
}

inline void print_separator() {
    int cw = get_content_width();
    std::cout << get_margin() << color::dim << make_line("─", cw) << color::reset << "\n";
}

inline void print_file_list_header() {
    std::cout << "\n" << get_margin() << color::bcyan << sym::folder << " Files on server:" << color::reset << "\n";
    print_separator();
}
inline std::string get_file_icon(const std::string& name) {
    auto dot_pos = name.find_last_of('.');
    if (dot_pos == std::string::npos) return sym::file_ic;
    
    std::string ext = name.substr(dot_pos);
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif" || ext == ".bmp" || ext == ".webp" || ext == ".svg")
        return sym::img_ic;
    if (ext == ".mp3" || ext == ".wav" || ext == ".flac" || ext == ".aac" || ext == ".ogg")
        return sym::music_ic;
    if (ext == ".mp4" || ext == ".mkv" || ext == ".avi" || ext == ".mov" || ext == ".webm")
        return sym::video_ic;
    if (ext == ".zip" || ext == ".tar" || ext == ".gz" || ext == ".rar" || ext == ".7z")
        return sym::arch_ic;
    if (ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".c" || ext == ".py" || ext == ".js" || ext == ".html" || ext == ".css" || ext == ".json" || ext == ".md" || ext == ".txt")
        return sym::code_ic;
        
    return sym::file_ic;
}

inline void print_file_entry(const std::string& name, const std::string& size) {
    std::string icon = get_file_icon(name);
    int name_w = str_display_width(name);
    // Align columns at position 35
    int pad = std::max(2, 35 - name_w);
    
    std::cout << get_margin() << color::white << "  " << icon << " " << color::bwhite << name
              << color::reset << color::dim << std::string(pad, ' ') << size << color::reset << "\n";
}

inline void print_help() {
    std::cout << "\n" << get_margin() << color::bcyan << "Available Commands:" << color::reset << "\n";
    print_separator();
    std::cout << get_margin() << color::byellow << "  LIST" << color::reset << color::dim << "                  List files on server" << color::reset << "\n";
    std::cout << get_margin() << color::byellow << "  UPLOAD" << color::reset << color::cyan << " <path>" << color::reset << color::dim << "         Upload a local file" << color::reset << "\n";
    std::cout << get_margin() << color::byellow << "  DOWNLOAD" << color::reset << color::cyan << " <filename>" << color::reset << color::dim << "   Download a file" << color::reset << "\n";
    std::cout << get_margin() << color::byellow << "  QUIT" << color::reset << color::dim << "                  Disconnect" << color::reset << "\n";
    print_separator();
    std::cout << "\n";
}

}  // namespace ui
