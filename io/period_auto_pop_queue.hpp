
#ifndef PERIOD_AUTO_POP_QUEQUE_HPP
#define PERIOD_AUTO_POP_QUEQUE_HPP

#include <cstdint>
#include <functional>
#include <vector>

template<typename __T>
class period_auto_pop_queue
{
public:
    period_auto_pop_queue(){};
    ~period_auto_pop_queue(){};

    typedef std::function<bool(const __T *_data, uint32_t _len)> ON_POP;

    void init(uint32_t _period_size, const ON_POP& _m_call)
    {
        this->period_size_ = _period_size;
        this->buffer.resize(period_size_);
		this->buffer_cursor = 0;
        this->m_call = _m_call;
	}

	void deinit()
	{
		this->buffer_cursor = 0;
		this->m_call = nullptr;
	}
    bool push(const __T *chunk, uint32_t len)
	{
		//LOGD("wwwwwOOOOOO=%d",len);
        if(buffer_cursor==period_size_)
		{
			//TODO:上次的m_call()失败，可以m_call()或选择复位(buffer_cursor=0)
		}
        if (buffer_cursor == 0 && len == period_size_)
		{
			return m_call(chunk, len);
		}
		else
		{
			uint32_t chunk_cursor=0;
			while (len)
			{
                uint32_t fill_len=std::min(period_size_-buffer_cursor, len);
				memcpy(&buffer[0]+buffer_cursor, chunk+chunk_cursor, fill_len);
				buffer_cursor+=fill_len;
				chunk_cursor+=fill_len;
				len-=fill_len;
                if(buffer_cursor==period_size_)
				{
                    if(!m_call(&buffer[0], period_size_)) return false;
					buffer_cursor=0;
				}
			}
		}
		
		return true;
	}

	bool is_empty()
	{
		return buffer_cursor == 0;
	}

	void flush()
	{
		if (!is_empty())
		m_call(&buffer[0], buffer_cursor);
	}

    void clear()
    {
        if (buffer_cursor!=0)
        {
            buffer_cursor=0;
        }
    }

private:
    ON_POP m_call;
    uint32_t period_size_;
    std::vector<__T> buffer;
	uint32_t buffer_cursor;
};

#endif
