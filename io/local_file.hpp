#ifndef LOCAL_FILE_HPP
#define LOCAL_FILE_HPP

#include <fstream>

#include "convert.h"
#include "io_util.hpp"
#include "ioable.h"

class local_file:public ioable
{
    public:
    ~local_file()
    {
        if(is_open_){close();}
    }
    bool open(const u8string& _path) override
    {
        if(is_open_){close();}
        
        path_name_=_path;
        path_=(utl::to_utf32(_path));
        ifs.open(path_, std::ios::binary);
        if(!ifs.is_open()) return false;
        io_len_=fs::file_size(path_);

        is_open_=true;
        return true;
    }
    void close() override
    {
        if(!is_open_){return;}
        ifs.close();
        io_pos_=0;
        io_len_=0;
        is_open_=false;
    }
    bool seekg(uint64_t _pos) override
    {
        if (!is_open_){throw std::logic_error("seekg():not open");}
        if(io_pos_==_pos){return true;}
        ifs.seekg(_pos, std::ios::beg);
        if(!ifs){return false;}
        io_pos_=_pos;
        return true;
    }
    bool read(uint8_t* _buf, uint32_t _len) override
    {
        if (!is_open_){throw std::logic_error("read():not open");}
        ifs.read(reinterpret_cast<char*>(_buf), _len);
        if(!ifs){return false;}
        io_pos_+=_len;
        return true;

    }
    uint64_t tellg() override
    {
        if (!is_open_){throw std::logic_error("tellg():not open");}
        return io_pos_;
    }
    uint64_t telllen() override
    {
        if (!is_open_){throw std::logic_error("telllen():not open");}
        return io_len_;
    }
    bool eof() override
    {
        if (!is_open_){throw std::logic_error("eof():not open");}
        if(io_pos_>io_len_){throw std::logic_error("eof():io_pos_>io_len_");}
        return io_pos_==io_len_;
    }
    bool is_open() override
    {
        return is_open_;
    }

    const u8string& get_path_name() override
    {
        return path_name_;
    }

    private:
    u8string path_name_;
    fs::path path_;
    fs::ifstream ifs;
    bool is_open_=false;

    uint64_t io_len_=0;
    uint64_t io_pos_=0;


};

#endif
