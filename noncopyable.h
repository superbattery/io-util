

#ifndef ____NONCOPYABLE_H
#define ____NONCOPYABLE_H

class noncopyable
{
protected:
    noncopyable()=default;
    virtual ~noncopyable(){}
public:

private:
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;
    //noncopyable(noncopyable&&) = delete;
    //noncopyable& operator=(noncopyable&&) = delete;

};

#endif
