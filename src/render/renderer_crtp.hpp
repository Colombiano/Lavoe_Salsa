// Base CRTP para renderizadores: fornece o metodo publico `render` que
// delega no `do_render` do tipo derivado. Satisfaz o conceito BarRenderer
// sem polimorfismo virtual em runtime.
#pragma once

#include <span>
#include "core/concepts.hpp"

namespace lv {

template <typename Derived>
struct RendererCRTP {
    template <ColorMap M>
    void render(std::span<const float> levels, const M& map, float t) {
        static_cast<Derived*>(this)->do_render(levels, map, t);
    }
};

} // namespace lv
