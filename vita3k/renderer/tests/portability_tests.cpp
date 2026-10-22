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

#include "vulkan/instance_extensions.h"

#include <gtest/gtest.h>

TEST(vulkan_portability, extension_and_flag_agree) {
    for (bool supported : { false, true }) {
        for (unsigned duplicates = 0; duplicates < 4; ++duplicates) {
            std::vector<const char *> extensions{ "VK_KHR_surface", "unrelated" };
            for (unsigned i = 0; i < duplicates; ++i)
                extensions.push_back(vk::KHRPortabilityEnumerationExtensionName);
            const auto flags = renderer::vulkan::configure_portability_enumeration(extensions, supported);
            EXPECT_EQ(bool(flags & vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR), supported);
            EXPECT_EQ(extensions.size(), supported ? 3u : 2u);
            EXPECT_STREQ(extensions[0], "VK_KHR_surface");
            EXPECT_STREQ(extensions[1], "unrelated");
            if (supported)
                EXPECT_STREQ(extensions[2], vk::KHRPortabilityEnumerationExtensionName);
        }
    }
}

TEST(vulkan_portability, empty_and_repeated_requests) {
    for (bool supported : { false, true }) {
        std::vector<const char *> extensions;
        const auto first = renderer::vulkan::configure_portability_enumeration(extensions, supported);
        const auto second = renderer::vulkan::configure_portability_enumeration(extensions, supported);
        EXPECT_EQ(first, second);
        EXPECT_EQ(extensions.size(), supported ? 1u : 0u);
        renderer::vulkan::configure_portability_enumeration(extensions, false);
        EXPECT_TRUE(extensions.empty());
    }
}
