#include "ThemeManager.h"

#include <AppKit/AppKit.h>

void ThemeManager::setTitlebarColorOnMac(WId windowId, QColor color)
{
    if (windowId == 0) {
        return;
    }

    NSView* view = (NSView*)windowId;
    NSWindow* window = [view window];
    window.titlebarAppearsTransparent = YES;
    window.backgroundColor = [NSColor colorWithRed:color.redF() green:color.greenF() blue:color.blueF() alpha:color.alphaF()];

    // Unfortunately there seems to be no easy way to set the titlebar text color.
    // The closest we can do without dubious hacks is set the dark/light mode state based on the brightness of the
    // background color, which should at least make the text readable even if we can't use the theme's text color.
    // It's a good idea to set this anyway since it also affects some other UI elements like text shadows.
    if (color.lightnessF() < 0.5) {
        window.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
    } else {
        window.appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
    }
}

void ThemeManager::setTitlebarColorOfAllWindowsOnMac(QColor color)
{
    NSArray<NSWindow*>* windows = [NSApp windows];
    for (NSWindow* window : windows) {
        setTitlebarColorOnMac((WId)window.contentView, color);
    }

    // We want to change the titlebar color of newly opened windows as well.
    // There's no notification for when a new window is opened, but we can set the color when a window switches
    // from occluded to visible, which also fires on open.
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    stopSettingNewWindowColorsOnMac();
    m_windowTitlebarObserver = [center addObserverForName:NSWindowDidChangeOcclusionStateNotification
                                                   object:nil
                                                    queue:[NSOperationQueue mainQueue]
                                               usingBlock:^(NSNotification* notification) {
                                                   NSWindow* window = notification.object;
                                                   setTitlebarColorOnMac((WId)window.contentView, color);
                                               }];
}

void ThemeManager::stopSettingNewWindowColorsOnMac()
{
    if (m_windowTitlebarObserver) {
        NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
        [center removeObserver:m_windowTitlebarObserver];
        m_windowTitlebarObserver = nil;
    }
}
