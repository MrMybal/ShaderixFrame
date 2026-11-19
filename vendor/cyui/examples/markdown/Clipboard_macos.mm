#include "Platform.h"
#import <AppKit/AppKit.h>
namespace platform {
Clipboard nativeClipboard() {
    Clipboard result;
    @autoreleasepool {
        NSPasteboard *board = [NSPasteboard generalPasteboard];
        NSArray<NSURL *> *urls = [board readObjectsForClasses:@[ [NSURL class] ]
                                                      options:@{
                                                          NSPasteboardURLReadingFileURLsOnlyKey : @YES
                                                      }];
        for (NSURL *url in urls)
            if (url.isFileURL)
                result.files.push_back(path(url.path.UTF8String));
        NSString *html = [board stringForType:NSPasteboardTypeHTML];
        if (html)
            result.html = html.UTF8String;
        NSData *tiff = [board dataForType:NSPasteboardTypeTIFF];
        if (tiff && tiff.length <= 64 * 1024 * 1024) {
            NSBitmapImageRep *bitmap = [[NSBitmapImageRep alloc] initWithData:tiff];
            if (bitmap && bitmap.pixelsWide > 0 && bitmap.pixelsHigh > 0 && bitmap.pixelsWide <= 16384 &&
                bitmap.pixelsHigh <= 16384 && bitmap.pixelsWide * bitmap.pixelsHigh <= 32 * 1024 * 1024) {
                NSData *png = [bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
                if (png && png.length <= 64 * 1024 * 1024) {
                    auto *bytes = static_cast<const unsigned char *>(png.bytes);
                    result.image.assign(bytes, bytes + png.length);
                    result.imageName = "clipboard.png";
                }
            }
        }
    }
    return result;
}
} // namespace platform
