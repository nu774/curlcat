#include <cstdlib>
#include <cstring>
#include <array>
#include <unistd.h>

namespace util {
  template <int N> class fifo {
    std::array<char, N> data_;
    std::size_t pos_;
  public:
    fifo(): pos_(0) {}
    bool empty() const { return pos_ == 0; }
    std::size_t count() const { return pos_; }
    std::size_t remaining() const { return data_.size() - pos_; } 
    char *read_ptr() { return &data_[0];}
    char *write_ptr() { return &data_[pos_]; }
    void commit(std::size_t n) { pos_ += n; }
    void consume(std::size_t n) {
      std::memmove(&data_[0], &data_[n], pos_ - n);
      pos_ -= n;
    }
  };

  class read_buffer {
    int fd_;
    bool eof_;
    fifo<65536> buf_;
  public:
    read_buffer(int fd): fd_(fd), eof_(false) {}
    read_buffer(const read_buffer&) = delete;
    read_buffer& operator=(const read_buffer&) = delete;
    
    int fd() const { return fd_;}
    bool eof() const { return eof_; }
    bool has_room() const { return !eof_ && buf_.remaining() > 0; }

    ssize_t read(void *buf, std::size_t count) {
      if (count > buf_.count())
        count = buf_.count();
      if (count > 0) {
        std::memcpy(buf, buf_.read_ptr(), count);
        buf_.consume(count);
      }
      return count;
    }
    void fill() {
      if (has_room()) {
        int n = ::read(fd(), buf_.write_ptr(), buf_.remaining());
        if (n > 0)
          buf_.commit(n);
        else
          eof_ = true;
      }
    }
  };
}
