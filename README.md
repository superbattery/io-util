# io-util

## including

### `auto produce buffer`

```
a buffer which can read data from network automatically when its data almost runs out.
```

## Usage

### `auto produce buffer`

* implement your own io class from ioable.h
* follow the example 

``` cpp
your_io_class remote_stream;
size_t buffer_size=20480;
auto_produce_buffer<your_io_class> buffer(remote_stream, buffer_size);
if(buffer.open("some_remote_url"))
{
    buffer.seekg(10);
    buffer.read(...);
    ...
}

buffer.close();
```
