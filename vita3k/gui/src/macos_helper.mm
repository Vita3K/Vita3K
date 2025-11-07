// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <util/fs.h>

#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

namespace gui {

// Convert PNG to ICNS using CGImageDestination
bool png_to_icns(const fs::path &png_path, const fs::path &icns_path) {
    @autoreleasepool {
        NSString *pngPathStr = [NSString stringWithUTF8String:png_path.c_str()];
        NSString *icnsPathStr = [NSString stringWithUTF8String:icns_path.c_str()];

        NSURL *sourceURL = [NSURL fileURLWithPath:pngPathStr];
        NSURL *destURL = [NSURL fileURLWithPath:icnsPathStr];

        // Load source PNG image
        CGImageSourceRef source = CGImageSourceCreateWithURL((__bridge CFURLRef)sourceURL, NULL);
        if (!source) {
            return false;
        }

        // Get CGImage from source
        CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, NULL);
        CFRelease(source);

        if (!image) {
            return false;
        }

        // Create ICNS destination
        CGImageDestinationRef dest = CGImageDestinationCreateWithURL(
            (__bridge CFURLRef)destURL,
            (__bridge CFStringRef)UTTypeICNS.identifier,
            1,
            NULL
        );

        if (!dest) {
            CGImageRelease(image);
            return false;
        }

        // Add image and finalize
        CGImageDestinationAddImage(dest, image, NULL);
        bool success = CGImageDestinationFinalize(dest);

        CFRelease(dest);
        CGImageRelease(image);

        return success;
    }
}

fs::path get_desktop_path() {
    @autoreleasepool {
        NSArray *paths = NSSearchPathForDirectoriesInDomains(
            NSDesktopDirectory,
            NSUserDomainMask,
            YES
        );

        if ([paths count] > 0) { // Should have at least one value
            NSString *desktopNSString = [paths objectAtIndex:0];
            std::string desktopStdString = [desktopNSString UTF8String];
            return fs::path(desktopStdString);
        }
        return fs::path();
    }
}

// Create launcher script
bool create_launcher(const fs::path &exe_path, const fs::path &macOS_path, const std::string &app_path) {
    @autoreleasepool {
        NSFileManager *fm = [NSFileManager defaultManager];

        NSString *exePathStr = [NSString stringWithUTF8String:exe_path.c_str()];
        NSString *macOSPath = [NSString stringWithUTF8String:macOS_path.c_str()];
        NSString *appPathStr = [NSString stringWithUTF8String:app_path.c_str()];
        NSString *launcherPath = [macOSPath stringByAppendingPathComponent:@"launcher"];

        NSString *scriptContent = [NSString stringWithFormat:
            @"#!/bin/bash\nexec \"%@\" -r \"%@\"\n",
            exePathStr, appPathStr];

        NSError *error = nil;
        bool success = [scriptContent writeToFile:launcherPath
                                       atomically:YES
                                         encoding:NSUTF8StringEncoding
                                            error:&error];

        // Make launcher executable
        [fm setAttributes:@{NSFilePosixPermissions: @0755}
             ofItemAtPath:launcherPath
                    error:nil];

        return success;
    }
}

// Create Info.plist
bool create_info_plist(const fs::path &contents_path, const std::string &app_name, const std::string &app_path) {
    @autoreleasepool {
        NSString *contentsPath = [NSString stringWithUTF8String:contents_path.c_str()];
        NSString *plistPath = [contentsPath stringByAppendingPathComponent:@"Info.plist"];
        NSString *bundleID = [NSString stringWithFormat:@"org.vita3k.%s", app_path.c_str()];
        NSString *appNameStr = [NSString stringWithUTF8String:app_name.c_str()];

        NSDictionary *plistDict = @{
            @"CFBundleExecutable": @"launcher",
            @"CFBundleIconFile": @"icon.icns",
            @"CFBundleIdentifier": bundleID,
            @"CFBundleName": appNameStr,
            @"CFBundleDisplayName": appNameStr,
            @"LSMinimumSystemVersion": @"11",
            @"LSMultipleInstancesProhibited": @YES
        };

        return [plistDict writeToFile:plistPath atomically:YES];
    }
}

} // namespace gui
