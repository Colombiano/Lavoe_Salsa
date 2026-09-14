// Analisador de espectro (policy-based): FFT + mapeamento log de
// frequencias + suavizacao configuravel por politica.
//
//     SpectrumAnalyzer<CavaSmoothing> dsp(56, 48000.f, params);
//     auto alturas = dsp.analyze(pcm);   // span<float> de qualquer tamanho
//
// Uso compartilhado pelo binario principal e pelos bindings Python.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>
#include "dsp/fft.hpp"
#include "dsp/smoothing.hpp"

namespace lv {

template <typename SmoothingPolicy = CavaSmoothing>
    requires SmoothingPolicyLike<SmoothingPolicy>
class SpectrumAnalyzer {
public:
    static constexpr std::size_t kFftSize = 2048;
    static constexpr std::size_t kBins = kFftSize / 2;  // 1024
    static constexpr float kFmin = 20.f;                // Hz
    static constexpr float kFmax = 16000.f;             // Hz (clampa no nyquist)

    SpectrumAnalyzer(std::size_t bars, float sample_rate, SmoothingParams params)
        : fft_(kFftSize), bars_(bars), rate_(sample_rate), params_(params) {
        hann_.resize(kFftSize);
        for (std::size_t i = 0; i < kFftSize; ++i)
            hann_[i] = 0.5f * (1.f - std::cos(2.f * std::acos(-1.f) *
                                              static_cast<float>(i) /
                                              static_cast<float>(kFftSize - 1)));
        raw_.resize(bars_);
        prev_.resize(bars_);
        rebuild_mapping();
    }

    // Nao copiavel, movivel (o plano FFT e caro de recriar).
    SpectrumAnalyzer(const SpectrumAnalyzer&) = delete;
    SpectrumAnalyzer& operator=(const SpectrumAnalyzer&) = delete;
    SpectrumAnalyzer(SpectrumAnalyzer&&) = default;
    SpectrumAnalyzer& operator=(SpectrumAnalyzer&&) = default;

    // PCM mono; qualquer tamanho (zero-padding se < FFT, cauda ignorada se >).
    std::span<const float> analyze(std::span<const float> pcm) {
        auto in = fft_.in();
        const std::size_t n = std::min(pcm.size(), in.size());
        for (std::size_t i = 0; i < n; ++i) in[i] = pcm[i] * hann_[i];
        for (std::size_t i = n; i < in.size(); ++i) in[i] = 0.f;

        fft_.execute();

        // Magnitudes normalizadas: senoide de amplitude A (0..1) com janela
        // de Hann da |X| ~= A * N/4 (coherent gain 0.5). Logo norm = |X|/(N/4)
        // aproxima a amplitude original por componente.
        const float scale = 4.f / static_cast<float>(kFftSize);
        auto out = fft_.out();
        for (std::size_t i = 0; i < bars_; ++i) {
            const std::size_t lo = lo_[i], hi = hi_[i];
            float sum = 0.f;
            for (std::size_t k = lo; k <= hi && k < out.size(); ++k)
                sum += std::hypot(out[k][0], out[k][1]);
            raw_[i] = sum / static_cast<float>(hi - lo + 1) * scale;
        }

        SmoothingPolicy::apply(prev_, raw_, params_);
        return prev_;
    }

    [[nodiscard]] std::size_t bars() const { return bars_; }
    [[nodiscard]] float sample_rate() const { return rate_; }

    void set_sample_rate(float rate) {
        if (rate > 0.f && std::abs(rate - rate_) > 1.f) {
            rate_ = rate;
            rebuild_mapping();
        }
    }

private:
    void rebuild_mapping() {
        const float nyquist = rate_ * 0.5f;
        const float fmax = std::min(kFmax, nyquist * 0.95f);
        const float bin_hz = rate_ / static_cast<float>(kFftSize);
        const float ratio = std::pow(fmax / kFmin, 1.f / static_cast<float>(bars_));
        lo_.resize(bars_);
        hi_.resize(bars_);
        for (std::size_t i = 0; i < bars_; ++i) {
            const float flo = kFmin * std::pow(ratio, static_cast<float>(i));
            const float fhi = kFmin * std::pow(ratio, static_cast<float>(i + 1));
            auto to_bin = [&](float f) {
                return std::clamp(static_cast<std::size_t>(f / bin_hz),
                                  std::size_t{1}, kBins - 1);
            };
            lo_[i] = to_bin(flo);
            hi_[i] = std::max(to_bin(fhi), lo_[i]);
        }
    }

    FftwPlanR2C fft_;
    std::size_t bars_;
    float rate_;
    SmoothingParams params_;
    std::vector<float> hann_;
    std::vector<float> raw_;
    std::vector<float> prev_;
    std::vector<std::size_t> lo_, hi_;
};

} // namespace lv
