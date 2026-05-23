// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <gui-qt/game_window.h>
#include <gui-qt/gui_settings.h>

#include <app/functions.h>
#include <config/config.h>
#include <config/state.h>
#include <config/version.h>
#include <dialog/state.h>
#include <display/state.h>
#include <ime/state.h>
#include <io/state.h>
#include <renderer/state.h>
#include <touch/state.h>
#include <util/log.h>

#include <QApplication>
#include <QCheckBox>
#include <QGuiApplication>
#include <QInputMethod>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QScreen>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>

#include <QtGui/qopenglcontext_platform.h>

#include <cstdint>

#if defined(HAVE_X11) || defined(HAVE_WAYLAND)
#include <QGuiApplication>
#include <qpa/qplatformnativeinterface.h>
#endif

#if QT_CONFIG(xcb_glx_plugin)
#include <GL/glx.h>
#endif

#ifdef _WIN32
#include <Windows.h>
#endif

namespace {

bool set_swap_interval(QOpenGLContext *context, int interval) {
    if (!context) {
        LOG_WARN("Cannot update OpenGL swap interval without a current context");
        return false;
    }

#ifdef _WIN32
    using WglSwapIntervalProc = BOOL(WINAPI *)(int);
    if (const auto proc = reinterpret_cast<WglSwapIntervalProc>(context->getProcAddress("wglSwapIntervalEXT")))
        return proc(interval) == TRUE;

    LOG_WARN("wglSwapIntervalEXT is not available");
    return false;
#else
#if QT_CONFIG(xcb_glx_plugin)
    using GlXSwapIntervalExtProc = void (*)(Display *, GLXDrawable, int);
    if (const auto proc = reinterpret_cast<GlXSwapIntervalExtProc>(context->getProcAddress("glXSwapIntervalEXT"))) {
        if (const auto drawable = glXGetCurrentDrawable()) {
            proc(glXGetCurrentDisplay(), drawable, interval);
            return true;
        }
    }
#endif

#if QT_CONFIG(egl)
    if (auto *egl = context->nativeInterface<QNativeInterface::QEGLContext>()) {
        using EglSwapIntervalProc = int (*)(void *, int);
        if (const auto proc = reinterpret_cast<EglSwapIntervalProc>(context->getProcAddress("eglSwapInterval")))
            return proc(egl->display(), interval) != 0;
    }
#endif

    LOG_WARN("No OpenGL swap interval update path is available on this platform");
    return false;
#endif
}

#ifdef _WIN32
constexpr DWORD dwm_window_corner_preference_attribute = 33;

enum class DwmWindowCornerPreference : DWORD {
    Default = 0,
    DoNotRound = 1,
    Round = 2,
    RoundSmall = 3
};

using DwmSetWindowAttributeProc = HRESULT(WINAPI *)(HWND, DWORD, LPCVOID, DWORD);

bool set_window_corner_preference(HWND hwnd, bool rounded) {
    static const auto proc = []() -> DwmSetWindowAttributeProc {
        const HMODULE dwmapi = LoadLibraryW(L"dwmapi.dll");
        if (!dwmapi)
            return nullptr;
        return reinterpret_cast<DwmSetWindowAttributeProc>(GetProcAddress(dwmapi, "DwmSetWindowAttribute"));
    }();

    if (!proc || !hwnd)
        return false;

    const auto preference = rounded
        ? DwmWindowCornerPreference::Round
        : DwmWindowCornerPreference::DoNotRound;
    return SUCCEEDED(proc(hwnd, dwm_window_corner_preference_attribute, &preference, sizeof(preference)));
}
#endif

} // namespace

GameWindow::GameWindow(EmuEnvState &emuenv, std::shared_ptr<GuiSettings> gui_settings, renderer::Backend backend, QScreen *screen)
    : QWindow(screen)
    , m_emuenv(emuenv)
    , m_gui_settings(std::move(gui_settings))
    , m_backend(backend) {
    if (m_backend == renderer::Backend::OpenGL) {
        setSurfaceType(QSurface::OpenGLSurface);

        m_format.setRenderableType(QSurfaceFormat::OpenGL);
        m_format.setMajorVersion(4);
        m_format.setMinorVersion(3);
        m_format.setProfile(QSurfaceFormat::CoreProfile);
        m_format.setDepthBufferSize(0);
        m_format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
        m_format.setSwapInterval(static_cast<int>(emuenv.cfg.current_config.v_sync));
        setFormat(m_format);
    } else {
#ifdef __APPLE__
        setSurfaceType(QSurface::VulkanSurface);
#endif
    }

    setMinimumSize(QSize(160, 90));
    resize(960, 544);
    setFlags(Qt::Window);

    const auto &cc = emuenv.cfg.current_config;
    m_title_backend_renderer = cc.backend_renderer;
    sync_native_window_preferences();
}

GameWindow::~GameWindow() {
    stop_ui_updates();
    destroy_render_context();
}

bool GameWindow::create_gl_context() {
    if (m_gl_context) {
        LOG_WARN("GL context already created");
        return true;
    }

    m_gl_context = new QOpenGLContext();

    QSurfaceFormat fmt = m_format;
#ifndef NDEBUG
    fmt.setOption(QSurfaceFormat::DebugContext);
#endif

    // Try GL versions 4.6 -> 4.5 -> 4.4
    constexpr int accept_minor_versions[] = { 6, 5, 4 };
    bool created = false;
    for (int minor : accept_minor_versions) {
        fmt.setMajorVersion(4);
        fmt.setMinorVersion(minor);
        m_gl_context->setFormat(fmt);
        if (m_gl_context->create()) {
            created = true;
            LOG_INFO("Created OpenGL {}.{} context", 4, minor);
            break;
        }
    }

    if (!created) {
        LOG_ERROR("Failed to create OpenGL context (need at least ver 4.4)");
        delete m_gl_context;
        m_gl_context = nullptr;
        return false;
    }

    return true;
}

bool GameWindow::make_current() {
    if (!m_gl_context) {
        LOG_ERROR("make_current called with no GL context");
        return false;
    }

    if (m_gl_migration_pending && QThread::currentThread() != m_gl_context->thread()) {
        m_render_thread_id.set_value(QThread::currentThread());
        m_gl_migration_done_future.wait();
        m_gl_migration_pending = false;
    }

    if (!m_gl_context->makeCurrent(this)) {
        LOG_ERROR("makeCurrent failed");
        return false;
    }

    return true;
}

renderer::DisplayHandle GameWindow::handle() const {
#ifdef _WIN32
    return renderer::Win32DisplayHandle{ reinterpret_cast<void *>(winId()) };
#elif defined(__APPLE__)
    return renderer::MacOSDisplayHandle{ reinterpret_cast<void *>(winId()) };
#elif defined(HAVE_X11) || defined(HAVE_WAYLAND)
    QPlatformNativeInterface *native = QGuiApplication::platformNativeInterface();
    if (!native) {
        LOG_ERROR("Could not get QPlatformNativeInterface, display protocol unknown");
        return {};
    }

#if defined(HAVE_WAYLAND)
    if (void *wl_display = native->nativeResourceForWindow("display", nullptr)) {
        if (void *wl_surface = native->nativeResourceForWindow("surface", const_cast<GameWindow *>(this))) {
            return renderer::WaylandDisplayHandle{ wl_display, wl_surface };
        }
    }
#endif

#if defined(HAVE_X11)
    void *display = native->nativeResourceForIntegration("display");
    void *connection = native->nativeResourceForIntegration("connection");
    return renderer::X11DisplayHandle{
        .display = display,
        .window = static_cast<std::uintptr_t>(winId()),
        .connection = connection,
    };
#else
    return {};
#endif
#else
    return {};
#endif
}

int GameWindow::drawable_width() const {
#ifdef _WIN32
    RECT rect;
    if (GetClientRect(reinterpret_cast<HWND>(winId()), &rect))
        return rect.right - rect.left;
#endif
    return static_cast<int>(width() * devicePixelRatio());
}

int GameWindow::drawable_height() const {
#ifdef _WIN32
    RECT rect;
    if (GetClientRect(reinterpret_cast<HWND>(winId()), &rect))
        return rect.bottom - rect.top;
#endif
    return static_cast<int>(height() * devicePixelRatio());
}

std::vector<std::string> GameWindow::font_dirs() const {
    const QStringList locations = QStandardPaths::standardLocations(QStandardPaths::FontsLocation);
    std::vector<std::string> dirs;
    dirs.reserve(locations.size());

    for (const QString &location : locations) {
        std::string font_dir = location.toUtf8().constData();
        if (!font_dir.ends_with('/') && !font_dir.ends_with('\\'))
            font_dir += '/';
        dirs.push_back(std::move(font_dir));
    }

    return dirs;
}

void *GameWindow::get_proc_address(const char *name) const {
    if (!m_gl_context)
        return nullptr;

    return reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(m_gl_context->getProcAddress(name)));
}

unsigned int GameWindow::default_fbo() const {
    return m_gl_context ? m_gl_context->defaultFramebufferObject() : 0;
}

void GameWindow::swap_buffers() {
    if (m_gl_context && isExposed()) {
        m_gl_context->swapBuffers(this);
    }
}

void GameWindow::done_current() {
    if (m_gl_context) {
        m_gl_context->doneCurrent();
        if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
            m_gl_context->moveToThread(QCoreApplication::instance()->thread());
        }
    }
}

bool GameWindow::set_vsync(bool enabled) {
    QOpenGLContext *context = QOpenGLContext::currentContext();
    if (!context)
        context = m_gl_context;

    return set_swap_interval(context, enabled ? 1 : 0);
}

void GameWindow::destroy_render_context() {
    if (m_gl_context) {
        m_gl_context->doneCurrent();
        delete m_gl_context;
        m_gl_context = nullptr;
    }
}

void GameWindow::prepare_for_render_thread() {
    if (m_backend != renderer::Backend::OpenGL || !m_gl_context)
        return;

    done_current();
    prepare_gl_for_render_thread();
}

void GameWindow::finalize_render_thread_start() {
    if (m_backend != renderer::Backend::OpenGL || !m_gl_context)
        return;

    complete_gl_migration();
}

void GameWindow::prepare_gl_for_render_thread() {
    m_render_thread_id = std::promise<QThread *>();
    m_render_thread_id_future = m_render_thread_id.get_future();
    m_gl_migration_done = std::promise<void>();
    m_gl_migration_done_future = m_gl_migration_done.get_future().share();
    m_gl_migration_pending = true;
}

void GameWindow::complete_gl_migration() {
    QThread *render_qthread = m_render_thread_id_future.get();
    m_gl_context->moveToThread(render_qthread);
    m_gl_migration_done.set_value();
}

void GameWindow::start_ui_updates() {
    if (m_ui_timer)
        return;

    m_ui_timer = new QTimer(this);
    m_ui_timer->setInterval(16);
    connect(m_ui_timer, &QTimer::timeout, this, &GameWindow::ui_tick);
    m_ui_timer->start();
}

void GameWindow::stop_ui_updates() {
    if (m_ui_timer) {
        m_ui_timer->stop();
        delete m_ui_timer;
        m_ui_timer = nullptr;
    }
    m_runtime_metrics = {};
}

void GameWindow::ui_tick() {
    sync_native_window_preferences();
    update_window_title();

    const bool dialog_ime_active = (m_emuenv.common_dialog.type == IME_DIALOG
        && m_emuenv.common_dialog.status == SCE_COMMON_DIALOG_STATUS_RUNNING);
    const bool ime_active = m_emuenv.ime.state || dialog_ime_active;
    auto *im = QGuiApplication::inputMethod();

    if (ime_active && !m_ime_was_active) {
        im->update(Qt::ImEnabled);
    } else if (!ime_active && m_ime_was_active) {
        im->hide();
        im->update(Qt::ImEnabled);
    }
    m_ime_was_active = ime_active;
}

void GameWindow::sync_native_window_preferences() {
#ifdef _WIN32
    const bool rounded_corners = m_gui_settings
        ? m_gui_settings->get_value(gui::gw_roundedCorners).toBool()
        : false;
    if (m_last_windows_rounded_corners.has_value() && *m_last_windows_rounded_corners == rounded_corners)
        return;

    set_window_corner_preference(reinterpret_cast<HWND>(winId()), rounded_corners);
    m_last_windows_rounded_corners = rounded_corners;
#endif
}

bool GameWindow::event(QEvent *e) {
    if (e->type() == QEvent::Close) {
        const bool confirm_exit_app = m_gui_settings
            ? m_gui_settings->get_value(gui::mw_confirmExitApp).toBool()
            : true;
        bool confirmed = true;

        if (confirm_exit_app) {
            QMessageBox box;
            box.setIcon(QMessageBox::Question);
            box.setWindowIcon(icon());
            box.setWindowTitle(tr("Exit App?"));
            box.setText(tr("Do you really want to exit the app?\n\nAny unsaved progress will be lost!"));
            box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            box.setDefaultButton(QMessageBox::No);
            box.setWindowModality(Qt::WindowModal);

            auto *dont_show_again = new QCheckBox(tr("Don't show this again"), &box);
            box.setCheckBox(dont_show_again);

            box.winId();
            if (auto *handle = box.windowHandle())
                handle->setTransientParent(this);

            confirmed = box.exec() == QMessageBox::Yes;
            if (confirmed && dont_show_again->isChecked() && m_gui_settings)
                m_gui_settings->set_value(gui::mw_confirmExitApp, false);
        }

        if (confirmed) {
            e->ignore();
            Q_EMIT closed();
            return true;
        }

        e->ignore();
        requestActivate();
        return true;
    }
    if (e->type() == QEvent::InputMethodQuery) {
        auto *query = static_cast<QInputMethodQueryEvent *>(e);
        const bool any_ime = m_emuenv.ime.state
            || (m_emuenv.common_dialog.type == IME_DIALOG
                && m_emuenv.common_dialog.status == SCE_COMMON_DIALOG_STATUS_RUNNING);
        if (any_ime) {
            if (query->queries() & Qt::ImEnabled)
                query->setValue(Qt::ImEnabled, true);
            if (query->queries() & Qt::ImHints)
                query->setValue(Qt::ImHints, static_cast<int>(Qt::ImhNone));
            return true;
        }
    }
    return QWindow::event(e);
}

void GameWindow::exposeEvent(QExposeEvent *e) {
    QWindow::exposeEvent(e);
}

void GameWindow::keyPressEvent(QKeyEvent *e) {
    e->ignore();
}

void GameWindow::keyReleaseEvent(QKeyEvent *e) {
    e->ignore();
}

void GameWindow::update_mouse_position(QMouseEvent *e) {
    auto &touch = m_emuenv.touch;
    const float scale_x = (width() > 0) ? static_cast<float>(drawable_width()) / width() : 1.0f;
    const float scale_y = (height() > 0) ? static_cast<float>(drawable_height()) / height() : 1.0f;
    touch.mouse_x = static_cast<float>(e->position().x()) * scale_x;
    touch.mouse_y = static_cast<float>(e->position().y()) * scale_y;
}

void GameWindow::mousePressEvent(QMouseEvent *e) {
    update_mouse_position(e);
    auto &touch = m_emuenv.touch;
    if (e->button() == Qt::LeftButton)
        touch.mouse_button_left = true;
    if (e->button() == Qt::RightButton)
        touch.mouse_button_right = true;
}

void GameWindow::mouseReleaseEvent(QMouseEvent *e) {
    update_mouse_position(e);
    auto &touch = m_emuenv.touch;
    if (e->button() == Qt::LeftButton)
        touch.mouse_button_left = false;
    if (e->button() == Qt::RightButton)
        touch.mouse_button_right = false;
}

void GameWindow::mouseMoveEvent(QMouseEvent *e) {
    update_mouse_position(e);
}

void GameWindow::focusInEvent(QFocusEvent *) {
    m_emuenv.touch.renderer_focused = true;
}

void GameWindow::focusOutEvent(QFocusEvent *) {
    m_emuenv.touch.renderer_focused = false;
    m_emuenv.touch.mouse_button_left = false;
    m_emuenv.touch.mouse_button_right = false;
}

void GameWindow::toggle_fullscreen() {
    if (visibility() == QWindow::FullScreen) {
        showNormal();
    } else {
        showFullScreen();
    }
}

void GameWindow::update_window_title() {
    if (!app::update_runtime_metrics(m_emuenv, m_runtime_metrics))
        return;

    const auto &cc = m_emuenv.cfg.current_config;
    const auto af = cc.anisotropic_filtering > 1
        ? fmt::format(" | AF {}x", cc.anisotropic_filtering)
        : "";
    const auto x = m_emuenv.display.next_rendered_frame.image_size.x * cc.resolution_multiplier;
    const auto y = m_emuenv.display.next_rendered_frame.image_size.y * cc.resolution_multiplier;
    const std::string title = fmt::format("{} ({}) | {} | {} FPS ({} ms) | {}x{}{} | {}",
        m_emuenv.current_app_title, m_emuenv.io.title_id,
        m_title_backend_renderer,
        m_emuenv.fps, m_emuenv.ms_per_frame,
        x, y, af, cc.screen_filter);

    setTitle(QString::fromStdString(title));
}
