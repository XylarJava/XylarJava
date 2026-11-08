#include "ITheme.h"
#include <QDir>
#include <QStyleFactory>
#include "Application.h"
#include "HintOverrideProxyStyle.h"
#include "rainbow.h"

void ITheme::apply(bool)
{
    APPLICATION->setStyleSheet(QString());
    QApplication::setStyle(new HintOverrideProxyStyle(QStyleFactory::create(qtTheme())));
    QApplication::setPalette(colorScheme());
    QDir::setSearchPaths("theme", searchPaths());
    APPLICATION->setStyleSheet(appStyleSheet());
}

QPalette ITheme::fadeInactive(QPalette in, qreal bias, QColor color)
{
    auto blend = [&in, bias, color](QPalette::ColorRole role) {
        QColor from = in.color(QPalette::Active, role);
        QColor blended = Rainbow::mix(from, color, bias);
        in.setColor(QPalette::Disabled, role, blended);
    };
    blend(QPalette::Window);
    blend(QPalette::WindowText);
    blend(QPalette::Base);
    blend(QPalette::AlternateBase);
    blend(QPalette::ToolTipBase);
    blend(QPalette::ToolTipText);
    blend(QPalette::Text);
    blend(QPalette::Button);
    blend(QPalette::ButtonText);
    blend(QPalette::BrightText);
    blend(QPalette::Link);
    blend(QPalette::Highlight);
    blend(QPalette::HighlightedText);
    return in;
}

LogColors ITheme::defaultLogColors(const QPalette& palette)
{
    LogColors result;

    const QColor& bg = palette.color(QPalette::Base);
    const QColor& fg = palette.color(QPalette::Text);

    auto blend = [bg, fg](QColor color) {
        if (Rainbow::luma(fg) > Rainbow::luma(bg)) {
            // for dark color schemes, produce a fitting color first
            color = Rainbow::tint(fg, color, 0.5);
        }
        // adapt contrast
        return Rainbow::mix(fg, color, 1);
    };

    result.background[MessageLevel::Fatal] = Qt::black;

    result.foreground[MessageLevel::Launcher] = blend(QColor("purple"));
    result.foreground[MessageLevel::Debug] = blend(QColor("green"));
    result.foreground[MessageLevel::Warning] = blend(QColor("orange"));
    result.foreground[MessageLevel::Error] = blend(QColor("red"));
    result.foreground[MessageLevel::Fatal] = blend(QColor("red"));

    return result;
}
