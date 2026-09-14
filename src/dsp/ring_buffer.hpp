// Ring buffer circular de amostras mono (thread-safe via mutex simples).
//
// O callback de leitura do PulseAudio (thread do mainloop) produz;
// o loop principal de renderizacao consome as amostras MAIS RECENTES a
// cada frame — atraso acumulado e simplesmente descartado.
#pragma once

#include <algorithm>
#include <cstddef>
#include <mutex>
#include <span>
#include <vector>

namespace lv {

class AudioRing {
public:
    explicit AudioRing(std::size_t capacity = 1u << 16) : buf_(capacity) {}

    void push(std::span<const float> samples) {
        std::lock_guard lock{m_};
        if (samples.size() >= buf_.size()) {  // mantem so a cauda
            const std::size_t off = samples.size() - buf_.size();
            std::copy_n(samples.begin() + static_cast<std::ptrdiff_t>(off),
                        buf_.size(), buf_.begin());
            w_ = 0;
            sz_ = buf_.size();
            return;
        }
        for (float s : samples) {
            buf_[w_] = s;
            w_ = (w_ + 1) % buf_.size();
        }
        sz_ = std::min(sz_ + samples.size(), buf_.size());
    }

    // Copia as `dst.size()` amostras mais recentes para o FINAL de dst
    // (zero-padding no inicio se o buffer ainda estiver pobre). Retorna o
    // numero de amostras validas copiadas.
    [[nodiscard]] std::size_t read_latest(std::span<float> dst) {
        std::lock_guard lock{m_};
        const std::size_t n = std::min(dst.size(), sz_);
        if (n == 0) {
            std::fill(dst.begin(), dst.end(), 0.f);
            return 0;
        }
        const std::size_t start = (w_ + buf_.size() - n) % buf_.size();
        if (start + n <= buf_.size()) {
            std::copy_n(buf_.begin() + static_cast<std::ptrdiff_t>(start), n,
                        dst.end() - static_cast<std::ptrdiff_t>(n));
        } else {
            const std::size_t first = buf_.size() - start;
            std::copy_n(buf_.begin() + static_cast<std::ptrdiff_t>(start), first,
                        dst.end() - static_cast<std::ptrdiff_t>(n));
            std::copy_n(buf_.begin(), n - first,
                        dst.end() - static_cast<std::ptrdiff_t>(n - first));
        }
        std::fill(dst.begin(), dst.end() - static_cast<std::ptrdiff_t>(n), 0.f);
        return n;
    }

    [[nodiscard]] std::size_t size() const {
        std::lock_guard lock{m_};
        return sz_;
    }

private:
    mutable std::mutex m_;
    std::vector<float> buf_;
    std::size_t w_ = 0;
    std::size_t sz_ = 0;
};

} // namespace lv
