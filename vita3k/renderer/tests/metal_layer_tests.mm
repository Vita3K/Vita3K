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

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

#include <vkutil/native_surface.h>
#include <gtest/gtest.h>

#include <future>

static CAMetalLayer *layer_for(NSView *view) {
    return (__bridge CAMetalLayer *)get_metal_layer_from_view((__bridge void *)view);
}

TEST(metal_layer, null_view) {
    EXPECT_EQ(get_metal_layer_from_view(nullptr), nullptr);
}

TEST(metal_layer, creates_and_reuses_child_without_replacing_host) {
    @autoreleasepool {
        NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 320, 240)];
        view.wantsLayer = YES;
        CALayer *host = view.layer;
        CALayer *other = [CALayer layer];
        [host addSublayer:other];
        const auto original_count = host.sublayers.count;
        CAMetalLayer *layer = layer_for(view);
        ASSERT_TRUE([layer isKindOfClass:[CAMetalLayer class]]);
        EXPECT_EQ(view.layer, host);
        EXPECT_EQ(layer_for(view), layer);
        EXPECT_EQ(host.sublayers.count, original_count + 1);
        EXPECT_TRUE([host.sublayers containsObject:other]);
    }
}

TEST(metal_layer, reuses_metal_backing_layer) {
    @autoreleasepool {
        NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 320, 240)];
        CAMetalLayer *existing = [CAMetalLayer layer];
        view.wantsLayer = YES;
        view.layer = existing;
        EXPECT_EQ(layer_for(view), existing);
    }
}

TEST(metal_layer, refreshes_geometry_and_scale) {
    @autoreleasepool {
        NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 320, 240)];
        CAMetalLayer *layer = layer_for(view);
        ASSERT_NE(layer, nil);
        for (const auto &size : { NSMakeSize(640, 360), NSMakeSize(960, 544) }) {
            [view setFrameSize:size];
            EXPECT_EQ(layer_for(view), layer);
            EXPECT_DOUBLE_EQ(layer.frame.size.width, view.bounds.size.width);
            EXPECT_DOUBLE_EQ(layer.frame.size.height, view.bounds.size.height);
            EXPECT_DOUBLE_EQ(layer.drawableSize.width, view.bounds.size.width * layer.contentsScale);
            EXPECT_DOUBLE_EQ(layer.drawableSize.height, view.bounds.size.height * layer.contentsScale);
            EXPECT_GT(layer.contentsScale, 0);
        }
    }
}

TEST(metal_layer, worker_only_retrieves_prepared_layer) {
    @autoreleasepool {
        NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 320, 240)];
        CAMetalLayer *layer = layer_for(view);
        auto worker = std::async(std::launch::async, [view] { return get_metal_layer_from_view((__bridge void *)view); });
        ASSERT_EQ(worker.wait_for(std::chrono::seconds(5)), std::future_status::ready);
        EXPECT_EQ(worker.get(), (__bridge void *)layer);
    }
}
