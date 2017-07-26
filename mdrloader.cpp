#include "mdrloader.h"
#include "bytearray.h"

#include <QDebug>
#include <QThread>

#define MAX_CONNECTION_TRY 50
#define SYNC_FRAME_LENGTH  20

#define CMD_CR 0x0D

MdrLoader::MdrLoader(QObject *parent) : QObject(parent),
    flasher(new Hex), firmware(new Hex), 
    work(NoWork), stop(false), dumpFile(0), mcuType(Mcu::Undef)
{
    Q_INIT_RESOURCE(mdrloader);
}

bool MdrLoader::checkAndLoadFiles()
{
    int errCode;
    
    if(work&Program || work&Verify ) {
        if( !QFile::exists(fwfn) ) {
            emit textChanged(tr("Файл прошивки не найден."));
            return false;
        }
        errCode = firmware->openHexFile(fwfn);
        if(Hex::EOF_DETECTED != errCode) {
            emit textChanged(tr("Невозможно открыть файл прошивки, код ошибки %1, строка %2.")
                             .arg(errCode)
                             .arg(firmware->stringNumber()));
            return false;
        }
        if( (firmware->baseAdress() & 0xFFU) != 0 ) {
            emit textChanged(tr("Адрес начала записываемого кода должен быть кратен 256 байт! 0x%1")
                             .arg(firmware->baseAdress(), 8, 16, QChar('0')));
            return false;
        }
        if( (firmware->baseAdress() < Mcu::mem[mcuType].flashAdr) || 
            (firmware->baseAdress() + firmware->sizeData() >=
             Mcu::mem[mcuType].flashAdr + Mcu::mem[mcuType].flashSize) ) {
            emit textChanged(tr("Загружаемая программа выходит за пределы диапазона flash-памяти! "
                                "Начинается 0x%1, заканчивается 0x%2")
                             .arg(firmware->baseAdress(), 8, 16, QChar('0'))
                             .arg(firmware->baseAdress()+firmware->sizeData(), 8, 16, QChar('0')));
            return false;
        }
    }
    if(work&Load) {
        if(dumpfn.isEmpty()) {
            emit textChanged(tr("Задайте имя файла для сохранения дампа прошивки "
                                "или снемите флажок Load."));
            return false;
        }
        /** TODO: Доработать до хекс тоже */
        if( !dumpfn.endsWith(".bin") ) dumpfn.append((".bin"));
        emit dumpFilenameChanged(dumpfn);
        dumpFile = new QFile;
        dumpFile->setFileName(dumpfn);
        if( !dumpFile->open(QIODevice::WriteOnly) ) {
            emit textChanged(tr("Не возможно открыть Файл для сохранения дампа прошивки: ")
                             .append(dumpFile->errorString()));
            delete dumpFile;
            dumpFile = 0;
            return false;
        }
    }
    if(flasherfn.isEmpty()) {
        switch (mcuType) {
        case Mcu::BE9X:
            flasherfn = ":/flashers/1986_BOOT_UART.hex";
            break;
        case Mcu::BE1:
            flasherfn = ":/flashers/1986ve1bootuarte.hex";
            break;
        case Mcu::BE4:
            flasherfn = ":/flashers/bootuartve4.hex";
            break;
        default:
            break;
        }
    }
    if( !QFile::exists(flasherfn) ) {
        emit textChanged(tr("Файл флешера не найден."));
        return false;
    }
    errCode = flasher->openHexFile(flasherfn);
    qDebug() <<"return =" << errCode <<" flasher filneme =" << flasherfn;
    if(Hex::EOF_DETECTED != errCode) {
        emit textChanged(tr("Ошибка при открытии *.hex файла флешера, "
                            "код ошибки %1, строка %2.")
                         .arg(errCode)
                         .arg(flasher->stringNumber()));
        return false;
    }/*
    if( (flasher->baseAdress() & 0xFFU) != 0 ) {
        emit textChanged(tr("Адрес начала записываемого кода должен быть кратен 256 байт! 0x%1")
                         .arg(flasher->baseAdress(), 8, 16, QChar('0')));
        return false;
    }*/
    if( (flasher->baseAdress() < Mcu::mem[mcuType].xramAdr) || 
        (flasher->baseAdress() + flasher->sizeData() >=
         Mcu::mem[mcuType].xramAdr + Mcu::mem[mcuType].xramSize) ) {
        emit textChanged(tr("Загружаемая программа выходит за пределы диапазона flash-памяти! "
                            "Начинается 0x%1, заканчивается 0x%2")
                         .arg(flasher->baseAdress(), 8, 16, QChar('0'))
                         .arg(flasher->baseAdress()+flasher->sizeData(), 8, 16, QChar('0')));
        return false;
    }
    //    qDebug() <<"  base =" << QString::number(flasher->baseAdress(),  16)
    //             <<" start =" << QString::number(flasher->startAdress(), 16)
    //             <<"  size =" << QString::number(flasher->sizeData(),  10);
    return true;
}

bool MdrLoader::checkConnect()
{
    int chankCount, repeatCount(0);
    ByteArray d;
    QByteArray ba, respond;
    
    port->setPortName(portname);
    port->setBaudRate(baudrate);
    if( !port->open(QIODevice::ReadWrite) ) {
        emit statusChanged(port->errorString());
        emit textChanged(port->errorString());
        return false;
    }
    d = "I";
    port->write(d);
    port->waitForReadyRead(100);
    respond = port->readAll();
    if(!respond.contains(Mcu::mem[mcuType].idStr)) {
        port->close();
        port->setBaudRate(9600);
        if( !port->open(QIODevice::ReadWrite) ) {
            emit statusChanged(port->errorString());
            emit textChanged(port->errorString());
            return false;
        }
        /// =================== Синхронизация
        ba.fill((char)0x00, SYNC_FRAME_LENGTH);
        do {
            port->write(ba);
            port->waitForReadyRead(50);
            respond += port->readAll();
            if(respond.contains(QByteArray::fromHex("0d0a3e"))) break;
            repeatCount++;
            emit statusChanged(tr("Синхронизация %1").arg(repeatCount));
        } while ( repeatCount < MAX_CONNECTION_TRY );
        emit stopButtonSetEnable(true);
        if(repeatCount >= MAX_CONNECTION_TRY) {
            emit statusChanged("Нет ответа от МК.");
            emit textChanged("Нет ответа от МК.");
            return false;
        }
        /// =================== Проверка связи
        ba.clear();
        ba.append(CMD_CR);
        port->write(ba);
        port->waitForReadyRead(100);
        respond = port->readAll();
        if(respond.contains(QByteArray::fromHex("0d0a3e"))) {
            emit statusChanged("МК ответил.");
            emit textChanged("Есть синхронизация.");
        } else {
            emit statusChanged("МК НЕ ответил.");
            emit textChanged("Проверка связи на стандартной скорости не прошла");
            return false;
        }
        /// =================== Установка скорости
        d = "B";
        d.append(baudrate);
        port->write(d);
        port->waitForReadyRead(100);
        /** Не удасться получить подтверждение принятия команды,
         *  т.к. отвечает МК уже на новой скорости через микросекунды
         *  http://forum.milandr.ru/viewtopic.php?p=20987#p20987
         */
        port->close();
        port->setBaudRate(baudrate);
        if( !port->open(QIODevice::ReadWrite) ) {
            emit statusChanged(port->errorString());
            emit textChanged(port->errorString());
            return false;
        }
        /// =================== Проверка связи
        ba.clear();
        ba.append(CMD_CR);
        port->write(ba);
        port->waitForReadyRead(100);
        respond = port->readAll();
        if(respond.contains(QByteArray::fromHex("0d0a3e"))) {
            emit statusChanged("МК работает на новой скорости.");
            emit textChanged("Проверка связи на новой скорости прошла успешно.");
        } else {
            emit statusChanged("МК НЕ работает на новой скорости.");
            emit textChanged("Проверка связи на новой скорости НЕ прошла!");
        }
        d = "L";
        d.append(flasher->baseAdress());
        d.append(flasher->sizeData());
        port->write(d);
        port->waitForReadyRead(100);
        respond = port->readAll();
        if(!respond.contains('L')) {
            emit textChanged("Не прошла команда загрузки флешера!");
            return false;
        }
        port->write(flasher->data());
        port->waitForReadyRead(2*flasher->sizeData());
        respond = port->readAll();
        if(!respond.contains('K')) {
            emit textChanged("Нет подтверждения загрузки флешера!");
            return false;
        }

        chankCount = (flasher->sizeData()+7)/8;
        for(int chank=0; chank<chankCount; chank++) {
            d = "Y";
            d.append(flasher->baseAdress()+chank*8);
            d.append((int)8);
            port->write(d);
            port->waitForReadyRead(100);
            respond = port->readAll();
            if(respond.size() < 10) {
                emit textChanged("Не прошла команда верификации флешера (размер)!");
                return false;
            }
            if( (respond.at(0)!='Y') || (respond.at(9)!='K') ) {
                emit textChanged("Не прошла команда верификации флешера (контроль)!");
                return false;
            }
            if(!respond.contains(flasher->data().mid(chank*8, 8))) {
                emit textChanged("Не прошла команда верификации флешера (верификация)!");
                return false;
            }
        }
/**  Вот по-хорошему тут надо писать flasher->startAdress(),
  *  НО тот HEX от vasili имеет нулевой указатель стартовой позиции.
  *  Надобы с этим разобраться...
  *  Кроме того, в миландровском загрузчике (как и во флешере vasili)
  *  принято передавать как стартовый адрес
  *  - для 1986ВЕ9х: адрес таблицы векторов прерывания;
  *  - для ВЕ1, ВЕ3: адрес первой исполняемой команды.
  * */
        d = "R";
        switch (mcuType) {
        case Mcu::BE9X:
            d.append(flasher->baseAdress());
            break;
        case Mcu::BE1:
        case Mcu::BE4:
            ba = flasher->data().mid(4, 4);
            ba[0] = ba[0] & 0xfe;
            d.append(ba);
            qDebug()<<"MdrLoader::checkconnect: runflasher command R ->"<<d.toHex();
            break;
        default:
            break;
        }
        port->write(d);
        port->waitForReadyRead(100);
        respond = port->readAll();
        if(!respond.contains('R')) {
            emit textChanged("Не прошла команда запуска флешера!");
            return false;
        }
        d = "I";
        port->write(d);
        port->waitForReadyRead(100);
        respond = port->readAll();
        if(!respond.contains(Mcu::mem[mcuType].idStr)) {
            emit textChanged("ошибка идентификации загрузчика!");
            return false;
        }
    }
    emit stopButtonSetEnable(true);
    emit textChanged("Флешер работает.");
    return true;
}

bool MdrLoader::dump()
{
    int chankCount;
    unsigned char ks;
    ByteArray d;
    QByteArray respond, dump;
    
    emit statusChanged(tr("Считывание прошивки..."));
    qDebug()<<"dump...";
    d = "A";
    d.append(Mcu::mem[mcuType].flashAdr);
    port->write(d);
    port->waitForReadyRead(100);
    respond = port->readAll();
    ks  = (unsigned char)d.at(1);
    ks += (unsigned char)d.at(2);
    ks += (unsigned char)d.at(3);
    ks += (unsigned char)d.at(4);
    if( respond.isEmpty() || (respond.at(0) != ks) ) {
        emit textChanged("Не удалось задать адрес для считывания прошивки.");
        return false;
    }
    dump.clear();
    chankCount = Mcu::mem[mcuType].flashSize / 8;
    d = "V";
    for(int chank = 0; chank < chankCount; chank++ ) {
        port->write(d);
        port->waitForReadyRead(100);
        respond = port->readAll();
        if(respond.size() == 8) {
            dump.append(respond);
        } else {
            emit textChanged(tr("Проблема при считывания прошивки на %1%.")
                             .arg((double)chank/chankCount*100.0));
            return false;
        }
        if( (chank & 0x1F) == 0) {
            emit statusChanged(tr("считываниe прошивки на %1%...")
                               .arg((double)chank/chankCount*100.0, 2, 'f', 1));
            if(stop) {
                emit textChanged("Операция отменена пользователем.");
                return false;
            }
        }
    }
    if(dumpFile) {
        emit statusChanged(tr("Запись даспа в файл..."));
        dumpFile->write(dump);
        dumpFile->waitForBytesWritten(5000);
        dumpFile->close();
        delete dumpFile;
        dumpFile = 0;
    } else {
        emit textChanged(tr("Не был создан Файл для сохранения дампа прошивки!"));
        return false;
    }
    emit statusChanged(tr("Считывание прошивки прошло успешно."));
    emit textChanged(tr("Считывание прошивки прошло успешно."));
    return true;
}

bool MdrLoader::erase()
{
    ByteArray d;
    QByteArray respond;
    unsigned char ks;
    qDebug()<<"ERASE...";
    // TODO: реализовать не полное стирание флешки (в прошивке ВЕ9х тоже
    if(stop) {
        emit textChanged("Операция отменена пользователем.");
        return false;
    }
    emit statusChanged(tr("Стирание ВСЕЙ flash-памяти..."));
    d = "E";
    switch (mcuType) {
    case Mcu::BE9X:
        break;
    case Mcu::BE1:
        d.append((unsigned int)0x0);
        ks = 0;
        break;
    case Mcu::BE4:
        //
        break;
    default:
        break;
    }
    port->write(d);
    if(mcuType == Mcu::BE1) {
        port->waitForReadyRead(200);
        respond = port->readAll();
        qDebug()<<"erase BE1 param respond"<<respond;
        if( (respond.size()<1) || (respond.at(0) != ks) ) {
            emit textChanged("Параметры команды очистки не прошли.");
        }
    }
    port->waitForReadyRead(2000);
    respond = port->readAll();
    if( (respond.size()<9) || (respond.at(0) != 'E') ) {
        emit textChanged("Команда очистки не прошла.");
        qDebug()<<d<<respond.toHex();
        return false;
    }
    quint32 addr, data;
    addr =            (unsigned char)respond.at(4);
    addr = addr*256 + (unsigned char)respond.at(3);
    addr = addr*256 + (unsigned char)respond.at(2);
    addr = addr*256 + (unsigned char)respond.at(1);
    
    data =            (unsigned char)respond.at(8);
    data = data*256 + (unsigned char)respond.at(7);
    data = data*256 + (unsigned char)respond.at(6);
    data = data*256 + (unsigned char)respond.at(5);
    
    if( (addr != Mcu::mem[mcuType].flashAdr + Mcu::mem[mcuType].flashSize)
     || (data != 0xffffffff)) {
        emit textChanged(tr("При стирании flash-памяти произошла ошибка"
                            " по адресу 0x%1 остались данные 0x%2.")
                         .arg(addr, 8, 16, QChar('0'))
                         .arg(data, 8, 16, QChar('0')));
        return false;
    }
    emit statusChanged(tr("Стирание ВСЕЙ flash-памяти прошло успешно."));
    emit textChanged(tr("Стирание ВСЕЙ flash-памяти прошло успешно."));
    return true;
}

bool MdrLoader::program()
{
    int chankCount;
    unsigned char ks;
    ByteArray d;
    QByteArray ba, respond;

    emit statusChanged(tr("Программирование %1 байт flash-памяти...")
                       .arg(firmware->sizeData()));
    emit textChanged(tr("Программирование %1 байт flash-памяти "
                        "с адреса 0x%2...")
                     .arg(firmware->sizeData())
                     .arg(firmware->baseAdress(), 8, 16, QChar('0')));
    qDebug()<<"programm...";
    d = "A";
    d.append(firmware->baseAdress());
    port->write(d);
    port->waitForReadyRead(100);
    respond = port->readAll();
    ks  = (unsigned char)d.at(1);
    ks += (unsigned char)d.at(2);
    ks += (unsigned char)d.at(3);
    ks += (unsigned char)d.at(4);
    if( respond.isEmpty() || (respond.at(0) != ks) ) {
        emit textChanged("Не удалось задать адрес для заиписи прошивки.");
        return false;
    }
    chankCount = (firmware->sizeData()+255) /256;
    d = "P";
    for(int chank = 0; chank < chankCount; chank++ ) {
        if(stop) {
            emit textChanged("Операция отменена пользователем.");
            return false;
        }
        ba = firmware->data().mid(256*chank, 256);
        int size = ba.size();
        if(size < 256) ba.append(QByteArray(256-size, (char)0xFF));
        //qDebug()<<"prog:ba.size ="<<ba.size()<<ba.toHex();
        port->write(d);
        port->write(ba);
        unsigned char ks = 0;
        for(int i=0; i<ba.size(); i++) ks+=(unsigned char)ba.at(i);
        port->waitForReadyRead(300);
        respond = port->readAll();
        if( (respond.size() < 1 ) || ((unsigned char)respond.at(0) != ks) ) {
            emit textChanged(tr("Проблема при передаче данных на %1%.")
                             .arg((double)chank/chankCount*100.0));
            qDebug()<<"r.size ="<<respond.size()
                   <<"r.data ="<<respond.toHex()
                  <<"ks ="<<ks<<"ba.size ="<<ba.size();
            return false;
        }
        emit statusChanged(tr("программирование прошивки на %1%...")
                           .arg((double)chank/chankCount*100.0, 2, 'f', 1));
    }
    emit statusChanged(tr("Программирование прошивки прошло успешно."));
    emit textChanged(tr("Программирование прошивки прошло успешно."));
    return true;
}

bool MdrLoader::verify()
{
    int chankCount;
    unsigned char ks;
    ByteArray d;
    QByteArray ba, respond;
    
    emit statusChanged(tr("Верификация %1 байт flash-памяти...")
                       .arg(firmware->sizeData()));
    emit textChanged(tr("Верификация %1 байт flash-памяти "
                        "с адреса 0x%2...")
                     .arg(firmware->sizeData())
                     .arg(firmware->baseAdress(), 8, 16, QChar('0')));
    qDebug()<<"verifieng...";
    d = "A";
    d.append(firmware->baseAdress());
    port->write(d);
    port->waitForReadyRead(100);
    respond = port->readAll();
    ks  = (unsigned char)d.at(1);
    ks += (unsigned char)d.at(2);
    ks += (unsigned char)d.at(3);
    ks += (unsigned char)d.at(4);
    if( respond.isEmpty() || (respond.at(0) != ks) ) {
        emit textChanged("Не удалось задать адрес для заиписи прошивки.");
        return false;
    }
    chankCount = (firmware->sizeData()+7) / 8;
    d = "V";
    for(int chank = 0; chank < chankCount; chank++ ) {
        ba = firmware->data().mid(8*chank, 8);
        int size = ba.size();
        if(size < 8) ba.append(QByteArray(8-size, (char)0xFF));
        port->write(d);
        port->waitForReadyRead(300);
        respond = port->readAll();
        if(respond.size() < 8) {
            emit textChanged(tr("Проблема при передаче данных на %1%.")
                             .arg((double)chank/chankCount*100.0));
            qDebug()<<"r.size ="<<respond.size()
                    <<"r.data ="<<respond.toHex()
                    <<"chank = "<<chank;
            return false;
        }
        if(respond != ba) {
            emit textChanged(tr("Ошибка верификации данных на %1%.")
                             .arg((double)chank/chankCount*100.0));
            emit textChanged(tr("По адресу 0x%1 считано %2, должно быть %3.")
                             .arg(firmware->baseAdress()+chank*8, 8, 16, QChar('0'))
                             .arg(QString(respond.toHex().toUpper()))
                             .arg(QString(ba.toHex().toUpper())) );
            return false;
        }
        if( (chank & 0x1F) == 0) {
            emit statusChanged(tr("верификация прошивки на %1%...")
                               .arg((double)chank/chankCount*100.0, 2, 'f', 1));
            if(stop) {
                emit textChanged("Операция отменена пользователем.");
                return false;
            }
        }
    }
    emit statusChanged(tr("Верификация прошивки прошла успешно."));
    emit textChanged(tr("Верификация прошивки прошла успешно."));
    return true;
}

bool MdrLoader::runFW()
{
    ByteArray d;
    QByteArray respond;
    // TODO: тестировать!
    qDebug()<<"RUN not implemented yet...";
    emit statusChanged(tr("Передача управления прошивке МК..."));
    d = "R";
    //d.append(Mcu::mem[mcuType].flashAdr);
    port->write(d);
    port->waitForReadyRead(100);
    respond = port->readAll();
    //qDebug()<<d.toHex()<<respond.toHex();
    if( respond.isEmpty() || (respond.at(0) != 'R') ) {
        emit textChanged("Ошибка обмена.");
        return false;
    }
    emit textChanged(tr("Исполняется программа из flash памяти по адресу 0x%1.")
                     .arg(Mcu::mem[mcuType].flashAdr, 8, 16, QChar('0')));
    return true;
}

bool MdrLoader::doWork(WorkType w)
{
    this->work = w;
    stop = false;
    
    if( (mcuType != Mcu::BE9X)&&(mcuType != Mcu::BE1)&&(mcuType != Mcu::BE4) ) {
        return false;
    }
    
    bool retval = checkAndLoadFiles();
    
    port = new QSerialPort;
    
    if(retval) retval &= checkConnect();
    
    if( (work & Load) && retval) {
        retval &= dump();
    }
    if( (work & Erase) && retval) {
        retval &= erase();
    }
    if( (work & Program) && retval) {
        retval &= program();
    }
    if( (work & Verify) && retval) {
        retval &= verify();
    }
    if( (work & Run) && retval) {
        retval &= runFW();
    }
    if(dumpFile) {
        dumpFile->close();
        delete dumpFile;
        dumpFile=0;
    }
    port->close();
    delete port;
    if(retval) emit statusChanged(tr("Всё прошло успешно!"));
    emit workFinished();
    return retval;
}
