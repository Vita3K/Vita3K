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

#include "ui_controls_dialog.h"

#include <gui-qt/controls_dialog.h>
#include <gui-qt/gui_settings.h>
#include <gui-qt/physical_key_qt.h>
#include <gui-qt/qt_utils.h>

#include <config/functions.h>
#include <config/state.h>
#include <ctrl/functions.h>
#include <ctrl/state.h>

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QStackedWidget>
#include <QStyle>
#include <QSvgRenderer>
#include <QToolButton>
#include <QVBoxLayout>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>

#include <algorithm>

namespace {

constexpr int BIND_BUTTON_MIN_WIDTH = 100;
constexpr int STACK_PAGE_MIN_WIDTH = 980;
constexpr int CAPTURE_TIMEOUT_MS = 5'000;
constexpr int16_t AXIS_DEADZONE = 16000;

QPixmap render_vita_svg(const QColor &color) {
    QSvgRenderer renderer(QStringLiteral(":/icons/PSV_Layout.svg"));
    QPixmap px(renderer.defaultSize() * 4);
    px.fill(Qt::transparent);
    QPainter painter(&px);
    painter.setRenderHints(QPainter::TextAntialiasing | QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    renderer.render(&painter, px.rect());
    painter.end();

    QImage img = px.toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const int alpha = qAlpha(line[x]);
            if (alpha > 0)
                line[x] = qRgba(color.red(), color.green(), color.blue(), alpha);
        }
    }

    return QPixmap::fromImage(img).scaled(550, 350, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QLabel *make_wrapped_label(const QString &text, QWidget *parent = nullptr) {
    auto *label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
    return label;
}

QToolButton *make_header_tool_button(QWidget *parent, const QString &text) {
    auto *button = new QToolButton(parent);
    button->setText(text);
    button->setCheckable(true);
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    button->setAutoRaise(true);
    button->setProperty("modeButton", true);
    return button;
}

void style_binding_button(QPushButton *button, bool capturing = false) {
    if (!button)
        return;
    button->setProperty("bindButton", true);
    button->setProperty("captureActive", capturing);
    gui::utils::refresh_theme_state(button);
}

QPushButton *make_bind_button() {
    auto *btn = new QPushButton();
    btn->setAutoDefault(false);
    btn->setFocusPolicy(Qt::ClickFocus);
    const QFontMetrics metrics(QApplication::font());
    const int text_width = metrics.horizontalAdvance(QObject::tr("Capturing...(%1)").arg(CAPTURE_TIMEOUT_MS / 1000));
    const int width = std::max(BIND_BUTTON_MIN_WIDTH, text_width + 32);
    btn->setMinimumWidth(width);
    btn->setMaximumWidth(width);
    style_binding_button(btn);
    return btn;
}

QGroupBox *make_button_group(const QString &title, QPushButton *&out_btn) {
    auto *gb = new QGroupBox(title);
    auto *layout = new QGridLayout(gb);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->addWidget(out_btn = make_bind_button(), 0, 0);
    return gb;
}

QGroupBox *make_diamond_group(const QString &title,
    QPushButton *&btn_up, QPushButton *&btn_down,
    QPushButton *&btn_left, QPushButton *&btn_right,
    const QString &label_up = QObject::tr("Up"),
    const QString &label_down = QObject::tr("Down"),
    const QString &label_left = QObject::tr("Left"),
    const QString &label_right = QObject::tr("Right")) {
    auto *gb = new QGroupBox(title);
    auto *layout = new QGridLayout(gb);
    layout->setHorizontalSpacing(6);
    layout->setVerticalSpacing(6);
    layout->addWidget(make_button_group(label_up, btn_up), 0, 1, 1, 2, Qt::AlignHCenter);
    layout->addWidget(make_button_group(label_left, btn_left), 1, 0, 1, 2);
    layout->addWidget(make_button_group(label_right, btn_right), 1, 2, 1, 2);
    layout->addWidget(make_button_group(label_down, btn_down), 2, 1, 1, 2, Qt::AlignHCenter);
    return gb;
}

QWidget *make_centered_widget(QWidget *child) {
    auto *wrapper = new QWidget();
    auto *layout = new QHBoxLayout(wrapper);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addStretch();
    layout->addWidget(child);
    layout->addStretch();
    return wrapper;
}

QScrollArea *make_scroll_page(QWidget *parent, QWidget *&content_widget, QVBoxLayout *&content_layout) {
    auto *scroll_area = new QScrollArea(parent);
    scroll_area->setWidgetResizable(true);
    scroll_area->setFrameShape(QFrame::NoFrame);

    content_widget = new QWidget(scroll_area);
    content_widget->setMinimumWidth(STACK_PAGE_MIN_WIDTH);

    content_layout = new QVBoxLayout(content_widget);
    content_layout->setContentsMargins(0, 0, 0, 0);
    content_layout->setSpacing(12);

    scroll_area->setWidget(content_widget);
    return scroll_area;
}

QLabel *make_vita_image_label(QWidget *parent, const QPixmap &pixmap) {
    auto *label = new QLabel(parent);
    label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    label->setMaximumSize(550, 350);
    label->setAlignment(Qt::AlignCenter);
    label->setPixmap(pixmap);
    return label;
}

QGroupBox *make_pstv_group(const QString &title,
    QPushButton *&btn_l2, QPushButton *&btn_l3,
    QPushButton *&btn_r3, QPushButton *&btn_r2) {
    auto *gb = new QGroupBox(title);
    auto *layout = new QGridLayout(gb);
    layout->setHorizontalSpacing(6);
    layout->setVerticalSpacing(6);
    layout->addWidget(make_button_group(QObject::tr("L2"), btn_l2), 0, 0);
    layout->addWidget(make_button_group(QObject::tr("L3"), btn_l3), 0, 1);
    layout->addWidget(make_button_group(QObject::tr("R3"), btn_r3), 0, 2);
    layout->addWidget(make_button_group(QObject::tr("R2"), btn_r2), 0, 3);
    return gb;
}

} // namespace

QString ControlsDialog::key_name(const input::PhysicalKeyCode key) {
    return QString::fromStdString(input::physical_key_display_name(key));
}

QString ControlsDialog::controller_button_name(SDL_GamepadType type, SDL_GamepadButton btn) {
    switch (btn) {
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return QStringLiteral("D-Pad Up");
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return QStringLiteral("D-Pad Down");
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return QStringLiteral("D-Pad Left");
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return QStringLiteral("D-Pad Right");
    default: break;
    }

    switch (type) {
    case SDL_GAMEPAD_TYPE_XBOX360:
    case SDL_GAMEPAD_TYPE_XBOXONE:
        switch (btn) {
        case SDL_GAMEPAD_BUTTON_BACK: return QStringLiteral("Back");
        case SDL_GAMEPAD_BUTTON_START: return QStringLiteral("Start");
        case SDL_GAMEPAD_BUTTON_LEFT_STICK: return QStringLiteral("LS");
        case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return QStringLiteral("RS");
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return QStringLiteral("LB");
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return QStringLiteral("RB");
        case SDL_GAMEPAD_BUTTON_SOUTH: return QStringLiteral("A");
        case SDL_GAMEPAD_BUTTON_EAST: return QStringLiteral("B");
        case SDL_GAMEPAD_BUTTON_WEST: return QStringLiteral("X");
        case SDL_GAMEPAD_BUTTON_NORTH: return QStringLiteral("Y");
        case SDL_GAMEPAD_BUTTON_GUIDE: return QStringLiteral("Guide");
        default: break;
        }
        break;
    case SDL_GAMEPAD_TYPE_STANDARD:
    case SDL_GAMEPAD_TYPE_PS3:
    case SDL_GAMEPAD_TYPE_PS4:
    case SDL_GAMEPAD_TYPE_PS5:
        switch (btn) {
        case SDL_GAMEPAD_BUTTON_BACK: return QStringLiteral("Select");
        case SDL_GAMEPAD_BUTTON_START: return QStringLiteral("Start");
        case SDL_GAMEPAD_BUTTON_LEFT_STICK: return QStringLiteral("L3");
        case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return QStringLiteral("R3");
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return QStringLiteral("L1");
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return QStringLiteral("R1");
        case SDL_GAMEPAD_BUTTON_SOUTH: return QStringLiteral("Cross");
        case SDL_GAMEPAD_BUTTON_EAST: return QStringLiteral("Circle");
        case SDL_GAMEPAD_BUTTON_WEST: return QStringLiteral("Square");
        case SDL_GAMEPAD_BUTTON_NORTH: return QStringLiteral("Triangle");
        case SDL_GAMEPAD_BUTTON_GUIDE: return QStringLiteral("PS");
        default: break;
        }
        break;
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
        switch (btn) {
        case SDL_GAMEPAD_BUTTON_BACK: return QStringLiteral("-");
        case SDL_GAMEPAD_BUTTON_START: return QStringLiteral("+");
        case SDL_GAMEPAD_BUTTON_LEFT_STICK: return QStringLiteral("LS");
        case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return QStringLiteral("RS");
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return QStringLiteral("L");
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return QStringLiteral("R");
        case SDL_GAMEPAD_BUTTON_SOUTH: return QStringLiteral("B");
        case SDL_GAMEPAD_BUTTON_EAST: return QStringLiteral("A");
        case SDL_GAMEPAD_BUTTON_WEST: return QStringLiteral("Y");
        case SDL_GAMEPAD_BUTTON_NORTH: return QStringLiteral("X");
        case SDL_GAMEPAD_BUTTON_GUIDE: return QStringLiteral("Home");
        default: break;
        }
        break;
    default:
        break;
    }

    const char *name = SDL_GetGamepadStringForButton(btn);
    return name ? QString::fromUtf8(name) : tr("Button %1").arg(static_cast<int>(btn));
}

QString ControlsDialog::controller_axis_name(SDL_GamepadType type, SDL_GamepadAxis axis) {
    switch (axis) {
    case SDL_GAMEPAD_AXIS_LEFTX: return QStringLiteral("Left X");
    case SDL_GAMEPAD_AXIS_LEFTY: return QStringLiteral("Left Y");
    case SDL_GAMEPAD_AXIS_RIGHTX: return QStringLiteral("Right X");
    case SDL_GAMEPAD_AXIS_RIGHTY: return QStringLiteral("Right Y");
    case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
        switch (type) {
        case SDL_GAMEPAD_TYPE_STANDARD:
        case SDL_GAMEPAD_TYPE_PS3:
        case SDL_GAMEPAD_TYPE_PS4:
        case SDL_GAMEPAD_TYPE_PS5:
            return QStringLiteral("L2");
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
            return QStringLiteral("ZL");
        default:
            return QStringLiteral("LT");
        }
    case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
        switch (type) {
        case SDL_GAMEPAD_TYPE_STANDARD:
        case SDL_GAMEPAD_TYPE_PS3:
        case SDL_GAMEPAD_TYPE_PS4:
        case SDL_GAMEPAD_TYPE_PS5:
            return QStringLiteral("R2");
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
            return QStringLiteral("ZR");
        default:
            return QStringLiteral("RT");
        }
    default:
        return tr("Axis %1").arg(static_cast<int>(axis));
    }
}

void ControlsDialog::save_config() {
    config::serialize_config(m_emuenv.cfg, m_emuenv.cfg.config_path);
}

ControlsDialog::ControlsDialog(EmuEnvState &emuenv,
    std::shared_ptr<GuiSettings> gui_settings)
    : QDialog(nullptr)
    , m_emuenv(emuenv)
    , m_gui_settings(std::move(gui_settings))
    , m_ui(std::make_unique<Ui::ControlsDialog>()) {
    m_ui->setupUi(this);
    setObjectName(QStringLiteral("controls_dialog"));
    setWindowTitle(tr("Controls"));
    setWindowModality(Qt::NonModal);
    setWindowFlag(Qt::Window, true);
    setWindowFlag(Qt::WindowSystemMenuHint, true);
    setWindowFlag(Qt::WindowMinMaxButtonsHint, true);
    setWindowFlag(Qt::WindowCloseButtonHint, true);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowIcon(QIcon(QStringLiteral(":/Vita3K.png")));
    setMinimumWidth(1250);
    m_ui->settingsCategory->setFrameShape(QFrame::NoFrame);
    m_ui->settingsCategory->setFocusPolicy(Qt::NoFocus);
    m_ui->settingsCategory->setSpacing(0);

    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_ui->settingsCategory, &QListWidget::currentRowChanged, m_ui->settingsContainer, &QStackedWidget::setCurrentIndex);
    connect(m_ui->settingsCategory, &QListWidget::currentRowChanged, this, [this](int) {
        cancel_keyboard_capture();
        cancel_controller_capture();
    });

    build_category_list();
    build_keyboard_page();
    build_hotkeys_page();
    for (int i = 0; i < MAX_CONTROLLERS; ++i)
        build_controller_page(i);

    init_keyboard_bindings();
    refresh_all_keyboard_labels();
    for (int i = 0; i < MAX_CONTROLLERS; ++i)
        set_controller_page_view(i, true);

    m_capture_countdown_timer = new QTimer(this);
    m_capture_countdown_timer->setInterval(16);
    connect(m_capture_countdown_timer, &QTimer::timeout, this, &ControlsDialog::update_capture_countdown);
    m_capture_countdown_timer->start();

    refresh_controller_tabs();
    m_ui->settingsCategory->setCurrentRow(0);

    if (m_gui_settings)
        restoreGeometry(m_gui_settings->get_value(gui::cd_geometry).toByteArray());
}

ControlsDialog::~ControlsDialog() {
    m_capture_countdown_timer->stop();
    cancel_keyboard_capture();
    cancel_controller_capture();
}

void ControlsDialog::changeEvent(QEvent *event) {
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
        refresh_controller_art();

    QDialog::changeEvent(event);
}

void ControlsDialog::closeEvent(QCloseEvent *event) {
    if (m_gui_settings) {
        m_gui_settings->set_value(gui::cd_geometry, saveGeometry(), false);
        m_gui_settings->sync();
    }

    QDialog::closeEvent(event);
}

void ControlsDialog::done(int result) {
    if (m_gui_settings) {
        m_gui_settings->set_value(gui::cd_geometry, saveGeometry(), false);
        m_gui_settings->sync();
    }

    QDialog::done(result);
}

void ControlsDialog::build_category_list() {
    m_keyboard_category_item = new QListWidgetItem(tr("Keyboard"));
    m_hotkeys_category_item = new QListWidgetItem(tr("Hotkeys"));

    m_ui->settingsCategory->addItem(m_keyboard_category_item);
    m_ui->settingsCategory->addItem(m_hotkeys_category_item);

    for (int i = 0; i < MAX_CONTROLLERS; ++i) {
        auto *item = new QListWidgetItem(tr("Controller Port %1\nNot Connected").arg(i + 1));
        m_controller_category_items[i] = item;
        m_ui->settingsCategory->addItem(item);
    }
}

QLayout *make_keyboard_bindings_layout(
    QWidget *image_parent,
    QPushButton *&btn_l1, QPushButton *&btn_r1,
    QPushButton *&btn_dpad_up, QPushButton *&btn_dpad_down,
    QPushButton *&btn_dpad_left, QPushButton *&btn_dpad_right,
    QPushButton *&btn_lstick_up, QPushButton *&btn_lstick_down,
    QPushButton *&btn_lstick_left, QPushButton *&btn_lstick_right,
    QPushButton *&btn_triangle, QPushButton *&btn_cross,
    QPushButton *&btn_square, QPushButton *&btn_circle,
    QPushButton *&btn_rstick_up, QPushButton *&btn_rstick_down,
    QPushButton *&btn_rstick_left, QPushButton *&btn_rstick_right,
    QPushButton *&btn_psbutton, QPushButton *&btn_select, QPushButton *&btn_start,
    QPushButton *&btn_l2, QPushButton *&btn_l3,
    QPushButton *&btn_r3, QPushButton *&btn_r2,
    QLabel **out_image_label, const QPixmap &vita_pixmap) {
    auto *surface = new QGridLayout();
    surface->setContentsMargins(0, 0, 0, 0);
    surface->setHorizontalSpacing(18);
    surface->setVerticalSpacing(12);

    auto *left_col = new QVBoxLayout();
    left_col->addWidget(make_diamond_group(QObject::tr("D-Pad"),
        btn_dpad_up, btn_dpad_down, btn_dpad_left, btn_dpad_right));
    left_col->addWidget(make_diamond_group(QObject::tr("Left Stick"),
        btn_lstick_up, btn_lstick_down, btn_lstick_left, btn_lstick_right));
    left_col->addStretch();
    surface->addLayout(left_col, 0, 0);

    auto *center_col = new QVBoxLayout();
    auto *top_grid = new QGridLayout();
    top_grid->setHorizontalSpacing(6);
    top_grid->setVerticalSpacing(6);
    top_grid->addWidget(make_button_group(QObject::tr("L1"), btn_l1), 0, 0);
    top_grid->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum), 0, 1, 1, 2);
    top_grid->addWidget(make_button_group(QObject::tr("R1"), btn_r1), 0, 3);
    center_col->addLayout(top_grid);

    auto *image_label = make_vita_image_label(image_parent, vita_pixmap);
    if (out_image_label)
        *out_image_label = image_label;
    center_col->addWidget(make_centered_widget(image_label));
    center_col->addStretch();

    auto *bottom_row = new QHBoxLayout();
    bottom_row->setSpacing(6);
    bottom_row->addWidget(make_button_group(QObject::tr("PS Button"), btn_psbutton));
    bottom_row->addStretch();
    bottom_row->addWidget(make_button_group(QObject::tr("Select"), btn_select));
    bottom_row->addWidget(make_button_group(QObject::tr("Start"), btn_start));
    center_col->addLayout(bottom_row);
    center_col->addWidget(make_pstv_group(QObject::tr("PS TV Mode"),
        btn_l2, btn_l3, btn_r3, btn_r2));
    center_col->addStretch();
    surface->addLayout(center_col, 0, 1);

    auto *right_col = new QVBoxLayout();
    right_col->addWidget(make_diamond_group(QObject::tr("Face Buttons"),
        btn_triangle, btn_cross, btn_square, btn_circle,
        QObject::tr("Triangle"),
        QObject::tr("Cross"),
        QObject::tr("Square"),
        QObject::tr("Circle")));
    right_col->addWidget(make_diamond_group(QObject::tr("Right Stick"),
        btn_rstick_up, btn_rstick_down, btn_rstick_left, btn_rstick_right));
    right_col->addStretch();
    surface->addLayout(right_col, 0, 2);

    return surface;
}

void ControlsDialog::build_keyboard_page() {
    m_keyboard_page.page = m_ui->page_keyboard;
    auto *root_layout = new QVBoxLayout(m_keyboard_page.page);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(12);

    auto *header_group = new QGroupBox(tr("Keyboard Mapping"), m_keyboard_page.page);
    auto *header_layout = new QHBoxLayout(header_group);
    m_keyboard_page.lbl_summary = make_wrapped_label(
        tr("Map your keyboard to Vita controls. Left-click a field to capture a key, "
           "press Escape to cancel, and right-click a keyboard binding to clear it."),
        header_group);
    header_layout->addWidget(m_keyboard_page.lbl_summary, 1);

    auto *view_layout = new QHBoxLayout();
    m_keyboard_page.btn_primary = make_header_tool_button(header_group, tr("Primary"));
    m_keyboard_page.btn_alt = make_header_tool_button(header_group, tr("Alternate"));
    view_layout->addWidget(m_keyboard_page.btn_primary);
    view_layout->addWidget(m_keyboard_page.btn_alt);
    header_layout->addLayout(view_layout);
    header_layout->addStretch();

    m_keyboard_page.btn_reset = new QPushButton(tr("Reset to Defaults"), header_group);
    m_keyboard_page.btn_reset->setAutoDefault(false);
    header_layout->addWidget(m_keyboard_page.btn_reset);
    root_layout->addWidget(header_group);

    m_keyboard_page.stack = new QStackedWidget(m_keyboard_page.page);
    root_layout->addWidget(m_keyboard_page.stack, 1);

    const QPixmap vita_pixmap = render_vita_svg(gui::utils::foreground_color(this));

    {
        QWidget *content_widget = nullptr;
        QVBoxLayout *content_layout = nullptr;
        auto *scroll = make_scroll_page(m_keyboard_page.stack, content_widget, content_layout);
        m_keyboard_page.stack->addWidget(scroll);

        content_layout->addLayout(make_keyboard_bindings_layout(
            this,
            m_keyboard_page.btn_l1, m_keyboard_page.btn_r1,
            m_keyboard_page.btn_dpad_up, m_keyboard_page.btn_dpad_down,
            m_keyboard_page.btn_dpad_left, m_keyboard_page.btn_dpad_right,
            m_keyboard_page.btn_lstick_up, m_keyboard_page.btn_lstick_down,
            m_keyboard_page.btn_lstick_left, m_keyboard_page.btn_lstick_right,
            m_keyboard_page.btn_triangle, m_keyboard_page.btn_cross,
            m_keyboard_page.btn_square, m_keyboard_page.btn_circle,
            m_keyboard_page.btn_rstick_up, m_keyboard_page.btn_rstick_down,
            m_keyboard_page.btn_rstick_left, m_keyboard_page.btn_rstick_right,
            m_keyboard_page.btn_psbutton, m_keyboard_page.btn_select, m_keyboard_page.btn_start,
            m_keyboard_page.btn_l2, m_keyboard_page.btn_l3,
            m_keyboard_page.btn_r3, m_keyboard_page.btn_r2,
            &m_keyboard_page.l_controller_image, vita_pixmap));
        content_layout->addStretch();
    }

    {
        QWidget *content_widget = nullptr;
        QVBoxLayout *content_layout = nullptr;
        auto *scroll = make_scroll_page(m_keyboard_page.stack, content_widget, content_layout);
        m_keyboard_page.stack->addWidget(scroll);

        content_layout->addLayout(make_keyboard_bindings_layout(
            this,
            m_keyboard_page.btn_l1_alt, m_keyboard_page.btn_r1_alt,
            m_keyboard_page.btn_dpad_up_alt, m_keyboard_page.btn_dpad_down_alt,
            m_keyboard_page.btn_dpad_left_alt, m_keyboard_page.btn_dpad_right_alt,
            m_keyboard_page.btn_lstick_up_alt, m_keyboard_page.btn_lstick_down_alt,
            m_keyboard_page.btn_lstick_left_alt, m_keyboard_page.btn_lstick_right_alt,
            m_keyboard_page.btn_triangle_alt, m_keyboard_page.btn_cross_alt,
            m_keyboard_page.btn_square_alt, m_keyboard_page.btn_circle_alt,
            m_keyboard_page.btn_rstick_up_alt, m_keyboard_page.btn_rstick_down_alt,
            m_keyboard_page.btn_rstick_left_alt, m_keyboard_page.btn_rstick_right_alt,
            m_keyboard_page.btn_psbutton_alt, m_keyboard_page.btn_select_alt, m_keyboard_page.btn_start_alt,
            m_keyboard_page.btn_l2_alt, m_keyboard_page.btn_l3_alt,
            m_keyboard_page.btn_r3_alt, m_keyboard_page.btn_r2_alt,
            nullptr, vita_pixmap));
        content_layout->addStretch();
    }

    connect(m_keyboard_page.btn_primary, &QToolButton::clicked,
        this, [this]() { set_keyboard_tab_view(true); });
    connect(m_keyboard_page.btn_alt, &QToolButton::clicked,
        this, [this]() { set_keyboard_tab_view(false); });
    connect(m_keyboard_page.btn_reset, &QPushButton::clicked,
        this, &ControlsDialog::reset_keyboard_defaults);

    set_keyboard_tab_view(true);
}

void ControlsDialog::set_keyboard_tab_view(bool show_primary) {
    m_keyboard_page.stack->setCurrentIndex(show_primary ? 0 : 1);
    const QSignalBlocker b1(m_keyboard_page.btn_primary);
    const QSignalBlocker b2(m_keyboard_page.btn_alt);
    m_keyboard_page.btn_primary->setChecked(show_primary);
    m_keyboard_page.btn_alt->setChecked(!show_primary);
}

void ControlsDialog::build_hotkeys_page() {
    m_hotkeys_page.page = m_ui->page_hotkeys;
    auto *root_layout = new QVBoxLayout(m_hotkeys_page.page);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(12);

    auto *header_group = new QGroupBox(tr("Hotkeys"), m_hotkeys_page.page);
    auto *header_layout = new QHBoxLayout(header_group);
    m_hotkeys_page.lbl_summary = make_wrapped_label(
        tr("Keyboard-only emulator actions."),
        header_group);
    header_layout->addWidget(m_hotkeys_page.lbl_summary, 1);
    header_layout->addStretch();

    m_hotkeys_page.btn_reset = new QPushButton(tr("Reset to Defaults"), header_group);
    m_hotkeys_page.btn_reset->setAutoDefault(false);
    header_layout->addWidget(m_hotkeys_page.btn_reset);
    root_layout->addWidget(header_group);

    QWidget *content_widget = nullptr;
    QVBoxLayout *content_layout = nullptr;
    root_layout->addWidget(make_scroll_page(m_hotkeys_page.page, content_widget, content_layout), 1);

    auto *rows_widget = new QWidget(content_widget);
    auto *rows_layout = new QGridLayout(rows_widget);
    rows_layout->setContentsMargins(0, 0, 0, 0);
    rows_layout->setHorizontalSpacing(18);
    rows_layout->setVerticalSpacing(8);
    rows_layout->setColumnStretch(0, 1);

    rows_layout->addWidget(new QLabel(tr("Primary"), rows_widget), 0, 1, Qt::AlignHCenter);
    rows_layout->addWidget(new QLabel(tr("Alternate"), rows_widget), 0, 2, Qt::AlignHCenter);

    auto add_row = [this, rows_layout](int row, const QString &name, QPushButton *&button, QPushButton *&alt_button) {
        auto *label = new QLabel(name);
        rows_layout->addWidget(label, row, 0);
        button = make_bind_button();
        rows_layout->addWidget(button, row, 1, Qt::AlignRight);
        alt_button = make_bind_button();
        rows_layout->addWidget(alt_button, row, 2, Qt::AlignRight);
    };

    add_row(1, tr("Fullscreen"), m_hotkeys_page.btn_fullscreen, m_hotkeys_page.btn_fullscreen_alt);
    add_row(2, tr("Toggle Front/Back Touch"), m_hotkeys_page.btn_toggle_touch, m_hotkeys_page.btn_toggle_touch_alt);
    add_row(3, tr("Replace Textures"), m_hotkeys_page.btn_tex_replace, m_hotkeys_page.btn_tex_replace_alt);
    add_row(4, tr("Take a Screenshot"), m_hotkeys_page.btn_screenshot, m_hotkeys_page.btn_screenshot_alt);
    add_row(5, tr("Pinch Modifier"), m_hotkeys_page.btn_pinch_mod, m_hotkeys_page.btn_pinch_mod_alt);
    add_row(6, tr("Alternate Pinch In"), m_hotkeys_page.btn_alt_pinch_in, m_hotkeys_page.btn_alt_pinch_in_alt);
    add_row(7, tr("Alternate Pinch Out"), m_hotkeys_page.btn_alt_pinch_out, m_hotkeys_page.btn_alt_pinch_out_alt);

    content_layout->addWidget(rows_widget);
    content_layout->addStretch();

    connect(m_hotkeys_page.btn_reset, &QPushButton::clicked, this, &ControlsDialog::reset_keyboard_defaults);
}

void ControlsDialog::build_controller_page(int index) {
    auto &t = m_ctrl_tabs[index];
    QWidget *pages[] = {
        m_ui->page_controller_1,
        m_ui->page_controller_2,
        m_ui->page_controller_3,
        m_ui->page_controller_4
    };
    t.page = pages[index];

    auto *root_layout = new QVBoxLayout(t.page);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(12);

    auto *header_group = new QGroupBox(tr("Controller Port %1").arg(index + 1), t.page);
    auto *header_layout = new QHBoxLayout(header_group);
    t.lbl_summary = make_wrapped_label(tr("No controller connected."), header_group);
    header_layout->addWidget(t.lbl_summary, 1);

    auto *view_layout = new QHBoxLayout();
    t.btn_bindings = make_header_tool_button(header_group, tr("Bindings"));
    t.btn_settings = make_header_tool_button(header_group, tr("Settings"));
    view_layout->addWidget(t.btn_bindings);
    view_layout->addWidget(t.btn_settings);
    header_layout->addLayout(view_layout);
    header_layout->addStretch();

    t.btn_reset = new QPushButton(tr("Reset Mapping"), header_group);
    t.btn_reset->setAutoDefault(false);
    header_layout->addWidget(t.btn_reset);
    root_layout->addWidget(header_group);

    t.stack = new QStackedWidget(t.page);
    root_layout->addWidget(t.stack, 1);

    QWidget *bindings_widget = nullptr;
    QVBoxLayout *bindings_layout = nullptr;
    t.stack->addWidget(make_scroll_page(t.stack, bindings_widget, bindings_layout));

    auto *bindings_surface = new QGridLayout();
    bindings_surface->setContentsMargins(0, 0, 0, 0);
    bindings_surface->setHorizontalSpacing(18);
    bindings_surface->setVerticalSpacing(12);

    auto *left_column = new QVBoxLayout();
    left_column->addWidget(make_diamond_group(tr("D-Pad"),
        t.btn_dpad_up, t.btn_dpad_down,
        t.btn_dpad_left, t.btn_dpad_right));
    left_column->addWidget(make_diamond_group(tr("Left Stick"),
        t.btn_lstick_up, t.btn_lstick_down,
        t.btn_lstick_left, t.btn_lstick_right));
    left_column->addStretch();
    bindings_surface->addLayout(left_column, 0, 0);

    auto *center_column = new QVBoxLayout();
    auto *top_grid = new QGridLayout();
    top_grid->setHorizontalSpacing(6);
    top_grid->setVerticalSpacing(6);
    top_grid->addWidget(make_button_group(tr("L1"), t.btn_l1), 0, 0);
    top_grid->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum), 0, 1, 1, 2);
    top_grid->addWidget(make_button_group(tr("R1"), t.btn_r1), 0, 3);
    center_column->addLayout(top_grid);

    t.l_controller_image = make_vita_image_label(this, render_vita_svg(gui::utils::foreground_color(this)));
    center_column->addWidget(make_centered_widget(t.l_controller_image));
    center_column->addStretch();

    auto *bottom_row = new QHBoxLayout();
    bottom_row->setSpacing(6);
    bottom_row->addWidget(make_button_group(tr("PS Button"), t.btn_psbutton));
    bottom_row->addStretch();
    bottom_row->addWidget(make_button_group(tr("Select"), t.btn_select));
    bottom_row->addWidget(make_button_group(tr("Start"), t.btn_start));
    center_column->addLayout(bottom_row);
    center_column->addWidget(make_pstv_group(tr("PS TV Mode"),
        t.btn_l2, t.btn_l3,
        t.btn_r3, t.btn_r2));
    center_column->addStretch();
    bindings_surface->addLayout(center_column, 0, 1);

    auto *right_column = new QVBoxLayout();
    right_column->addWidget(make_diamond_group(tr("Face Buttons"),
        t.btn_triangle, t.btn_cross,
        t.btn_square, t.btn_circle,
        tr("Triangle"),
        tr("Cross"),
        tr("Square"),
        tr("Circle")));
    right_column->addWidget(make_diamond_group(tr("Right Stick"),
        t.btn_rstick_up, t.btn_rstick_down,
        t.btn_rstick_left, t.btn_rstick_right));
    right_column->addStretch();
    bindings_surface->addLayout(right_column, 0, 2);

    bindings_layout->addLayout(bindings_surface);
    bindings_layout->addStretch();

    QWidget *settings_widget = nullptr;
    QVBoxLayout *settings_layout = nullptr;
    t.stack->addWidget(make_scroll_page(t.stack, settings_widget, settings_layout));

    auto *behavior_group = new QGroupBox(tr("Shared Controller Settings"), settings_widget);
    auto *behavior_layout = new QGridLayout(behavior_group);
    behavior_layout->addWidget(new QLabel(tr("Analog Stick Multiplier:"), behavior_group), 0, 0);
    auto *analog_layout = new QHBoxLayout();
    t.slider_analog_multiplier = new QSlider(Qt::Horizontal, behavior_group);
    t.slider_analog_multiplier->setRange(10, 200);
    t.lbl_analog_multiplier_value = new QLabel(QStringLiteral("1.0x"), behavior_group);
    t.lbl_analog_multiplier_value->setMinimumWidth(36);
    t.lbl_analog_multiplier_value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    analog_layout->addWidget(t.slider_analog_multiplier, 1);
    analog_layout->addWidget(t.lbl_analog_multiplier_value);
    behavior_layout->addLayout(analog_layout, 0, 1);
    t.chk_disable_motion = new QCheckBox(tr("Disable Motion Controls"), behavior_group);
    behavior_layout->addWidget(t.chk_disable_motion, 1, 0, 1, 2);
    settings_layout->addWidget(behavior_group);
    settings_layout->addStretch();

    const int ci = index;
    connect(t.btn_bindings, &QToolButton::clicked, this, [this, ci]() { set_controller_page_view(ci, true); });
    connect(t.btn_settings, &QToolButton::clicked, this, [this, ci]() { set_controller_page_view(ci, false); });
    connect(t.btn_reset, &QPushButton::clicked, this, [this, ci]() { reset_controller_defaults(ci); });
    connect(t.slider_analog_multiplier, &QSlider::valueChanged, this, [this](int value) {
        set_analog_multiplier(value / 100.0f);
    });
    connect(t.chk_disable_motion, &QCheckBox::toggled, this, [this](bool checked) {
        set_motion_disabled(checked);
    });

    t.bindings = {
        { t.btn_l1, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER },
        { t.btn_dpad_up, SDL_GAMEPAD_BUTTON_DPAD_UP },
        { t.btn_dpad_down, SDL_GAMEPAD_BUTTON_DPAD_DOWN },
        { t.btn_dpad_left, SDL_GAMEPAD_BUTTON_DPAD_LEFT },
        { t.btn_dpad_right, SDL_GAMEPAD_BUTTON_DPAD_RIGHT },
        { t.btn_select, SDL_GAMEPAD_BUTTON_BACK },
        { t.btn_psbutton, SDL_GAMEPAD_BUTTON_GUIDE },
        { t.btn_r1, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER },
        { t.btn_triangle, SDL_GAMEPAD_BUTTON_NORTH },
        { t.btn_cross, SDL_GAMEPAD_BUTTON_SOUTH },
        { t.btn_square, SDL_GAMEPAD_BUTTON_WEST },
        { t.btn_circle, SDL_GAMEPAD_BUTTON_EAST },
        { t.btn_start, SDL_GAMEPAD_BUTTON_START },
        { t.btn_l3, SDL_GAMEPAD_BUTTON_LEFT_STICK },
        { t.btn_r3, SDL_GAMEPAD_BUTTON_RIGHT_STICK },
    };

    for (int bi = 0; bi < static_cast<int>(t.bindings.size()); ++bi)
        connect(t.bindings[bi].button, &QPushButton::clicked, this, [this, ci, bi]() { start_controller_capture(ci, bi); });

    t.axis_bindings = {
        { t.btn_lstick_up, 1 },
        { t.btn_lstick_down, 1 },
        { t.btn_lstick_left, 0 },
        { t.btn_lstick_right, 0 },
        { t.btn_rstick_up, 3 },
        { t.btn_rstick_down, 3 },
        { t.btn_rstick_left, 2 },
        { t.btn_rstick_right, 2 },
        { t.btn_l2, 4 },
        { t.btn_r2, 5 },
    };

    for (int ai = 0; ai < static_cast<int>(t.axis_bindings.size()); ++ai)
        connect(t.axis_bindings[ai].button, &QPushButton::clicked, this, [this, ci, ai]() { start_axis_capture(ci, ai); });

    set_controller_page_enabled(index, false);
}

void ControlsDialog::refresh_shared_controller_settings() {
    const int slider_value = std::clamp(static_cast<int>(std::lround(m_emuenv.cfg.controller_analog_multiplier * 100.0f)), 10, 200);

    for (auto &tab : m_ctrl_tabs) {
        if (tab.slider_analog_multiplier && tab.lbl_analog_multiplier_value) {
            const QSignalBlocker slider_blocker(tab.slider_analog_multiplier);
            tab.slider_analog_multiplier->setValue(slider_value);
            tab.lbl_analog_multiplier_value->setText(QStringLiteral("%1x").arg(slider_value / 100.0, 0, 'f', 1));
        }
        if (tab.chk_disable_motion) {
            const QSignalBlocker check_blocker(tab.chk_disable_motion);
            tab.chk_disable_motion->setChecked(m_emuenv.cfg.disable_motion);
        }
    }
}

void ControlsDialog::set_analog_multiplier(float value) {
    m_emuenv.cfg.controller_analog_multiplier = value;
    save_config();
    refresh_shared_controller_settings();
}

void ControlsDialog::set_motion_disabled(bool disabled) {
    m_emuenv.cfg.disable_motion = disabled;
    save_config();
    refresh_shared_controller_settings();
}

void ControlsDialog::init_keyboard_bindings() {
    auto &cfg = m_emuenv.cfg;

    m_kb_bindings = {
        { m_keyboard_page.btn_dpad_up, &cfg.keyboard_button_up },
        { m_keyboard_page.btn_dpad_down, &cfg.keyboard_button_down },
        { m_keyboard_page.btn_dpad_left, &cfg.keyboard_button_left },
        { m_keyboard_page.btn_dpad_right, &cfg.keyboard_button_right },
        { m_keyboard_page.btn_cross, &cfg.keyboard_button_cross },
        { m_keyboard_page.btn_circle, &cfg.keyboard_button_circle },
        { m_keyboard_page.btn_square, &cfg.keyboard_button_square },
        { m_keyboard_page.btn_triangle, &cfg.keyboard_button_triangle },
        { m_keyboard_page.btn_l1, &cfg.keyboard_button_l1 },
        { m_keyboard_page.btn_r1, &cfg.keyboard_button_r1 },
        { m_keyboard_page.btn_select, &cfg.keyboard_button_select },
        { m_keyboard_page.btn_start, &cfg.keyboard_button_start },
        { m_keyboard_page.btn_psbutton, &cfg.keyboard_button_psbutton },
        { m_keyboard_page.btn_lstick_up, &cfg.keyboard_leftstick_up },
        { m_keyboard_page.btn_lstick_down, &cfg.keyboard_leftstick_down },
        { m_keyboard_page.btn_lstick_left, &cfg.keyboard_leftstick_left },
        { m_keyboard_page.btn_lstick_right, &cfg.keyboard_leftstick_right },
        { m_keyboard_page.btn_rstick_up, &cfg.keyboard_rightstick_up },
        { m_keyboard_page.btn_rstick_down, &cfg.keyboard_rightstick_down },
        { m_keyboard_page.btn_rstick_left, &cfg.keyboard_rightstick_left },
        { m_keyboard_page.btn_rstick_right, &cfg.keyboard_rightstick_right },
        { m_keyboard_page.btn_l2, &cfg.keyboard_button_l2 },
        { m_keyboard_page.btn_r2, &cfg.keyboard_button_r2 },
        { m_keyboard_page.btn_l3, &cfg.keyboard_button_l3 },
        { m_keyboard_page.btn_r3, &cfg.keyboard_button_r3 },
        { m_hotkeys_page.btn_fullscreen, &cfg.keyboard_gui_fullscreen },
        { m_hotkeys_page.btn_fullscreen_alt, &cfg.keyboard_gui_fullscreen_alt },
        { m_hotkeys_page.btn_toggle_touch, &cfg.keyboard_gui_toggle_touch },
        { m_hotkeys_page.btn_toggle_touch_alt, &cfg.keyboard_gui_toggle_touch_alt },
        { m_hotkeys_page.btn_tex_replace, &cfg.keyboard_toggle_texture_replacement },
        { m_hotkeys_page.btn_tex_replace_alt, &cfg.keyboard_toggle_texture_replacement_alt },
        { m_hotkeys_page.btn_screenshot, &cfg.keyboard_take_screenshot },
        { m_hotkeys_page.btn_screenshot_alt, &cfg.keyboard_take_screenshot_alt },
        { m_hotkeys_page.btn_pinch_mod, &cfg.keyboard_pinch_modifier },
        { m_hotkeys_page.btn_pinch_mod_alt, &cfg.keyboard_pinch_modifier_alt },
        { m_hotkeys_page.btn_alt_pinch_in, &cfg.keyboard_alternate_pinch_in },
        { m_hotkeys_page.btn_alt_pinch_in_alt, &cfg.keyboard_alternate_pinch_in_alt },
        { m_hotkeys_page.btn_alt_pinch_out, &cfg.keyboard_alternate_pinch_out },
        { m_hotkeys_page.btn_alt_pinch_out_alt, &cfg.keyboard_alternate_pinch_out_alt },
        { m_keyboard_page.btn_dpad_up_alt, &cfg.keyboard_button_up_alt },
        { m_keyboard_page.btn_dpad_down_alt, &cfg.keyboard_button_down_alt },
        { m_keyboard_page.btn_dpad_left_alt, &cfg.keyboard_button_left_alt },
        { m_keyboard_page.btn_dpad_right_alt, &cfg.keyboard_button_right_alt },
        { m_keyboard_page.btn_cross_alt, &cfg.keyboard_button_cross_alt },
        { m_keyboard_page.btn_circle_alt, &cfg.keyboard_button_circle_alt },
        { m_keyboard_page.btn_square_alt, &cfg.keyboard_button_square_alt },
        { m_keyboard_page.btn_triangle_alt, &cfg.keyboard_button_triangle_alt },
        { m_keyboard_page.btn_l1_alt, &cfg.keyboard_button_l1_alt },
        { m_keyboard_page.btn_r1_alt, &cfg.keyboard_button_r1_alt },
        { m_keyboard_page.btn_select_alt, &cfg.keyboard_button_select_alt },
        { m_keyboard_page.btn_start_alt, &cfg.keyboard_button_start_alt },
        { m_keyboard_page.btn_psbutton_alt, &cfg.keyboard_button_psbutton_alt },
        { m_keyboard_page.btn_lstick_up_alt, &cfg.keyboard_leftstick_up_alt },
        { m_keyboard_page.btn_lstick_down_alt, &cfg.keyboard_leftstick_down_alt },
        { m_keyboard_page.btn_lstick_left_alt, &cfg.keyboard_leftstick_left_alt },
        { m_keyboard_page.btn_lstick_right_alt, &cfg.keyboard_leftstick_right_alt },
        { m_keyboard_page.btn_rstick_up_alt, &cfg.keyboard_rightstick_up_alt },
        { m_keyboard_page.btn_rstick_down_alt, &cfg.keyboard_rightstick_down_alt },
        { m_keyboard_page.btn_rstick_left_alt, &cfg.keyboard_rightstick_left_alt },
        { m_keyboard_page.btn_rstick_right_alt, &cfg.keyboard_rightstick_right_alt },
        { m_keyboard_page.btn_l2_alt, &cfg.keyboard_button_l2_alt },
        { m_keyboard_page.btn_r2_alt, &cfg.keyboard_button_r2_alt },
        { m_keyboard_page.btn_l3_alt, &cfg.keyboard_button_l3_alt },
        { m_keyboard_page.btn_r3_alt, &cfg.keyboard_button_r3_alt },

    };

    for (int i = 0; i < static_cast<int>(m_kb_bindings.size()); ++i) {
        auto *btn = m_kb_bindings[i].button;
        btn->installEventFilter(this);
        connect(btn, &QPushButton::clicked, this, [this, i]() { start_keyboard_capture(i); });
    }
}

void ControlsDialog::refresh_all_keyboard_labels() {
    for (auto &binding : m_kb_bindings)
        binding.button->setText(key_name(*binding.config_value));
}

void ControlsDialog::start_keyboard_capture(int index) {
    cancel_keyboard_capture();
    cancel_controller_capture();
    m_kb_capturing_index = index;
    auto *btn = m_kb_bindings[index].button;
    begin_capture_countdown();
    btn->setText(tr("Capturing...(%1)").arg(capture_seconds_remaining()));
    style_binding_button(btn, true);
    grabKeyboard();
}

void ControlsDialog::cancel_keyboard_capture() {
    if (m_kb_capturing_index < 0)
        return;

    releaseKeyboard();
    auto &binding = m_kb_bindings[m_kb_capturing_index];
    style_binding_button(binding.button, false);
    binding.button->setText(key_name(*binding.config_value));
    m_kb_capturing_index = -1;
    stop_capture_countdown_if_idle();
}

void ControlsDialog::keyPressEvent(QKeyEvent *event) {
    if (m_kb_capturing_index < 0) {
        if (!event->isAutoRepeat() && m_ctrl_capturing_tab >= 0 && event->key() == Qt::Key_Escape) {
            cancel_controller_capture();
            event->accept();
            return;
        }
        QDialog::keyPressEvent(event);
        return;
    }
    if (event->isAutoRepeat())
        return;

    if (event->key() == Qt::Key_Escape) {
        cancel_keyboard_capture();
        return;
    }
    const auto key = physical_key_from_qt_event(*event);
    if (key == input::PhysicalKeyCode::Unbound) {
        QMessageBox::warning(this, tr("Unsupported Key"),
            tr("This key cannot be mapped on the current platform."));
        return;
    }
    if (has_duplicate_key(key, m_kb_capturing_index)) {
        QMessageBox::warning(this, tr("Duplicate Key"),
            tr("The key \"%1\" is already assigned to another action.").arg(key_name(key)));
        return;
    }

    auto &binding = m_kb_bindings[m_kb_capturing_index];
    *binding.config_value = key;
    save_config();
    cancel_keyboard_capture();
    refresh_all_keyboard_labels();
}

bool ControlsDialog::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::MouseButtonRelease) {
        auto *mouse_event = static_cast<QMouseEvent *>(event);
        if (mouse_event->button() == Qt::RightButton && m_kb_capturing_index < 0) {
            for (int i = 0; i < static_cast<int>(m_kb_bindings.size()); ++i) {
                if (m_kb_bindings[i].button == watched) {
                    *m_kb_bindings[i].config_value = input::PhysicalKeyCode::Unbound;
                    m_kb_bindings[i].button->setText(key_name(input::PhysicalKeyCode::Unbound));
                    save_config();
                    return true;
                }
            }
        }
    }

    return QDialog::eventFilter(watched, event);
}

bool ControlsDialog::has_duplicate_key(const input::PhysicalKeyCode key, const int exclude_index) const {
    if (key == input::PhysicalKeyCode::Unbound)
        return false;

    for (int i = 0; i < static_cast<int>(m_kb_bindings.size()); ++i) {
        if (i != exclude_index && *m_kb_bindings[i].config_value == key)
            return true;
    }

    return false;
}

void ControlsDialog::begin_capture_countdown() {
    m_capture_timer.restart();
    m_last_capture_seconds_remaining = -1;
    refresh_capture_countdown();
}

int ControlsDialog::capture_seconds_remaining() const {
    if (!m_capture_timer.isValid())
        return 0;

    const int remaining_ms = std::max(0, CAPTURE_TIMEOUT_MS - static_cast<int>(m_capture_timer.elapsed()));
    return (remaining_ms + 999) / 1000;
}

void ControlsDialog::refresh_capture_countdown() {
    if (m_kb_capturing_index < 0 && m_ctrl_capturing_tab < 0)
        return;

    if (!m_capture_timer.isValid())
        begin_capture_countdown();

    if (m_capture_timer.elapsed() >= CAPTURE_TIMEOUT_MS) {
        cancel_keyboard_capture();
        cancel_controller_capture();
        return;
    }

    const int seconds_remaining = capture_seconds_remaining();
    if (seconds_remaining == m_last_capture_seconds_remaining)
        return;

    m_last_capture_seconds_remaining = seconds_remaining;
    if (m_kb_capturing_index >= 0) {
        m_kb_bindings[m_kb_capturing_index].button->setText(tr("Capturing...(%1)").arg(seconds_remaining));
    } else if (m_ctrl_capturing_tab >= 0 && m_ctrl_capturing_index >= 0) {
        auto &tab = m_ctrl_tabs[m_ctrl_capturing_tab];
        auto *button = m_ctrl_capturing_axis
            ? tab.axis_bindings[m_ctrl_capturing_index].button
            : tab.bindings[m_ctrl_capturing_index].button;
        button->setText(tr("Capturing...(%1)").arg(seconds_remaining));
    }
}

void ControlsDialog::stop_capture_countdown_if_idle() {
    if (m_kb_capturing_index >= 0 || m_ctrl_capturing_tab >= 0)
        return;

    m_capture_timer.invalidate();
    m_last_capture_seconds_remaining = -1;
}

void ControlsDialog::reset_keyboard_defaults() {
    config::reset_keyboard_bindings(m_emuenv.cfg);
    save_config();
    refresh_all_keyboard_labels();
}

void ControlsDialog::sync_controller_state() {
    refresh_controller_tabs();

    if (m_ctrl_capturing_tab >= 0 && (m_ctrl_capturing_tab >= MAX_CONTROLLERS || !m_ctrl_tabs[m_ctrl_capturing_tab].connected))
        cancel_controller_capture();
}

void ControlsDialog::refresh_controller_tabs() {
    auto &ctrl = m_emuenv.ctrl;
    for (int i = 0; i < MAX_CONTROLLERS; ++i) {
        auto &tab = m_ctrl_tabs[i];
        tab.connected = false;
        tab.port = -1;
        tab.guid = {};
        tab.type = SDL_GAMEPAD_TYPE_STANDARD;
    }

    for (const auto &[guid, controller] : ctrl.controllers) {
        const int port = controller.port;
        if (port < 0 || port >= MAX_CONTROLLERS)
            continue;

        auto &tab = m_ctrl_tabs[port];
        tab.connected = true;
        tab.port = port;
        tab.guid = guid;
        tab.type = SDL_GetGamepadType(controller.controller.get());
        const QString device_name = QString::fromUtf8(controller.name);
        tab.lbl_summary->setText(tr("Connected to %1. Use Bindings to remap controls or Settings for shared controller options.").arg(device_name));
        m_controller_category_items[port]->setText(tr("Controller Port %1\n%2").arg(port + 1).arg(device_name));
    }

    for (int i = 0; i < MAX_CONTROLLERS; ++i) {
        auto &tab = m_ctrl_tabs[i];
        if (!tab.connected) {
            tab.lbl_summary->setText(tr("No controller connected."));
            m_controller_category_items[i]->setText(tr("Controller Port %1\nNot Connected").arg(i + 1));
        }
        set_controller_page_enabled(i, tab.connected);
        refresh_controller_page_labels(i);
    }

    refresh_shared_controller_settings();
}

void ControlsDialog::refresh_controller_page_labels(int index) {
    auto &tab = m_ctrl_tabs[index];
    auto &binds = m_emuenv.cfg.controller_binds;
    for (int binding_index = 0; binding_index < static_cast<int>(tab.bindings.size()); ++binding_index) {
        auto &binding = tab.bindings[binding_index];
        if (index == m_ctrl_capturing_tab && !m_ctrl_capturing_axis && binding_index == m_ctrl_capturing_index)
            continue;
        if (binding.vita_button == SDL_GAMEPAD_BUTTON_INVALID) {
            binding.button->setText(QStringLiteral("-"));
            continue;
        }

        const int bind_index = static_cast<int>(binding.vita_button);
        SDL_GamepadButton mapped = binding.vita_button;
        if (bind_index >= 0 && bind_index < static_cast<int>(binds.size()))
            mapped = static_cast<SDL_GamepadButton>(binds[bind_index]);
        binding.button->setText(controller_button_name(tab.type, mapped));
    }

    auto &axis_binds = m_emuenv.cfg.controller_axis_binds;
    for (int axis_binding_index = 0; axis_binding_index < static_cast<int>(tab.axis_bindings.size()); ++axis_binding_index) {
        auto &axis_binding = tab.axis_bindings[axis_binding_index];
        if (index == m_ctrl_capturing_tab && m_ctrl_capturing_axis && axis_binding_index == m_ctrl_capturing_index)
            continue;
        const int axis_index = axis_binding.axis_index;
        SDL_GamepadAxis mapped = static_cast<SDL_GamepadAxis>(axis_index);
        if (axis_index >= 0 && axis_index < static_cast<int>(axis_binds.size()))
            mapped = static_cast<SDL_GamepadAxis>(axis_binds[axis_index]);
        axis_binding.button->setText(controller_axis_name(tab.type, mapped));
    }
}

void ControlsDialog::refresh_controller_art() {
    const QPixmap vita_pixmap = render_vita_svg(gui::utils::foreground_color(this));

    if (m_keyboard_page.l_controller_image)
        m_keyboard_page.l_controller_image->setPixmap(vita_pixmap);

    for (auto &tab : m_ctrl_tabs) {
        if (tab.l_controller_image)
            tab.l_controller_image->setPixmap(vita_pixmap);
    }
}

void ControlsDialog::set_controller_page_enabled(int index, bool enabled) {
    auto &tab = m_ctrl_tabs[index];
    for (auto &binding : tab.bindings)
        binding.button->setEnabled(enabled);
    for (auto &axis_binding : tab.axis_bindings)
        axis_binding.button->setEnabled(enabled);
    tab.slider_analog_multiplier->setEnabled(enabled);
    tab.chk_disable_motion->setEnabled(enabled);
    tab.btn_reset->setEnabled(enabled);
}

void ControlsDialog::set_controller_page_view(int index, bool show_bindings) {
    auto &tab = m_ctrl_tabs[index];
    tab.stack->setCurrentIndex(show_bindings ? 0 : 1);
    const QSignalBlocker bindings_blocker(tab.btn_bindings);
    const QSignalBlocker settings_blocker(tab.btn_settings);
    tab.btn_bindings->setChecked(show_bindings);
    tab.btn_settings->setChecked(!show_bindings);
}

void ControlsDialog::start_controller_capture(int tab_index, int binding_index) {
    cancel_keyboard_capture();
    cancel_controller_capture();
    m_ctrl_capturing_tab = tab_index;
    m_ctrl_capturing_index = binding_index;
    m_ctrl_capturing_axis = false;

    auto *btn = m_ctrl_tabs[tab_index].bindings[binding_index].button;
    begin_capture_countdown();
    btn->setText(tr("Capturing...(%1)").arg(capture_seconds_remaining()));
    style_binding_button(btn, true);
}

void ControlsDialog::start_axis_capture(int tab_index, int axis_binding_index) {
    cancel_keyboard_capture();
    cancel_controller_capture();
    m_ctrl_capturing_tab = tab_index;
    m_ctrl_capturing_index = axis_binding_index;
    m_ctrl_capturing_axis = true;

    auto *btn = m_ctrl_tabs[tab_index].axis_bindings[axis_binding_index].button;
    begin_capture_countdown();
    btn->setText(tr("Capturing...(%1)").arg(capture_seconds_remaining()));
    style_binding_button(btn, true);
}

void ControlsDialog::cancel_controller_capture() {
    if (m_ctrl_capturing_tab < 0 || m_ctrl_capturing_index < 0)
        return;

    auto &tab = m_ctrl_tabs[m_ctrl_capturing_tab];
    if (m_ctrl_capturing_axis)
        style_binding_button(tab.axis_bindings[m_ctrl_capturing_index].button, false);
    else
        style_binding_button(tab.bindings[m_ctrl_capturing_index].button, false);

    m_ctrl_capturing_tab = -1;
    m_ctrl_capturing_index = -1;
    m_ctrl_capturing_axis = false;
    stop_capture_countdown_if_idle();

    for (int i = 0; i < MAX_CONTROLLERS; ++i)
        refresh_controller_page_labels(i);
}

bool ControlsDialog::handle_sdl_event(const SDL_Event &event) {
    if (m_ctrl_capturing_tab < 0 || m_ctrl_capturing_index < 0)
        return false;

    const auto &tab = m_ctrl_tabs[m_ctrl_capturing_tab];
    SDL_JoystickID active_gamepad_id = 0;
    {
        const std::lock_guard<std::mutex> lock(m_emuenv.ctrl.mutex);
        const auto it = m_emuenv.ctrl.controllers.find(tab.guid);
        if (it == m_emuenv.ctrl.controllers.end() || !it->second.controller || !SDL_GamepadConnected(it->second.controller.get())) {
            cancel_controller_capture();
            return false;
        }
        active_gamepad_id = SDL_GetGamepadID(it->second.controller.get());
    }

    if (m_ctrl_capturing_axis) {
        if (event.type != SDL_EVENT_GAMEPAD_AXIS_MOTION || event.gaxis.which != active_gamepad_id)
            return false;
        if (std::abs(event.gaxis.value) < AXIS_DEADZONE)
            return true;

        auto &axis_binding = m_ctrl_tabs[m_ctrl_capturing_tab].axis_bindings[m_ctrl_capturing_index];
        auto &axis_binds = m_emuenv.cfg.controller_axis_binds;
        if (axis_binds.size() <= static_cast<size_t>(axis_binding.axis_index))
            axis_binds.resize(axis_binding.axis_index + 1, 0);
        axis_binds[axis_binding.axis_index] = static_cast<short>(event.gaxis.axis);
        save_config();
        cancel_controller_capture();
        return true;
    }

    if (event.type != SDL_EVENT_GAMEPAD_BUTTON_DOWN || event.gbutton.which != active_gamepad_id)
        return false;

    auto &binding = m_ctrl_tabs[m_ctrl_capturing_tab].bindings[m_ctrl_capturing_index];
    if (binding.vita_button == SDL_GAMEPAD_BUTTON_INVALID)
        return false;

    const int bind_index = static_cast<int>(binding.vita_button);
    auto &binds = m_emuenv.cfg.controller_binds;
    if (binds.size() <= static_cast<size_t>(bind_index))
        binds.resize(bind_index + 1, 0);
    binds[bind_index] = static_cast<short>(event.gbutton.button);
    save_config();
    cancel_controller_capture();
    return true;
}

void ControlsDialog::update_capture_countdown() {
    refresh_capture_countdown();
}

void ControlsDialog::reset_controller_defaults(int /*index*/) {
    m_emuenv.cfg.controller_binds = {
        SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH,
        SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_GUIDE, SDL_GAMEPAD_BUTTON_START,
        SDL_GAMEPAD_BUTTON_LEFT_STICK, SDL_GAMEPAD_BUTTON_RIGHT_STICK,
        SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
        SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT
    };
    m_emuenv.cfg.controller_axis_binds = {
        SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY,
        SDL_GAMEPAD_AXIS_RIGHTX, SDL_GAMEPAD_AXIS_RIGHTY,
        SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER
    };
    save_config();
    for (int i = 0; i < MAX_CONTROLLERS; ++i)
        refresh_controller_page_labels(i);
}
