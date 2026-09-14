// Bindings Python (pybind11) do analisador de espectro do Lavoe Salsa.
//
//     import lavoe_py
//     alturas = lavoe_py.analyze([0.0, 0.5, ...])   # list[float] ou numpy
//
// Aceita PCM mono float de qualquer tamanho (zero-padding interno ate
// 2048 amostras) e retorna as alturas das 56 barras (0.0 .. 1.0).
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "dsp/spectrum.hpp"

namespace py = pybind11;

namespace {

lv::SpectrumAnalyzer<lv::CavaSmoothing>& analyzer() {
    // instancia unica (padroes), reuso do plano FFT entre chamadas
    static lv::SpectrumAnalyzer<lv::CavaSmoothing> instance(
        56, 48000.f, lv::SmoothingParams{});
    return instance;
}

} // namespace

PYBIND11_MODULE(lavoe_py, m) {
    m.doc() = "Analise de espectro do Lavoe Salsa (Hector Lavoe, 1946-1993)";

    m.def(
        "analyze",
        [](py::array_t<float, py::array::forcecast> pcm) {
            const py::buffer_info info = pcm.request();
            const auto* data = static_cast<const float*>(info.ptr);
            const std::size_t n = static_cast<std::size_t>(info.size);
            const auto heights = analyzer().analyze({data, n});
            return std::vector<float>(heights.begin(), heights.end());
        },
        py::arg("pcm"),
        "Retorna as alturas das 56 barras (0..1) a partir de PCM mono float "
        "(list ou numpy, qualquer tamanho).");
}
