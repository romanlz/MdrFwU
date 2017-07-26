#ifndef BYTEARRAY_H
#define BYTEARRAY_H

#include "QByteArray"

class ByteArray : public QByteArray
{
public:
    ByteArray();
    ByteArray(const QByteArray &);
    ~ByteArray();
    ByteArray & operator=(const char * str);
    ByteArray &append(const QByteArray &other);
    
    ByteArray &append(const          int   i);
    ByteArray &append(const unsigned int   i);
    ByteArray &append(const short          i);
    ByteArray &append(const unsigned short i);
    
    ByteArray &appendCrc16CCITT();
};

#endif // BYTEARRAY_H
