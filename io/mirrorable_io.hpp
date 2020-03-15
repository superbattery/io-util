
#ifndef MIRROR_IO_HPP
#define MIRROR_IO_HPP

#define __MIRROR_IO_MEM_TYPE

#include <cassert>
#include <list>
#include <iterator>

#include <qdebug.h>


#include "mio/mmap.hpp"
#include "ioable.h"
#include "util.h"

template <typename __ioable_imp>
class mirror_io : public ioable
{
public:
    explicit mirror_io(__ioable_imp &_io_imp, const u8string &_map_path) : io_imp_(_io_imp)
      ,map_path(_map_path)
      ,curr_mirror_view_it_(mirror_views_.end())
    {
    }

    ~mirror_io()
    {
        if (is_open_)
        {
            close();
        }
    }

    bool open(const u8string &_path) override
    {
        if (is_open_)
        {
            close();
        }

        if (!io_imp_.open(_path))
        {
            return false;
        }
        io_len_ = io_imp_.telllen();

        assert(io_len_>=0);


#ifdef __MIRROR_IO_MEM_TYPE
        try
        {
            mirror_.resize(io_len_);
        }
        catch (const std::exception &e)
        {
            throw e;
        }
#else
        if(!allocate_file(map_path, io_len_)){return false;}
        std::error_code error;
        mirror_ = mio::make_mmap_sink(map_path, 0, io_len_, error);
        if(error)
        {
            return false;
        }
#endif

        is_open_ = true;
        return true;
    }
    void close() override
    {
        if (!is_open_)
        {
            return;
        }

#ifdef __MIRROR_IO_MEM_TYPE
        mirror_.clear();
#else
        save(false);//TODO: not necessary to call save()
        mirror_.unmap();
#endif
        mirror_views_.clear();
        curr_mirror_view_it_=mirror_views_.end();

        io_imp_.close();
        io_pos_ = 0;
        io_len_ = 0;
        is_open_ = false;
    }
    bool seekg(uint64_t _pos) override
    {
        assert(is_open_);
        assert(io_len_>0);
        assert(_pos>=0&&_pos<io_len_);

        curr_mirror_view_it_=mirror_views_.size()==0?mirror_views_.end():mirror_views_.begin();
        for(typename std::list<range>::iterator it=mirror_views_.begin();it!=mirror_views_.end();++it)
        {
            auto& elem=*it;
            if(elem.contains(_pos))
            {
                curr_mirror_view_it_=it;
                break;
            }
            else if(elem.a>_pos&&it!=mirror_views_.begin())
            {
                if(!io_imp_.seekg(_pos)){return false;}
                curr_mirror_view_it_=std::prev(it);
                break;
            }
            else
            {
                if(std::next(it)==mirror_views_.end())
                {
                    //the last one
                    if(!io_imp_.seekg(_pos)){return false;}
                    curr_mirror_view_it_=it;
                    break;
                }
            }
        }


        io_pos_ = _pos;
        return true;
    }
    bool read(uint8_t *_buf, uint32_t _len) override
    {
        assert(is_open_);
        assert(_len>=0&&_len<=io_len_);
        //assert(io_pos_+_len<=io_len_-1);
        assert(io_pos_+_len<=io_len_);

        if(_len==0){return true;}

        if(!fill_mirror(range(io_pos_, io_pos_+_len-1))){return false;}

        memcpy(_buf, mirror_.data()+io_pos_, _len);
        io_pos_ += _len;
        return true;
    }
    uint64_t tellg() override
    {
        assert(is_open_);

        //当io_len==0时，io_pos的值是无效的
        if(io_len_==0){throw std::logic_error("tellg():io_len==0");}
        return io_pos_;
    }
    uint64_t telllen() override
    {
        assert(is_open_);
        return io_len_;
    }
    bool eof() override
    {
        assert(is_open_);
        //assert(io_pos_<io_len_);
        assert(io_pos_<=io_len_);

        //当io_len==0时，io_pos的值是无效的
        //if(io_len_==0){throw std::logic_error("eof():io_len==0");}
        return io_pos_ == io_len_;
    }
    bool is_open() override
    {
        return is_open_;
    }

    const u8string &get_path_name() override
    {
        return io_imp_.get_path_name();
    }

public:
    void set_map_type(bool _type)
    {
    }

    void set_map_path(const u8string& _path)
    {
        map_path=_path;
    }

    bool totally_mirrord()
    {
        if(io_len_==0){return true;}
        if(mirror_views_.size()!=1){
            return false;
        }
        else{
            auto& elem=mirror_views_.front();
            return elem.a==0&&elem.b==io_len_-1;
        }
    }

#ifndef __MIRROR_IO_MEM_TYPE
    bool save(bool _total=true)
    {
        if(_total&&!totally_mirrord())
        {
            assert(io_len_>0);
            if(!seekg(0)){return false;}
            if(!fill_mirror(range(0, io_len_-1))){return false;}
        }

        std::error_code err;
        mirror_.sync(err);
        return !err;
    }
#endif

private:

    struct range{
    public:
        range(uint64_t _a, uint64_t _b)
        {
            if(_b<_a){throw std::logic_error("range custruct fail:b<a");}
            a=_a;
            b=_b;
        }


        uint64_t a;
        uint64_t b;

        bool contains(const range& r1) const
        {
            return r1.a>=a&&r1.b<=b;
        }

        bool in(const range& r1) const
        {
            return r1.a<=a&&r1.b>=b;
        }

        bool contains(uint64_t _p) const
        {
            return _p>=a&&_p<=b;
        }

        bool intersects(const range& r1) const
        {
            return !(r1.a>b||r1.b<a);
        }

        bool left_intersects(const range& r1) const
        {
            return (r1.b>=a&&r1.b<=b)&&r1.a<a;
        }

        bool right_intersects(const range& r1) const
        {
            return (r1.a>=a&&r1.a<=b)&&r1.b>b;
        }

        //紧贴
        bool beside(const range& r1) const
        {
            return (r1.b==a-1)||(r1.a==b+1);
        }

        //左紧贴
        bool left_beside(const range& r1) const
        {
            return (r1.b==a-1);
        }
        //右紧贴
        bool right_beside(const range& r1) const
        {
            return (r1.a==b+1);
        }

        //两者在空间上无特殊关系
        bool detach(const range& r1) const
        {
            return r1.b<a-1||r1.a>b+1;
        }

        bool equals(const range& r1) const
        {
            return r1.a==a&&r1.b==b;
        }

        uint64_t size() const//min=0
        {
            return b-a+1;
        }

        range operator>(const range& r1) const
        {
            return b-a>r1.b-r1.a;
        }

        range operator<(const range& r1) const
        {
            return b-a<r1.b-r1.a;
        }

        /*range operator==(const range& r1)
        {
            return b-a==r1.b-r1.a;
        }*/

        range get_intersect(const range& r1) const
        {
            range ret(0,0);
            if(in(r1))
            {
                ret.a=a;
                ret.b=b;
            }
            else if(contains(r1))
            {
                ret.a=r1.a;
                ret.b=r1.b;
            }
            else if(left_intersects(r1))
            {
                ret.a=a;
                ret.b=r1.b;
            }
            else if(right_intersects(r1))
            {
                ret.a=r1.a;
                ret.b=b;
            }
            else
            {
                throw std::logic_error("cannot get_intersect");
            }
            return ret;
        }

        range& sub_intersect(const range& r1)
        {
            if(left_intersects(r1))
            {
                a=r1.b+1;
            }
            else if(right_intersects(r1))
            {
                b=r1.a-1;
            }
            else if(contains(r1)&&!equals(r1))
            {
                if(r1.a==a)//contains r1
                {
                    a=r1.b+1;
                }
                else if(r1.b==b)
                {
                    b=r1.a-1;
                }
                else
                {
                    throw std::logic_error("2cannot apply operator-");
                }
            }
            else
            {
                throw std::logic_error("cannot apply operator-");
            }
            return *this;
        }
    };

    bool allocate_file(const std::string& u8_path, const int size)
    {
        std::ofstream file(u8_path, std::ios::binary);
        if(!file.is_open()){return false;}

        uint64_t to_write=size;
        const char buf[102400]={0};
        while(to_write>0)
        {
            auto m=std::min(sizeof (buf), to_write);
            file.write(buf, m);
            if(!file){return false;}
            to_write-=m;
        }
        return true;
    }

    bool fill_mirror(const range& _r1)
    {
        //warning: 调用前应确保curr_mirror_view_it_是正确的，否则UB
        //若此次的range与上一次的正向连续，该方法会自动正确计算curr_mirror_view_it_，
        //否则应调用seekg()以重新计算curr_mirror_view_it_

        assert(is_open_);

        for(auto& d:mirror_views_)
            qDebug()<<"=="<<mirror_views_.size()<<"=="<<d.a<<","<<d.b;
        qDebug()<<"==============================================="<<io_pos_;

        range _r1_tmp=_r1;
        bool is_complete=false;


        /*auto it=std::find_if(curr_mirror_view_it_, mirror_views_.end(), [](const range& r1)
        {

        });

        if(it==mirror_views_.end())
        {
            curr_mirror_view_idx_=-1;
        }
        else{
            curr_mirror_view_idx_=it-mirror_views_.begin();
        }*/

        for(typename std::list<range>::iterator it=curr_mirror_view_it_;it!=mirror_views_.end();++it)
        {
            //寻找间隙并填充数据
            range &elem=*it;
            range r_intersect(0,0);

            if(_r1_tmp.in(elem)||_r1_tmp.equals(elem))
            {
                is_complete=true;
                break;
            }
            else if(_r1_tmp.left_intersects(elem))
            {
                r_intersect=_r1_tmp.get_intersect(elem);
                _r1_tmp.sub_intersect(r_intersect);
            }
            else if((_r1_tmp.contains(elem)&&!_r1_tmp.equals(elem))||_r1_tmp.right_intersects(elem))
            {
                //填充前面一段间隙
                if(_r1_tmp.a<elem.a)
                {
                    range r_gap(_r1_tmp.a, elem.a-1);
                    if(!read_data(r_gap)){return false;}
                    _r1_tmp.sub_intersect(r_gap);
                }

                r_intersect=_r1_tmp.get_intersect(elem);
                if(_r1_tmp.equals(r_intersect))
                {
                    is_complete=true;
                    break;
                }
                else
                {
                    _r1_tmp.sub_intersect(r_intersect);
                }
            }
            else if(!_r1_tmp.intersects(elem)&& elem.a>_r1_tmp.b)//且在右侧
            {
                if(!read_data(_r1_tmp)){return false;}
                is_complete=true;
                break;
            }
        }

        //后面没有mirror了
        if(!is_complete)
        {
            if(!read_data(_r1_tmp)){return false;}
        }

        //拼接连续的mirror_views
        typename std::list<range>::iterator left_it=mirror_views_.end();
        typename std::list<range>::iterator right_it=mirror_views_.end();

        if(curr_mirror_view_it_==mirror_views_.end())
        {
            mirror_views_.push_back(_r1);
            //更新curr_mirror_view_it_
            curr_mirror_view_it_=mirror_views_.begin();
        }
        else if(_r1.in(*curr_mirror_view_it_))
        {
            //curr_mirror_view_it_=first_range;
        }
        else
        {
            for(typename std::list<range>::iterator it=curr_mirror_view_it_;it!=mirror_views_.end();)
            {
                range &elem=*it;
                if(_r1.left_intersects(elem)||_r1.left_beside(elem))
                {
                    left_it=it;
                }
                else if (_r1.right_intersects(elem)||_r1.right_beside(elem)) {
                    right_it=it;
                }
                else if(elem.a>_r1.b)
                {
                    break;
                }
                else if(_r1.contains(elem))
                {
                    //此时curr_mirror_view_it_应该会>=it
                    //若curr_mirror_view_it_==it应更新它
                    bool eq=curr_mirror_view_it_==it;
                    it=mirror_views_.erase(it);
                    if(eq){curr_mirror_view_it_=it;}
                    continue;
                }
                ++it;
            }


            if(left_it!=mirror_views_.end())
            {
                if(right_it!=mirror_views_.end())
                {
                    (*left_it).b=(*right_it).b;
                    right_it=mirror_views_.erase(right_it);
                }
                else
                {
                    (*left_it).b=_r1.b;
                }
                curr_mirror_view_it_=left_it;
            }
            else
            {
                if(right_it!=mirror_views_.end())
                {
                    (*right_it).a=_r1.a;
                    curr_mirror_view_it_=right_it;
                }
                else
                {
                    //插入后应更新curr_mirror_view_it_
                    curr_mirror_view_it_=mirror_views_.insert(curr_mirror_view_it_==mirror_views_.end()?
                                                                  mirror_views_.end():
                                                                  std::next(curr_mirror_view_it_), _r1);
                }
            }
        }
        return true;
    }

    bool read_data(const range& r1)
    {
        if(!io_imp_.seekg(r1.a)){return false;}
        if(!io_imp_.read(reinterpret_cast<uint8_t*>(mirror_.data())+r1.a, r1.size())){return false;}
        return true;
    }

private:
#ifdef __MIRROR_IO_MEM_TYPE
    std::vector<uint8_t> mirror_;
#else
    mio::mmap_sink mirror_;
#endif
    u8string map_path;
    std::list<range> mirror_views_;
    //std::set<std::pair<uint64_t, uint64_t>> mirror_views_;

    //int32_t curr_mirror_view_idx_=-1;
    typename std::list<range>::iterator curr_mirror_view_it_;


private:
    __ioable_imp &io_imp_;
    uint64_t io_pos_ = 0;//特殊情况：当io_len==0时，io_pos的值是无效的
    uint64_t io_len_ = 0;
    bool is_open_ = false;
};

#endif
