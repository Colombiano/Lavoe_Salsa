// Concepts proprios do projeto (C++20).
//
// Estes conceitos definem os contratos entre as camadas:
//   AudioSource  - fornece amostras PCM mono recentes (ex.: captura Pulse)
//   ColorMap     - mapeia uma barra/nivel para uma cor (gradiente ou Lua)
//   BarRenderer  - desenha alturas de barras com um ColorMap
#pragma once

#include <span>
#include <concepts>
#include "dsp/color.hpp"

namespace lv {

template <typename S>
concept AudioSource = requires(S& src, std::span<float> dst) {
    // Copia as amostras mais recentes para dst (zero-padding se faltar).
    // Retorna quantas amostras validas havia. Deve ser thread-safe.
    { src.read_latest(dst) } -> std::same_as<std::size_t>;
};

// Refinamento opcional: fontes que conhecem a taxa negociada em runtime.
template <typename S>
concept SampleRateSource = requires(const S& src) {
    { src.sample_rate() } -> std::convertible_to<float>;
};

template <typename M>
concept ColorMap = requires(const M& m, float t, int i, float level) {
    { M::kPerStrip } -> std::convertible_to<bool>;
} && (
    requires(const M& m, float t) {
        // kPerStrip == true: cor por faixa de pixel (gradiente vertical)
        { m.sample(t) } -> std::convertible_to<Color>;
    } ||
    requires(const M& m, int i, float level, float t) {
        // kPerStrip == false: cor unica por barra (hook Lua)
        { m.bar_color(i, level, t) } -> std::convertible_to<Color>;
    }
);

template <typename R, typename M>
concept BarRenderer = ColorMap<M> && requires(R& r, std::span<const float> levels,
                                              const M& map, float t) {
    { r.render(levels, map, t) } -> std::same_as<void>;
};

} // namespace lv
