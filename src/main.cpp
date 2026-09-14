// Lavoe Salsa - ponto de entrada.
//
// Loop principal generico: run_frames conecta uma AudioSource, um
// BarRenderer e um SpectrumAnalyzer via concepts/requires, com throttle de
// FPS e saida CSV opcional (--dump).
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <exception>
#include <iostream>
#include <optional>
#include <span>
#include <thread>
#include <vector>

#include "audio/pulse_capture.hpp"
#include "core/cli.hpp"
#include "core/config.hpp"
#include "core/concepts.hpp"
#include "core/lua.hpp"
#include "core/version.hpp"
#include "dsp/spectrum.hpp"
#include "render/gradient_map.hpp"
#include "render/lua_colormap.hpp"
#include "render/x_renderer.hpp"

namespace {

std::atomic<bool> g_stop{false};

extern "C" void on_signal(int) {
    g_stop.store(true, std::memory_order_relaxed);
}

void print_csv(std::span<const float> heights) {
    for (std::size_t i = 0; i < heights.size(); ++i) {
        if (i) std::fputc(',', stdout);
        std::fprintf(stdout, "%.3f",
                     static_cast<double>(heights[i]));
    }
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

// Renderer nulo para o modo --dump (sem janela X).
class NullRenderer : public lv::RendererCRTP<NullRenderer> {
public:
    template <lv::ColorMap M>
    void do_render(std::span<const float>, const M&, float) {}
};

// ------------------------------------------------------------ loop generico

template <lv::AudioSource Src, lv::ColorMap Map, typename Ren, typename Analyzer>
    requires lv::BarRenderer<Ren, Map>
void run_frames(Src& src, Ren& ren, const Map& cmap, Analyzer& dsp,
                const lv::Config& cfg, bool dump) {
    constexpr std::size_t kN = Analyzer::kFftSize;
    std::vector<float> pcm(kN);

    using clock = std::chrono::steady_clock;
    const auto frame_dt = std::chrono::duration<double>(1.0 / static_cast<double>(cfg.fps));
    const auto t0 = clock::now();
    auto next = t0;

    while (!g_stop.load(std::memory_order_relaxed)) {
        // Refinamento opcional: fontes que expoem a taxa negociada atualizam
        // o mapeamento de frequencias em runtime (if constexpr, sem custo
        // quando a fonte nao suporta).
        if constexpr (lv::SampleRateSource<Src>) {
            const float r = src.sample_rate();
            if (r > 0.f) dsp.set_sample_rate(r);
        }

        const std::size_t got = src.read_latest(pcm);
        if (got > 0) {
            const std::span<const float> heights = dsp.analyze(pcm);
            const float t = std::chrono::duration<float>(clock::now() - t0).count();
            if (dump) {
                print_csv(heights);
            } else {
                ren.render(heights, cmap, t);
            }
        }

        next += std::chrono::duration_cast<clock::duration>(frame_dt);
        const auto now = clock::now();
        if (now < next) {
            std::this_thread::sleep_until(next);
        } else if (now > next + std::chrono::duration_cast<clock::duration>(frame_dt * 4)) {
            next = now;  // ressincroniza apos atraso grande
        }
    }

    if (!dump) ren.render(std::span<const float>{}, cmap, 0.f);  // limpa a janela
}

} // namespace

int main(int argc, char** argv) {
    using namespace lv;

    try {
        Options opts;
        if (auto code = parse_cli(argc, argv, opts)) return *code;

        std::signal(SIGINT, on_signal);
        std::signal(SIGTERM, on_signal);

        LuaState lua;
        const Config cfg = load_config(lua, opts.config);

        std::fprintf(stderr,
                     "%s %s - capturando o monitor do sink padrao do PulseAudio\n",
                     kAppTitle, kVersion);

        PulseCapture audio;
        SpectrumAnalyzer<CavaSmoothing> analyzer(
            static_cast<std::size_t>(cfg.bars), cfg.sample_rate, cfg.smoothing);

        if (opts.dump) {
            // modo ferramenta: CSV no stdout, sem janela X
            NullRenderer null_renderer;
            GradientMap cmap{cfg.gradient};
            run_frames(audio, null_renderer, cmap, analyzer, cfg, true);
        } else if (cfg.bar_color_ref != LUA_NOREF) {
            XRenderer renderer(cfg);
            LuaColorMap cmap{lua.get(), cfg.bar_color_ref};
            run_frames(audio, renderer, cmap, analyzer, cfg, false);
        } else {
            XRenderer renderer(cfg);
            GradientMap cmap{cfg.gradient};
            run_frames(audio, renderer, cmap, analyzer, cfg, false);
        }
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "lavoe-salsa: erro: %s\n", e.what());
        return 1;
    }
}
