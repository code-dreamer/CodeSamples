#ifndef DIRECTOR__1CF6A219_B68F_4022_8F9E_30842F4F1CFA
#define DIRECTOR__1CF6A219_B68F_4022_8F9E_30842F4F1CFA

#include "server.h"
#include <QSystemTrayIcon>

class Server;
class LoginWindow;
class Systray;
class AvailabilityWidget;
class NotificationWidget;
class PopupProcessor;

class Director : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Director)

private:
    enum State {

    };

public:
    explicit Director(QWidget* mainWnd);
    ~Director();

    void createObjects();
    void initConnections();

signals:
    void wizardStarted(bool started);

protected:
    virtual bool eventFilter(QObject* obj, QEvent* event);

private slots:
    void onAppInit();
    void onClose();
    void onServerRequestResponse(ServerRequest* request);
    void onJobsTimer();
    void onPingTimer();
    void onLoginTimer();
    void jobAppended(Job job);

    void startWizard();
    void startWizardIfNeeded();

    void onIconActivated(QSystemTrayIcon::ActivationReason reason);

    void onSetAvailabilityClicked();
    void onShowVacanciesClicked();

    void onNotificationsVisibilityClicked();

private:
    void sendUserInfo(User* data);
    bool userDateToOld() const;

private:
    QWidget* mainWnd_;
    Systray* systray_;
    LoginWindow* loginWnd_;
    AvailabilityWidget* availWidget_;
    Server* server_;
    QTimer* jobsTimer_;
    QTimer* pingTimer_;
    QTimer* loginTimer_;
    PopupProcessor* popupProcessor_;

    bool closing_;
    bool userRegistrationActive_;
    bool changeAvailabilityActive_;
};

#endif // DIRECTOR__1CF6A219_B68F_4022_8F9E_30842F4F1CFA
