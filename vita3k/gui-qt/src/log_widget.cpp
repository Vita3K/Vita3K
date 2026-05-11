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

#include <gui-qt/log_widget.h>
#include <gui-qt/qt_utils.h>

#include <util/log.h>

#include <spdlog/common.h>

#include <QEvent>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPointer>
#include <QScrollBar>
#include <QShortcut>
#include <QSize>
#include <QStyle>
#include <QTextDocument>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <deque>
#include <mutex>
#include <utility>

namespace {

constexpr int SAFE_UNBOUNDED_LOG_BUFFER_SIZE = 50000;
constexpr size_t MAX_LOG_DRAIN_BATCH = 256;
constexpr size_t MIN_PENDING_LOG_LINES = 512;
constexpr size_t MAX_PENDING_LOG_LINES = 8192;

struct PendingLogMessage {
    std::string msg;
    int level = 0;
    size_t repeat_count = 1;
};

QFont make_log_font(const QString &family = {}) {
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(9);
    if (!family.isEmpty())
        font.setFamily(family);
    return font;
}

size_t pending_log_limit_for_buffer_size(const int requested_size) {
    const int effective_size = (requested_size <= 0) ? SAFE_UNBOUNDED_LOG_BUFFER_SIZE : requested_size;
    return std::clamp(static_cast<size_t>(effective_size) * 4, MIN_PENDING_LOG_LINES, MAX_PENDING_LOG_LINES);
}

static struct {
    std::mutex mutex;
    QPointer<LogWidget> widget;
    std::deque<PendingLogMessage> backlog;
    size_t dropped_lines = 0;
    size_t pending_limit = pending_log_limit_for_buffer_size(1000);
    bool drain_posted = false;
} s_log_state;

} // namespace

LogWidget::LogWidget(QWidget *parent)
    : custom_dock_widget(tr("Log"), parent) {
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_log = new QPlainTextEdit(container);
    m_log->setReadOnly(true);
    m_log->setFont(make_log_font());
    m_log->document()->setMaximumBlockCount(effective_buffer_size(1000));
    m_log->setObjectName(QStringLiteral("log_frame"));
    layout->addWidget(m_log);
    setWidget(container);

    m_search_container = new QWidget(m_log->viewport());
    m_search_container->setObjectName(QStringLiteral("log_search_container"));
    auto *search_layout = new QHBoxLayout(m_search_container);
    search_layout->setContentsMargins(6, 6, 6, 6);
    search_layout->setSpacing(4);

    m_search_edit = new QLineEdit(m_search_container);
    m_search_edit->setClearButtonEnabled(true);
    m_search_edit->setMinimumWidth(220);
    m_search_edit->installEventFilter(this);
    search_layout->addWidget(m_search_edit);

    m_search_prev_button = new QToolButton(m_search_container);
    m_search_prev_button->setAutoRaise(true);
    search_layout->addWidget(m_search_prev_button);

    m_search_next_button = new QToolButton(m_search_container);
    m_search_next_button->setAutoRaise(true);
    search_layout->addWidget(m_search_next_button);

    m_search_close_button = new QToolButton(m_search_container);
    m_search_close_button->setAutoRaise(true);
    search_layout->addWidget(m_search_close_button);

    m_search_container->hide();
    m_log->viewport()->installEventFilter(this);

    auto *find_shortcut = new QShortcut(QKeySequence::Find, this);
    connect(find_shortcut, &QShortcut::activated, this, &LogWidget::show_search_bar);

    auto *find_next_shortcut = new QShortcut(QKeySequence::FindNext, this);
    connect(find_next_shortcut, &QShortcut::activated, this, [this]() { find_text(false); });

    auto *find_previous_shortcut = new QShortcut(QKeySequence::FindPrevious, this);
    connect(find_previous_shortcut, &QShortcut::activated, this, [this]() { find_text(true); });

    connect(m_search_edit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_current_search_cursor = QTextCursor();
        if (text.isEmpty()) {
            m_log->setExtraSelections({});
            return;
        }

        find_text(false);
    });
    connect(m_search_edit, &QLineEdit::returnPressed, this, [this]() { find_text(false); });
    connect(m_search_prev_button, &QToolButton::clicked, this, [this]() { find_text(true); });
    connect(m_search_next_button, &QToolButton::clicked, this, [this]() { find_text(false); });
    connect(m_search_close_button, &QToolButton::clicked, this, &LogWidget::hide_search_bar);

    update_search_ui_text();
}

LogWidget::~LogWidget() {
    const std::lock_guard<std::mutex> lock(s_log_state.mutex);
    if (s_log_state.widget == this) {
        s_log_state.widget = nullptr;
        s_log_state.drain_posted = false;
    }
}

void LogWidget::register_callback() {
    logging::set_log_callback([](std::string msg, int level) {
        if (msg.empty())
            return;

        QPointer<LogWidget> target;
        {
            const std::lock_guard<std::mutex> lock(s_log_state.mutex);
            if (!s_log_state.backlog.empty()) {
                auto &last = s_log_state.backlog.back();
                if ((last.level == level) && (last.msg == msg)) {
                    last.repeat_count++;
                } else {
                    s_log_state.backlog.push_back(PendingLogMessage{ std::move(msg), level, 1 });
                }
            } else {
                s_log_state.backlog.push_back(PendingLogMessage{ std::move(msg), level, 1 });
            }

            while (s_log_state.backlog.size() > s_log_state.pending_limit) {
                s_log_state.dropped_lines += s_log_state.backlog.front().repeat_count;
                s_log_state.backlog.pop_front();
            }

            if (s_log_state.widget && !s_log_state.drain_posted) {
                s_log_state.drain_posted = true;
                target = s_log_state.widget;
            }
        }

        if (target)
            QMetaObject::invokeMethod(target.data(), &LogWidget::drain_pending_messages, Qt::QueuedConnection);
    });
}

void LogWidget::attach() {
    bool should_schedule_drain = false;
    {
        const std::lock_guard<std::mutex> lock(s_log_state.mutex);
        s_log_state.widget = this;
        if (!s_log_state.backlog.empty() && !s_log_state.drain_posted) {
            s_log_state.drain_posted = true;
            should_schedule_drain = true;
        }
    }

    if (should_schedule_drain)
        QMetaObject::invokeMethod(this, &LogWidget::drain_pending_messages, Qt::QueuedConnection);
}

void LogWidget::on_log_message(const QString &msg, int level) {
    m_entries.emplace_back(msg, level);
    trim_entries();
    append_log_entry(msg, level);
}

void LogWidget::drain_pending_messages() {
    std::deque<PendingLogMessage> batch;
    size_t dropped_lines = 0;
    bool should_schedule_next = false;

    {
        const std::lock_guard<std::mutex> lock(s_log_state.mutex);
        dropped_lines = std::exchange(s_log_state.dropped_lines, 0);

        const size_t count = std::min(MAX_LOG_DRAIN_BATCH, s_log_state.backlog.size());
        for (size_t i = 0; i < count; ++i) {
            batch.push_back(std::move(s_log_state.backlog.front()));
            s_log_state.backlog.pop_front();
        }

        should_schedule_next = !s_log_state.backlog.empty();
        s_log_state.drain_posted = should_schedule_next;
    }

    if (dropped_lines > 0) {
        on_log_message(
            QStringLiteral("[Qt log] Dropped %1 queued log lines before they could be rendered.")
                .arg(static_cast<qulonglong>(dropped_lines)),
            static_cast<int>(spdlog::level::warn));
    }

    if (!batch.empty()) {
        m_log->setUpdatesEnabled(false);
        for (const auto &entry : batch) {
            QString qmsg = QString::fromStdString(entry.msg).trimmed();
            if (qmsg.isEmpty())
                continue;

            if (entry.repeat_count > 1) {
                qmsg += QStringLiteral(" [x%1]").arg(static_cast<qulonglong>(entry.repeat_count));
            }

            on_log_message(qmsg, entry.level);
        }

        m_log->setUpdatesEnabled(true);
        m_log->viewport()->update();
    }

    if (m_search_container && m_search_container->isVisible() && m_search_edit && !m_search_edit->text().isEmpty())
        update_search_highlights();

    if (should_schedule_next)
        QMetaObject::invokeMethod(this, &LogWidget::drain_pending_messages, Qt::QueuedConnection);
}

void LogWidget::append_log_entry(const QString &msg, int level, bool preserve_scroll) {
    QScrollBar *bar = m_log->verticalScrollBar();
    const bool at_bottom = !preserve_scroll || bar->value() == bar->maximum();

    QTextCursor cursor = m_log->textCursor();
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat fmt;
    fmt.setForeground(color_for_level(level));

    cursor.insertText(msg + "\n", fmt);

    if (at_bottom)
        bar->setValue(bar->maximum());
}

QColor LogWidget::color_for_level(int level) const {
    using L = spdlog::level::level_enum;

    switch (static_cast<L>(level)) {
    case L::critical:
        return gui::utils::get_label_color(
            QStringLiteral("log_level_fatal"),
            QColor(QStringLiteral("#ff00ff")),
            QColor(QStringLiteral("#ff66ff")));
    case L::err:
        return gui::utils::get_label_color(
            QStringLiteral("log_level_error"),
            QColor(QStringLiteral("#C02F1D")),
            QColor(QStringLiteral("#ff8080")));
    case L::warn:
        return gui::utils::get_label_color(
            QStringLiteral("log_level_warning"),
            QColor(QStringLiteral("#BA8745")),
            QColor(QStringLiteral("#ffd27f")));
    case L::info:
        return gui::utils::get_label_color(
            QStringLiteral("log_level_notice"),
            QColor(Qt::black),
            QColor(Qt::white));
    default:
        return gui::utils::get_label_color(
            QStringLiteral("log_level_trace"),
            QColor(QStringLiteral("#808080")),
            QColor(Qt::white));
    }
}

void LogWidget::trim_entries() {
    const int max_blocks = m_log->document()->maximumBlockCount();
    while (max_blocks > 0 && static_cast<int>(m_entries.size()) > max_blocks)
        m_entries.pop_front();
}

void LogWidget::set_log_buffer_size(int size) {
    m_log->document()->setMaximumBlockCount(effective_buffer_size(size));
    {
        const std::lock_guard<std::mutex> lock(s_log_state.mutex);
        s_log_state.pending_limit = pending_log_limit_for_buffer_size(size);
        while (s_log_state.backlog.size() > s_log_state.pending_limit) {
            s_log_state.dropped_lines += s_log_state.backlog.front().repeat_count;
            s_log_state.backlog.pop_front();
        }
    }
    trim_entries();
    if (m_search_edit && !m_search_edit->text().isEmpty())
        update_search_highlights();
}

int LogWidget::log_buffer_size() const {
    return m_log->document()->maximumBlockCount();
}

int LogWidget::effective_buffer_size(int requested_size) const {
    return (requested_size <= 0) ? SAFE_UNBOUNDED_LOG_BUFFER_SIZE : requested_size;
}

void LogWidget::set_log_font_family(const QString &family) {
    if (!m_log)
        return;

    m_log->setFont(make_log_font(family));
}

QString LogWidget::log_font_family() const {
    return m_log ? m_log->font().family() : QString();
}

void LogWidget::repaint_text_colors() {
    const QScrollBar *bar = m_log->verticalScrollBar();
    const int old_value = bar->value();
    const bool at_bottom = old_value == bar->maximum();

    m_log->clear();

    for (const auto &[msg, level] : m_entries)
        append_log_entry(msg, level, false);

    if (!at_bottom)
        m_log->verticalScrollBar()->setValue(old_value);

    if (m_search_edit && !m_search_edit->text().isEmpty())
        update_search_highlights();
}

void LogWidget::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange)
        update_search_ui_text();
    else if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
        update_search_ui_icons();

    custom_dock_widget::changeEvent(event);
}

bool LogWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_search_edit && event->type() == QEvent::KeyPress) {
        const auto *key_event = static_cast<QKeyEvent *>(event);
        if (key_event->key() == Qt::Key_Escape) {
            hide_search_bar();
            return true;
        }
    }

    if (watched == m_log->viewport() && (event->type() == QEvent::Resize || event->type() == QEvent::Show))
        reposition_search_bar();

    return custom_dock_widget::eventFilter(watched, event);
}

void LogWidget::show_search_bar() {
    if (!m_search_container)
        return;

    m_search_container->show();
    reposition_search_bar();
    update_search_ui_icons();
    m_search_edit->setFocus(Qt::ShortcutFocusReason);
    m_search_edit->selectAll();
    update_search_highlights();
}

void LogWidget::hide_search_bar() {
    if (!m_search_container || !m_search_edit)
        return;

    m_search_edit->clear();
    m_current_search_cursor = QTextCursor();
    m_log->setExtraSelections({});
    m_search_container->hide();
    m_log->setFocus(Qt::ShortcutFocusReason);
}

void LogWidget::find_text(bool backwards) {
    if (!m_search_edit)
        return;

    const QString query = m_search_edit->text();
    if (query.isEmpty())
        return;

    QTextDocument::FindFlags flags;
    if (backwards)
        flags |= QTextDocument::FindBackward;

    QTextCursor start_cursor = m_current_search_cursor;
    if (start_cursor.isNull())
        start_cursor = m_log->textCursor();

    start_cursor.setPosition(backwards ? start_cursor.selectionStart() : start_cursor.selectionEnd());

    QTextCursor match = m_log->document()->find(query, start_cursor, flags);
    if (match.isNull()) {
        QTextCursor wrap_cursor(m_log->document());
        wrap_cursor.movePosition(backwards ? QTextCursor::End : QTextCursor::Start);
        match = m_log->document()->find(query, wrap_cursor, flags);
    }

    if (!match.isNull()) {
        m_current_search_cursor = match;
        m_log->setTextCursor(match);
        m_log->centerCursor();
    }

    update_search_highlights();
}

void LogWidget::reposition_search_bar() {
    if (!m_search_container || !m_log)
        return;

    m_search_container->adjustSize();
    const QSize size = m_search_container->sizeHint();
    const QRect viewport_rect = m_log->viewport()->rect();
    const int margin = 8;
    m_search_container->setGeometry(
        viewport_rect.right() - size.width() - margin,
        viewport_rect.top() + margin,
        size.width(),
        size.height());
    m_search_container->raise();
}

void LogWidget::update_search_highlights() {
    if (!m_search_edit) {
        return;
    }

    const QString query = m_search_edit->text();
    if (query.isEmpty()) {
        m_log->setExtraSelections({});
        return;
    }

    QList<QTextEdit::ExtraSelection> selections;
    QTextCursor scan_cursor(m_log->document());
    while (true) {
        scan_cursor = m_log->document()->find(query, scan_cursor);
        if (scan_cursor.isNull())
            break;

        QTextEdit::ExtraSelection selection;
        selection.cursor = scan_cursor;
        selection.format.setBackground(QColor(QStringLiteral("#d9c36a")));
        selection.format.setForeground(Qt::black);
        selections.push_back(selection);
    }

    if (m_current_search_cursor.isNull())
        m_current_search_cursor = m_log->document()->find(query);

    if (!m_current_search_cursor.isNull()) {
        QTextEdit::ExtraSelection current_selection;
        current_selection.cursor = m_current_search_cursor;
        current_selection.format.setBackground(palette().color(QPalette::Highlight));
        current_selection.format.setForeground(palette().color(QPalette::HighlightedText));
        selections.push_back(current_selection);
    }

    m_log->setExtraSelections(selections);
}

void LogWidget::update_search_ui_text() {
    if (!m_search_edit || !m_search_container)
        return;

    m_search_edit->setPlaceholderText(tr("Find in log..."));
    update_search_ui_icons();
    m_search_prev_button->setToolTip(tr("Find Previous"));
    m_search_next_button->setToolTip(tr("Find Next"));
    m_search_close_button->setToolTip(tr("Close Search"));
}

void LogWidget::update_search_ui_icons() {
    if (!m_search_prev_button || !m_search_next_button || !m_search_close_button)
        return;

    const QColor foreground = gui::utils::get_foreground_color(m_search_container);
    const QString theme_suffix = foreground.lightnessF() > 0.5f
        ? QStringLiteral("light")
        : QStringLiteral("dark");
    m_search_prev_button->setIcon(QIcon(QStringLiteral(":/icons/chevron_up_%1.svg").arg(theme_suffix)));
    m_search_next_button->setIcon(QIcon(QStringLiteral(":/icons/chevron_down_%1.svg").arg(theme_suffix)));
    m_search_close_button->setIcon(QIcon(QStringLiteral(":/icons/cross_%1.svg").arg(theme_suffix)));
}
