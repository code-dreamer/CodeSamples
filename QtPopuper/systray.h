#ifndef SYSTRAY__6599D039_B7C5_4544_9D30_CA5CA640CC9F
#define SYSTRAY__6599D039_B7C5_4544_9D30_CA5CA640CC9F

#include <QtCore>
#include <QtGui>

class MyAction;

class Systray : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Systray)

public:
    explicit Systray(QWidget* mainWnd);
    virtual ~Systray();

signals:
    void exitClicked();
    void iconActivated(QSystemTrayIcon::ActivationReason);
    void startWizardClicked();
    void setAvailabilityClicked();
    void showVacanciesClicked();
    void notificationsVisibilityClicked();

public slots:
    void onAvailabilityChanged(User::Availability availability);
    void onWizardStarted(bool started);
    void onNotificationVisibilityChanged(bool visible);

private slots:
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void createTrayIcon(QWidget* widget);
    QList<QAction*> createActions(QObject* parent);
    QAction* formateAvailabilityAction(QAction* action);
    static QString notificationVisibilityText();

private:
    QSystemTrayIcon* trayIcon_;
    QAction* setAvailabilityAction_;
    QAction* runWizardAction_;
    QAction* showRecruitmentsAction_;
    QAction* notificationsVisibilityAction_;

private:
    bool wizardActive_;
};

#endif // SYSTRAY__6599D039_B7C5_4544_9D30_CA5CA640CC9F
