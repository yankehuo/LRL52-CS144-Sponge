#include "stream_reassembler.hh"
#include <algorithm>
#include <limits>
#include <iostream>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

// template <typename... Targs>
// void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity),
    _stream(capacity), _unassembled_cnt(0), _eof_index(std::numeric_limits<size_t>::max()) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // |0...|readn...|writen...|outbound
    // capacity == unread(write-read)+unassembled(outbount-writen)
    size_t first_unassembled = _output.bytes_written();
    size_t st = std::max(first_unassembled, index);
    size_t ed = std::min({index + data.size(), first_unassembled + _capacity - _output.buffer_size(), _eof_index});
    if (eof) {
        _eof_index = std::min(_eof_index, index + data.size());
    }

    // truncate to get suitable area
    for (size_t i = st, j = i - index; i < ed; ++i, ++j) {
        auto &[ch, used] = _stream[i % _capacity];
        if (used) {
            if (ch != data[j]) {
                std::cerr << "Bytes order is not continuous!" << std::endl;
            }
        } else {
            ch = data[j];
            used = true;
            ++_unassembled_cnt;
        }
    }

    // write, and move write ptr
    std::string str;
    while (first_unassembled < _eof_index && _stream[first_unassembled % _capacity].second) {
        str.push_back(_stream[first_unassembled % _capacity].first);
        _stream[first_unassembled % _capacity] = {0, false};
        --_unassembled_cnt;
        ++first_unassembled;
    }
    _output.write(str);
    if (_eof_index == first_unassembled) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    return _unassembled_cnt;
}

bool StreamReassembler::empty() const { 
    return _unassembled_cnt == 0;
}
