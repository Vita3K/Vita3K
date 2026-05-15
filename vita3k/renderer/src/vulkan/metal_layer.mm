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

extern "C" void *get_metal_layer_from_view(void *nsview) {
    NSView *view = (__bridge NSView *)nsview;
    if (!view)
        return nullptr;

    view.wantsLayer = YES;
    CALayer *layer = view.layer;
    if (![layer isKindOfClass:[CAMetalLayer class]]) {
        CAMetalLayer *metal_layer = [CAMetalLayer layer];
        NSScreen *screen = view.window.screen ?: NSScreen.mainScreen;
        metal_layer.contentsScale = screen ? screen.backingScaleFactor : 1.0;
        view.layer = metal_layer;
        layer = metal_layer;
    }

    CAMetalLayer *metal_layer = (CAMetalLayer *)layer;
    NSScreen *screen = view.window.screen ?: NSScreen.mainScreen;
    const CGFloat scale = screen ? screen.backingScaleFactor : view.window.backingScaleFactor;
    const CGRect bounds = view.bounds;
    metal_layer.frame = bounds;
    metal_layer.contentsScale = scale > 0.0 ? scale : 1.0;
    if (bounds.size.width > 0.0 && bounds.size.height > 0.0) {
        metal_layer.drawableSize = CGSizeMake(bounds.size.width * metal_layer.contentsScale,
            bounds.size.height * metal_layer.contentsScale);
    }

    return (__bridge void *)layer;
}
