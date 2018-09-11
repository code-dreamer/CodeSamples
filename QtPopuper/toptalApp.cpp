#include "stdafx.h"

#include "toptalApp.h"
#include "appInfo.h"
#include "director.h"
#include "mainWindow.h"
#include "utils.h"
#include "appSettings.h"
#include "loginWindow.h"
#include "notificationWidget.h"

TopTalApp::TopTalApp(int &argc, char **argv) :
    QApplication(argc, argv),
    mainWnd_(NULL),
    appSettings_(NULL)
{
    setAppInfo();
    setQuitOnLastWindowClosed(false);
    appSettings_ = new AppSettings(this);

    mainWnd_ = new MainWindow();

    new Director(mainWnd_);

    emit appInit();
}

TopTalApp::~TopTalApp() {
    utils::safe_delete(mainWnd_);
}

void TopTalApp::setAppInfo() {
    setOrganizationDomain(QLatin1String(AppInfo::g_productHomePageUrl));
    setOrganizationName(QLatin1String(AppInfo::g_organizationName));
    setApplicationName(QLatin1String(AppInfo::g_productName));

    QString appVersion = QString(QLatin1String("%1.%2.%3"))
		.arg(QLatin1String(AppInfo::g_majorVersion))
		.arg(QLatin1String(AppInfo::g_minorVersion))
		.arg(QLatin1String(AppInfo::g_buildVersion));
    setApplicationVersion(appVersion);
}


AppSettings* TopTalApp::settings() {
    return appSettings_;
}
