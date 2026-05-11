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

#include <gui-qt/physical_key_qt.h>

#include <QGuiApplication>
#include <QKeyEvent>

namespace {

input::NativeScanCodeSet qt_native_scan_code_set() {
#if defined(Q_OS_WIN)
    return input::NativeScanCodeSet::Windows;
#elif defined(Q_OS_MACOS)
    return input::NativeScanCodeSet::MacOS;
#else
    const QString platform_name = QGuiApplication::platformName().toLower();
    if (platform_name.startsWith(QStringLiteral("wayland"))
        || platform_name.startsWith(QStringLiteral("xcb"))) {
        return input::NativeScanCodeSet::Xkb;
    }

    return input::NativeScanCodeSet::Evdev;
#endif
}

} // namespace

input::PhysicalKeyCode physical_key_from_qt_event(const QKeyEvent &event) {
    return input::physical_key_from_native_scan_code(qt_native_scan_code_set(), event.nativeScanCode());
}
