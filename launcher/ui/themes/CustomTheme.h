#pragma once

#include <QFileInfo>
#include "ITheme.h"

class CustomTheme : public ITheme {
   public:
    CustomTheme(ITheme* baseTheme, QFileInfo& file, bool isManifest);
    virtual ~CustomTheme() {}

    QString id() override;
    QString name() override;
    QString tooltip() override;
    bool hasStyleSheet() override;
    QString appStyleSheet() override;
    QPalette colorScheme() override;
    double fadeAmount() override;
    QColor fadeColor() override;
    QString qtTheme() override;
    LogColors logColorScheme() override { return m_logColors; }
    QStringList searchPaths() override;

   private:
    bool read(const QString& path, bool& hasCustomLogColors);

    QPalette m_palette;
    QColor m_fadeColor;
    double m_fadeAmount;
    QString m_styleSheet;
    QString m_name;
    QString m_id;
    QString m_widgets;
    QString m_qssFilePath;
    LogColors m_logColors;
    /**
     * The tooltip could be defined in the theme json,
     * or composed of other fields that could be in there.
     * like author, license, etc.
     */
    QString m_tooltip = "";
};
