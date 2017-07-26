#ifndef MDRLOADER_H
#define MDRLOADER_H

#include "hex.h"
#include "mcudescriptor.h"

#include <QObject>
#include <QSerialPort>
#include <QFile>
#include <QString>
#include <QMetaType>

class MdrLoader : public QObject
{
    Q_OBJECT
    
public:
    enum WorkTypeFlag {
        NoWork =  0,
        Load =    1,
        Erase =   2,
        Program = 4,
        Verify =  8,
        Run =    16
    };
    Q_DECLARE_FLAGS(WorkType, WorkTypeFlag)
    Q_FLAG(WorkType)
    
    explicit MdrLoader(QObject *parent = 0);
    void setFirmwareFilename(QString filename) { fwfn = filename; }
    void setFlasherFilename(QString filename) { flasherfn = filename; }
    void setDumpFilename(QString filename) { dumpfn = filename; }
    void setPortName(QString portname) { this->portname = portname; }
    void setBaudRate(QSerialPort::BaudRate baudrate){this->baudrate = baudrate;}
    void setMcuType(Mcu::Type type) { mcuType = type; }
    bool doWork(WorkType work);
    
signals:
    void dumpFilenameChanged(QString dumpfn);
    void statusChanged(QString s);
    void textChanged(QString s);
    void stopButtonSetEnable(bool);
    void workFinished();
    
public slots:
    void stopWork() { stop = true; }
    
private:
    bool checkAndLoadFiles();
    bool checkConnect();
    bool dump();
    bool erase();
    bool program();
    bool verify();
    bool runFW();
    Hex *flasher, *firmware; // TODO: создавать и удалять их динамически, как dumpFile
    QSerialPort *port;
    WorkType work;
    bool stop;
    QFile *dumpFile;
    QString fwfn, dumpfn, flasherfn, portname;
    QSerialPort::BaudRate baudrate;
    Mcu::Type mcuType;
};

#endif // MDRLOADER_H
