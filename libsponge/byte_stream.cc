#include "byte_stream.hh"
#include <iostream>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

// template <typename... Targs>
// void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

inline static size_t getModNumber(size_t x, size_t mod) {
    while (x >= mod) {
        x -= mod;
    }
    return x;
}

// now que len: _capacity+1, cap: _capacity; at first, construction is empty
ByteStream::ByteStream(const size_t capacity) : _que(capacity + 1), _front(0), _rear(capacity),
    _capacity(capacity), _write_cnt(0), _read_cnt(0) {}

size_t ByteStream::write(const string &data) {
    size_t data_len = data.size();
    size_t actual_len = std::min(data_len, remaining_capacity());
    for (size_t i = 0; i < actual_len; ++i) {
        _rear = getModNumber(_rear + 1, _capacity + 1);
        _que[_rear] = data[i];
    }
    _write_cnt += actual_len;
    return actual_len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    std::string ans;
    size_t n = std::min(len, buffer_size());
    ans.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        ans.push_back(_que[getModNumber(_front + i, _capacity + 1)]);
    }
    return ans;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    auto n = min(len, buffer_size());
    _front = getModNumber(_front + n, _capacity + 1);
    _read_cnt += n;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    auto ans = peek_output(len);
    pop_output(len);
    return ans;
}

void ByteStream::end_input() {
    _input_eof = true;
}

bool ByteStream::input_ended() const {
    return _input_eof;
}

size_t ByteStream::buffer_size() const {
    // std::cerr << "_front:" << _front << ", _rear:" << _rear << std::endl;
    return getModNumber(_rear - _front + 1 + _capacity + 1, _capacity + 1);
}

bool ByteStream::buffer_empty() const {
    return buffer_size() == 0;
}

bool ByteStream::eof() const {
    return _input_eof && buffer_empty();
}

size_t ByteStream::bytes_written() const {
    return _write_cnt;
}

size_t ByteStream::bytes_read() const {
    return _read_cnt;
}

size_t ByteStream::remaining_capacity() const { 
    return _capacity - buffer_size();
}
