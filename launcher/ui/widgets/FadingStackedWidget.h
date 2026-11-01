#pragma once

#include <QStackedWidget>

/**
 * A QStackedWidget that cross-fades when the current page changes: the old
 * page is captured, laid over the new one and faded out.
 */
class FadingStackedWidget : public QStackedWidget {
    Q_OBJECT

   public:
    explicit FadingStackedWidget(QWidget* parent = nullptr);

    void setCurrentIndexAnimated(int index);

    int duration() const { return m_duration; }
    void setDuration(int msecs) { m_duration = msecs; }

   private:
    int m_duration = 220;
};
