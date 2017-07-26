#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "bytearray.h"

#include <QFileDialog>
#include <QDebug>
#include <QTextStream>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QThread>
#include <QCompleter>
#include <QFileSystemModel>
#include <QMessageBox>
#include <QSettings>

bool lessThan(QSerialPortInfo i1, QSerialPortInfo i2)
{
    return i1.portName() < i2.portName();
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    mdrLoader(new MdrLoader)
{
    ui->setupUi(this);
    QList<QSerialPortInfo> list;
    QSerialPortInfo i;
    list.append(QSerialPortInfo::availablePorts());
    qSort(list.begin(), list.end(), lessThan);
    foreach(i, list) {
        ui->portNameCB->addItem(i.portName().append(" = ")+i.description(),
                                 QVariant(i.systemLocation()));
    }
    mdrLoader->moveToThread(&loaderThread);
    connect(&loaderThread, &QThread::finished, mdrLoader, &QObject::deleteLater);
    connect(this, &MainWindow::operate, mdrLoader, &MdrLoader::doWork);
    loaderThread.start();
    
    connect(mdrLoader, SIGNAL(statusChanged(QString)), ui->sL, SLOT(setText(QString)));
    connect(mdrLoader, SIGNAL(textChanged(QString)), ui->t, SLOT(appendPlainText(QString)));
    connect(mdrLoader, SIGNAL(dumpFilenameChanged(QString)), ui->dumpfnLE, SLOT(setText(QString)));
    connect(mdrLoader, SIGNAL(stopButtonSetEnable(bool)), ui->touchMCUPB, SLOT(setEnabled(bool)));
    connect(mdrLoader, SIGNAL(workFinished()), SLOT(loaderFinished()));
    
    readSettings();
}

MainWindow::~MainWindow()
{
    loaderThread.quit();
    loaderThread.wait();
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (true /*userReallyWantsToQuit()*/ ) {
        writeSettings();
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::on_touchMCUPB_clicked()
{
    bool ok;
    
    if(ui->touchMCUPB->isChecked() == false) {
        mdrLoader->stopWork();
        return;
    }
    ui->touchMCUPB->setText("STOP!");
    ui->touchMCUPB->setDisabled(true);
    ui->t->clear();

    mdrLoader->setPortName(ui->portNameCB->currentData().toString());
    mdrLoader->setBaudRate((QSerialPort::BaudRate)ui->portBaudrateCB->currentText().toUInt(&ok));
    mdrLoader->setDumpFilename(ui->dumpfnLE->text());
    mdrLoader->setFirmwareFilename(ui->fwfnLE->text());
    mdrLoader->setFlasherFilename(ui->flasherfnLE->text());
    Mcu::Type mcuType = Mcu::Undef;
    if(ui->mcu9xRB->isChecked()) mcuType = Mcu::BE9X;
    if(ui->mcu1RB->isChecked()) mcuType = Mcu::BE1;
    if(ui->mcu4RB->isChecked()) mcuType = Mcu::BE4;
    mdrLoader->setMcuType(mcuType);

    MdrLoader::WorkType work = MdrLoader::NoWork;
    if(ui->loadCB->isChecked())    work |= MdrLoader::Load;
    if(ui->eraseCB->isChecked())   work |= MdrLoader::Erase;
    if(ui->programCB->isChecked()) work |= MdrLoader::Program;
    if(ui->verifyCB->isChecked())  work |= MdrLoader::Verify;
    if(ui->runCB->isChecked())     work |= MdrLoader::Run;

    emit operate(work);
}

void MainWindow::loaderFinished()
{
    ui->touchMCUPB->setChecked(false);
    ui->touchMCUPB->setText("Start");
    ui->touchMCUPB->setEnabled(true);
}

void MainWindow::on_loadCB_clicked(bool checked)
{
    ui->dumpfnLE->setEnabled(checked);
    ui->dumpfnPB->setEnabled(checked);
}

void MainWindow::on_openFilePB_clicked()
{
    QString fn = QFileDialog::getOpenFileName(this,
                                              tr("Файл прошивки"),
                                              ui->fwfnLE->text(),
                                              tr("*.hex"));
    ui->fwfnLE->setText(QDir::toNativeSeparators(fn));
}

void MainWindow::on_dumpfnPB_clicked()
{
    QString fn = QFileDialog::getSaveFileName(this,
                                              tr("Файл дампа"),
                                              ui->dumpfnLE->text()/*,
                                              tr("*.hex")*/);
    ui->dumpfnLE->setText(QDir::toNativeSeparators(fn));
}

void MainWindow::on_flasherfnPB_clicked()
{
    QString fn = QFileDialog::getOpenFileName(this,
                                              tr("Файл флешера"),
                                              ui->dumpfnLE->text()/*,
                                              tr("*.hex")*/);
    ui->flasherfnLE->setText(QDir::toNativeSeparators(fn));
}

void MainWindow::on_action_Qt_triggered()
{
    QMessageBox::aboutQt(this);
}

void MainWindow::on_about_triggered()
{
    QString text("Эта версия программы во многом аналогична программам от vasili (forum.milandr.ru), ");
    text.append("однако имеются и определённые различия:\n");
    text.append("- пытается быть чуть более информативной\n");
    text.append("- проверяет наличие флешера в ОЗУ МК перед синхронизацией\n");
    text.append("- какие-то мелочи (если они важны и существенны - дайте знать)\n");
    text.append("Планируется поддержка всех или почти всех МК Миландр.\n");
    text.append("Планируется консольная версия "
                "(точнее совмещение в одном файле консольной и графической версии).\n");
    text.append("Доработать загрузчик для возможности частичного стирания основной памяти, "
                "работы с информационной памятью.\n");
    text.append("Предложения и замечания принимаются и приветствуются в личку на форуме "
                "forum.milandr.ru пользователю prostoRoman или на prostoRoman@gmail.com\n");
    text.append("\nЛицензия, исходники позже. Информация здесь может быть уже не актуальна ;)\n");
    text.append("25.07.2017 Роман");
    QMessageBox mb(QMessageBox::Information,
                   tr("О программе ").append(windowTitle()),
                   text,
                   QMessageBox::Ok,
                   this
                   );
    mb.exec();
}


void MainWindow::writeSettings()
{
    QSettings settings;
    //qDebug()<<"sett file"<<settings.fileName();
    settings.beginGroup("MainWindow");
    settings.setValue("size", size());
    settings.setValue("pos", pos());
    settings.setValue("fwfn", ui->fwfnLE->text());
    settings.setValue("flasherfn", ui->flasherfnLE->text());
    settings.setValue("be9", ui->mcu9xRB->isChecked());
    settings.setValue("be1", ui->mcu1RB->isChecked());
    settings.setValue("be4", ui->mcu4RB->isChecked());
    settings.setValue("load", ui->loadCB->isChecked());
    settings.setValue("erase", ui->eraseCB->isChecked());
    settings.setValue("program", ui->programCB->isChecked());
    settings.setValue("verify", ui->verifyCB->isChecked());
    settings.setValue("run", ui->runCB->isChecked());
    settings.setValue("baudrate", ui->portBaudrateCB->currentText().toUInt());
    settings.setValue("portname", ui->portNameCB->currentData().toString());
    settings.endGroup();
}

void MainWindow::readSettings()
{
    QSettings settings;
    //qDebug()<<"sett file"<<settings.fileName();
    settings.beginGroup("MainWindow");
    resize(settings.value("size", QSize(720, 640)).toSize());
    move(settings.value("pos", QPoint(200, 200)).toPoint());
    ui->fwfnLE->setText(settings.value("fwfn", qApp->applicationDirPath()).toString());
    ui->flasherfnLE->setText(settings.value("flasherfn").toString());
    ui->mcu9xRB->setChecked(settings.value("be9", true).toBool());
    ui->mcu1RB->setChecked(settings.value("be1", false).toBool());
    ui->mcu4RB->setChecked(settings.value("be4", false).toBool());
    ui->loadCB->setChecked(settings.value("load", false).toBool());
    ui->eraseCB->setChecked(settings.value("erase", true).toBool());
    ui->programCB->setChecked(settings.value("program", true).toBool());
    ui->verifyCB->setChecked(settings.value("verify", true).toBool());
    ui->runCB->setChecked(settings.value("run", true).toBool());
    //qDebug()<<"sett baud ="<<settings.value("baudrate", 115200).toUInt();
    //qDebug()<<"sett find ="<<ui->portBaudrateCB->findText(settings.value("baudrate", 115200).toString());
    ui->portBaudrateCB->setCurrentIndex(ui->portBaudrateCB->findText(
                                            settings.value("baudrate", 115200).toString()));
    ui->portNameCB->setCurrentIndex(ui->portNameCB->findData(
                                            settings.value("portname", "\\\\.\\COM1").toString()));
    settings.endGroup();
}
