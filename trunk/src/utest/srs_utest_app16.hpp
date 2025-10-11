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
#include <srs_protocol_srt.hpp>
#include <srs_utest_app10.hpp>
#include <srs_utest_app11.hpp>
#include <srs_utest_app6.hpp>

// Mock ISrsSrtSocket for testing SrsSrtConnection
class MockSrtSocket : public ISrsSrtSocket
{
public:
    srs_utime_t recv_timeout_;
    srs_utime_t send_timeout_;
    int64_t recv_bytes_;
    int64_t send_bytes_;
    srs_error_t recvmsg_error_;
    srs_error_t sendmsg_error_;
    int recvmsg_called_count_;
    int sendmsg_called_count_;
    std::string last_recv_data_;
    std::string last_send_data_;

public:
    MockSrtSocket();
    virtual ~MockSrtSocket();

public:
    virtual srs_error_t recvmsg(void *buf, size_t size, ssize_t *nread);
    virtual srs_error_t sendmsg(void *buf, size_t size, ssize_t *nwrite);
    virtual void set_recv_timeout(srs_utime_t tm);
    virtual void set_send_timeout(srs_utime_t tm);
    virtual srs_utime_t get_send_timeout();
    virtual srs_utime_t get_recv_timeout();
    virtual int64_t get_send_bytes();
    virtual int64_t get_recv_bytes();
};

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

// Mock ISrsProtocolReadWriter for testing SrsSrtRecvThread
class MockSrtProtocolReadWriter : public ISrsProtocolReadWriter
{
public:
    srs_error_t read_error_;
    int read_count_;
    bool simulate_timeout_;
    std::string test_data_;
    srs_utime_t recv_timeout_;
    srs_utime_t send_timeout_;
    int64_t recv_bytes_;
    int64_t send_bytes_;

public:
    MockSrtProtocolReadWriter();
    virtual ~MockSrtProtocolReadWriter();

public:
    virtual srs_error_t read(void *buf, size_t size, ssize_t *nread);
    virtual srs_error_t read_fully(void *buf, size_t size, ssize_t *nread);
    virtual srs_error_t write(void *buf, size_t size, ssize_t *nwrite);
    virtual srs_error_t writev(const iovec *iov, int iov_size, ssize_t *nwrite);
    virtual void set_recv_timeout(srs_utime_t tm);
    virtual srs_utime_t get_recv_timeout();
    virtual int64_t get_recv_bytes();
    virtual void set_send_timeout(srs_utime_t tm);
    virtual srs_utime_t get_send_timeout();
    virtual int64_t get_send_bytes();
};

// Mock ISrsCoroutine for testing SrsSrtRecvThread
class MockSrtCoroutine : public ISrsCoroutine
{
public:
    srs_error_t pull_error_;
    int pull_count_;
    bool started_;
    SrsContextId cid_;

public:
    MockSrtCoroutine();
    virtual ~MockSrtCoroutine();

public:
    virtual srs_error_t start();
    virtual void stop();
    virtual void interrupt();
    virtual srs_error_t pull();
    virtual const SrsContextId &cid();
    virtual void set_cid(const SrsContextId &cid);
};

// Mock SrsSrtSource for testing SrsMpegtsSrtConn::on_srt_packet
class MockSrtSourceForPacket : public SrsSrtSource
{
public:
    int on_packet_called_count_;
    srs_error_t on_packet_error_;
    SrsSrtPacket *last_packet_;

public:
    MockSrtSourceForPacket();
    virtual ~MockSrtSourceForPacket();
    virtual srs_error_t on_packet(SrsSrtPacket *packet);
};

// Mock ISrsAppConfig for testing SrsMpegtsSrtConn HTTP hooks
class MockAppConfigForSrtHooks : public MockAppConfig
{
public:
    SrsConfDirective *on_connect_directive_;
    SrsConfDirective *on_close_directive_;
    SrsConfDirective *on_publish_directive_;
    SrsConfDirective *on_unpublish_directive_;
    SrsConfDirective *on_play_directive_;
    SrsConfDirective *on_stop_directive_;

public:
    MockAppConfigForSrtHooks();
    virtual ~MockAppConfigForSrtHooks();
    virtual SrsConfDirective *get_vhost_on_connect(std::string vhost);
    virtual SrsConfDirective *get_vhost_on_close(std::string vhost);
    virtual SrsConfDirective *get_vhost_on_publish(std::string vhost);
    virtual SrsConfDirective *get_vhost_on_unpublish(std::string vhost);
    virtual SrsConfDirective *get_vhost_on_play(std::string vhost);
    virtual SrsConfDirective *get_vhost_on_stop(std::string vhost);
    void set_on_connect_urls(const std::vector<std::string> &urls);
    void set_on_close_urls(const std::vector<std::string> &urls);
    void set_on_publish_urls(const std::vector<std::string> &urls);
    void set_on_unpublish_urls(const std::vector<std::string> &urls);
    void set_on_play_urls(const std::vector<std::string> &urls);
    void set_on_stop_urls(const std::vector<std::string> &urls);
    void clear_on_connect_directive();
    void clear_on_close_directive();
    void clear_on_publish_directive();
    void clear_on_unpublish_directive();
    void clear_on_play_directive();
    void clear_on_stop_directive();
};

// Mock ISrsHttpHooks for testing SrsMpegtsSrtConn HTTP hooks
class MockHttpHooksForSrt : public ISrsHttpHooks
{
public:
    std::vector<std::pair<std::string, ISrsRequest *> > on_connect_calls_;
    int on_connect_count_;
    srs_error_t on_connect_error_;
    std::vector<std::tuple<std::string, ISrsRequest *, int64_t, int64_t> > on_close_calls_;
    int on_close_count_;
    std::vector<std::pair<std::string, ISrsRequest *> > on_publish_calls_;
    int on_publish_count_;
    srs_error_t on_publish_error_;
    std::vector<std::pair<std::string, ISrsRequest *> > on_unpublish_calls_;
    int on_unpublish_count_;
    std::vector<std::pair<std::string, ISrsRequest *> > on_play_calls_;
    int on_play_count_;
    srs_error_t on_play_error_;
    std::vector<std::pair<std::string, ISrsRequest *> > on_stop_calls_;
    int on_stop_count_;

public:
    MockHttpHooksForSrt();
    virtual ~MockHttpHooksForSrt();
    virtual srs_error_t on_connect(std::string url, ISrsRequest *req);
    virtual void on_close(std::string url, ISrsRequest *req, int64_t send_bytes, int64_t recv_bytes);
    virtual srs_error_t on_publish(std::string url, ISrsRequest *req);
    virtual void on_unpublish(std::string url, ISrsRequest *req);
    virtual srs_error_t on_play(std::string url, ISrsRequest *req);
    virtual void on_stop(std::string url, ISrsRequest *req);
    virtual srs_error_t on_dvr(SrsContextId cid, std::string url, ISrsRequest *req, std::string file);
    virtual srs_error_t on_hls(SrsContextId cid, std::string url, ISrsRequest *req, std::string file, std::string ts_url,
                               std::string m3u8, std::string m3u8_url, int sn, srs_utime_t duration);
    virtual srs_error_t on_hls_notify(SrsContextId cid, std::string url, ISrsRequest *req, std::string ts_url, int nb_notify);
    virtual srs_error_t discover_co_workers(std::string url, std::string &host, int &port);
    virtual srs_error_t on_forward_backend(std::string url, ISrsRequest *req, std::vector<std::string> &rtmp_urls);
    void clear_calls();
};

// Mock SrsProtocolUtility for testing discover_candidates
class MockProtocolUtility : public SrsProtocolUtility
{
public:
    std::vector<SrsIPAddress *> mock_ips_;

public:
    MockProtocolUtility();
    virtual ~MockProtocolUtility();
    virtual std::vector<SrsIPAddress *> &local_ips();
    void add_ip(std::string ip, std::string ifname, bool is_ipv4, bool is_loopback, bool is_internet);
    void clear_ips();
};

// Mock ISrsAppConfig for testing discover_candidates
class MockAppConfigForDiscoverCandidates : public MockAppConfig
{
public:
    std::string rtc_server_candidates_;
    bool use_auto_detect_network_ip_;
    std::string rtc_server_ip_family_;

public:
    MockAppConfigForDiscoverCandidates();
    virtual ~MockAppConfigForDiscoverCandidates();
    virtual std::string get_rtc_server_candidates();
    virtual bool get_use_auto_detect_network_ip();
    virtual std::string get_rtc_server_ip_family();
};

#endif
