// ColorMap padrao: gradiente vertical por faixa de pixel (LUT de 256 cores).
//
// O construtor aceita stops do config Lua; com lista vazia, usa a LUT
// gerada em TEMPO DE COMPILACAO (kDefaultGradientLut).
#pragma once

#include <algorithm>
#include <array>
#include <vector>
#include "dsp/gradient.hpp"

namespace lv {

class GradientMap {
public:
    static constexpr bool kPerStrip = true;
    static constexpr std::size_t kLutSize = 256;

    explicit GradientMap(const std::vector<GradientStop>& stops) {
        if (stops.empty()) {
            lut_ = kDefaultGradientLut;
        } else {
            const auto v = build_gradient_lut_rt(stops, lut_.size());
            std::copy(v.begin(), v.end(), lut_.begin());
        }
    }

    [[nodiscard]] Color sample(float t) const {
        const float u = clamp01(t);
        return lut_[static_cast<std::size_t>(u * static_cast<float>(lut_.size() - 1) + 0.5f)];
    }

private:
    std::array<Color, kLutSize> lut_{};
};

} // namespace lv
