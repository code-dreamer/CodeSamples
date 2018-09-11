#include "stdafx.h"

#include "widgetShower.h"
#include "windowHelpers.h"
#include "utils.h"

const int g_visibleAnimateTime = 630;

WidgetShower::WidgetShower(QObject *parent, ShowPos showPos) :
    QObject(parent),
    showPos_(showPos),
    firstShow_(true)
{
}

bool WidgetShower::eventFilter(QObject* obj, QEvent* event) {
    bool processed = false;
    QWidget* widget = qobject_cast<QWidget*>(obj);
    if (widget != 0) {
	if (widget->isWindow() && !widget->isFullScreen() && !widget->isMaximized()) {
	    processed = processEvent(widget, event);
	}
    }

    return processed;
}

bool WidgetShower::processEvent(QWidget* widget, QEvent* event) {
    Q_ASSERT(widget != 0);
    Q_ASSERT(event != 0);

    if (event->type() == QEvent::Show) {
	if (firstShow_) {
	    firstShow_ = false;
	    WidgetShower::moveWidget(widget, showPos_);
	}
    }

    return false;
}



void WidgetShower::moveWidget(QWidget* const widget, ShowPos showPos) {
    if (showPos == Center) {
	windowHelpers::centerOnScreen(widget);
    }
    else if (showPos == RightBottom) {
	windowHelpers::moveToRightBottom(widget);
    }

    QPoint p = widget->geometry().topLeft();
    const QPoint topLeft = widget->mapToGlobal( widget->geometry().topLeft() );

    windowHelpers::moveUderAllAppWindows(widget);
}
