#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}
void NetworkInterface::_send(const EthernetAddress &dst, const uint16_t type, BufferList &&payload) {
    EthernetFrame eth_frame;
    eth_frame.header().src = _ethernet_address;
    eth_frame.header().dst = dst;
    eth_frame.header().type = type;
    eth_frame.payload() = std::move(payload);
    _frames_out.push(std::move(eth_frame));
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    auto it = _arp_table.find(next_hop_ip);
    if (it != _arp_table.end()) {
        _send(it->second.eth_addr, EthernetHeader::TYPE_IPv4, dgram.serialize());
    } else {
        // if no request arp, then send arp message
        if (_waiting_arp_response_ip_addr.find(next_hop_ip) == _waiting_arp_response_ip_addr.end()) {
            ARPMessage arp_msg;
            arp_msg.opcode = ARPMessage::OPCODE_REQUEST;
            arp_msg.sender_ethernet_address = _ethernet_address;
            arp_msg.target_ethernet_address = {};
            arp_msg.sender_ip_address = _ip_address.ipv4_numeric();
            arp_msg.target_ip_address = next_hop_ip;
            _send(ETHERNET_BROADCAST, EthernetHeader::TYPE_ARP, arp_msg.serialize());
            _waiting_arp_response_ip_addr[next_hop_ip] = ARP_RESPONSE_TTL_MS;
        }
        _waiting_internet_datagrams[next_hop_ip].emplace_back(next_hop, dgram);
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    auto &header = frame.header();
    if (header.dst != ETHERNET_BROADCAST && header.dst != _ethernet_address) {
        return std::nullopt;
    }
    if (header.type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram dgram;
        if (dgram.parse(frame.payload()) == ParseResult::NoError) {
            return dgram;
        }
    } else if (header.type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp_msg;
        if (arp_msg.parse(frame.payload()) != ParseResult::NoError) {
            return std::nullopt;
        }
        const uint32_t my_ip = _ip_address.ipv4_numeric();
        const uint32_t src_ip = arp_msg.sender_ip_address;
        if (arp_msg.opcode == ARPMessage::OPCODE_REQUEST && arp_msg.target_ip_address == my_ip) {
            ARPMessage arp_reply;
            arp_reply.opcode = ARPMessage::OPCODE_REPLY;
            arp_reply.sender_ethernet_address = _ethernet_address;
            arp_reply.target_ethernet_address = arp_msg.sender_ethernet_address;
            arp_reply.sender_ip_address = my_ip;
            arp_reply.target_ip_address = src_ip;
            _send(arp_msg.sender_ethernet_address, EthernetHeader::TYPE_ARP, arp_reply.serialize());
        }
        _arp_table[src_ip] = {arp_msg.sender_ethernet_address, ARP_ENTRY_TTL_MS};
        auto it = _waiting_internet_datagrams.find(src_ip);
        if (it != _waiting_internet_datagrams.end()) {
            for (auto &[next_hop, dgram] : it->second) {
                _send(arp_msg.sender_ethernet_address, EthernetHeader::TYPE_IPv4, dgram.serialize());
            }
            _waiting_internet_datagrams.erase(it);
        }
    }
    return std::nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    for (auto it = _arp_table.begin(); it != _arp_table.end(); ) {
        // ms_since_last_tick has up to time to live
        if (it->second.ttl <= ms_since_last_tick) {
            it = _arp_table.erase(it);
        } else {
            it->second.ttl -= ms_since_last_tick;
            it = std::next(it);
        }
    }
    for (auto it = _waiting_arp_response_ip_addr.begin(); it != _waiting_arp_response_ip_addr.end(); ) {
        if (it->second <= ms_since_last_tick) {
            auto it_dest = _waiting_internet_datagrams.find(it->first);
            if (it_dest != _waiting_internet_datagrams.end()) {
                _waiting_internet_datagrams.erase(it_dest);
            }
            it = _waiting_arp_response_ip_addr.erase(it);
        } else {
            it->second -= ms_since_last_tick;
            it = std::next(it);
        }
    }
}
