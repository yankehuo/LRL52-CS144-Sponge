#include "byte_stream.hh"
#include <iostream>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

// template <typename... Targs>
// void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

// now que len: _capacity+1, cap: _capacity; at first, construction is empty
ByteStream::ByteStream(const size_t capacity) : _buffer(Buffer()),
    _capacity(capacity), _write_cnt(0), _read_cnt(0) {}

size_t ByteStream::write(const string &data) {
    size_t actual_len = std::min(data.size(), remaining_capacity());
    _buffer.append(BufferList(std::move(string().assign(data.begin(), data.begin() + actual_len))));
    _write_cnt += actual_len;
    return actual_len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t actual_len = std::min(len, buffer_size());
    std::string peek_str = _buffer.concatenate();
    return string().assign(peek_str.begin(), peek_str.begin() + actual_len);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t actual_len = std::min(len, buffer_size());
    _buffer.remove_prefix(actual_len);
    _read_cnt += actual_len;
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
    return _buffer.size();
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
