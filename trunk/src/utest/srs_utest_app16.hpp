//
// Copyright (c) 2013-2025 The SRS Authors
//
// SPDX-License-Identifier: MIT
//

#ifndef SRS_UTEST_APP16_HPP
#define SRS_UTEST_APP16_HPP

/*
#include <srs_utest_app16.hpp>
*/
#include <srs_utest.hpp>

#include <srs_app_listener.hpp>
#include <srs_utest_app10.hpp>
#include <srs_utest_app11.hpp>
#include <srs_utest_app6.hpp>

// Mock ISrsUdpHandler for testing SrsUdpListener
class MockUdpHandler : public ISrsUdpHandler
{
public:
    bool on_udp_packet_called_;
    int packet_count_;
    std::string last_packet_data_;
    int last_packet_size_;

public:
    MockUdpHandler();
    virtual ~MockUdpHandler();

public:
    virtual srs_error_t on_udp_packet(const sockaddr *from, const int fromlen, char *buf, int nb_buf);
};

// Mock ISrsUdpMuxHandler for testing SrsUdpMuxListener
class MockUdpMuxHandler : public ISrsUdpMuxHandler
{
public:
    bool on_udp_packet_called_;
    int packet_count_;
    std::string last_peer_ip_;
    int last_peer_port_;
    std::string last_packet_data_;
    int last_packet_size_;

public:
    MockUdpMuxHandler();
    virtual ~MockUdpMuxHandler();

public:
    virtual srs_error_t on_udp_packet(ISrsUdpMuxSocket *skt);
};

#endif
