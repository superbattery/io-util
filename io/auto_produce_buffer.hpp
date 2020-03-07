#ifndef AUTO_PRODUCE_BUFFER_HPP
#define AUTO_PRODUCE_BUFFER_HPP

#include <cstdint>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <functional>
#include <algorithm>
#include <memory>

#include "ring_buffer.h"
#include "ioable.h"

//#include <QtDebug>

template <typename __ioable_imp>
class auto_produce_buffer : public ioable
{
public:
    explicit auto_produce_buffer(__ioable_imp &_io_imp, uint32_t _buf_size = 102400) : ring_buffer(_buf_size),
                                                                                       io_imp_(_io_imp)
    {
    }

    ~auto_produce_buffer()
    {
        if (is_open_)
        {
            close();
        }
    }

    virtual bool open(const u8string &_path)
    {
        if (is_open_)
        {
            close();
        }
        if (!io_imp_.open(_path))
            return false;
        io_len_ = io_imp_.telllen();

        start_th();
        is_open_ = true;
        return true;
    }
    virtual void close()
    {
        if (!is_open_)
        {
            return;
        }
        stop_th();
        io_imp_.close();
        ring_buffer.clear();

        can_push = false;
        push_error = false;
        is_push_th_blocked = false;

        io_pos_ = 0;
        io_len_ = 0;

        is_open_ = false;
    }
    virtual bool seekg(uint64_t _pos)
    {
        if (!is_open_)
        {
            throw std::logic_error("auto_produce_buffer not open!");
        }
        wait_th_status(true);
        if (!io_imp_.seekg(_pos))
        {
            return false;
        }
        io_pos_ = _pos;
        ring_buffer.clear();
        return true;
    }
    virtual bool read(uint8_t *_buf, uint32_t _len)
    {
        if (!is_open_)
        {
            throw std::logic_error("auto_produce_buffer not open!");
        }
        uint32_t to_read = _len;
        while (to_read > 0)
        {
            //qDebug()<<"======="<<ring_buffer.size()*1.0/ring_buffer.capacity()<<" this:"
            //     <<QString::number(to_read)<<"io:"<<QString::number(io_imp_.tellg())<<"/"<<QString::number(io_imp_.telllen());

            size_t curr_buffer_size=ring_buffer.size();
            bool need_push = curr_buffer_size < ring_buffer.capacity() * 1.0 / 2;

            if (need_push && !push_error)//TODO 若发生错误，则马上取消push，最后交由调用者决定是否继续push。
            {
                notify_push(true);
            }

            //wait push
            if (need_push && ring_buffer.size() < to_read)
            {
                wait_th_status(true, [&, _pre_size = curr_buffer_size]() -> bool {
                    return (ring_buffer.size() > _pre_size) || io_imp_.eof() || push_error;
                });
                if (push_error)
                {
                    push_error = false;
                    return false;
                }
                else if (io_imp_.eof() && to_read > ring_buffer.size())
                {
                    throw std::logic_error("auto_produce_buffer to_read>file_length!");
                }
                //else if(!flag_run) throw 1;
            }

            auto copy_size = std::min<uint32_t>(to_read, ring_buffer.size());
            ring_buffer.read(_buf, copy_size);
            to_read -= copy_size;
            _buf += copy_size;
        }
        io_pos_ += _len;
        return true;
    }
    virtual uint64_t tellg()
    {
        if (!is_open_)
        {
            throw std::logic_error("auto_produce_buffer not open!");
        }
        return io_pos_;
    }
    virtual uint64_t telllen()
    {
        if (!is_open_)
        {
            throw std::logic_error("auto_produce_buffer not open!");
        }
        return io_len_;
    }
    virtual bool eof()
    {
        if (!is_open_)
        {
            throw std::logic_error("auto_produce_buffer not open!");
        }
        if (io_pos_ > io_len_)
        {
            throw std::logic_error("eof():io_pos_ > io_len_");
        }
        return io_pos_ == io_len_;
    }

    virtual bool is_open()
    {
        return is_open_;
    }

    virtual const u8string &get_path_name()
    {
        return io_imp_.get_path_name();
    }

private:
    template <typename __other_cond>
    void wait_th_status(bool _blocked, __other_cond _other_cond)
    {
        std::unique_lock<std::mutex> lck(mtx_push_th_blocked);
        while (!((is_push_th_blocked == _blocked) && _other_cond()))
        {
            cv_push_th_blocked.wait(lck);
        }
    }

    void wait_th_status(bool _blocked)
    {
        std::unique_lock<std::mutex> lck(mtx_push_th_blocked);
        cv_push_th_blocked.wait(lck, [this, _blocked] { return (is_push_th_blocked == _blocked); });
    }

    void notify_th_status(bool _blocked)
    {
        {
            std::lock_guard<std::mutex> lck(mtx_push_th_blocked);
            is_push_th_blocked = _blocked;
        }
        cv_push_th_blocked.notify_all();
    }

    void notify_push(bool _push)
    {
        {
            std::lock_guard<std::mutex> lck(mtx_can_push);
            can_push = _push;
        }

        if (_push)
            cv_can_push.notify_all();
    }

    void stop_th()
    {
        {
            std::lock_guard<std::mutex> lck(mtx_can_push);
            flag_run = false;
        }
        cv_can_push.notify_all();
        if (th_fill_sink.joinable())
            th_fill_sink.join();
    }

    void start_th()
    {
        flag_run = true;
        th_fill_sink = std::thread([this]() {
            while (flag_run)
            {
                std::unique_lock<std::mutex> lck(mtx_can_push);
                cv_can_push.wait(lck, [this] { notify_th_status(!can_push);return can_push || !flag_run; });
                can_push = false;
                if (!flag_run)
                    break;
                lck.unlock();

                push_error = (!fill_sink());
            }
        });
        wait_th_status(true);
    }

    bool fill_sink()
    {
        uint32_t to_push = std::min<uint32_t>(io_len_ - io_imp_.tellg(),
                                              ring_buffer.capacity() - ring_buffer.size());
        std::vector<uint8_t> to_push_buf(to_push);

        if (!io_imp_.read(&to_push_buf[0], to_push))
        {
            return false;
        }
        else
        {
            ring_buffer.write(&to_push_buf[0], to_push);
        }
        return true;
    }

private:
    ring_buffer_s ring_buffer;
    std::thread th_fill_sink;

    bool can_push = false;
    std::mutex mtx_can_push;
    std::condition_variable cv_can_push;

    bool flag_run = false;
    bool is_push_th_blocked = false;
    std::mutex mtx_push_th_blocked;
    std::condition_variable cv_push_th_blocked;

    bool push_error = false;

private:
    __ioable_imp &io_imp_;
    uint64_t io_pos_ = 0;
    uint64_t io_len_ = 0;
    bool is_open_ = false;
};

#endif // AUTO_PRODUCE_BUFFER_HPP
