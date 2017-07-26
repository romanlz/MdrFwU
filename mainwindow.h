#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "mdrloader.h"
#include <QThread>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    
signals:
    void operate(MdrLoader::WorkType work);
        
private slots:
    void on_openFilePB_clicked();
    void on_touchMCUPB_clicked();
    void on_loadCB_clicked(bool checked);
    void on_dumpfnPB_clicked();
    void loaderFinished();
    void on_flasherfnPB_clicked();
    void on_action_Qt_triggered();
    void on_about_triggered();
    
private:
    void closeEvent(QCloseEvent *event);
    void writeSettings();
    void readSettings();
    Ui::MainWindow *ui;
    MdrLoader *mdrLoader;
    QThread loaderThread;
};

#endif // MAINWINDOW_H
