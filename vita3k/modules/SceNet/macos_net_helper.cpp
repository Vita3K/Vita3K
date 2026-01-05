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

#include "macos_net_helper.h"

#include <SystemConfiguration/SystemConfiguration.h>
#include <cstring>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_dl.h>

// Check if interface is physical (en*)
bool is_physical_interface(const char *name) {
    return name && strncmp(name, "en", 2) == 0;
}

// Get the primary network interface name using SystemConfiguration
bool get_primary_interface_name(char *dest, size_t bufferSize) {
    bool success = false;
    auto size = static_cast<CFIndex>(bufferSize);

    if (size < 0) [[unlikely]] // Overflow from size_t to CFIndex (practically unreachable)
        return false;

    auto store = SCDynamicStoreCreate(nullptr, CFSTR("Vita3K"), nullptr, nullptr);
    if (!store)
        return false;

    // Try IPv4 first
    auto dict = static_cast<CFDictionaryRef>(SCDynamicStoreCopyValue(store, CFSTR("State:/Network/Global/IPv4")));
    if (dict) {
        auto iface = static_cast<CFStringRef>(CFDictionaryGetValue(dict, CFSTR("PrimaryInterface")));
        if (iface && CFStringGetCString(iface, dest, size, kCFStringEncodingUTF8)) {
            success = true;
        }
        CFRelease(dict);
    }

    // Fallback to IPv6
    if (!success) {
        dict = static_cast<CFDictionaryRef>(SCDynamicStoreCopyValue(store, CFSTR("State:/Network/Global/IPv6")));
        if (dict) {
            auto iface = static_cast<CFStringRef>(CFDictionaryGetValue(dict, CFSTR("PrimaryInterface")));
            if (iface && CFStringGetCString(iface, dest, size, kCFStringEncodingUTF8)) {
                success = true;
            }
            CFRelease(dict);
        }
    }

    CFRelease(store);
    return success;
}

// Get MAC address from a physical interface
// If hint is a physical interface, use it; otherwise find first active en*
bool get_mac_address(const char *hint, uint8_t mac[6]) {
    struct ifaddrs *iflist = nullptr;
    if (getifaddrs(&iflist) != 0)
        return false;

    const char *target = is_physical_interface(hint) ? hint : nullptr;
    bool success = false;

    for (struct ifaddrs *cur = iflist; cur; cur = cur->ifa_next) {
        if (!cur->ifa_addr || !cur->ifa_name)
            continue;
        if (!(cur->ifa_flags & IFF_UP) || (cur->ifa_flags & IFF_LOOPBACK))
            continue;
        if (cur->ifa_addr->sa_family != AF_LINK)
            continue;
        if (!is_physical_interface(cur->ifa_name))
            continue;

        // If we have a target, only match that; otherwise take first
        if (target && strcmp(cur->ifa_name, target) != 0)
            continue;

        auto sdl = reinterpret_cast<struct sockaddr_dl *>(cur->ifa_addr);
        if (sdl->sdl_alen == 6) {
            memcpy(mac, LLADDR(sdl), 6);
            success = true;
            break;
        }
    }

    freeifaddrs(iflist);
    return success;
}
