#include "audio/pulse_capture.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>
#include <pulse/pulseaudio.h>
#include <pulse/rtclock.h>

#include "dsp/ring_buffer.hpp"

namespace lv {

namespace {

constexpr pa_sample_spec kSpec = {PA_SAMPLE_FLOAT32LE, 48000, 2};
constexpr pa_stream_flags_t kStreamFlags = PA_STREAM_DONT_MOVE;
constexpr int kWatchdogPeriodUs = 2 * 1000 * 1000;  // 2 s

// pa_rtclock_timeval nao e exportado; montamos o timeval a partir de
// pa_rtclock_now() (microssegundos no relogio do mainloop).
timeval rtclock_timeval_now() {
    const pa_usec_t us = pa_rtclock_now();
    timeval tv{};
    tv.tv_sec = static_cast<time_t>(us / 1000000ULL);
    tv.tv_usec = static_cast<suseconds_t>(us % 1000000ULL);
    return tv;
}

const char* ctx_state_str(pa_context_state_t s) {
    switch (s) {
        case PA_CONTEXT_CONNECTING: return "conectando";
        case PA_CONTEXT_AUTHORIZING: return "autorizando";
        case PA_CONTEXT_SETTING_NAME: return "definindo nome";
        case PA_CONTEXT_READY: return "pronto";
        case PA_CONTEXT_FAILED: return "FALHOU";
        case PA_CONTEXT_TERMINATED: return "terminado";
        default: return "?";
    }
}

const char* stream_state_str(pa_stream_state_t s) {
    switch (s) {
        case PA_STREAM_CREATING: return "criando";
        case PA_STREAM_READY: return "pronto";
        case PA_STREAM_FAILED: return "FALHOU";
        case PA_STREAM_TERMINATED: return "terminado";
        default: return "?";
    }
}

} // namespace

struct PulseCapture::Impl {
    std::string app_name;
    pa_threaded_mainloop* loop = nullptr;
    pa_mainloop_api* api = nullptr;
    pa_context* ctx = nullptr;
    pa_stream* stream = nullptr;
    pa_time_event* watchdog = nullptr;

    std::string sink;                       // sink padrao atual
    AudioRing ring{1u << 16};               // amostras mono
    std::atomic<float> rate{48000.f};
    std::atomic<bool> connected_flag{false};
    std::atomic<pa_usec_t> last_data_us_{0};  // ultima amostra recebida
    bool context_ok = false;
    pa_usec_t stream_attempt_us_ = 0;  // quando o stream atual foi criado
    int thirst_retries_ = 0;           // reconexoes "pronto mas sedento"

    // ------------------------------------------------------- callbacks
    static void ctx_state_cb(pa_context* c, void* userdata);
    static void subscribe_cb(pa_context* c, pa_subscription_event_type_t t,
                             std::uint32_t idx, void* userdata);
    static void server_info_cb(pa_context* c, const pa_server_info* i,
                               void* userdata);
    static void stream_state_cb(pa_stream* s, void* userdata);
    static void stream_read_cb(pa_stream* s, std::size_t nbytes, void* userdata);
    static void watchdog_cb(pa_mainloop_api* a, pa_time_event* e,
                            const timeval* tv, void* userdata);

    // Chamadas sempre na thread do mainloop.
    void recreate_context_locked();
    void request_server_info_locked();
    void maybe_start_stream_locked(const std::string& default_sink);
    void teardown_stream_locked();
    void maintain_locked();  // logica do watchdog
};

void PulseCapture::Impl::ctx_state_cb(pa_context* c, void* userdata) {
    auto* self = static_cast<Impl*>(userdata);
    const pa_context_state_t st = pa_context_get_state(c);
    std::fprintf(stderr, "lavoe-salsa[pulse]: contexto %s\n",
                 ctx_state_str(st));
    switch (st) {
        case PA_CONTEXT_READY:
            self->context_ok = true;
            pa_context_subscribe(c, PA_SUBSCRIPTION_MASK_SERVER, nullptr, nullptr);
            self->request_server_info_locked();
            break;
        case PA_CONTEXT_FAILED:
        case PA_CONTEXT_TERMINATED:
            self->context_ok = false;
            self->connected_flag = false;
            self->teardown_stream_locked();
            break;
        default:
            break;
    }
}

void PulseCapture::Impl::subscribe_cb(pa_context*,
                                      pa_subscription_event_type_t t,
                                      std::uint32_t, void* userdata) {
    if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) ==
        PA_SUBSCRIPTION_EVENT_SERVER) {
        // sink padrao (ou outro estado do servidor) mudou: re-resolve
        auto* self = static_cast<Impl*>(userdata);
        self->request_server_info_locked();
    }
}

void PulseCapture::Impl::server_info_cb(pa_context*, const pa_server_info* i,
                                        void* userdata) {
    auto* self = static_cast<Impl*>(userdata);
    if (i && i->default_sink_name) self->maybe_start_stream_locked(i->default_sink_name);
}

void PulseCapture::Impl::stream_state_cb(pa_stream* s, void* userdata) {
    auto* self = static_cast<Impl*>(userdata);
    const pa_stream_state_t st = pa_stream_get_state(s);
    std::fprintf(stderr, "lavoe-salsa[pulse]: stream %s\n",
                 stream_state_str(st));
    switch (st) {
        case PA_STREAM_READY:
            self->connected_flag = true;
            if (const pa_sample_spec* ss = pa_stream_get_sample_spec(s))
                self->rate = static_cast<float>(ss->rate);
            std::fprintf(stderr, "lavoe-salsa[pulse]: gravando de '%s' (%s, %u Hz, %u canais)\n",
                         pa_stream_get_device_name(s),
                         pa_sample_format_to_string(pa_stream_get_sample_spec(s)->format),
                         pa_stream_get_sample_spec(s)->rate,
                         pa_stream_get_sample_spec(s)->channels);
            break;
        case PA_STREAM_FAILED:
        case PA_STREAM_TERMINATED:
            self->connected_flag = false;
            break;
        default:
            break;
    }
}

void PulseCapture::Impl::stream_read_cb(pa_stream* s, std::size_t, void* userdata) {
    auto* self = static_cast<Impl*>(userdata);
    std::vector<float> mono;
    while (true) {
        const void* data = nullptr;
        std::size_t nbytes = 0;
        if (pa_stream_peek(s, &data, &nbytes) < 0 || nbytes == 0) break;
        if (data) {
            const std::size_t frames = nbytes / (sizeof(float) * 2);
            const float* f = static_cast<const float*>(data);
            mono.reserve(mono.size() + frames);
            for (std::size_t i = 0; i < frames; ++i)
                mono.push_back((f[2 * i] + f[2 * i + 1]) * 0.5f);  // mixdown
        }
        pa_stream_drop(s);
    }
    if (!mono.empty()) {
        self->last_data_us_.store(pa_rtclock_now(), std::memory_order_relaxed);
        self->thirst_retries_ = 0;
        self->ring.push(mono);
    }
}

void PulseCapture::Impl::watchdog_cb(pa_mainloop_api* a, pa_time_event* e,
                                     const timeval*, void* userdata) {
    auto* self = static_cast<Impl*>(userdata);
    self->maintain_locked();
    timeval next = rtclock_timeval_now();
    pa_timeval_add(&next, kWatchdogPeriodUs);
    a->time_restart(e, &next);
}

// ------------------------------------------------------------- internals

void PulseCapture::Impl::recreate_context_locked() {
    if (ctx) {
        pa_context_disconnect(ctx);
        pa_context_unref(ctx);
        ctx = nullptr;
    }
    context_ok = false;
    connected_flag = false;
    sink.clear();
    ctx = pa_context_new(api, app_name.c_str());
    if (!ctx) return;
    pa_context_set_state_callback(ctx, &ctx_state_cb, this);
    pa_context_set_subscribe_callback(ctx, &subscribe_cb, this);
    pa_context_connect(ctx, nullptr, PA_CONTEXT_NOFLAGS, nullptr);
}

void PulseCapture::Impl::request_server_info_locked() {
    if (ctx && context_ok)
        pa_context_get_server_info(ctx, &server_info_cb, this);
}

void PulseCapture::Impl::maybe_start_stream_locked(
    const std::string& default_sink) {
    // Nunca derruba um stream que esta CREATING/READY no alvo certo — a
    // criacao pode levar alguns instantes e reentradas desta funcao (ex.:
    // evento SERVER seguido de resposta de get_server_info) nao devem
    // reiniciar o processo.
    if (sink == default_sink && stream) {
        const pa_stream_state_t st = pa_stream_get_state(stream);
        if (st == PA_STREAM_CREATING || st == PA_STREAM_READY) return;
    }

    sink = default_sink;
    stream_attempt_us_ = pa_rtclock_now();
    teardown_stream_locked();

    pa_channel_map map;
    pa_channel_map_init_stereo(&map);
    stream = pa_stream_new(ctx, app_name.c_str(), &kSpec, &map);
    if (!stream) return;

    pa_stream_set_state_callback(stream, &stream_state_cb, this);
    pa_stream_set_read_callback(stream, &stream_read_cb, this);

    const std::string target = default_sink + ".monitor";
    // attr NULL: deixa o servidor escolher o fragmento. Um fragsize fixo
    // pequeno demais pode fazer o servidor segurar os dados no pipewire.
    if (pa_stream_connect_record(stream, target.c_str(), nullptr, kStreamFlags) < 0) {
        std::fprintf(stderr, "lavoe-salsa[pulse]: connect_record(%s) falhou: %s\n",
                     target.c_str(), pa_strerror(pa_context_errno(ctx)));
        pa_stream_unref(stream);
        stream = nullptr;
    }
}

void PulseCapture::Impl::teardown_stream_locked() {
    connected_flag = false;
    if (!stream) return;
    pa_stream_disconnect(stream);
    pa_stream_unref(stream);
    stream = nullptr;
}

void PulseCapture::Impl::maintain_locked() {
    // Recria o contexto APENAS se ele falhou de fato. Um contexto lento
    // (ainda autenticando) nao deve ser derrubado — o servidor pode levar
    // mais de 2s para responder em momentos de carga.
    const pa_context_state_t cs = ctx ? pa_context_get_state(ctx)
                                      : PA_CONTEXT_TERMINATED;
    if (!ctx || cs == PA_CONTEXT_FAILED || cs == PA_CONTEXT_TERMINATED) {
        recreate_context_locked();
        return;
    }
    // Stream: so re-age se nao existir ou se falhou. CREATING recebe um
    // periodo de graca de 10s antes de uma nova tentativa.
    if (stream) {
        const pa_stream_state_t st = pa_stream_get_state(stream);
        if (st == PA_STREAM_CREATING &&
            pa_rtclock_now() - stream_attempt_us_ < 10 * 1000 * 1000)
            return;
        if (st == PA_STREAM_READY) {
            // "Pronto mas sedento": READY ha varios segundos sem nenhuma
            // amostra. O monitor do sink as vezes nao alimenta streams que
            // foram criados durante o silencio (sink suspenso) — reconectar
            // resolve. Em silencio prolongado, o intervalo cresce
            // (5s, 10s, 20s, 40s...) para nao ficar chaveando.
            const pa_usec_t ref = std::max(
                stream_attempt_us_,
                last_data_us_.load(std::memory_order_relaxed));
            const pa_usec_t limite =
                (5LL << std::min(thirst_retries_, 3)) * 1000 * 1000;
            if (pa_rtclock_now() - ref > limite) {
                std::fprintf(stderr,
                             "lavoe-salsa[pulse]: stream READY sem dados; reconectando\n");
                ++thirst_retries_;
                teardown_stream_locked();
                stream_attempt_us_ = pa_rtclock_now();
                maybe_start_stream_locked(sink);
            }
            return;
        }
        if (st != PA_STREAM_FAILED && st != PA_STREAM_TERMINATED) return;
    }
    request_server_info_locked();  // re-resolve e reconecta via callback
}

// ------------------------------------------------------------ public API

PulseCapture::PulseCapture(std::string app_name) : impl_(new Impl) {
    impl_->app_name = std::move(app_name);
    impl_->loop = pa_threaded_mainloop_new();
    if (!impl_->loop) {
        delete impl_;
        impl_ = nullptr;
        throw std::runtime_error("pa_threaded_mainloop_new falhou");
    }
    impl_->api = pa_threaded_mainloop_get_api(impl_->loop);

    if (pa_threaded_mainloop_start(impl_->loop) < 0) {
        pa_threaded_mainloop_free(impl_->loop);
        delete impl_;
        impl_ = nullptr;
        throw std::runtime_error("pa_threaded_mainloop_start falhou");
    }

    pa_threaded_mainloop_lock(impl_->loop);
    impl_->recreate_context_locked();
    // watchdog: mantem tudo vivo (reconexao apos quedas)
    timeval tv = rtclock_timeval_now();
    pa_timeval_add(&tv, kWatchdogPeriodUs);
    impl_->watchdog = impl_->api->time_new(impl_->api, &tv, &Impl::watchdog_cb, impl_);
    pa_threaded_mainloop_unlock(impl_->loop);
}

PulseCapture::~PulseCapture() {
    if (!impl_) return;
    pa_threaded_mainloop_lock(impl_->loop);
    if (impl_->watchdog) impl_->api->time_free(impl_->watchdog);
    impl_->teardown_stream_locked();
    if (impl_->ctx) {
        pa_context_disconnect(impl_->ctx);
        pa_context_unref(impl_->ctx);
    }
    pa_threaded_mainloop_unlock(impl_->loop);
    pa_threaded_mainloop_stop(impl_->loop);
    pa_threaded_mainloop_free(impl_->loop);
    delete impl_;
}

PulseCapture::PulseCapture(PulseCapture&& other) noexcept
    : impl_(std::exchange(other.impl_, nullptr)) {}

PulseCapture& PulseCapture::operator=(PulseCapture&& other) noexcept {
    if (this != &other) {
        this->~PulseCapture();
        impl_ = std::exchange(other.impl_, nullptr);
    }
    return *this;
}

std::size_t PulseCapture::read_latest(std::span<float> dst) {
    return impl_ ? impl_->ring.read_latest(dst) : 0;
}

float PulseCapture::sample_rate() const {
    return impl_ ? impl_->rate.load(std::memory_order_relaxed) : 48000.f;
}

bool PulseCapture::connected() const {
    return impl_ && impl_->connected_flag.load(std::memory_order_relaxed);
}

} // namespace lv
