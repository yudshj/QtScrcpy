#include "path.h"

#import <Cocoa/Cocoa.h>

static QString fromNSString(NSString *value)
{
    return value ? QString::fromUtf8([value fileSystemRepresentation]) : QString();
}

QString Path::GetCurrentPath()
{
    return fromNSString([[NSBundle mainBundle] bundlePath]);
}

QString Path::GetApplicationSupportPath()
{
    NSArray<NSString *> *paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
    if (paths.count == 0) {
        return QString();
    }
    return fromNSString([paths.firstObject stringByAppendingPathComponent:@"QtScrcpy"]);
}
