
#ifndef MIRROR_IO_HPP
#define MIRROR_IO_HPP

#include <vector>
#include <map>
#include <list>
#include <set>
#include <iterator>

#include <qdebug.h>

#include "ioable.h"
#include "util.h"

template <typename __ioable_imp>
class mirror_io : public ioable
{
public:
    explicit mirror_io(__ioable_imp &_io_imp) : io_imp_(_io_imp)
      ,curr_mirror_view_it_(mirror_views_.end())/*, curr_mirror_view_(mirror_views_.front())*/
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

        try
        {
            mirror_.resize(io_len_);
        }
        catch (const std::exception &e)
        {
            throw e;
        }

        is_open_ = true;
        return true;
    }
    void close() override
    {
        if (!is_open_)
        {
            return;
        }

        mirror_.clear();
        mirror_views_.clear();
        curr_mirror_view_it_=mirror_views_.end();

        io_imp_.close();
        io_pos_ = 0;
        io_len_ = 0;
        is_open_ = false;
    }
    bool seekg(uint64_t _pos) override
    {
        if (!is_open_)
        {
            throw std::logic_error("seekg():not open");
        }


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
                curr_mirror_view_it_=it;
                curr_mirror_view_it_--;
                break;
            }
            else
            {
                typename std::list<range>::iterator tt=it;
                if(++tt==mirror_views_.end())
                {
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
        if (!is_open_)
        {
            throw std::logic_error("read():not open");
        }
        if(_len<=0){return true;}

        for(auto& d:mirror_views_)
        qDebug()<<"=="<<mirror_views_.size()<<"=="<<d.a<<","<<d.b;
        qDebug()<<"==============================================="<<io_pos_;

        range r_all_to_read(io_pos_, io_pos_+_len-1);
        range r_to_read=r_all_to_read;
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
            range &elem=*it;
            range r_intersect(0,0);

            if(r_to_read.in(elem)||r_to_read.equals(elem))
            {
                //r_intersect=r_to_read.get_intersect(elem);
                //memcpy(mirror_.data()+r_intersect.a, mirror_.data()+r_intersect.b, r_intersect.size());
                //curr_mirror_view_it_=it;
                is_complete=true;
                break;
            }
            else if(r_to_read.left_intersects(elem))
            {
                r_intersect=r_to_read.get_intersect(elem);
                //memcpy(mirror_.data()+r_intersect.a, mirror_.data()+r_intersect.b, r_intersect.size());
                r_to_read.sub_intersect(r_intersect);
            }
            else if((r_to_read.contains(elem)&&!r_to_read.equals(elem))||r_to_read.right_intersects(elem))
            {
                //读取前面一段间隙
                if(r_to_read.a<elem.a)
                {
                    range r_gap(r_to_read.a, elem.a-1);
                    if(!read_data(r_gap)){return false;}
                    r_to_read.sub_intersect(r_gap);
                }

                //拷贝交集
                r_intersect=r_to_read.get_intersect(elem);
                //memcpy(mirror_.data()+r_intersect.a, mirror_.data()+r_intersect.b, r_intersect.size());

                if(r_to_read.equals(r_intersect))
                {
                    //curr_mirror_view_it_=it;
                    is_complete=true;
                    break;
                }
                else
                {
                    r_to_read.sub_intersect(r_intersect);
                }

            }
            else if(!r_to_read.intersects(elem)&& elem.a>r_to_read.b)//且在右侧
            {
                if(!read_data(r_to_read)){return false;}
                is_complete=true;
                break;
            }
        }

        //后面没有mirror了
        if(!is_complete)
        {
            if(!read_data(r_to_read)){return false;}
        }


        typename std::list<range>::iterator left_it=mirror_views_.end();
        typename std::list<range>::iterator right_it=mirror_views_.end();

        if(curr_mirror_view_it_==mirror_views_.end())
        {
            mirror_views_.push_back(r_all_to_read);
            curr_mirror_view_it_=mirror_views_.begin();
        }
        else if(r_all_to_read.in(*curr_mirror_view_it_))
        {
            //curr_mirror_view_it_=first_range;
        }
        else
        {
            for(typename std::list<range>::iterator it=curr_mirror_view_it_;it!=mirror_views_.end();)
            {
                range &elem=*it;
                if(r_all_to_read.left_intersects(elem)||r_all_to_read.left_beside(elem))
                {
                    left_it=it;
                }
                else if (r_all_to_read.right_intersects(elem)||r_all_to_read.right_beside(elem)) {
                    right_it=it;
                }
                else if(elem.a>r_all_to_read.b)
                {
                    break;
                }
                else if(r_all_to_read.contains(elem))
                {
                    mirror_views_.erase(it);
                    continue;
                }
                ++it;
            }


            if(left_it!=mirror_views_.end())
            {
                if(right_it!=mirror_views_.end())
                {
                    (*left_it).b=(*right_it).b;
                    mirror_views_.erase(right_it);
                }
                else
                {
                    (*left_it).b=r_all_to_read.b;
                }
                curr_mirror_view_it_=left_it;
            }
            else
            {
                if(right_it!=mirror_views_.end())
                {
                    (*right_it).a=r_all_to_read.a;
                    curr_mirror_view_it_=right_it;
                }
                else
                {
                    typename std::list<range>::iterator tt=curr_mirror_view_it_;
                    curr_mirror_view_it_=mirror_views_.insert(++tt, r_all_to_read);
                }
            }
        }




        memcpy(_buf, mirror_.data()+io_pos_, _len);

        io_pos_ += _len;
        return true;
    }
    uint64_t tellg() override
    {
        if (!is_open_)
        {
            throw std::logic_error("tellg():not open");
        }
        return io_pos_;
    }
    uint64_t telllen() override
    {
        if (!is_open_)
        {
            throw std::logic_error("telllen():not open");
        }
        return io_len_;
    }
    bool eof() override
    {
        if (!is_open_)
        {
            throw std::logic_error("eof():not open");
        }
        if (io_pos_ > io_len_)
        {
            throw std::logic_error("eof():io_pos_>io_len_");
        }
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


    bool fill_mirror(uint64_t _pos, uint32_t _len)
    {
        range r(_pos, _pos+_len-1);
        //mirror_views_.find()
        return true;
    }

    bool read_data(const range& r1)
    {
        if(!io_imp_.seekg(r1.a)){return false;}
        if(!io_imp_.read(mirror_.data()+r1.a, r1.size())){return false;}
        return true;
    }

private:
    std::vector<uint8_t> mirror_;
    std::list<range> mirror_views_;
    //std::set<std::pair<uint64_t, uint64_t>> mirror_views_;
    //std::map<uint64_t, uint64_t> mirror_views_;

    bool is_seek_on_mirror_=false;
    uint64_t last_seek_pos_=0;
    uint64_t last_read_len=0;
    //std::pair<uint64_t, uint64_t> &curr_mirror_view_;
    int32_t curr_mirror_view_idx_=-1;
    typename std::list<range>::iterator curr_mirror_view_it_;

    

private:
    __ioable_imp &io_imp_;
    uint64_t io_pos_ = 0;
    uint64_t io_len_ = 0;
    bool is_open_ = false;
};

#endif
