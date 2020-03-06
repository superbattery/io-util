
#ifndef IOABLE_H
#define IOABLE_H

#include <cstdint>
#include <string>

#include "noncopyable.h"

class ioable:public noncopyable
{

public:
    typedef std::string u8string;

    virtual bool open(const u8string& _path)=0;
    virtual void close()=0;
    virtual bool seekg(uint64_t _pos)=0;
    virtual bool read(uint8_t* _buf, uint32_t _len)=0;
    virtual uint64_t tellg()=0;
    virtual uint64_t telllen()=0;
    virtual bool eof()=0;
    virtual bool is_open()=0;
    virtual const u8string& get_path_name()=0;

};

#endif
