#include "stdafx.h"

#include "director.h"
#include "qt_prec.h"
#include "toptalApp.h"
#include "server.h"
#include "loginWindow.h"
#include "utils.h"
#include "server.h"
#include "loginWindow.h"
#include "systray.h"
#include "appSettings.h"
#include "availabilityWidget.h"
#include "notificationWidget.h"
#include "ServerRequests.h"
#include "popupProcessor.h"
#include "windowHelpers.h"

const int lastVacanciesShowCount = 1;
const int pingTimeout = 5000;
const int jobsTimeout = 300000;	//5 min.
const int loginTimeout = 3600000;   //1 hour

const int dayOfWeekForRelogin = Qt::Monday;

Director::Director(QWidget* mainWnd) :
    QObject(mainWnd),
    mainWnd_(mainWnd),
    systray_(0),
    loginWnd_(0),
    availWidget_(0),
    server_(NULL),
    jobsTimer_(0),
    pingTimer_(0),
    loginTimer_(0),
    popupProcessor_(0),
    closing_(false),
    userRegistrationActive_(false),
    changeAvailabilityActive_(false)
{
    CHECKED_CONNECT_EX(qTopTalApp, SIGNAL(appInit()), this, SLOT(onAppInit()), Qt::QueuedConnection);
}

Director::~Director() {
    delete loginWnd_;
    delete availWidget_;
}

void Director::onClose() {
    closing_ = true;

    server_->shutdown();

    loginWnd_->close();
    availWidget_->close();
    popupProcessor_->tryStop();
    jobsTimer_->stop();
    pingTimer_->stop();
    loginTimer_->stop();

    qTopTalApp->settings()->saveAll();

    QCoreApplication::processEvents();
    qTopTalApp->exit();
}

void Director::createObjects() {
    systray_ = new Systray(mainWnd_);
    server_ = new Server(this);

    loginWnd_ = new LoginWindow(NULL);
    loginWnd_->installEventFilter(this);

    availWidget_ = new AvailabilityWidget(NULL);
    availWidget_->installEventFilter(this);

    popupProcessor_ = new PopupProcessor(this);

    jobsTimer_ = new QTimer(this);
    CHECKED_CONNECT(jobsTimer_, SIGNAL(timeout()), this, SLOT(onJobsTimer()));
    jobsTimer_->start(jobsTimeout);

    pingTimer_ = new QTimer(this);
    CHECKED_CONNECT(pingTimer_, SIGNAL(timeout()), this, SLOT(onPingTimer()));
    pingTimer_->start(pingTimeout);

    loginTimer_ = new QTimer(this);
    CHECKED_CONNECT(loginTimer_, SIGNAL(timeout()), this, SLOT(onLoginTimer()));
    loginTimer_->start(loginTimeout);
}

void Director::onAppInit() {
    createObjects();
    initConnections();

    qTopTalApp->settings()->readAll();

    QTimer::singleShot(10, this, SLOT(startWizardIfNeeded()));
}

void Director::startWizard() {
    emit wizardStarted(true);
    userRegistrationActive_ = true;

    if (qTopTalApp->settings()->isFirstStart()) {
	loginWnd_->setWaitingState(true);
	qApp->setActiveWindow(loginWnd_);
	loginWnd_->show();
    }
    else {
	User* user = qTopTalApp->settings()->user();
	if (!user->loggedIn()) {
	    if (!user->login().isEmpty())
		sendUserInfo(user);
	    else {
		loginWnd_->setWaitingState(true);
		loginWnd_->show();
	    }
	}
	else {
	    availWidget_->setAvailabilityType(User::AvailabilityOnThisWeek);
	    qApp->setActiveWindow(availWidget_);
	    availWidget_->show();
	}
    }
}

void Director::initConnections() {
    CHECKED_CONNECT(systray_, SIGNAL(exitClicked()), this, SLOT(onClose()));
    CHECKED_CONNECT(systray_, SIGNAL(iconActivated(QSystemTrayIcon::ActivationReason)), this, SLOT(onIconActivated(QSystemTrayIcon::ActivationReason)));
    CHECKED_CONNECT(systray_, SIGNAL(startWizardClicked()), this, SLOT(startWizard()));
    CHECKED_CONNECT(systray_, SIGNAL(setAvailabilityClicked()), this, SLOT(onSetAvailabilityClicked()));
    CHECKED_CONNECT(systray_, SIGNAL(showVacanciesClicked()), this, SLOT(onShowVacanciesClicked()));
    CHECKED_CONNECT(systray_, SIGNAL(notificationsVisibilityClicked()), this, SLOT(onNotificationsVisibilityClicked()));
    CHECKED_CONNECT(this, SIGNAL(wizardStarted(bool)), systray_, SLOT(onWizardStarted(bool)));
    CHECKED_CONNECT(popupProcessor_, SIGNAL(notificationVisibilityChanged(bool)), systray_, SLOT(onNotificationVisibilityChanged(bool)));

    CHECKED_CONNECT_EX(server_, SIGNAL(requestResponse(ServerRequest*)), this, SLOT(onServerRequestResponse(ServerRequest*)), Qt::QueuedConnection);

    CHECKED_CONNECT(qTopTalApp->settings(), SIGNAL(jobAppended(Job)), this, SLOT(jobAppended(Job)));

    User* user = qTopTalApp->settings()->user();
    CHECKED_CONNECT(user, SIGNAL(availabilityChanged(User::Availability)), systray_, SLOT(onAvailabilityChanged(User::Availability)));
}

void Director::onServerRequestResponse(ServerRequest* request) {
    Q_ASSERT(request != NULL);

    if (closing_) {
	return;
    }

    if (request->isError()) {
	QMessageBox::critical(0, tr("Server Request Failed"), tr("Responce from server: '%1'").arg(request->error()));
    }

    if (request->type() == Requests::partnersRequest) {
	PartnersRequest* partnersRequest = static_cast<PartnersRequest*>(request);
	const QList<Partner>& partners = partnersRequest->getPartners(); //TODO: move partners to settings
	qTopTalApp->settings()->setPartners(partners);

	loginWnd_->showInfo(tr("Data received"));
	loginWnd_->setWaitingState(false);
	loginWnd_->setPartners(partners);
    }
    else if (request->type() == Requests::loginRequest) {
	LoginRequest* loginRequest = static_cast<LoginRequest*>(request);
	User* user = loginRequest->user();
	if (!user->loggedIn()) {
	    QMessageBox::critical(0, tr("Login failed"), tr("You've entered:\nLogin: '%1'\nPassword: '%2'\nRequest failed. Please, enter correct username and password.").arg(user->login()).arg(user->password()));
	    qApp->setActiveWindow(loginWnd_);
	    loginWnd_->show();
	}
	else {
	    qTopTalApp->settings()->writeUserToSettings();

	    popupProcessor_->syncShowNotification(Notification(tr("Login success"), tr("You logged in")), 1000);

	    startWizardIfNeeded();
	    user->updateLoginDate();
	}
    }
    else if (request->type() == Requests::availabilityRequest) {
	AvailabilityRequest* availRequest = static_cast<AvailabilityRequest*>(request);
	User::AvailabilityType availType = availRequest->availabilityType();

	if (availType == User::AvailabilityOnNextMonth) {
	    qTopTalApp->settings()->saveAll();
	}
	else {
	    if (availType == User::AvailabilityOnThisWeek) {
		availType = User::AvailabilityOnNextWeek;
		if (!request->isError()) {
		    User::Availability availability = availRequest->availability();
		    qTopTalApp->settings()->user()->setAvailability(availability);
		}
	    }
	    else if (availType == User::AvailabilityOnNextWeek) {
		availType = User::AvailabilityOnNextMonth;
	    }

	    availWidget_->setAvailabilityType(availType);
	    qApp->setActiveWindow(availWidget_);
	    availWidget_->show();
	}
    }
    else if (request->type() == Requests::jobsRequest) {
	JobsRequest* jobsRequest = static_cast<JobsRequest*>(request);
	qTopTalApp->settings()->appendJobs(jobsRequest->jobs());
    }
}

bool Director::eventFilter(QObject* obj, QEvent* event) {
    if (closing_) {
	return false;
    }

    if (obj == loginWnd_) {
	if (event->type() == QEvent::Show) {
	    loginWnd_->showInfo(tr("Waiting data from server..."));

	    ServerRequest* request = server_->createRequest(Requests::partnersRequest);
	    server_->sendRequest(request);
	}
	else if (event->type() == QEvent::Close) {
	    User* user = qTopTalApp->settings()->user();
	    sendUserInfo(loginWnd_->formateUser(user));
	}

	return false;
    }

    if (obj == availWidget_ && event->type() == QEvent::Close) {
	AvailabilityRequest* request = static_cast<AvailabilityRequest*>( server_->createRequest(Requests::availabilityRequest) );
	request->setAvailability(availWidget_->availability());
	request->setAvailabilityType(availWidget_->availabilityType());

	User::AvailabilityType availType = request->availabilityType();

	request->setAvailabilityType(availType);
	server_->sendRequest(request);

	if (availType == User::AvailabilityOnNextMonth) {
	    userRegistrationActive_ = false;

	    QTimer::singleShot(10, this, SLOT(onJobsTimer()));

	    emit wizardStarted(false);
	}

	return false;
    }

    return false;
}

void Director::onJobsTimer() {
    if ( !qTopTalApp->settings()->user()->loggedIn())
	return;

    if (!userRegistrationActive_) {
	ServerRequest* request = server_->createRequest(Requests::jobsRequest);
	server_->sendRequest(request);
    }
}

void Director::onPingTimer() {
    const User* user = qTopTalApp->settings()->user();
    if (user->loggedIn()) {
	ServerRequest* request = server_->createRequest(Requests::pingRequest);
	server_->sendRequest(request);
    }
}

void Director::onLoginTimer() {
    if (!userRegistrationActive_) {
	startWizardIfNeeded();
    }
}

void Director::startWizardIfNeeded() {
    User* user = qTopTalApp->settings()->user();
    if (userDateToOld() || !user->loggedIn()) {
	startWizard();
    }
    else {
	userRegistrationActive_ = false;
    }
}

bool Director::userDateToOld() const {
    bool tooOld = false;

    const QDate lastLoginDate = qTopTalApp->settings()->user()->lastLoginDateTime().date();
    tooOld = !lastLoginDate.isValid();
    if (lastLoginDate.isValid()) {
	const QDate currentDate = QDateTime::currentDateTime().date();
	QDate reloginDate = currentDate;
	while (reloginDate.dayOfWeek() != dayOfWeekForRelogin) {
	    reloginDate = reloginDate.addDays(-1);
	}

	if (lastLoginDate < reloginDate && lastLoginDate != currentDate) {
	    tooOld = true;
	}
    }

    return tooOld;
}

void Director::sendUserInfo(User* user) {
    LoginRequest* loginRequest = static_cast<LoginRequest*>( server_->createRequest(Requests::loginRequest) );
    loginRequest->setUser(user);
    server_->sendRequest(loginRequest);
}

void Director::jobAppended(Job job) {
    popupProcessor_->appendNotification(Notification(tr("A new job!"), job.description, job.url.toString(), job.title));
}

void Director::onIconActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::DoubleClick) {
	QWidget* activeWindow = qApp->activeWindow();
	if (activeWindow != 0) {
	    activeWindow->raise();
	}
    }
}

void Director::onSetAvailabilityClicked() {
    changeAvailabilityActive_ = true;
    availWidget_->setAvailabilityType(User::AvailabilityOnThisWeek);
    qApp->setActiveWindow(availWidget_);
    availWidget_->show();
}

void Director::onShowVacanciesClicked() {
    const int jobsCount = qTopTalApp->settings()->jobsCount();
    int lastInd = jobsCount - lastVacanciesShowCount;
    if (lastInd < 0) {
	lastInd = 0;
    }
    for (int i = jobsCount-1; i >= lastInd; --i) {
	const Job job = qTopTalApp->settings()->job(i);
	popupProcessor_->appendNotification(Notification(tr("A new job!"), job.description, job.url.toString(), job.title));
    }
}

void Director::onNotificationsVisibilityClicked() {
    bool showNotifications = qTopTalApp->settings()->showNotifications();
    showNotifications = !showNotifications;
    qTopTalApp->settings()->setShowNotifications(showNotifications);
}
