#ifndef HEX_H
#define HEX_H

#include <QByteArray>
#include <QString>

class Hex
{
public:
    typedef enum {
        EOF_DETECTED,
        NOT_OPENED,
        CANNOT_OPEN_FILE,
        TOO_SHORT_STRING,
        START_CODE_MISSING,
        STRING_LENGHT_MISMATCH,
        CHECKSUM_MISMATCH,
        UNKNOWN_RECORD_TYPE,
        UNSUPPORTED_I16HEX_TYPE,
        NEXT_ESA_ISNT_CONTINUE_CURRENT_DATA,
        NEXT_ELA_ISNT_CONTINUE_CURRENT_DATA,
        NEXT_OFFSET_ISNT_CONTINUE_CURRENT_DATA,
        EOF_ISNT_DETECTED,
        EMPTY_LINE_IS_READED
    } FileParseError;
    
    explicit Hex();
    quint32 startAdress() { return start; }
    quint32 baseAdress() { return base; }
    quint32 stringNumber() { return stringNum; }
    QByteArray &data() { return d; }
    int sizeData() { return d.size(); }
    
    FileParseError openHexFile(QString fileName);
    FileParseError lastError() { return error; }
private:
    quint32 start;
    quint32 base;
    quint32 stringNum;
    QByteArray d;
    FileParseError error;
};

#endif // HEX_H
