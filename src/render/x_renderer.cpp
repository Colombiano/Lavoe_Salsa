#include "render/x_renderer.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <X11/Xatom.h>

#include "core/version.hpp"
#include "render/gradient_map.hpp"
#include "render/lua_colormap.hpp"

namespace lv {

namespace {

// Localiza um visual TrueColor de 32 bits (ARGB) na tela.
// Retorna array XVisualInfo* (visual escolhido em [0]) para o caller dar XFree.
XVisualInfo* find_argb_visual(Display* dpy, int screen) {
    XVisualInfo templ{};
    templ.screen = screen;
    templ.depth = 32;
    templ.c_class = TrueColor;
    int n = 0;
    XVisualInfo* infos = XGetVisualInfo(dpy, VisualScreenMask | VisualDepthMask |
                                              VisualClassMask,
                                        &templ, &n);
    int chosen = -1;
    for (int i = 0; i < n; ++i) {
        XRenderPictFormat* fmt = XRenderFindVisualFormat(dpy, infos[i].visual);
        if (fmt && fmt->type == PictTypeDirect && fmt->direct.alphaMask) {
            chosen = i;
            break;
        }
    }
    if (chosen < 0) {
        XFree(infos);
        return nullptr;
    }
    if (chosen > 0) infos[0] = infos[chosen];
    return infos;
}

// Indice da LUT para a faixa de pixel y (0 = base da barra).
int strip_index(int y, int bh) {
    const float t = static_cast<float>(y) / static_cast<float>(std::max(1, bh));
    return static_cast<int>(t * static_cast<float>(GradientMap::kLutSize - 1) +
                            0.5f);
}

} // namespace

XRenderer::XRenderer(const Config& cfg) : dpy_() {
    Display* dpy = dpy_.get();
    const int screen = DefaultScreen(dpy);

    XVisualInfo* visual = find_argb_visual(dpy, screen);
    if (!visual) throw std::runtime_error("nao achei visual ARGB de 32 bits");

    const std::vector<Rect> monitors = query_monitors(dpy);
    const Rect rect = compute_window_rect(cfg, monitors);
    std::fprintf(stderr, "lavoe-salsa: monitores:");
    for (const Rect& m : monitors)
        std::fprintf(stderr, " [%d,%d %dx%d]", m.x, m.y, m.w, m.h);
    std::fprintf(stderr, "\nlavoe-salsa: janela em %d,%d %dx%d\n", rect.x,
                 rect.y, rect.w, rect.h);

    win_ = XWindow(dpy, RootWindow(dpy, screen),
                   XWindow::RectArg{rect.x, rect.y, rect.w, rect.h}, visual,
                   kAppTitle);
    XFree(visual);
    set_net_wm_state();
    XMapWindow(dpy, win_.get());

    pic_ = XPicture(dpy, win_.get());
    win_w_ = rect.w;
    win_h_ = rect.h;
    win_x_ = rect.x;
    win_y_ = rect.y;
    bars_ = cfg.bars;
    bar_w_ = cfg.bar_width;
    bar_gap_ = cfg.bar_gap;
    pad_ = cfg.padding;

    // limpa uma vez (garante fundo transparente desde o primeiro map)
    XRenderColor clear{0, 0, 0, 0};
    XRenderFillRectangle(dpy, PictOpSrc, pic_.get(), &clear, 0, 0,
                         static_cast<unsigned>(win_w_),
                         static_cast<unsigned>(win_h_));
    XSync(dpy, False);
}

void XRenderer::set_net_wm_state() {
    Display* dpy = dpy_.get();
    Window w = win_.get();
    const Atom state = XInternAtom(dpy, "_NET_WM_STATE", False);
    Atom states[4] = {XInternAtom(dpy, "_NET_WM_STATE_STICKY", False),
                      XInternAtom(dpy, "_NET_WM_STATE_BELOW", False),
                      XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False),
                      XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER", False)};
    XChangeProperty(dpy, w, state, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(states), 4);
    // sem decoracao de titulo (barra de janela) — janela crua de overlay
    struct {
        unsigned long flags;
        unsigned long functions;
        unsigned long decorations;
        long input_mode;
        unsigned long status;
    } motif{2, 0, 0, 0, 0};
    const Atom motif_atom = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
    XChangeProperty(dpy, w, motif_atom, motif_atom, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(&motif), 5);
    // um desktop de verdade faria o mesmo pre-map; reforca pos-map
    XMapWindow(dpy, w);
    XChangeProperty(dpy, w, state, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(states), 4);
}

void XRenderer::fill(int x, int y, unsigned w, unsigned h, Color c) {
    XRenderColor xc{static_cast<unsigned short>(c.r * 65535.f + 0.5f),
                    static_cast<unsigned short>(c.g * 65535.f + 0.5f),
                    static_cast<unsigned short>(c.b * 65535.f + 0.5f),
                    65535};
    XRenderFillRectangle(dpy_.get(), PictOpSrc, pic_.get(), &xc, x, y, w, h);
}

template <ColorMap M>
void XRenderer::do_render(std::span<const float> levels, const M& map, float t) {
    Display* dpy = dpy_.get();

    // Redimensionamento pelo usuario? (a janela e redimensionavel)
    while (XPending(dpy)) {
        XEvent ev;
        XNextEvent(dpy, &ev);
        if (ev.type == ConfigureNotify) {
            win_w_ = ev.xconfigure.width;
            win_h_ = ev.xconfigure.height;
        }
    }

    // fundo totalmente transparente
    XRenderColor clear{0, 0, 0, 0};
    XRenderFillRectangle(dpy, PictOpSrc, pic_.get(), &clear, 0, 0,
                         static_cast<unsigned>(win_w_),
                         static_cast<unsigned>(win_h_));
    if (levels.empty()) {
        XFlush(dpy);
        return;
    }

    const int n = std::min<int>(static_cast<int>(levels.size()), bars_);
    const int total_w = n * bar_w_ + (n - 1) * bar_gap_;
    const int x0 = (win_w_ - total_w) / 2;
    const int base = win_h_ - pad_;
    const int max_h = std::max(1, win_h_ - 2 * pad_);

    for (int i = 0; i < n; ++i) {
        const float level = std::clamp(levels[static_cast<std::size_t>(i)], 0.f, 1.f);
        const int bh = static_cast<int>(level * static_cast<float>(max_h) + 0.5f);
        if (bh <= 0) continue;
        const int bx = x0 + i * (bar_w_ + bar_gap_);

        if constexpr (M::kPerStrip) {
            // Gradiente vertical: faixas de 1px com mesclagem de runs da LUT
            int run_y0 = 0;
            int run_idx = strip_index(0, bh);
            for (int y = 1; y <= bh; ++y) {
                const int idx = (y < bh) ? strip_index(y, bh) : -1;
                if (idx != run_idx) {
                    const float tt = static_cast<float>(run_idx) /
                                     static_cast<float>(GradientMap::kLutSize - 1);
                    fill(bx, base - y, static_cast<unsigned>(bar_w_),
                         static_cast<unsigned>(y - run_y0), map.sample(tt));
                    run_y0 = y;
                    run_idx = idx;
                }
            }
        } else {
            fill(bx, base - bh, static_cast<unsigned>(bar_w_),
                 static_cast<unsigned>(bh), map.bar_color(i, level, t));
        }
    }
    // Nudge de recomposite: o muffin (compositor do Cinnamon) nem sempre
    // recomposita janelas BELOW/SKIP_* em damage de conteudo puro (XRender/
    // XPutImage). Um XMoveWindow de 1px alternado forca a recomposicao a
    // cada frame; o movimento e imperceptivel e os estados _NET_WM_STATE
    // nao sao afetados.
    static bool toggle = false;
    XMoveWindow(dpy, win_.get(), win_x_ + (toggle ? 1 : 0), win_y_);
    toggle = !toggle;

    XFlush(dpy);
}

// forca a instanciacao dos ColorMaps usados no main
template void XRenderer::do_render<GradientMap>(std::span<const float>,
                                                const GradientMap&, float);
template void XRenderer::do_render<LuaColorMap>(std::span<const float>,
                                                const LuaColorMap&, float);

} // namespace lv
