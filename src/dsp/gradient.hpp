// Gradiente de cores: tabela de lookup gerada por interpolacao linear.
//
// O gradiente PADRAO e computado em TEMPO DE COMPILACAO via funcao
// `consteval` (make_default_gradient_lut), que recebe os stops como
// std::array<GradientStop, N> e devolve uma std::array<Color, 256> pronta.
// Stops vindos do config Lua usam o mesmo algoritmo em runtime
// (build_gradient_lut_rt), mantendo o caminho de compile-time intacto.
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>
#include "dsp/color.hpp"

namespace lv {

struct GradientStop {
    float pos;   // 0.0 (base) .. 1.0 (topo)
    Color color;
};

constexpr Color lerp(Color a, Color b, float u) {
    return Color{a.r + (b.r - a.r) * u, a.g + (b.g - a.g) * u,
                 a.b + (b.b - a.b) * u};
}

constexpr float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

template <std::size_t N>
constexpr std::array<GradientStop, N> sorted_stops(std::array<GradientStop, N> s) {
    for (std::size_t i = 1; i < N; ++i) {
        GradientStop key = s[i];
        std::size_t j = i;
        while (j > 0 && s[j - 1].pos > key.pos) {
            s[j] = s[j - 1];
            --j;
        }
        s[j] = key;
    }
    return s;
}

constexpr Color interp_stops(std::span<const GradientStop> stops, float t) {
    if (stops.empty()) return Color{};
    if (stops.size() == 1 || t <= stops.front().pos) return stops.front().color;
    if (t >= stops.back().pos) return stops.back().color;
    for (std::size_t k = 0; k + 1 < stops.size(); ++k) {
        if (t <= stops[k + 1].pos) {
            const float span = stops[k + 1].pos - stops[k].pos;
            const float u = span > 0.f ? clamp01((t - stops[k].pos) / span) : 0.f;
            return lerp(stops[k].color, stops[k + 1].color, u);
        }
    }
    return stops.back().color;
}

// ------------------------------------------------------------ compile-time

template <std::size_t N, std::size_t M>
constexpr std::array<Color, M> build_gradient_lut(std::array<GradientStop, N> stops) {
    static_assert(N >= 2, "gradiente precisa de pelo menos 2 stops");
    const auto s = sorted_stops(stops);
    std::array<Color, M> lut{};
    for (std::size_t i = 0; i < M; ++i) {
        const float t = M > 1 ? static_cast<float>(i) / static_cast<float>(M - 1) : 0.f;
        lut[i] = interp_stops(std::span{s}, t);
    }
    return lut;
}

// Funcao consteval: obriga a avaliacao em tempo de compilacao.
// Esta e a funcao pedida na especificacao — a LUT default NAO existe em
// runtime; ela ja nasce pronta dentro do binario.
consteval std::array<Color, 256> make_default_gradient_lut() {
    return build_gradient_lut<4, 256>(std::array{
        GradientStop{0.00f, "#a89984"_rgb},  // base: cinza-areia
        GradientStop{0.35f, "#b57614"_rgb},  // mostarda
        GradientStop{0.70f, "#d65d0e"_rgb},  // laranja queimado
        GradientStop{1.00f, "#cc241d"_rgb},  // topo: vermelho
    });
}

inline constexpr auto kDefaultGradientLut = make_default_gradient_lut();

// ------------------------------------------------------------ runtime

inline std::vector<Color> build_gradient_lut_rt(std::span<const GradientStop> stops,
                                                std::size_t m) {
    std::vector<GradientStop> s{stops.begin(), stops.end()};
    std::sort(s.begin(), s.end(), [](const GradientStop& a, const GradientStop& b) {
        return a.pos < b.pos;
    });
    std::vector<Color> lut(m);
    for (std::size_t i = 0; i < m; ++i) {
        const float t = m > 1 ? static_cast<float>(i) / static_cast<float>(m - 1) : 0.f;
        lut[i] = interp_stops(std::span{s}, t);
    }
    return lut;
}

} // namespace lv
