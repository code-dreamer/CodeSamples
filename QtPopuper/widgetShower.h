#ifndef WIDGETSHOWER__9B403667_26A6_4442_BB2E_ADC80DE2C51E
#define WIDGETSHOWER__9B403667_26A6_4442_BB2E_ADC80DE2C51E

#include <QtCore>

class WidgetShower : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(WidgetShower)

public:
    enum ShowPos {
	Center,
	RightBottom
    };

public:
    explicit WidgetShower(QObject *parent = 0, ShowPos showPos = RightBottom);

protected:
    virtual bool eventFilter(QObject* obj, QEvent* event);

private:
    bool processEvent(QWidget* widget, QEvent* event);
    static void moveWidget(QWidget* const widget, ShowPos showPos);

private:
    ShowPos showPos_;
    bool firstShow_;
};

#endif // WIDGETSHOWER__9B403667_26A6_4442_BB2E_ADC80DE2C51E
