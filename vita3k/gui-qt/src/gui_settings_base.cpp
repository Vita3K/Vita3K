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

#include <gui-qt/gui_settings_base.h>

#include <QByteArray>
#include <QDataStream>

GuiSettingsBase::GuiSettingsBase(QObject *parent)
    : QObject(parent) {
}

GuiSettingsBase::~GuiSettingsBase() = default;

void GuiSettingsBase::sync() {
    if (m_settings)
        m_settings->sync();
}

QString GuiSettingsBase::get_settings_dir() const {
    return m_settings_dir.absolutePath();
}

QVariant GuiSettingsBase::get_value(const QString &key, const QString &name, const QVariant &def) const {
    return m_settings ? m_settings->value(key + "/" + name, def) : def;
}

QVariant GuiSettingsBase::get_value(const GuiSave &entry) const {
    return get_value(entry.key, entry.name, entry.def);
}

bool GuiSettingsBase::contains_value(const QString &key, const QString &name) const {
    return m_settings ? m_settings->contains(key + "/" + name) : false;
}

bool GuiSettingsBase::contains_value(const GuiSave &entry) const {
    return contains_value(entry.key, entry.name);
}

QVariant GuiSettingsBase::list_to_var(const GuiPairList &list) {
    QByteArray ba;
    QDataStream stream(&ba, QIODevice::WriteOnly);
    stream << list;
    return QVariant(ba);
}

GuiPairList GuiSettingsBase::var_to_list(const QVariant &var) {
    GuiPairList list;
    QByteArray ba = var.toByteArray();
    QDataStream stream(&ba, QIODevice::ReadOnly);
    stream >> list;
    return list;
}

void GuiSettingsBase::set_value(const GuiSave &entry, const QVariant &value, bool do_sync) const {
    set_value(entry.key, entry.name, value, do_sync);
}

void GuiSettingsBase::set_value(const QString &key, const QVariant &value, bool do_sync) const {
    if (m_settings) {
        m_settings->setValue(key, value);
        if (do_sync)
            m_settings->sync();
    }
}

void GuiSettingsBase::set_value(const QString &key, const QString &name, const QVariant &value, bool do_sync) const {
    if (m_settings) {
        m_settings->beginGroup(key);
        m_settings->setValue(name, value);
        m_settings->endGroup();
        if (do_sync)
            m_settings->sync();
    }
}

void GuiSettingsBase::remove_value(const QString &key, const QString &name, bool do_sync) const {
    if (m_settings) {
        m_settings->beginGroup(key);
        m_settings->remove(name);
        m_settings->endGroup();
        if (do_sync)
            m_settings->sync();
    }
}

void GuiSettingsBase::remove_value(const GuiSave &entry, bool do_sync) const {
    remove_value(entry.key, entry.name, do_sync);
}
