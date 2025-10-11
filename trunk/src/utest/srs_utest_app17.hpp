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

#include <srs_app_caster_flv.hpp>
#include <srs_app_config.hpp>
#include <srs_app_factory.hpp>
#include <srs_app_listener.hpp>
#include <srs_app_mpegts_udp.hpp>
#include <srs_kernel_pithy_print.hpp>
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

// Mock ISrsAppConfig for testing SrsHttpFlvListener
class MockAppConfigForHttpFlvListener : public MockAppConfig
{
public:
    int stream_caster_listen_port_;

public:
    MockAppConfigForHttpFlvListener();
    virtual ~MockAppConfigForHttpFlvListener();

public:
    virtual int get_stream_caster_listen(SrsConfDirective *conf);
};

// Mock SrsTcpListener for testing SrsHttpFlvListener
class MockTcpListenerForHttpFlv : public SrsTcpListener
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
    MockTcpListenerForHttpFlv(ISrsTcpHandler *h);
    virtual ~MockTcpListenerForHttpFlv();

public:
    virtual ISrsListener *set_endpoint(const std::string &i, int p);
    virtual ISrsListener *set_label(const std::string &label);
    virtual srs_error_t listen();
    void close();
    void reset();
};

// Mock ISrsAppCasterFlv for testing SrsHttpFlvListener
class MockAppCasterFlv : public ISrsAppCasterFlv
{
public:
    bool initialize_called_;
    bool on_tcp_client_called_;
    srs_error_t initialize_error_;
    srs_error_t on_tcp_client_error_;

public:
    MockAppCasterFlv();
    virtual ~MockAppCasterFlv();

public:
    virtual srs_error_t initialize(SrsConfDirective *c);
    virtual srs_error_t on_tcp_client(ISrsListener *listener, srs_netfd_t stfd);
    virtual srs_error_t start();
    virtual bool empty();
    virtual size_t size();
    virtual void add(ISrsResource *conn, bool *exists);
    virtual void add_with_id(const std::string &id, ISrsResource *conn);
    virtual void add_with_fast_id(uint64_t id, ISrsResource *conn);
    virtual void add_with_name(const std::string &name, ISrsResource *conn);
    virtual ISrsResource *at(int index);
    virtual ISrsResource *find_by_id(std::string id);
    virtual ISrsResource *find_by_fast_id(uint64_t id);
    virtual ISrsResource *find_by_name(std::string name);
    virtual void remove(ISrsResource *c);
    virtual void subscribe(ISrsDisposingHandler *h);
    virtual void unsubscribe(ISrsDisposingHandler *h);
    virtual srs_error_t serve_http(ISrsHttpResponseWriter *w, ISrsHttpMessage *r);
    void reset();
};

// Mock ISrsAppConfig for testing SrsAppCasterFlv
class MockAppConfigForAppCasterFlv : public MockAppConfig
{
public:
    std::string stream_caster_output_;

public:
    MockAppConfigForAppCasterFlv();
    virtual ~MockAppConfigForAppCasterFlv();

public:
    virtual std::string get_stream_caster_output(SrsConfDirective *conf);
};

// Mock SrsHttpServeMux for testing SrsAppCasterFlv
class MockHttpServeMuxForAppCasterFlv : public SrsHttpServeMux
{
public:
    bool handle_called_;
    std::string handle_pattern_;
    ISrsHttpHandler *handle_handler_;
    srs_error_t handle_error_;

public:
    MockHttpServeMuxForAppCasterFlv();
    virtual ~MockHttpServeMuxForAppCasterFlv();

public:
    virtual srs_error_t handle(std::string pattern, ISrsHttpHandler *handler);
    void reset();
};

// Mock SrsResourceManager for testing SrsAppCasterFlv
class MockResourceManagerForAppCasterFlv : public SrsResourceManager
{
public:
    bool start_called_;
    srs_error_t start_error_;

public:
    MockResourceManagerForAppCasterFlv();
    virtual ~MockResourceManagerForAppCasterFlv();

public:
    virtual srs_error_t start();
    void reset();
};

// Mock ISrsHttpResponseReader for testing SrsDynamicHttpConn::do_proxy
class MockHttpResponseReaderForDynamicConn : public ISrsHttpResponseReader
{
public:
    std::string content_;
    size_t read_pos_;
    bool eof_;

public:
    MockHttpResponseReaderForDynamicConn();
    virtual ~MockHttpResponseReaderForDynamicConn();

public:
    virtual bool eof();
    virtual srs_error_t read(void *buf, size_t size, ssize_t *nread);
    void reset();
};

// Mock ISrsFlvDecoder for testing SrsDynamicHttpConn::do_proxy
class MockFlvDecoderForDynamicConn : public ISrsFlvDecoder
{
public:
    bool read_tag_header_called_;
    bool read_tag_data_called_;
    bool read_previous_tag_size_called_;
    srs_error_t read_tag_header_error_;
    srs_error_t read_tag_data_error_;
    srs_error_t read_previous_tag_size_error_;

    // Mock return values for read_tag_header
    char tag_type_;
    int32_t tag_size_;
    uint32_t tag_time_;

    // Mock tag data to return
    char *tag_data_;
    int tag_data_size_;

public:
    MockFlvDecoderForDynamicConn();
    virtual ~MockFlvDecoderForDynamicConn();

public:
    virtual srs_error_t initialize(ISrsReader *fr);
    virtual srs_error_t read_header(char header[9]);
    virtual srs_error_t read_tag_header(char *ptype, int32_t *pdata_size, uint32_t *ptime);
    virtual srs_error_t read_tag_data(char *data, int32_t size);
    virtual srs_error_t read_previous_tag_size(char previous_tag_size[4]);
    void reset();
};

// Mock ISrsBasicRtmpClient for testing SrsDynamicHttpConn::do_proxy
class MockRtmpClientForDynamicConn : public ISrsBasicRtmpClient
{
public:
    bool connect_called_;
    bool publish_called_;
    bool close_called_;
    bool send_and_free_message_called_;
    srs_error_t connect_error_;
    srs_error_t publish_error_;
    srs_error_t send_and_free_message_error_;
    int stream_id_;
    int send_message_count_;

public:
    MockRtmpClientForDynamicConn();
    virtual ~MockRtmpClientForDynamicConn();

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

// Mock ISrsAppFactory for testing SrsDynamicHttpConn::do_proxy
class MockAppFactoryForDynamicConn : public SrsAppFactory
{
public:
    MockRtmpClientForDynamicConn *mock_rtmp_client_;
    bool create_rtmp_client_called_;

public:
    MockAppFactoryForDynamicConn();
    virtual ~MockAppFactoryForDynamicConn();

public:
    virtual ISrsBasicRtmpClient *create_rtmp_client(std::string url, srs_utime_t cto, srs_utime_t sto);
    void reset();
};

// Mock ISrsPithyPrint for testing SrsDynamicHttpConn::do_proxy
class MockPithyPrintForDynamicConn : public ISrsPithyPrint
{
public:
    bool elapse_called_;
    bool can_print_called_;
    bool can_print_result_;
    srs_utime_t age_value_;

public:
    MockPithyPrintForDynamicConn();
    virtual ~MockPithyPrintForDynamicConn();

public:
    virtual void elapse();
    virtual bool can_print();
    virtual srs_utime_t age();
    void reset();
};

// Mock ISrsResourceManager for testing SrsDynamicHttpConn
class MockResourceManagerForDynamicConn : public ISrsResourceManager
{
public:
    bool remove_called_;
    ISrsResource *removed_resource_;

public:
    MockResourceManagerForDynamicConn();
    virtual ~MockResourceManagerForDynamicConn();

public:
    virtual srs_error_t start();
    virtual bool empty();
    virtual size_t size();
    virtual void add(ISrsResource *conn, bool *exists = NULL);
    virtual void add_with_id(const std::string &id, ISrsResource *conn);
    virtual void add_with_fast_id(uint64_t id, ISrsResource *conn);
    virtual void add_with_name(const std::string &name, ISrsResource *conn);
    virtual ISrsResource *at(int index);
    virtual ISrsResource *find_by_id(std::string id);
    virtual ISrsResource *find_by_fast_id(uint64_t id);
    virtual ISrsResource *find_by_name(std::string name);
    virtual void remove(ISrsResource *c);
    virtual void subscribe(ISrsDisposingHandler *h);
    virtual void unsubscribe(ISrsDisposingHandler *h);
    void reset();
};

// Mock ISrsHttpConn for testing SrsDynamicHttpConn
class MockHttpConnForDynamicConn : public ISrsHttpConn
{
public:
    std::string remote_ip_;
    SrsContextId context_id_;

public:
    MockHttpConnForDynamicConn();
    virtual ~MockHttpConnForDynamicConn();

public:
    virtual ISrsKbpsDelta *delta();
    virtual srs_error_t start();
    virtual srs_error_t cycle();
    virtual srs_error_t pull();
    virtual srs_error_t set_crossdomain_enabled(bool v);
    virtual srs_error_t set_auth_enabled(bool auth_enabled);
    virtual srs_error_t set_jsonp(bool v);
    virtual std::string remote_ip();
    virtual const SrsContextId &get_id();
    virtual std::string desc();
    virtual void expire();
};

// Mock ISrsHttpConnOwner for testing SrsHttpConn::cycle()
class MockHttpConnOwnerForCycle : public ISrsHttpConnOwner
{
public:
    bool on_start_called_;
    bool on_http_message_called_;
    bool on_message_done_called_;
    bool on_conn_done_called_;
    srs_error_t on_start_error_;
    srs_error_t on_http_message_error_;
    srs_error_t on_message_done_error_;
    srs_error_t on_conn_done_error_;

public:
    MockHttpConnOwnerForCycle();
    virtual ~MockHttpConnOwnerForCycle();

public:
    virtual srs_error_t on_start();
    virtual srs_error_t on_http_message(ISrsHttpMessage *r, ISrsHttpResponseWriter *w);
    virtual srs_error_t on_message_done(ISrsHttpMessage *r, ISrsHttpResponseWriter *w);
    virtual srs_error_t on_conn_done(srs_error_t r0);
    void reset();
};

// Mock ISrsHttpParser for testing SrsHttpConn::cycle()
class MockHttpParserForCycle : public ISrsHttpParser
{
public:
    bool initialize_called_;
    bool parse_message_called_;
    srs_error_t initialize_error_;
    srs_error_t parse_message_error_;
    ISrsHttpMessage *mock_message_;

public:
    MockHttpParserForCycle();
    virtual ~MockHttpParserForCycle();

public:
    virtual srs_error_t initialize(enum llhttp_type type);
    virtual void set_jsonp(bool allow_jsonp);
    virtual srs_error_t parse_message(ISrsReader *reader, ISrsHttpMessage **ppmsg);
    void reset();
};

// Mock ISrsProtocolReadWriter for testing SrsHttpConn::cycle()
class MockProtocolReadWriterForCycle : public ISrsProtocolReadWriter
{
public:
    srs_utime_t recv_timeout_;
    srs_utime_t send_timeout_;

public:
    MockProtocolReadWriterForCycle();
    virtual ~MockProtocolReadWriterForCycle();

public:
    virtual srs_error_t read_fully(void *buf, size_t size, ssize_t *nread);
    virtual srs_error_t read(void *buf, size_t size, ssize_t *nread);
    virtual void set_recv_timeout(srs_utime_t tm);
    virtual srs_utime_t get_recv_timeout();
    virtual int64_t get_recv_bytes();
    virtual srs_error_t write(void *buf, size_t size, ssize_t *nwrite);
    virtual void set_send_timeout(srs_utime_t tm);
    virtual srs_utime_t get_send_timeout();
    virtual int64_t get_send_bytes();
    virtual srs_error_t writev(const iovec *iov, int iov_size, ssize_t *nwrite);
};

// Mock ISrsCoroutine for testing SrsHttpConn::cycle()
class MockCoroutineForCycle : public ISrsCoroutine
{
public:
    bool pull_called_;
    srs_error_t pull_error_;

public:
    MockCoroutineForCycle();
    virtual ~MockCoroutineForCycle();

public:
    virtual srs_error_t start();
    virtual void stop();
    virtual void interrupt();
    virtual srs_error_t pull();
    virtual const SrsContextId &cid();
    virtual void set_cid(const SrsContextId &cid);
    void reset();
};

// Mock SrsHttpMessage for testing SrsHttpConn::cycle()
class MockHttpMessageForCycle : public SrsHttpMessage
{
public:
    bool is_keep_alive_;

public:
    MockHttpMessageForCycle();
    virtual ~MockHttpMessageForCycle();

public:
    virtual bool is_keep_alive();
    virtual ISrsRequest *to_request(std::string vhost);
};

// Mock ISrsResourceManager for testing SrsHttpxConn
class MockResourceManagerForHttpxConn : public ISrsResourceManager
{
public:
    bool remove_called_;
    ISrsResource *removed_resource_;

public:
    MockResourceManagerForHttpxConn();
    virtual ~MockResourceManagerForHttpxConn();

public:
    virtual srs_error_t start();
    virtual bool empty();
    virtual size_t size();
    virtual void add(ISrsResource *conn, bool *exists = NULL);
    virtual void add_with_id(const std::string &id, ISrsResource *conn);
    virtual void add_with_fast_id(uint64_t id, ISrsResource *conn);
    virtual void add_with_name(const std::string &name, ISrsResource *conn);
    virtual ISrsResource *at(int index);
    virtual ISrsResource *find_by_id(std::string id);
    virtual ISrsResource *find_by_fast_id(uint64_t id);
    virtual ISrsResource *find_by_name(std::string name);
    virtual void remove(ISrsResource *c);
    virtual void subscribe(ISrsDisposingHandler *h);
    virtual void unsubscribe(ISrsDisposingHandler *h);
    void reset();
};

// Mock ISrsStatistic for testing SrsHttpxConn
class MockStatisticForHttpxConn : public ISrsStatistic
{
public:
    bool on_disconnect_called_;
    bool kbps_add_delta_called_;
    std::string disconnect_id_;
    std::string kbps_id_;

public:
    MockStatisticForHttpxConn();
    virtual ~MockStatisticForHttpxConn();

public:
    virtual void on_disconnect(std::string id, srs_error_t err);
    virtual srs_error_t on_client(std::string id, ISrsRequest *req, ISrsExpire *conn, SrsRtmpConnType type);
    virtual srs_error_t on_video_info(ISrsRequest *req, SrsVideoCodecId vcodec, int avc_profile, int avc_level, int width, int height);
    virtual srs_error_t on_audio_info(ISrsRequest *req, SrsAudioCodecId acodec, SrsAudioSampleRate asample_rate, SrsAudioChannels asound_type, SrsAacObjectType aac_object);
    virtual void on_stream_publish(ISrsRequest *req, std::string publisher_id);
    virtual void on_stream_close(ISrsRequest *req);
    virtual void kbps_add_delta(std::string id, ISrsKbpsDelta *delta);
    virtual void kbps_sample();
    virtual srs_error_t dumps_vhosts(SrsJsonArray *arr);
    virtual srs_error_t dumps_streams(SrsJsonArray *arr, int start, int count);
    virtual srs_error_t dumps_clients(SrsJsonArray *arr, int start, int count);
    virtual srs_error_t dumps_metrics(int64_t &send_bytes, int64_t &recv_bytes, int64_t &nstreams, int64_t &nclients, int64_t &total_nclients, int64_t &nerrs);
    void reset();
};

// Mock ISrsHttpConn for testing SrsHttpxConn
class MockHttpConnForHttpxConn : public ISrsHttpConn
{
public:
    std::string remote_ip_;
    SrsContextId context_id_;
    ISrsKbpsDelta *delta_;

public:
    MockHttpConnForHttpxConn();
    virtual ~MockHttpConnForHttpxConn();

public:
    virtual ISrsKbpsDelta *delta();
    virtual srs_error_t start();
    virtual srs_error_t cycle();
    virtual srs_error_t pull();
    virtual srs_error_t set_crossdomain_enabled(bool v);
    virtual srs_error_t set_auth_enabled(bool auth_enabled);
    virtual srs_error_t set_jsonp(bool v);
    virtual std::string remote_ip();
    virtual const SrsContextId &get_id();
    virtual std::string desc();
    virtual void expire();
};

#endif
