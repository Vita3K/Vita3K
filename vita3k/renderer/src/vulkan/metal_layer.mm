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

static NSString *const Vita3KMetalLayerName = @"Vita3KMetalLayer";

static CAMetalLayer *find_metal_layer(CALayer *host_layer) {
    if ([host_layer isKindOfClass:[CAMetalLayer class]])
        return (CAMetalLayer *)host_layer;
    for (CALayer *layer in host_layer.sublayers) {
        if ([layer isKindOfClass:[CAMetalLayer class]] &&
            [layer.name isEqualToString:Vita3KMetalLayerName])
            return (CAMetalLayer *)layer;
    }
    return nil;
}

static CAMetalLayer *configure_metal_layer(NSView *view) {
    [view setWantsLayer:YES];
    CALayer *host_layer = view.layer;
    if (!host_layer)
        return nil;

    CAMetalLayer *metal_layer = find_metal_layer(host_layer);
    if (!metal_layer) {
        metal_layer = [CAMetalLayer layer];
        metal_layer.name = Vita3KMetalLayerName;
        metal_layer.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
        [host_layer addSublayer:metal_layer];
    }

    const CGFloat scale = view.window ? view.window.backingScaleFactor
                                     : (NSScreen.mainScreen ? NSScreen.mainScreen.backingScaleFactor : 1.0);
    const CGSize size = view.bounds.size;
    metal_layer.frame = view.bounds;
    metal_layer.contentsScale = scale;
    metal_layer.drawableSize = CGSizeMake(size.width * scale, size.height * scale);
    metal_layer.opaque = YES;
    return metal_layer;
}

extern "C" void *get_metal_layer_from_view(void *nsview) {
    if (!nsview)
        return nullptr;

    @autoreleasepool {
        NSView *view = (__bridge NSView *)nsview;
        // AppKit mutations belong to the UI thread. Render threads only retrieve
        // the layer prepared by the native window, without waiting on the UI.
        CAMetalLayer *layer = [NSThread isMainThread]
            ? configure_metal_layer(view)
            : find_metal_layer(view.layer);
        return (__bridge void *)layer;
    }
}
