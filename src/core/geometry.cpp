#include "core/geometry.hpp"

#include <X11/Xlib.h>
#include <algorithm>
#include <cstdio>
#include <dlfcn.h>
#include <memory>
#include <sstream>
#include <string>

namespace lv {

namespace {

// ---------------------------------------------------------------- Xinerama
// Estrutura publica e estavel ha decadas; resolvemos a funcao via dlopen
// para nao depender de pacote -dev nem de link direto.
struct XineramaScreenInfoCompat {
    int screen_number;
    short x_org;
    short y_org;
    short width;
    short height;
};
using XineramaQueryScreensFn = XineramaScreenInfoCompat* (*)(Display*, int*);

std::vector<Rect> via_xinerama(Display* dpy) {
    // NOTA: nunca dar dlclose nesta biblioteca. A primeira chamada de
    // XineramaQueryScreens registra uma extensao no Display cujo hook
    // close_display aponta para codigo dentro da libXinerama; se a
    // biblioteca for descarregada, XCloseDisplay pula para um endereco
    // invalido no shutdown (SIGSEGV). O handle vive ate o fim do processo.
    static void* h = dlopen("libXinerama.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!h) return {};
    static const auto fn = reinterpret_cast<XineramaQueryScreensFn>(
        dlsym(h, "XineramaQueryScreens"));
    if (!fn) return {};
    int n = 0;
    XineramaScreenInfoCompat* info = fn(dpy, &n);
    std::vector<Rect> out;
    if (info && n > 0) {
        out.reserve(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i)
            out.push_back(Rect{info[i].x_org, info[i].y_org, info[i].width,
                               info[i].height});
        XFree(info);
    }
    return out;
}

// ---------------------------------------------------------------- xrandr
// Fallback: parseia `xrandr --query` (cabecalhos "conectados" trazem
// a geometria WxH+X+Y). Ignora saidas desconectadas.
std::vector<Rect> via_xrandr_cli() {
    std::vector<Rect> out;
    FILE* pipe = popen("xrandr --query 2>/dev/null", "r");
    if (!pipe) return out;
    std::string line;
    char chunk[512];
    while (fgets(chunk, sizeof chunk, pipe)) {
        line += chunk;
        if (line.back() != '\n') continue;  // linha pode ser maior que chunk
        // formato: "HDMI-0 connected primary 1920x1440+0+0 ..."
        const auto conn = line.find(" connected");
        if (conn == std::string::npos || line[0] == ' ') {
            line.clear();
            continue;
        }
        // procura token WxH+X+Y a partir da posicao de "connected"
        std::istringstream iss(line.substr(conn));
        std::string tok;
        while (iss >> tok) {
            const auto x = tok.find('x'), plus = tok.find('+', x);
            if (x != std::string::npos && plus != std::string::npos &&
                plus + 1 < tok.size()) {
                try {
                    Rect r;
                    r.w = std::stoi(tok.substr(0, x));
                    r.h = std::stoi(tok.substr(x + 1, plus - x - 1));
                    r.x = std::stoi(tok.substr(plus + 1));
                    const auto plus2 = tok.find('+', plus + 1);
                    r.y = plus2 != std::string::npos
                              ? std::stoi(tok.substr(plus2 + 1)) : 0;
                    if (r.w > 0 && r.h > 0) out.push_back(r);
                } catch (...) {
                }
                break;
            }
        }
        line.clear();
    }
    pclose(pipe);
    return out;
}

} // namespace

std::vector<Rect> query_monitors(Display* dpy) {
    if (dpy) {
        if (auto mons = via_xinerama(dpy); !mons.empty()) {
            // indices seguem a posicao: 0 = monitor mais a esquerda
            std::sort(mons.begin(), mons.end(),
                      [](const Rect& a, const Rect& b) { return a.x < b.x; });
            return mons;
        }
    }
    if (auto mons = via_xrandr_cli(); !mons.empty()) {
        std::sort(mons.begin(), mons.end(),
                  [](const Rect& a, const Rect& b) { return a.x < b.x; });
        return mons;
    }
    if (dpy)
        return {Rect{0, 0, DisplayWidth(dpy, DefaultScreen(dpy)),
                     DisplayHeight(dpy, DefaultScreen(dpy))}};
    return {Rect{0, 0, 1920, 1080}};
}

Rect compute_window_rect(const Config& cfg, const std::vector<Rect>& monitors) {
    if (cfg.x && cfg.y) return Rect{*cfg.x, *cfg.y, cfg.w, cfg.h};
    if (monitors.empty()) return Rect{0, 0, cfg.w, cfg.h};

    const int idx = std::clamp(cfg.monitor, 0,
                               static_cast<int>(monitors.size()) - 1);
    const Rect& m = monitors[static_cast<std::size_t>(idx)];
    const int x = m.x + (m.w - cfg.w) / 2;
    const int y = m.y + m.h - cfg.h - cfg.bottom_margin;
    return Rect{x, y, cfg.w, cfg.h};
}

} // namespace lv
