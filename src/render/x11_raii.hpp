// Handles RAII move-only para recursos X11 (Display, janela ARGB, Picture).
#pragma once

#include <stdexcept>
#include <utility>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xcomposite.h>

namespace lv {

class XDisplay {
public:
    explicit XDisplay(const char* name = nullptr) : dpy_(XOpenDisplay(name)) {
        if (!dpy_) throw std::runtime_error("nao consegui abrir o display X");
    }
    ~XDisplay() {
        if (dpy_) XCloseDisplay(dpy_);
    }
    XDisplay(const XDisplay&) = delete;
    XDisplay& operator=(const XDisplay&) = delete;
    XDisplay(XDisplay&& other) noexcept
        : dpy_(std::exchange(other.dpy_, nullptr)) {}
    XDisplay& operator=(XDisplay&& other) noexcept {
        if (this != &other) {
            if (dpy_) XCloseDisplay(dpy_);
            dpy_ = std::exchange(other.dpy_, nullptr);
        }
        return *this;
    }
    [[nodiscard]] Display* get() const { return dpy_; }

private:
    Display* dpy_ = nullptr;
};

// Janela ARGB (visual 32-bit) sem decoracoes, com colormap proprio.
class XWindow {
public:
    XWindow() = default;

    struct RectArg {
        int x, y, w, h;
    };

    // Cria a janela sobre `dpy` (ownership do Display fica com o caller).
    XWindow(Display* dpy, Window parent, RectArg rect, XVisualInfo* visual,
            const char* title)
        : dpy_(dpy) {
        cmap_ = XCreateColormap(dpy, parent, visual->visual, AllocNone);
        XSetWindowAttributes attr{};
        attr.colormap = cmap_;
        attr.background_pixel = 0;
        attr.border_pixel = 0;
        win_ = XCreateWindow(dpy, parent, rect.x, rect.y, rect.w, rect.h, 0,
                             visual->depth, InputOutput, visual->visual,
                             CWColormap | CWBackPixel | CWBorderPixel, &attr);
        if (!win_) throw std::runtime_error("XCreateWindow falhou");
        XStoreName(dpy, win_, title);
    }

    ~XWindow() { destroy(); }

    XWindow(const XWindow&) = delete;
    XWindow& operator=(const XWindow&) = delete;
    XWindow(XWindow&& other) noexcept
        : dpy_(std::exchange(other.dpy_, nullptr)),
          win_(std::exchange(other.win_, 0)),
          cmap_(std::exchange(other.cmap_, 0)) {}
    XWindow& operator=(XWindow&& other) noexcept {
        if (this != &other) {
            destroy();
            dpy_ = std::exchange(other.dpy_, nullptr);
            win_ = std::exchange(other.win_, 0);
            cmap_ = std::exchange(other.cmap_, 0);
        }
        return *this;
    }

    [[nodiscard]] Window get() const { return win_; }

private:
    void destroy() {
        if (dpy_ && win_) XDestroyWindow(dpy_, win_);
        if (dpy_ && cmap_) XFreeColormap(dpy_, cmap_);
        dpy_ = nullptr;
        win_ = 0;
        cmap_ = 0;
    }

    Display* dpy_ = nullptr;
    Window win_ = 0;
    Colormap cmap_ = 0;
};

// Picture XRender ligada a uma janela (liberada no dtor).
class XPicture {
public:
    XPicture() = default;
    XPicture(Display* dpy, Window win) : dpy_(dpy) {
        XWindowAttributes wa{};
        XGetWindowAttributes(dpy, win, &wa);
        XRenderPictFormat* fmt = XRenderFindVisualFormat(dpy, wa.visual);
        if (!fmt) throw std::runtime_error("visual sem formato XRender");
        pic_ = XRenderCreatePicture(dpy, win, fmt, 0, nullptr);
        if (!pic_) throw std::runtime_error("XRenderCreatePicture falhou");
    }
    ~XPicture() { destroy(); }
    XPicture(const XPicture&) = delete;
    XPicture& operator=(const XPicture&) = delete;
    XPicture(XPicture&& other) noexcept
        : dpy_(std::exchange(other.dpy_, nullptr)),
          pic_(std::exchange(other.pic_, 0)) {}
    XPicture& operator=(XPicture&& other) noexcept {
        if (this != &other) {
            destroy();
            dpy_ = std::exchange(other.dpy_, nullptr);
            pic_ = std::exchange(other.pic_, 0);
        }
        return *this;
    }
    [[nodiscard]] Picture get() const { return pic_; }

private:
    void destroy() {
        if (dpy_ && pic_) XRenderFreePicture(dpy_, pic_);
        dpy_ = nullptr;
        pic_ = 0;
    }
    Display* dpy_ = nullptr;
    Picture pic_ = 0;
};

} // namespace lv
