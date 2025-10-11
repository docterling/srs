//
// Copyright (c) 2013-2025 The SRS Authors
//
// SPDX-License-Identifier: MIT
//

#ifndef SRS_UTEST_APP17_HPP
#define SRS_UTEST_APP17_HPP

/*
#include <srs_utest_app17.hpp>
*/
#include <srs_utest.hpp>

#include <srs_app_config.hpp>
#include <srs_app_factory.hpp>
#include <srs_app_listener.hpp>
#include <srs_app_mpegts_udp.hpp>
#include <srs_protocol_raw_avc.hpp>
#include <srs_protocol_rtmp_conn.hpp>
#include <srs_utest_app10.hpp>
#include <srs_utest_app11.hpp>
#include <srs_utest_app6.hpp>

// Mock ISrsAppConfig for testing SrsUdpCasterListener
class MockAppConfigForUdpCaster : public MockAppConfig
{
public:
    int stream_caster_listen_port_;

public:
    MockAppConfigForUdpCaster();
    virtual ~MockAppConfigForUdpCaster();

public:
    virtual int get_stream_caster_listen(SrsConfDirective *conf);
};

// Mock ISrsIpListener for testing SrsUdpCasterListener
class MockIpListenerForUdpCaster : public ISrsIpListener
{
public:
    std::string endpoint_ip_;
    int endpoint_port_;
    std::string label_;
    bool set_endpoint_called_;
    bool set_label_called_;
    bool listen_called_;
    bool close_called_;

public:
    MockIpListenerForUdpCaster();
    virtual ~MockIpListenerForUdpCaster();

public:
    virtual ISrsListener *set_endpoint(const std::string &i, int p);
    virtual ISrsListener *set_label(const std::string &label);
    virtual srs_error_t listen();
    virtual void close();
    void reset();
};

// Mock ISrsMpegtsOverUdp for testing SrsUdpCasterListener
class MockMpegtsOverUdp : public ISrsMpegtsOverUdp
{
public:
    bool initialize_called_;
    srs_error_t initialize_error_;

public:
    MockMpegtsOverUdp();
    virtual ~MockMpegtsOverUdp();

public:
    virtual srs_error_t initialize(SrsConfDirective *c);
    virtual srs_error_t on_ts_message(SrsTsMessage *msg);
    virtual srs_error_t on_udp_packet(const sockaddr *from, const int fromlen, char *buf, int nb_buf);
    void reset();
};

// Mock ISrsRawH264Stream for testing SrsMpegtsOverUdp::on_ts_video
class MockMpegtsRawH264Stream : public ISrsRawH264Stream
{
public:
    bool annexb_demux_called_;
    bool is_sps_called_;
    bool is_pps_called_;
    bool sps_demux_called_;
    bool pps_demux_called_;
    srs_error_t annexb_demux_error_;
    srs_error_t sps_demux_error_;
    srs_error_t pps_demux_error_;

    // Mock return values
    char *demux_frame_;
    int demux_frame_size_;
    bool is_sps_result_;
    bool is_pps_result_;
    std::string sps_output_;
    std::string pps_output_;

public:
    MockMpegtsRawH264Stream();
    virtual ~MockMpegtsRawH264Stream();

public:
    virtual srs_error_t annexb_demux(SrsBuffer *stream, char **pframe, int *pnb_frame);
    virtual bool is_sps(char *frame, int nb_frame);
    virtual bool is_pps(char *frame, int nb_frame);
    virtual srs_error_t sps_demux(char *frame, int nb_frame, std::string &sps);
    virtual srs_error_t pps_demux(char *frame, int nb_frame, std::string &pps);
    virtual srs_error_t mux_sequence_header(std::string sps, std::string pps, std::string &sh);
    virtual srs_error_t mux_ipb_frame(char *frame, int frame_size, std::string &ibp);
    virtual srs_error_t mux_avc2flv(std::string video, int8_t frame_type, int8_t avc_packet_type, uint32_t dts, uint32_t pts, char **flv, int *nb_flv);
    void reset();
};

// Mock ISrsBasicRtmpClient for testing SrsMpegtsOverUdp::on_ts_video
class MockMpegtsRtmpClient : public ISrsBasicRtmpClient
{
public:
    bool connect_called_;
    bool publish_called_;
    bool close_called_;
    srs_error_t connect_error_;
    srs_error_t publish_error_;
    int stream_id_;

public:
    MockMpegtsRtmpClient();
    virtual ~MockMpegtsRtmpClient();

public:
    virtual srs_error_t connect();
    virtual void close();
    virtual srs_error_t publish(int chunk_size, bool with_vhost = true, std::string *pstream = NULL);
    virtual srs_error_t play(int chunk_size, bool with_vhost = true, std::string *pstream = NULL);
    virtual void kbps_sample(const char *label, srs_utime_t age);
    virtual srs_error_t recv_message(SrsRtmpCommonMessage **pmsg);
    virtual srs_error_t decode_message(SrsRtmpCommonMessage *msg, SrsRtmpCommand **ppacket);
    virtual srs_error_t send_and_free_messages(SrsMediaPacket **msgs, int nb_msgs);
    virtual srs_error_t send_and_free_message(SrsMediaPacket *msg);
    virtual void set_recv_timeout(srs_utime_t timeout);
    virtual int sid();
    void reset();
};

// Mock ISrsAppFactory for testing SrsMpegtsOverUdp::rtmp_write_packet
class MockAppFactoryForMpegtsOverUdp : public SrsAppFactory
{
public:
    MockMpegtsRtmpClient *mock_rtmp_client_;
    bool create_rtmp_client_called_;

public:
    MockAppFactoryForMpegtsOverUdp();
    virtual ~MockAppFactoryForMpegtsOverUdp();

public:
    virtual ISrsBasicRtmpClient *create_rtmp_client(std::string url, srs_utime_t cto, srs_utime_t sto);
    void reset();
};

#endif
