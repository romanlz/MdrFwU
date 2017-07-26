#ifndef MCUDESCRIPTOR_H
#define MCUDESCRIPTOR_H

namespace Mcu {
struct AbstractMcu {
    const quint32 flashAdr;
    const quint32 flashSize;
    const quint32 ramAdr;
    const quint32 ramSize;
    const quint32 xramAdr;
    const quint32 xramSize;
    const char *idStr;
};

enum Type {
    Undef= 0,
    BE9X = 1,
    BE1  = 2,
    BE4  = 3
};

const AbstractMcu mem[] = {
//  flashAdr  flashSize      ramAdr    ramSize     xramAdr   xramSize   idStr
{          0,         0,          0,         0,          0,         0, ""            },
{ 0x08000000,  128*1024, 0x20000000,   32*1024, 0x20000000,   32*1024, "1986BOOTUART"},
{        0x0,  128*1024, 0x20000000,   32*1024, 0x20100000,   16*1024, "1986BOOTUART"},
{        0x0,  128*1024, 0x20000000,   16*1024, 0x20000000,   16*1024, "BOOTUARTVE4" }
};

} // namespace Mcu

#endif // MCUDESCRIPTOR_H

