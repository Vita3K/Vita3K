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

#pragma once

#include <QBackingStore>
#include <QPixmap>
#include <QTransform>
#include <QWindow>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

class QTimer;
class QVariantAnimation;

struct EmuEnvState;

struct FrameLayout {
    QPointF pos;
    QSizeF size;
};

struct StyleLayout {
    QPointF gate_pos;
    std::map<std::string, FrameLayout> frames;
};

struct FrameData {
    std::string id;
    std::string multi;
    uint32_t autoflip = 0;
};

struct FrameItemData {
    struct TextEntry {
        QColor color;
        float size = 0.f;
        QString text;
    };

    std::vector<QPixmap> backgrounds;
    std::vector<QPixmap> images;
    QSize bg_size;
    QSize img_size;
    std::string target;
    std::vector<TextEntry> texts;

    int current_item = 0;
    uint64_t last_flip_time = 0;
};

class LiveAreaWidget : public QWindow {
    Q_OBJECT

public:
    explicit LiveAreaWidget(EmuEnvState &emuenv, const std::string &app_path, QWindow *parent = nullptr);
    ~LiveAreaWidget();

signals:
    void play_clicked();

protected:
    bool event(QEvent *e) override;
    void exposeEvent(QExposeEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;

private:
    void request_redraw();
    void render_now();
    void load_contents();
    void parse_template(const QString &xml_path, const QString &contents_dir);
    QPixmap load_image(const QString &dir, const std::string &name);
    void start_gate_animation();
    void update_autoflip();

    void paint_live_area(QPainter &p);
    void paint_manual(QPainter &p);
    void load_manual_pages();
    void enter_manual();
    void exit_manual();

    QTransform virtual_transform() const;
    QPointF to_virtual(const QPointF &widget_pos) const;
    QRectF gate_rect() const;
    QRectF start_button_rect() const;
    QRectF manual_button_rect() const;

    const StyleLayout &find_style() const;

    EmuEnvState &m_emuenv;
    std::string m_app_path;
    QString m_app_fs_path;
    QString m_contents_dir;
    std::string m_style_type;

    QPixmap m_background;
    QPixmap m_gate_image;
    QPixmap m_app_icon;
    QPixmap m_pic0;

    std::vector<FrameData> m_frames;
    std::map<std::string, FrameItemData> m_frame_items;

    bool m_manual_exists = false;

    bool m_showing_manual = false;
    std::vector<QPixmap> m_manual_pages;
    int m_manual_page = 0;

    QBackingStore m_backing_store;
    QTimer *m_autoflip_timer = nullptr;
    QVariantAnimation *m_gate_anim = nullptr;
    qreal m_gate_anim_progress = 0.0;
    bool m_gate_animating = false;

    static const std::map<std::string, StyleLayout> s_styles;
};
