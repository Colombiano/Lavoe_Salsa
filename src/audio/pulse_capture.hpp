// Captura de audio via libpulse (threaded mainloop).
//
// - Grava da fonte .monitor do sink PADRAO (captura qualquer player:
//   ytm-player, mpv, navegador...).
// - Re-resolve o sink padrao quando o servidor avisa (evento SERVER) e
//   reconecta; watchdog reconecta apos queda do servidor/stream.
// - Move-semantics de verdade via pimpl: o objeto em movimento transfere
//   o ponteiro estavel `impl_`, usado pelos callbacks do Pulse.
#pragma once

#include <cstddef>
#include <span>
#include <string>

namespace lv {

class PulseCapture {
public:
    explicit PulseCapture(std::string app_name = "lavoe-salsa");
    ~PulseCapture();

    PulseCapture(const PulseCapture&) = delete;
    PulseCapture& operator=(const PulseCapture&) = delete;
    PulseCapture(PulseCapture&& other) noexcept;
    PulseCapture& operator=(PulseCapture&& other) noexcept;

    // Copia as amostras mono mais recentes para dst (zero-padding).
    [[nodiscard]] std::size_t read_latest(std::span<float> dst);

    // Taxa de amostragem real negociada com o servidor (48000 ate la).
    [[nodiscard]] float sample_rate() const;

    // True quando o stream de gravacao esta ativo.
    [[nodiscard]] bool connected() const;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace lv
