#include "tcp_connection.hh"

#include <iostream>
#include <limits>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const {
    return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const {
    return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const {
    return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const {
    return _time_since_last_segment_received;
}

void TCPConnection::_set_rst_state(const bool send_rst) {
    if (send_rst) {
        TCPSegment seg;
        seg.header().seqno = _sender.next_seqno();
        seg.header().rst = true;
        _segments_out.emplace(std::move(seg));
    }
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _linger_after_streams_finish = false;
    _is_active = false;
}

// judge whether need too add ack by receiver
void TCPConnection::_add_ackno_and_window_to_send() {
    while (!_sender.segments_out().empty()) {
        auto seg = std::move(_sender.segments_out().front());
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
        }
        seg.header().win = std::min(static_cast<size_t>(std::numeric_limits<uint16_t>::max()), _receiver.window_size());
        _segments_out.emplace(std::move(seg));
    }
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    // clear up, last means the end
    _time_since_last_segment_received = 0;
    const auto &header = seg.header();
    if (header.rst) {
        _set_rst_state(false);
        return;
    }
    _receiver.segment_received(seg);
    // segment that's occupied sequence space need ack ?
    bool need_empty_ack = seg.length_in_sequence_space() > 0;
    if (header.ack) {
        _sender.ack_received(header.ackno, header.win);
        if (need_empty_ack && !_segments_out.empty()) {
            need_empty_ack = false;
        }
    }

    // LISTEN: receive SYN, -> FSM: SYN RECEIVED ? why connect
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::CLOSED) {
        connect();
        return;
    }

    // Passive CLOSE ? FSM: CLOSE WAIT, only change linger state
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED) {
        _linger_after_streams_finish = false;
    }

    // Passive CLOSE ? FSM: CLOSED
    if (!_linger_after_streams_finish &&
        TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED) {
        _is_active = false;
        return;
    }

    // Keep-alive
    if (_receiver.ackno().has_value() && seg.length_in_sequence_space() == 0 && 
        seg.header().seqno == _receiver.ackno().value() - 1) {
        need_empty_ack = true;
    }

    if (need_empty_ack) {
        _sender.send_empty_segment();
    }
    // why while loop to process sender.segment ? emit as much as possible
    _add_ackno_and_window_to_send();
}

bool TCPConnection::active() const {
    return _is_active;
}

size_t TCPConnection::write(const string &data) {
    auto ans = _sender.stream_in().write(data);
    _sender.fill_window();
    _add_ackno_and_window_to_send();
    return ans;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    // send rst
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        while (!_sender.segments_out().empty()) {
            _sender.segments_out().pop();
        }
        _set_rst_state(true);
        return;
    }
    _add_ackno_and_window_to_send();
    // Active CLOSE
    if (_linger_after_streams_finish && 
        TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED &&
        _time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
        _is_active = false;
        _linger_after_streams_finish = false;
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    _add_ackno_and_window_to_send();
}

void TCPConnection::connect() {
    _sender.fill_window();
    _add_ackno_and_window_to_send();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _set_rst_state(true);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
