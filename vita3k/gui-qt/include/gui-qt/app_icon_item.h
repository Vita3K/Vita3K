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

#include <app/state.h>

#include <QPixmap>
#include <QTableWidgetItem>

#include <atomic>
#include <functional>
#include <mutex>

class AppIconItem : public QTableWidgetItem {
public:
    explicit AppIconItem()
        : QTableWidgetItem() {}
    void set_icon_load_func(std::function<void()> fn) {
        m_load_fn = std::move(fn);
    }

    void set_app(const app::AppEntry *app) { m_app = app; }
    const app::AppEntry *app() const { return m_app; }

    void set_has_custom_config(bool v) { m_has_custom_config = v; }
    bool has_custom_config() const { return m_has_custom_config; }

    void request_icon_load() const {
        if (m_loading.test_and_set(std::memory_order_acq_rel))
            return;

        if (m_load_fn)
            m_load_fn();
    }

    bool is_loading() const {
        return m_loading.test(std::memory_order_acquire);
    }

    void set_pixmap(const QPixmap &px) {
        std::lock_guard lock(m_pixmap_mutex);
        m_pixmap = px;
    }

    QPixmap pixmap_copy() const {
        std::lock_guard lock(m_pixmap_mutex);
        return m_pixmap;
    }

private:
    mutable std::mutex m_pixmap_mutex;
    mutable std::atomic_flag m_loading = ATOMIC_FLAG_INIT;
    std::function<void()> m_load_fn;
    const app::AppEntry *m_app{};
    bool m_has_custom_config{};
    QPixmap m_pixmap;
};
