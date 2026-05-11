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

#include <gui-qt/live_area_widget.h>

#include <config/state.h>
#include <emuenv/state.h>

#include <pugixml.hpp>

#include <QDesktopServices>
#include <QDir>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>
#include <QTimer>
#include <QUrl>
#include <QVariantAnimation>

#include <algorithm>
#include <filesystem>

static constexpr qreal REF_W = 960.0;
static constexpr qreal REF_H = 544.0;

// clang-format off
const std::map<std::string, StyleLayout> LiveAreaWidget::s_styles = {
    { "a1", {
        { 620.0, 361.0 }, {
            { "frame1", { { 900.0, 414.0 }, { 260.0, 260.0 } } },
            { "frame2", { { 320.0, 414.0 }, { 260.0, 260.0 } } },
            { "frame3", { { 900.0, 154.0 }, { 840.0, 150.0 } } }
        }
    } },
    { "a2", {
        { 620.0, 395.0 }, {
            { "frame1", { { 900.0, 404.0 }, { 260.0, 400.0 } } },
            { "frame2", { { 320.0, 404.0 }, { 260.0, 400.0 } } },
            { "frame3", { { 640.0, 204.0 }, { 320.0, 200.0 } } }
        }
    } },
    { "a3", {
        { 620.0, 395.0 }, {
            { "frame1", { { 900.0, 414.0 }, { 260.0, 200.0 } } },
            { "frame2", { { 320.0, 414.0 }, { 260.0, 200.0 } } },
            { "frame3", { { 900.0, 214.0 }, { 260.0, 210.0 } } },
            { "frame4", { { 640.0, 214.0 }, { 320.0, 210.0 } } },
            { "frame5", { { 320.0, 214.0 }, { 260.0, 210.0 } } }
        }
    } },
    { "a4", {
        { 620.0, 395.0 }, {
            { "frame1", { { 900.0, 414.0 }, { 260.0, 200.0 } } },
            { "frame2", { { 320.0, 414.0 }, { 260.0, 200.0 } } },
            { "frame3", { { 900.0, 214.0 }, { 840.0, 70.0 } } },
            { "frame4", { { 900.0, 144.0 }, { 840.0, 70.0 } } },
            { "frame5", { { 900.0, 74.0 },  { 840.0, 70.0 } } }
        }
    } },
    { "a5", {
        { 380.0, 395.0 }, {
            { "frame1", { { 900.0, 412.0 }, { 480.0, 68.0 } } },
            { "frame2", { { 900.0, 344.0 }, { 480.0, 68.0 } } },
            { "frame3", { { 900.0, 276.0 }, { 480.0, 68.0 } } },
            { "frame4", { { 900.0, 208.0 }, { 480.0, 68.0 } } },
            { "frame5", { { 900.0, 140.0 }, { 480.0, 68.0 } } },
            { "frame6", { { 900.0, 72.0 },  { 480.0, 68.0 } } },
            { "frame7", { { 420.0, 214.0 }, { 360.0, 210.0 } } }
        }
    } },
    { "psmobile", {
        { 380.0, 345.0 }, {
            { "frame1", { { 866.0, 414.0 }, { 446.0, 154.0 } } },
            { "frame2", { { 866.0, 249.5 }, { 446.0, 109.0 } } },
            { "frame3", { { 866.0, 119.0 }, { 196.0, 58.0 } } },
            { "frame4", { { 866.0, 34.0 },  { 772.0, 30.0 } } }
        }
    } },
};
// clang-format on

static std::vector<FrameItemData::TextEntry> parse_text_entries(const pugi::xml_node &text_node) {
    std::vector<FrameItemData::TextEntry> entries;
    for (auto &str_child : text_node) {
        if (str_child.name() != std::string_view("str"))
            continue;
        FrameItemData::TextEntry te;
        std::string color_str = str_child.attribute("color").as_string();
        if (!color_str.empty()) {
            if (color_str[0] == '#')
                te.color = QColor(QString::fromStdString(color_str));
            else
                te.color = QColor("#" + QString::fromStdString(color_str));
        }
        te.size = str_child.attribute("size").as_float(0.f);
        te.text = QString::fromStdString(str_child.text().as_string());
        entries.push_back(te);
    }
    return entries;
}

static uint64_t current_time_secs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch())
                                     .count());
}

static std::string strip_whitespace(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c != '\n' && c != '\r' && c != ' ' && c != '\t')
            out.push_back(c);
    }
    return out;
}

LiveAreaWidget::LiveAreaWidget(EmuEnvState &emuenv, const std::string &app_path, QWindow *parent)
    : QWindow(parent)
    , m_emuenv(emuenv)
    , m_app_path(app_path)
    , m_backing_store(this) {
    setSurfaceType(QSurface::RasterSurface);
    setMinimumSize(QSize(160, 90));
    resize(960, 544);
    setFlags(Qt::Window);

    const auto pref = QString::fromStdString(emuenv.pref_path.string());
    m_app_fs_path = pref + "/ux0/app/" + QString::fromStdString(app_path);

    load_contents();

    QDir manual_dir(m_app_fs_path + "/sce_sys/manual");
    if (manual_dir.exists() && !manual_dir.isEmpty())
        m_manual_exists = true;

    m_autoflip_timer = new QTimer(this);
    m_autoflip_timer->setInterval(500);
    connect(m_autoflip_timer, &QTimer::timeout, this, &LiveAreaWidget::update_autoflip);
    m_autoflip_timer->start();

    m_gate_anim = new QVariantAnimation(this);
    m_gate_anim->setStartValue(0.0);
    m_gate_anim->setEndValue(1.0);
    m_gate_anim->setDuration(600);
    m_gate_anim->setEasingCurve(QEasingCurve::OutCubic);

    connect(m_gate_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &val) {
        m_gate_anim_progress = val.toReal();
        request_redraw();
    });

    connect(
        m_gate_anim, &QVariantAnimation::finished, this, [this]() {
        m_gate_animating = false;
        m_gate_anim_progress = 0.0;
        request_redraw();
        emit play_clicked(); }, Qt::QueuedConnection);
}

LiveAreaWidget::~LiveAreaWidget() = default;

void LiveAreaWidget::request_redraw() {
    requestUpdate();
}

bool LiveAreaWidget::event(QEvent *e) {
    if (e->type() == QEvent::UpdateRequest) {
        render_now();
        return true;
    }

    return QWindow::event(e);
}

void LiveAreaWidget::exposeEvent(QExposeEvent *) {
    if (isExposed())
        render_now();
}

void LiveAreaWidget::render_now() {
    if (!isExposed())
        return;

    const QSize backing_size = size() * devicePixelRatio();
    if (backing_size.isEmpty())
        return;

    if (m_backing_store.size() != backing_size)
        m_backing_store.resize(backing_size);

    const QRegion region(QRect(QPoint(0, 0), backing_size));
    m_backing_store.beginPaint(region);

    if (QPaintDevice *device = m_backing_store.paintDevice()) {
        QPainter p(device);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.fillRect(QRect(QPoint(0, 0), size()), Qt::black);
        p.setTransform(virtual_transform());

        if (m_showing_manual)
            paint_manual(p);
        else
            paint_live_area(p);
    }

    m_backing_store.endPaint();
    m_backing_store.flush(region, this);
}

void LiveAreaWidget::load_contents() {
    QString contents_dir = m_app_fs_path + "/sce_sys/livearea/contents";

    QDir retail_dir(m_app_fs_path + "/sce_sys/retail/livearea");
    if (retail_dir.exists())
        contents_dir = m_app_fs_path + "/sce_sys/retail/livearea/contents";

    QString template_path = contents_dir + "/template.xml";

    if (!QFile::exists(template_path)) {
        const auto pref = QString::fromStdString(m_emuenv.pref_path.string());
        contents_dir = pref + "/vs0/data/internal/livearea/default/sce_sys/livearea/contents";
        template_path = contents_dir + "/template.xml";
    }

    if (QFile::exists(template_path)) {
        parse_template(template_path, contents_dir);
    } else {
        m_style_type = "a1";
    }

    const QString icon_path = m_app_fs_path + "/sce_sys/icon0.png";
    if (QFile::exists(icon_path))
        m_app_icon.load(icon_path);

    const QString pic0_path = m_app_fs_path + "/sce_sys/pic0.png";
    if (QFile::exists(pic0_path))
        m_pic0.load(pic0_path);
}

QPixmap LiveAreaWidget::load_image(const QString &dir, const std::string &name) {
    if (name.empty())
        return {};
    QString path = dir + "/" + QString::fromStdString(name);
    QPixmap pix;
    pix.load(path);
    return pix;
}

void LiveAreaWidget::parse_template(const QString &xml_path, const QString &contents_dir) {
    pugi::xml_document doc;
    if (!doc.load_file(xml_path.toStdString().c_str()))
        return;

    m_contents_dir = contents_dir;

    auto livearea = doc.child("livearea");
    m_style_type = livearea.attribute("style").as_string("a1");

    if (!s_styles.contains(m_style_type))
        m_style_type = "a1";

    std::string bg_name;
    auto bg_node = livearea.child("livearea-background").child("image");
    if (bg_node) {
        if (!bg_node.child("lang").text().empty()) {
            for (auto &child : livearea.child("livearea-background")) {
                if (child.attribute("default").as_string() == std::string("on")) {
                    bg_name = child.text().as_string();
                    break;
                }
            }
            if (bg_name.empty())
                bg_name = bg_node.text().as_string();
        } else {
            bg_name = bg_node.text().as_string();
        }
    }
    bg_name = strip_whitespace(bg_name);
    m_background = load_image(contents_dir, bg_name);

    std::string gate_name;
    for (auto &gate : livearea.child("gate")) {
        if (!gate.child("lang").empty()) {
            if (gate.attribute("default").as_string() == std::string("on")) {
                gate_name = gate.text().as_string();
                break;
            }
        } else if (!gate.child("cntry").empty()) {
            gate_name = gate.text().as_string();
        } else {
            gate_name = gate.text().as_string();
        }
    }
    gate_name = strip_whitespace(gate_name);
    m_gate_image = load_image(contents_dir, gate_name);

    m_frames.clear();
    m_frame_items.clear();

    for (auto &node : livearea) {
        if (node.attribute("id").empty())
            continue;

        FrameData fd;
        fd.id = node.attribute("id").as_string();
        fd.multi = node.attribute("multi").as_string();
        fd.autoflip = node.attribute("autoflip").as_uint();
        m_frames.push_back(fd);

        FrameItemData &item = m_frame_items[fd.id];

        auto liveitem = node.child("liveitem");
        if (!liveitem)
            continue;

        std::string target_str = liveitem.child("target").text().as_string();
        target_str = strip_whitespace(target_str);
        item.target = target_str;

        std::vector<std::string> bg_names, img_names;
        for (auto &li : node) {
            if (li.name() != std::string_view("liveitem"))
                continue;
            std::string bg = strip_whitespace(li.child("background").text().as_string());
            std::string img = strip_whitespace(li.child("image").text().as_string());
            if (!bg.empty())
                bg_names.push_back(bg);
            if (!img.empty())
                img_names.push_back(img);

            auto text_node = li.child("text");
            item.texts = parse_text_entries(text_node);
        }

        if (bg_names.empty()) {
            std::string bg = strip_whitespace(liveitem.child("background").text().as_string());
            if (!bg.empty())
                bg_names.push_back(bg);
        }
        if (img_names.empty()) {
            std::string img = strip_whitespace(liveitem.child("image").text().as_string());
            if (!img.empty())
                img_names.push_back(img);
        }

        for (auto &n : bg_names) {
            QPixmap pix = load_image(contents_dir, n);
            if (!pix.isNull()) {
                if (item.bg_size.isEmpty())
                    item.bg_size = pix.size();
                item.backgrounds.push_back(std::move(pix));
            }
        }
        for (auto &n : img_names) {
            QPixmap pix = load_image(contents_dir, n);
            if (!pix.isNull()) {
                if (item.img_size.isEmpty())
                    item.img_size = pix.size();
                item.images.push_back(std::move(pix));
            }
        }

        if (item.texts.empty()) {
            auto text_node = liveitem.child("text");
            item.texts = parse_text_entries(text_node);
        }
    }
}

QTransform LiveAreaWidget::virtual_transform() const {
    const qreal s = std::min(width() / REF_W, height() / REF_H);
    const qreal ox = (width() - REF_W * s) / 2.0;
    const qreal oy = (height() - REF_H * s) / 2.0;
    QTransform t;
    t.translate(ox, oy);
    t.scale(s, s);
    return t;
}

QPointF LiveAreaWidget::to_virtual(const QPointF &widget_pos) const {
    return virtual_transform().inverted().map(widget_pos);
}

const StyleLayout &LiveAreaWidget::find_style() const {
    auto it = s_styles.find(m_style_type);
    if (it == s_styles.end())
        it = s_styles.find("a1");
    return it->second;
}

QRectF LiveAreaWidget::gate_rect() const {
    const auto &style = find_style();
    return { REF_W - style.gate_pos.x(), REF_H - style.gate_pos.y(), 280.0, 158.0 };
}

QRectF LiveAreaWidget::start_button_rect() const {
    const auto gr = gate_rect();
    return { gr.x() + (gr.width() - 120.0) / 2.0,
        gr.y() + gr.height() - 32.0 - 8.0,
        120.0, 32.0 };
}

QRectF LiveAreaWidget::manual_button_rect() const {
    if (!m_manual_exists)
        return {};
    const auto &style = find_style();
    const qreal gx = REF_W - style.gate_pos.x();
    const qreal x = gx + (280.0 - 70.0) / 2.0;
    const qreal y = REF_H - 505.0;
    return { x, y, 70.0, 70.0 };
}

void LiveAreaWidget::paint_live_area(QPainter &p) {
    const auto &style = find_style();

    const QRectF bg_rect(REF_W - 900.0, REF_H - 500.0, 840.0, 500.0);
    if (!m_background.isNull())
        p.drawPixmap(bg_rect, m_background, QRectF(m_background.rect()));
    else
        p.fillRect(bg_rect, QColor(148, 164, 173));

    for (const auto &frame : m_frames) {
        auto style_it = style.frames.find(frame.id);
        if (style_it == style.frames.end())
            continue;

        const auto &fl = style_it->second;
        const qreal fx = REF_W - fl.pos.x();
        const qreal fy = REF_H - fl.pos.y();
        const qreal fw = fl.size.width();
        const qreal fh = fl.size.height();
        const QRectF frame_rect(fx, fy, fw, fh);

        auto item_it = m_frame_items.find(frame.id);
        if (item_it == m_frame_items.end())
            continue;

        const auto &item = item_it->second;
        const auto idx = static_cast<size_t>(item.current_item);

        if (!item.backgrounds.empty()) {
            size_t bg_idx = std::min(idx, item.backgrounds.size() - 1);
            const auto &pix = item.backgrounds[bg_idx];
            p.drawPixmap(frame_rect, pix, QRectF(pix.rect()));
        }

        if (!item.images.empty()) {
            size_t img_idx = std::min(idx, item.images.size() - 1);
            const auto &pix = item.images[img_idx];
            qreal iw = pix.width();
            qreal ih = pix.height();
            if (iw > fw || ih > fh) {
                qreal ratio = std::min(fw / iw, fh / ih);
                iw *= ratio;
                ih *= ratio;
            }
            qreal ix = fx + (fw - iw) / 2.0;
            qreal iy = fy + (fh - ih) / 2.0;
            p.drawPixmap(QRectF(ix, iy, iw, ih), pix, QRectF(pix.rect()));
        }

        if (!item.texts.empty()) {
            size_t text_idx = (frame.autoflip > 0) ? std::min(idx, item.texts.size() - 1) : 0;
            const auto &te = item.texts[text_idx];
            p.setPen(te.color);
            qreal font_size = te.size > 0 ? te.size / 2.0 : 6.0;
            QFont f = p.font();
            f.setPixelSize(std::max(static_cast<int>(font_size), 8));
            p.setFont(f);
            p.drawText(frame_rect, Qt::AlignCenter | Qt::TextWordWrap, te.text);
        }
    }

    QRectF gr = gate_rect();
    if (m_gate_animating) {
        qreal t = m_gate_anim_progress;

        if (t > 0.0 && !m_pic0.isNull()) {
            p.save();

            qreal darken = std::min(t / 0.3, 1.0);
            p.fillRect(QRectF(0, 0, REF_W, REF_H), QColor(0, 0, 0, static_cast<int>(180 * darken)));

            QTransform inv = virtual_transform().inverted();
            QRectF widget_in_virtual = inv.mapRect(QRectF(0, 0, width(), height()));

            qreal zoom_t = (t < 0.3) ? 0.0 : (t - 0.3) / 0.7;
            qreal eased = 1.0 - (1.0 - zoom_t) * (1.0 - zoom_t);

            QRectF pic_rect(
                gr.x() + (widget_in_virtual.x() - gr.x()) * eased,
                gr.y() + (widget_in_virtual.y() - gr.y()) * eased,
                gr.width() + (widget_in_virtual.width() - gr.width()) * eased,
                gr.height() + (widget_in_virtual.height() - gr.height()) * eased);

            qreal pic_alpha = std::min(t / 0.15, 1.0);
            p.setOpacity(pic_alpha);
            p.drawPixmap(pic_rect, m_pic0, QRectF(m_pic0.rect()));
            p.setOpacity(1.0);

            p.restore();
        } else if (t > 0.0) {
            qreal darken = std::min(t / 0.3, 1.0);
            p.fillRect(QRectF(0, 0, REF_W, REF_H), QColor(0, 0, 0, static_cast<int>(220 * darken)));
        }

        return;
    }

    {
        p.save();
        QPainterPath clip;
        clip.addRoundedRect(gr, 10.0, 10.0);
        p.setClipPath(clip);

        if (!m_gate_image.isNull()) {
            p.drawPixmap(gr, m_gate_image, QRectF(m_gate_image.rect()));
        } else {
            p.setBrush(QColor(47, 51, 50));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(gr, 10.0, 10.0);

            if (!m_app_icon.isNull()) {
                qreal ix = gr.x() + (gr.width() - 94.0) / 2.0;
                qreal iy = gr.y() + 15.5;
                p.drawPixmap(QRectF(ix, iy, 94.0, 94.0), m_app_icon, QRectF(m_app_icon.rect()));
            }
        }

        p.restore();
    }

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(192, 192, 192), 3.0));
    p.drawRoundedRect(gr, 10.0, 10.0);

    {
        QRectF sbr = start_button_rect();
        p.setBrush(QColor(20, 168, 222));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(sbr, 10.0, 10.0);

        QFont f = p.font();
        f.setPixelSize(14);
        f.setBold(true);
        p.setFont(f);
        p.setPen(QColor(255, 255, 255));
        p.drawText(sbr, Qt::AlignCenter, tr("Start"));
    }

    if (m_manual_exists) {
        QRectF mr = manual_button_rect();
        p.setBrush(QColor(202, 0, 106));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(mr, 12.0, 12.0);

        QFont f = p.font();
        f.setPixelSize(12);
        f.setBold(false);
        p.setFont(f);
        p.setPen(Qt::white);
        p.drawText(mr, Qt::AlignCenter, tr("Manual"));
    }
}

void LiveAreaWidget::paint_manual(QPainter &p) {
    if (m_manual_pages.empty())
        return;

    const auto &page = m_manual_pages[m_manual_page];
    p.drawPixmap(QRectF(0, 0, REF_W, REF_H), page, QRectF(page.rect()));

    p.fillRect(QRectF(0, REF_H - 32, REF_W, 32), QColor(0, 0, 0, 120));

    QFont f = p.font();
    f.setPixelSize(14);
    f.setBold(false);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(QRectF(0, REF_H - 32, REF_W, 32), Qt::AlignCenter,
        tr("%1 / %2").arg(m_manual_page + 1).arg(m_manual_pages.size()));

    const int count = static_cast<int>(m_manual_pages.size());
    f.setPixelSize(22);
    f.setBold(true);
    p.setFont(f);

    if (m_manual_page > 0) {
        p.setBrush(QColor(0, 0, 0, 80));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(8, REF_H / 2 - 28, 36, 56), 8, 8);
        p.setPen(QColor(255, 255, 255, 200));
        p.drawText(QRectF(8, REF_H / 2 - 28, 36, 56), Qt::AlignCenter, QStringLiteral("<"));
    }
    if (m_manual_page < count - 1) {
        p.setBrush(QColor(0, 0, 0, 80));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(REF_W - 44, REF_H / 2 - 28, 36, 56), 8, 8);
        p.setPen(QColor(255, 255, 255, 200));
        p.drawText(QRectF(REF_W - 44, REF_H / 2 - 28, 36, 56), Qt::AlignCenter, QStringLiteral(">"));
    }

    p.setBrush(QColor(0, 0, 0, 120));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRectF(REF_W - 44, 4, 36, 28), 6, 6);
    f.setPixelSize(16);
    p.setFont(f);
    p.setPen(QColor(255, 255, 255, 220));
    p.drawText(QRectF(REF_W - 44, 4, 36, 28), Qt::AlignCenter, QStringLiteral("\u2715"));
}

void LiveAreaWidget::mousePressEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton) {
        QWindow::mousePressEvent(e);
        return;
    }

    const QPointF vpos = to_virtual(e->position());

    if (m_showing_manual) {
        if (QRectF(REF_W - 44, 4, 36, 28).contains(vpos)) {
            exit_manual();
            e->accept();
            return;
        }

        if (vpos.x() < REF_W / 2.0) {
            if (m_manual_page > 0) {
                m_manual_page--;
                request_redraw();
            }
        } else {
            if (m_manual_page < static_cast<int>(m_manual_pages.size()) - 1) {
                m_manual_page++;
                request_redraw();
            }
        }
        e->accept();
        return;
    }

    if (m_gate_animating) {
        e->accept();
        return;
    }

    if (gate_rect().contains(vpos)) {
        start_gate_animation();
        e->accept();
        return;
    }

    if (m_manual_exists && manual_button_rect().contains(vpos)) {
        enter_manual();
        e->accept();
        return;
    }

    auto it_style = s_styles.find(m_style_type);
    if (it_style == s_styles.end())
        it_style = s_styles.find("a1");

    for (const auto &frame : m_frames) {
        auto fl_it = it_style->second.frames.find(frame.id);
        if (fl_it == it_style->second.frames.end())
            continue;

        auto item_it = m_frame_items.find(frame.id);
        if (item_it == m_frame_items.end())
            continue;

        const auto &target = item_it->second.target;
        if (target.empty() || target.find("psts:") != std::string::npos)
            continue;

        const auto &fl = fl_it->second;
        QRectF fr(REF_W - fl.pos.x(), REF_H - fl.pos.y(),
            fl.size.width(), fl.size.height());

        if (fr.contains(vpos)) {
            QDesktopServices::openUrl(QUrl(QString::fromStdString(target)));
            e->accept();
            return;
        }
    }

    QWindow::mousePressEvent(e);
}

void LiveAreaWidget::mouseMoveEvent(QMouseEvent *e) {
    if (m_gate_animating) {
        setCursor(Qt::ArrowCursor);
        return;
    }

    if (m_showing_manual) {
        setCursor(Qt::PointingHandCursor);
        return;
    }

    const QPointF vpos = to_virtual(e->position());
    bool pointing = false;

    if (gate_rect().contains(vpos))
        pointing = true;
    else if (m_manual_exists && manual_button_rect().contains(vpos))
        pointing = true;
    else {
        auto it_style = s_styles.find(m_style_type);
        if (it_style == s_styles.end())
            it_style = s_styles.find("a1");

        for (const auto &frame : m_frames) {
            auto fl_it = it_style->second.frames.find(frame.id);
            if (fl_it == it_style->second.frames.end())
                continue;
            auto item_it = m_frame_items.find(frame.id);
            if (item_it == m_frame_items.end() || item_it->second.target.empty())
                continue;
            if (item_it->second.target.find("psts:") != std::string::npos)
                continue;

            const auto &fl = fl_it->second;
            QRectF fr(REF_W - fl.pos.x(), REF_H - fl.pos.y(),
                fl.size.width(), fl.size.height());
            if (fr.contains(vpos)) {
                pointing = true;
                break;
            }
        }
    }

    setCursor(pointing ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void LiveAreaWidget::resizeEvent(QResizeEvent *e) {
    m_backing_store.resize(e->size() * devicePixelRatio());
    QWindow::resizeEvent(e);
    request_redraw();
}

void LiveAreaWidget::start_gate_animation() {
    if (m_gate_animating)
        return;

    m_gate_animating = true;
    m_gate_anim_progress = 0.0;

    m_gate_anim->start();
}

void LiveAreaWidget::update_autoflip() {
    bool needs_update = false;
    const auto now = current_time_secs();

    for (auto &frame : m_frames) {
        if (frame.autoflip == 0)
            continue;

        auto it = m_frame_items.find(frame.id);
        if (it == m_frame_items.end())
            continue;

        auto &item = it->second;
        if (item.last_flip_time == 0)
            item.last_flip_time = now;

        while (item.last_flip_time + frame.autoflip <= now) {
            item.last_flip_time += frame.autoflip;

            size_t count = std::max(item.backgrounds.size(), item.images.size());
            count = std::max(count, item.texts.size());
            if (count > 0) {
                item.current_item = (item.current_item + 1) % count;
                needs_update = true;
            }
        }
    }

    if (needs_update)
        request_redraw();
}

void LiveAreaWidget::keyPressEvent(QKeyEvent *e) {
    if (m_showing_manual) {
        switch (e->key()) {
        case Qt::Key_Left:
            if (m_manual_page > 0) {
                m_manual_page--;
                request_redraw();
            }
            break;
        case Qt::Key_Right:
            if (m_manual_page < static_cast<int>(m_manual_pages.size()) - 1) {
                m_manual_page++;
                request_redraw();
            }
            break;
        case Qt::Key_Escape:
            exit_manual();
            break;
        default:
            QWindow::keyPressEvent(e);
            return;
        }
        e->accept();
        return;
    }
    QWindow::keyPressEvent(e);
}

void LiveAreaWidget::load_manual_pages() {
    m_manual_pages.clear();

    QString base = m_app_fs_path + "/sce_sys/manual";

    const auto lang = QString("%1").arg(m_emuenv.cfg.sys_lang, 2, 10, QChar('0'));
    QDir lang_dir(base + "/" + lang);
    if (lang_dir.exists() && !lang_dir.isEmpty())
        base = lang_dir.absolutePath();

    for (int i = 1; i <= 999; ++i) {
        QString filename = QString("%1.png").arg(i, 3, 10, QChar('0'));
        QString path = base + "/" + filename;
        QPixmap pix;
        if (!pix.load(path))
            break;
        m_manual_pages.push_back(std::move(pix));
    }
}

void LiveAreaWidget::enter_manual() {
    load_manual_pages();
    if (m_manual_pages.empty())
        return;
    m_manual_page = 0;
    m_showing_manual = true;
    request_redraw();
}

void LiveAreaWidget::exit_manual() {
    m_showing_manual = false;
    m_manual_pages.clear();
    request_redraw();
}
