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

#include <gui-qt/gui_save.h>

#include <QDir>
#include <QObject>
#include <QSettings>

#include <memory>

using GuiPairList = QList<QPair<QString, QString>>;

class GuiSettingsBase : public QObject {
    Q_OBJECT

public:
    explicit GuiSettingsBase(QObject *parent = nullptr);
    ~GuiSettingsBase() override;

    void sync();

    QString get_settings_dir() const;

    QVariant get_value(const QString &key, const QString &name, const QVariant &def) const;
    QVariant get_value(const GuiSave &entry) const;
    bool contains_value(const QString &key, const QString &name) const;
    bool contains_value(const GuiSave &entry) const;

    static QVariant list_to_var(const GuiPairList &list);
    static GuiPairList var_to_list(const QVariant &var);

public Q_SLOTS:
    void remove_value(const QString &key, const QString &name, bool do_sync = true) const;
    void remove_value(const GuiSave &entry, bool do_sync = true) const;

    void set_value(const GuiSave &entry, const QVariant &value, bool do_sync = true) const;
    void set_value(const QString &key, const QVariant &value, bool do_sync = true) const;
    void set_value(const QString &key, const QString &name, const QVariant &value, bool do_sync = true) const;

protected:
    std::unique_ptr<QSettings> m_settings;
    QDir m_settings_dir;
};
