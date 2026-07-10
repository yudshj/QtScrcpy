#include <QDebug>
#include <QWidget>
#include <Windows.h>

#include "winmousetap.h"

WinMouseTap::WinMouseTap() {}

WinMouseTap::~WinMouseTap() {}

void WinMouseTap::initMouseEventTap() {}

void WinMouseTap::quitMouseEventTap() {}

bool WinMouseTap::enableMouseEventTap(QRect rc, bool enabled)
{
    if (enabled && rc.isEmpty()) {
        return false;
    }
    if (enabled) {
        RECT mainRect;
        mainRect.left = (LONG)rc.left();
        mainRect.right = (LONG)rc.right();
        mainRect.top = (LONG)rc.top();
        mainRect.bottom = (LONG)rc.bottom();
        return ClipCursor(&mainRect) != 0;
    } else {
        return ClipCursor(Q_NULLPTR) != 0;
    }
}
