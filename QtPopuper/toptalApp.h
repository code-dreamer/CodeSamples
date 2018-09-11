#ifndef TOPTALAPP__236EFA48_D084_4B0E_9951_8B7F9811F60F
#define TOPTALAPP__236EFA48_D084_4B0E_9951_8B7F9811F60F

#include <QtCore>
#include <QtGui>

#define qTopTalApp (static_cast<TopTalApp *>(QCoreApplication::instance()))

class MainWindow;
class AppSettings;
class Server;

class TopTalApp : public QApplication
{
    Q_OBJECT
    Q_DISABLE_COPY(TopTalApp)

public:
    TopTalApp(int &argc, char **argv);
    virtual ~TopTalApp();

public:
    AppSettings* settings();

signals:
    void appInit();

private:
    void setAppInfo();

private:
    MainWindow* mainWnd_;
    AppSettings* appSettings_;
};

#endif // TOPTALAPP__236EFA48_D084_4B0E_9951_8B7F9811F60F
