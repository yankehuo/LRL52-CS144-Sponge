#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <algorithm>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

// template <typename... Targs>
// void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _timer(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const {
    return _bytes_in_flight;
}

// assembled tcp segments from application bytestream
void TCPSender::fill_window() {
    uint16_t window_size = std::max(_window_size, static_cast<uint16_t>(1));
    while (_bytes_in_flight < window_size) {
        TCPSegment segment;
        if (!_set_syn_flag) {
            segment.header().syn = true;
            _set_syn_flag = true;
        }
        // window_size include SYN FIN flag, first read data as much as possible, FIN could be process later on 
        auto payload_size = min({TCPConfig::MAX_PAYLOAD_SIZE, window_size - _bytes_in_flight - segment.header().syn, _stream.buffer_size()});
        auto payload = _stream.read(payload_size);
        segment.payload() = Buffer(std::move(payload));
        // syn payload fin, process fin flag
        if (!_set_fin_flag && _stream.eof() && _bytes_in_flight + segment.length_in_sequence_space() < window_size) {
            segment.header().fin = true;
            _set_fin_flag = true;
        }
        // syn payload fin, syn or fin flag dont carry data
        uint64_t length = segment.length_in_sequence_space();
        if (length == 0) {
            break;
        }
        segment.header().seqno = next_seqno();
        _segments_out.emplace(segment);

        if (!_timer.is_running()) {
            _timer.restart();
        }
        // backup
        _outstanding_segment.emplace(_next_seqno, std::move(segment));
        _next_seqno += length;
        _bytes_in_flight += length;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    auto abs_ackno = unwrap(ackno, _isn, next_seqno_absolute());
    if (abs_ackno > next_seqno_absolute()) {
        return;
    }
    bool is_succeessful = false;
    while (!_outstanding_segment.empty()) {
        auto &[abs_seq, segment] = _outstanding_segment.front();
        // abs included, [abs, abs+len-1] == len, abs+len-1 < ack
        if (abs_seq + segment.length_in_sequence_space() - 1 < abs_ackno) {
            is_succeessful = true;
            _bytes_in_flight -= segment.length_in_sequence_space();
            _outstanding_segment.pop();
        } else {
            break;
        }
    }
    if (is_succeessful) {
        _consecutive_retransmissions_count = 0;
        _timer.set_timeout(_initial_retransmission_timeout);
        _timer.restart();
    }
    if (_bytes_in_flight == 0) {
        _timer.stop();
    }
    _window_size = window_size;
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer.tick(ms_since_last_tick);
    if (_timer.check_timeout() && !_outstanding_segment.empty()) {
        _segments_out.emplace(_outstanding_segment.front().second);
        if (_window_size > 0) {
            ++_consecutive_retransmissions_count;
            _timer.set_timeout(_timer.get_timeout() * 2);
        }
        _timer.restart();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const {
    return _consecutive_retransmissions_count;
}

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno = next_seqno();
    _segments_out.emplace(std::move(segment));
}
