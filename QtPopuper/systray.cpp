#include "stdafx.h"

#include "systray.h"
#include "utils.h"
#include "appSettings.h"
#include "toptalApp.h"

const QByteArray g_availabilityActionUndefined = "Availabiity is not defined";

Systray::Systray(QWidget* mainWnd) :
    QObject(mainWnd),
    trayIcon_(0),
    setAvailabilityAction_(0),
    runWizardAction_(0),
    showRecruitmentsAction_(0),
    wizardActive_(false)
{
    createTrayIcon(mainWnd);
}

Systray::~Systray() {
}

void Systray::createTrayIcon(QWidget* widget) {
    Q_ASSERT(widget != NULL);

    QMenu* trayIconMenu = new QMenu(widget);

    QList<QAction*> actions = createActions(this);
    foreach (QAction* action, actions) {
	if (action != NULL) {
	    trayIconMenu->addAction(action);
	}
	else
	    trayIconMenu->addSeparator();
    }

    if (trayIcon_ != 0) {
	utils::safe_delete(trayIcon_);
    }
    trayIcon_ = new QSystemTrayIcon(this);
    trayIcon_->setContextMenu(trayIconMenu);
    CHECKED_CONNECT(trayIcon_, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SIGNAL(iconActivated(QSystemTrayIcon::ActivationReason)));
    CHECKED_CONNECT(trayIcon_, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(onTrayIconActivated(QSystemTrayIcon::ActivationReason)));

    QIcon icon = AppSettings::mainIcon();
    if ( !icon.isNull() ) {
	trayIcon_->setIcon(icon);

	trayIcon_->setToolTip(qApp->applicationName());
	trayIcon_->show();
    }
    else {
	QMessageBox::critical(0, QLatin1String("icon ton found"), QLatin1String("icon ton found"));
    }
}

QList<QAction*> Systray::createActions(QObject* parent) {
    QList<QAction*> actions;

    runWizardAction_ = new QAction(tr("Run Scheduling Wizard"), parent);
    CHECKED_CONNECT(runWizardAction_, SIGNAL(triggered()), parent, SIGNAL(startWizardClicked()));
    actions.append(runWizardAction_);

    showRecruitmentsAction_ = new QAction(tr("Show Recruitment Opportunities"), parent);
    CHECKED_CONNECT(showRecruitmentsAction_, SIGNAL(triggered()), parent, SIGNAL(showVacanciesClicked()));
    actions.append(showRecruitmentsAction_);

    setAvailabilityAction_ = new QAction(tr(g_availabilityActionUndefined), parent);
    setAvailabilityAction_->setEnabled(false);
    CHECKED_CONNECT(setAvailabilityAction_, SIGNAL(triggered()), parent, SIGNAL(setAvailabilityClicked()));
    actions.append(setAvailabilityAction_);

    notificationsVisibilityAction_ = new QAction(notificationVisibilityText(), parent);
    CHECKED_CONNECT(notificationsVisibilityAction_, SIGNAL(triggered()), parent, SIGNAL(notificationsVisibilityClicked()));
    actions.append(notificationsVisibilityAction_);

    QAction* action = new QAction(tr("Quit"), parent);
    CHECKED_CONNECT(action, SIGNAL(triggered()), parent, SIGNAL(exitClicked()));
    actions.append(action);

    return actions;
}

void Systray::onAvailabilityChanged(User::Availability availability) {
    QString tooltip = qApp->applicationName() + QLatin1String("\nCurrent Availability: ");

    const QIcon icon = AppSettings::availabilityIcon(availability);

    if (!icon.isNull()) {
	tooltip += User::availabilityToHumanStr(availability);
	trayIcon_->setIcon(icon);
	trayIcon_->setToolTip(tooltip);
    }

    formateAvailabilityAction(setAvailabilityAction_);
}

QAction* Systray::formateAvailabilityAction(QAction* action) {
    Q_ASSERT(action != 0);

    const User* currUser = qTopTalApp->settings()->user();
    const User::Availability currentAvailability = currUser->availability();
    const QString strAvailablity = User::availabilityToHumanStr(currentAvailability);
    if (!strAvailablity.isEmpty()) {
	const QString menuText = QString(QLatin1String("Current Availability: %1 (change)...")).arg(strAvailablity);
	action->setText(menuText);
    }

    const QIcon availabilityIcon = AppSettings::availabilityIcon(currentAvailability);
    if (!availabilityIcon.isNull()) {
	action->setIcon(availabilityIcon);
    }

    //action->setEnabled(!wizardActive_);

    return action;
}

void Systray::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason != QSystemTrayIcon::Context && reason != QSystemTrayIcon::Trigger) {
	return;
    }

    bool windowvisible = false;
    QWidgetList topLevelWidgets = QApplication::topLevelWidgets();
    foreach (QWidget* widget, topLevelWidgets) {
	if (widget->isVisible() && !widget->objectName().isEmpty()) {
	    windowvisible = true;
	    break;
	}
    }

    runWizardAction_->setEnabled(!windowvisible);
    showRecruitmentsAction_->setEnabled(!windowvisible);

    const int jobsCount = qTopTalApp->settings()->jobsCount();
    if (jobsCount == 0) {
	showRecruitmentsAction_->setEnabled( !(jobsCount == 0) );
    }

    if (setAvailabilityAction_ != 0) {
	if (setAvailabilityAction_->text() != QLatin1String(g_availabilityActionUndefined)) {
	    formateAvailabilityAction(setAvailabilityAction_);
	    setAvailabilityAction_->setEnabled(!windowvisible);
	}
    }

    notificationsVisibilityAction_->setText( notificationVisibilityText() );
}

QString Systray::notificationVisibilityText() {
    const bool showNotifications = qTopTalApp->settings()->showNotifications();
    return (showNotifications ? QLatin1String("Hide notifications") : QLatin1String("Show notifications"));
}

void Systray::onWizardStarted(bool started) {
    wizardActive_ = started;
    /*setAvailabilityAction_->setEnabled(!wizardActive_);
    runWizardAction_->setEnabled(!wizardActive_);
    showRecruitmentsAction_->setEnabled(!wizardActive_);*/
}

void Systray::onNotificationVisibilityChanged(bool visible) {
    wizardActive_ = visible;
    /*setAvailabilityAction_->setEnabled(!wizardActive_);
    runWizardAction_->setEnabled(!wizardActive_);
    showRecruitmentsAction_->setEnabled(!wizardActive_);*/
}
