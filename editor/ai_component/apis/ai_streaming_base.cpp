#include "ai_streaming_base.h"

bool AIStreamingBase::is_valid_utf8(const uint8_t* tdata, int len) {
    int i = 0;
    while (i < len) {
        if (tdata[i] <= 0x7F) { i++; continue; } // ASCII
        // 多字节UTF-8序列检查
        if ((tdata[i] & 0xE0) == 0xC0) { if (i+1 >= len || (tdata[i+1] & 0xC0) != 0x80) return false; i+=2; }
        else if ((tdata[i] & 0xF0) == 0xE0) { if (i+2 >= len || (tdata[i+1] & 0xC0) != 0x80 || (tdata[i+2] & 0xC0) != 0x80) return false; i+=3; }
        else if ((tdata[i] & 0xF8) == 0xF0) { if (i+3 >= len || (tdata[i+1] & 0xC0) != 0x80 || (tdata[i+2] & 0xC0) != 0x80 || (tdata[i+3] & 0xC0) != 0x80) return false; i+=4; }
        else return false;
    }
    return true;
}