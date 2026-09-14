// Politicas de suavizacao (policy-based design).
//
// Cada politica e um functor estatico, stateless: o estado (niveis
// anteriores) fica no SpectrumAnalyzer. O trait kLogDomain seleciona via
// if constexpr o caminho de normalizacao dentro da politica.
#pragma once

#include <algorithm>
#include <cmath>
#include <span>

namespace lv {

struct SmoothingParams {
    float attack = 0.65f;      // subida (0..1, maior = mais rapido)
    float decay = 0.18f;       // descida (0..1, menor = gravidade mais suave)
    float noise_floor = 0.02f; // piso de ruido
    float gamma = 1.0f;        // curva perceptual
    float db_floor = -55.f;    // piso de dB para o dominio logaritmico
};

// Politica padrao, estilo cava: normalizacao logaritmica (dB) + ataque
// instantaneo rapido e queda com gravidade.
struct CavaSmoothing {
    static constexpr bool kLogDomain = true;

    static void apply(std::span<float> prev, std::span<const float> raw,
                      const SmoothingParams& p) {
        for (std::size_t i = 0; i < raw.size(); ++i) {
            float v = raw[i];
            if constexpr (kLogDomain) {
                const float db = 20.f * std::log10(v + 1e-6f);
                v = std::clamp((db - p.db_floor) / -p.db_floor, 0.f, 1.f);
            } else {
                v = std::clamp(v, 0.f, 1.f);
            }
            v = std::max(0.f, v - p.noise_floor) / (1.f - p.noise_floor);
            v = std::pow(std::clamp(v, 0.f, 1.f), p.gamma);

            if (v > prev[i])
                prev[i] += (v - prev[i]) * p.attack;  // ataque
            else
                prev[i] += (v - prev[i]) * p.decay;   // queda (gravidade)
            prev[i] = std::clamp(prev[i], 0.f, 1.f);
        }
    }
};

// Politica simples: EMA (media movel exponencial) em dominio linear.
// Demonstra o lado "policy-based" do design: troca-se o template e muda
// o comportamento do analisador inteiro.
struct ExponentialSmoothing {
    static constexpr bool kLogDomain = false;

    static void apply(std::span<float> prev, std::span<const float> raw,
                      const SmoothingParams& p) {
        const float alpha = 0.35f;
        for (std::size_t i = 0; i < raw.size(); ++i) {
            const float v = std::clamp(raw[i], 0.f, 1.f);
            const float target = (v > prev[i]) ? v : v;
            prev[i] += (target - prev[i]) * (v > prev[i] ? p.attack : alpha);
            prev[i] = std::clamp(prev[i], 0.f, 1.f);
        }
    }
};

// Conceito: politica precisa expor o trait e o functor estatico.
template <typename P>
concept SmoothingPolicyLike = requires(std::span<float> prev,
                                       std::span<const float> raw,
                                       const SmoothingParams& p) {
    { P::kLogDomain } -> std::convertible_to<bool>;
    { P::apply(prev, raw, p) } -> std::same_as<void>;
};

} // namespace lv
