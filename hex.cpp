#include "hex.h"
#include <QDebug>
#include <QFile>

Hex::Hex() : start(0), base(0), stringNum(0), error(NOT_OPENED)
{    
}

Hex::FileParseError Hex::openHexFile(QString fileName)
{
    quint32 dataType = 0, offset = 0, newBase, newStart, currOffset;
    QFile file(fileName);
    if( !file.open(QIODevice::ReadOnly|QIODevice::Text) ) {
        error= CANNOT_OPEN_FILE;
        return error;
    }
    d.clear(); 
    start = base = stringNum = 0;
    error= EOF_ISNT_DETECTED;
    while( !file.atEnd() && error == EOF_ISNT_DETECTED ) {
        /// Чтение и проверка строки
        QByteArray line = file.readLine();
        stringNum++;
        if(line.isEmpty())     { error= EMPTY_LINE_IS_READED; break; }
        if(line.length() < 11) { error= TOO_SHORT_STRING;     break; }
        if(line.at(0) != ':')  { error= START_CODE_MISSING;   break; }
        uint dataLen = QByteArray::fromHex(line.mid(1,2)).at(0);
        if(line.length() < int(11 + dataLen*2) ) { 
            error= STRING_LENGHT_MISMATCH;
            break;
        }
        /// Разбор полей параграфа и проверка КС
        QByteArray raw = QByteArray::fromHex(line.mid(1, 10+dataLen*2));
        offset =              (unsigned char)raw.at(1);
        offset = offset*256 + (unsigned char)raw.at(2);
        dataType = (unsigned char)raw.at(3);
        unsigned char ccs = 0;
        for(int i = 0; i < raw.size()-1; i++) {
            ccs += raw.at(i);
        }
        ccs = 1 + ~ccs;
        if(ccs != (unsigned char)raw.at(raw.size()-1)) {
            qDebug()<<"ccs ="<<ccs<<" cs ="<< (unsigned char)raw.at(raw.size()-1);
            error= CHECKSUM_MISMATCH;
            break;
        }
        //qDebug() << "strLen =" << strLen << "offset =" << offset << "dataType =" << dataType << line;
        /// Разбор данных
        switch (dataType) {
        case 00: // запись содержит данные двоичного файла.
            currOffset = (base+d.size()) & 0xFFFFU;
            if(d.size() == 0) {
                base += offset;
            } else if(currOffset != offset) {
                qDebug()<<"currOffset ="<<QString::number(currOffset,16)
                       <<" offset ="<<offset;
                error= NEXT_OFFSET_ISNT_CONTINUE_CURRENT_DATA;
                break;
            }
            d.append(raw.mid(4, dataLen));
            break;
        case 01: // запись является концом файла.
            error= EOF_DETECTED;
            break;
        case 02: // запись адреса сегмента (подробнее см. ниже).
            newBase =               (unsigned char)raw.at(4);
            newBase = newBase*256 + (unsigned char)raw.at(5);
            newBase <<= 4;
            if(d.size() != 0) { // если есть данные
                if(newBase != base+d.size()) { // и не продолжение данных
                    error= NEXT_ESA_ISNT_CONTINUE_CURRENT_DATA;
                    break;
                }
            } else { // если данных нет
                base &= 0x0F;
                base |= newBase;
                //qDebug()<<"newBase ESA ="<<newBase<<"offset ="<<offset;
            }
            break;
        case 03: // Start Segment Address Record
            error= UNSUPPORTED_I16HEX_TYPE;
            break;
        case 04: // запись расширенного адреса 
            newBase =               (unsigned char)raw.at(4);
            newBase = newBase*256 + (unsigned char)raw.at(5);
            newBase <<= 16;
            if(d.size() != 0) { // если есть данные
                if(newBase != base+d.size()) { // и не продолжение данных
                    error= NEXT_ELA_ISNT_CONTINUE_CURRENT_DATA;
                    break;
                }
            } else { // если данных нет
                base &= 0xFFFF;
                base |= newBase;
                //qDebug()<<"newBase ELA ="<<newBase<<"offset ="<<offset;
            }
            break;
        case 05: // Start Linear Address Record.
            newStart =                (unsigned char)raw.at(4);
            newStart = newStart*256 + (unsigned char)raw.at(5);
            newStart = newStart*256 + (unsigned char)raw.at(6);
            newStart = newStart*256 + (unsigned char)raw.at(7);
            start = newStart;
            break;
        default: // не известный тип...
            error= UNKNOWN_RECORD_TYPE;
            break;
        }
    }
    file.close();
    return error;
}

