// Plano FFTW (r2c, single precision) embrulhado em RAII move-only.
#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>
#include <fftw3.h>

namespace lv {

// Deleter customizado: FFTW exige alinhamento proprio (fftw_malloc/free).
struct FftwFree {
    void operator()(void* p) const {
        if (p) fftwf_free(p);
    }
};
template <typename T>
using fftw_ptr = std::unique_ptr<T, FftwFree>;

class FftwPlanR2C {
public:
    explicit FftwPlanR2C(std::size_t n) : n_(n) {
        in_.reset(static_cast<float*>(fftwf_malloc(sizeof(float) * n)));
        out_.reset(static_cast<fftwf_complex*>(
            fftwf_malloc(sizeof(fftwf_complex) * (n / 2 + 1))));
        if (!in_ || !out_) throw std::bad_alloc();
        // FFTW_ESTIMATE: nao le/escreve os buffers durante o planejamento.
        plan_ = fftwf_plan_dft_r2c_1d(static_cast<int>(n), in_.get(), out_.get(),
                                      FFTW_ESTIMATE);
        if (!plan_) throw std::runtime_error("fftwf_plan_dft_r2c_1d falhou");
    }

    ~FftwPlanR2C() {
        if (plan_) fftwf_destroy_plan(plan_);
    }

    FftwPlanR2C(const FftwPlanR2C&) = delete;
    FftwPlanR2C& operator=(const FftwPlanR2C&) = delete;

    FftwPlanR2C(FftwPlanR2C&& other) noexcept
        : in_(std::move(other.in_)), out_(std::move(other.out_)),
          plan_(std::exchange(other.plan_, nullptr)), n_(other.n_) {}

    FftwPlanR2C& operator=(FftwPlanR2C&& other) noexcept {
        if (this != &other) {
            if (plan_) fftwf_destroy_plan(plan_);
            in_ = std::move(other.in_);
            out_ = std::move(other.out_);
            plan_ = std::exchange(other.plan_, nullptr);
            n_ = other.n_;
        }
        return *this;
    }

    [[nodiscard]] std::size_t size() const { return n_; }

    std::span<float> in() { return {in_.get(), n_}; }
    std::span<const fftwf_complex> out() const { return {out_.get(), n_ / 2 + 1}; }

    void execute() { fftwf_execute(plan_); }

private:
    fftw_ptr<float> in_;
    fftw_ptr<fftwf_complex> out_;
    fftwf_plan plan_ = nullptr;
    std::size_t n_ = 0;
};

} // namespace lv
