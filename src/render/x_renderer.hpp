// Renderizador X11 + XRender: overlay ARGB transparente sobre o wallpaper.
//
// - Janela com visual 32-bit (ARGB), sem decoracoes, estados _NET_WM_STATE:
//   STICKY, BELOW, SKIP_TASKBAR, SKIP_PAGER (fica sobre o wallpaper, abaixo
//   das janelas normais, em todos os workspaces).
// - Fundo 100% transparente (PictOpSrc com alfa 0); barras desenhadas com
//   XRenderFillRectangle em gradiente vertical.
// - Processa ConfigureNotify: a janela e redimensionavel com o mouse.
#pragma once

#include <algorithm>
#include <span>
#include "core/config.hpp"
#include "core/geometry.hpp"
#include "render/renderer_crtp.hpp"
#include "render/x11_raii.hpp"

namespace lv {

class XRenderer : public RendererCRTP<XRenderer> {
public:
    explicit XRenderer(const Config& cfg);
    ~XRenderer() = default;

    XRenderer(const XRenderer&) = delete;
    XRenderer& operator=(const XRenderer&) = delete;
    XRenderer(XRenderer&&) = default;
    XRenderer& operator=(XRenderer&&) = default;

    // Chamado via RendererCRTP::render (satisfaz BarRenderer).
    template <ColorMap M>
    void do_render(std::span<const float> levels, const M& map, float t);

    // Numero de colunas a renderizar (largura atual da janela). Refinamento
    // opcional do concept BarRenderer: run_frames usa via if constexpr para
    // pedir ao DSP exatamente uma amplitude por pixel.
    [[nodiscard]] std::size_t columns() const {
        return static_cast<std::size_t>(std::max(1, win_w_));
    }

private:
    void fill(int x, int y, unsigned w, unsigned h, Color c,
              unsigned short alpha = 65535);
    void set_net_wm_state();
    template <ColorMap M>
    void draw_waveform(std::span<const float> levels, const M& map, float t);
    template <ColorMap M>
    void draw_bars(std::span<const float> levels, const M& map, float t);
    template <ColorMap M>
    Color column_color(const M& map, std::size_t col, float level,
                       float t) const;

    XDisplay dpy_;
    XWindow win_;
    XPicture pic_;

    int win_w_ = 0, win_h_ = 0;
    int win_x_ = 0, win_y_ = 0;  // para o nudge de recomposite
    int bars_ = 0, bar_w_ = 0, bar_gap_ = 0, pad_ = 0;
    Mode mode_ = Mode::Bars;
    float wave_scale_ = 1.f;
    bool wave_mirror_ = true;
    Color axis_color_{};
};

} // namespace lv
