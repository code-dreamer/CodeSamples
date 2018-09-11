#ifndef WIDGETROUNDERER__C90F0B73_EDB7_4625_A13F_325423A7E473
#define WIDGETROUNDERER__C90F0B73_EDB7_4625_A13F_325423A7E473

#include <QtCore>

// incapsulate window with round corners

class WidgetRounderer : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(WidgetRounderer)

public:
    explicit WidgetRounderer(QObject *parent = 0);

signals:

protected:
    virtual bool eventFilter(QObject* obj, QEvent* event);

private:
    bool processEvent(QWidget* widget, QEvent* event) const;

private:
    static QPolygon createInnerRoundedPolygon(const QRect& boundedRect); //TODO: make method more flexible.

public slots:

};

#endif // WIDGETROUNDERER__C90F0B73_EDB7_4625_A13F_325423A7E473
