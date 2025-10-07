//
// Copyright (c) 2013-2025 The SRS Authors
//
// SPDX-License-Identifier: MIT
//

#ifndef SRS_UTEST_APP13_HPP
#define SRS_UTEST_APP13_HPP

/*
#include <srs_utest_app13.hpp>
*/
#include <srs_utest.hpp>

#include <srs_app_config.hpp>
#include <srs_app_edge.hpp>
#include <srs_app_factory.hpp>
#include <srs_app_rtc_source.hpp>
#include <srs_app_rtmp_conn.hpp>
#include <srs_app_rtsp_conn.hpp>
#include <srs_app_rtsp_source.hpp>
#include <srs_app_statistic.hpp>
#include <srs_kernel_balance.hpp>
#include <srs_kernel_flv.hpp>
#include <srs_protocol_http_client.hpp>
#include <srs_protocol_http_stack.hpp>
#include <srs_protocol_json.hpp>
#include <srs_protocol_rtmp_conn.hpp>
#include <srs_protocol_rtmp_stack.hpp>
#include <srs_protocol_rtsp_stack.hpp>
#include <srs_utest_app6.hpp>
#include <srs_utest_app12.hpp>

// Mock request class for testing edge upstream
class MockEdgeRequest : public ISrsRequest
{
public:
    MockEdgeRequest(std::string vhost = "__defaultVhost__", std::string app = "live", std::string stream = "test");
    virtual ~MockEdgeRequest();
    virtual ISrsRequest *copy();
    virtual std::string get_stream_url();
    virtual void update_auth(ISrsRequest *req);
    virtual void strip();
    virtual ISrsRequest *as_http();
};

// Mock config class for testing edge upstream
class MockEdgeConfig : public MockAppConfig
{
public:
    SrsConfDirective *edge_origin_directive_;
    std::string edge_transform_vhost_;
    int chunk_size_;

public:
    MockEdgeConfig();
    virtual ~MockEdgeConfig();
    void reset();

public:
    // Override methods needed for edge upstream testing
    virtual SrsConfDirective *get_vhost_edge_origin(std::string vhost);
    virtual std::string get_vhost_edge_transform_vhost(std::string vhost);
    virtual int get_chunk_size(std::string vhost);
    virtual srs_utime_t get_vhost_edge_origin_connect_timeout(std::string vhost);
    virtual srs_utime_t get_vhost_edge_origin_stream_timeout(std::string vhost);
};

// Mock RTMP client for testing edge upstream
class MockEdgeRtmpClient : public ISrsBasicRtmpClient
{
public:
    bool connect_called_;
    bool play_called_;
    bool close_called_;
    bool recv_message_called_;
    bool decode_message_called_;
    bool set_recv_timeout_called_;
    bool kbps_sample_called_;
    srs_error_t connect_error_;
    srs_error_t play_error_;
    std::string play_stream_;
    srs_utime_t recv_timeout_;
    std::string kbps_label_;
    srs_utime_t kbps_age_;

public:
    MockEdgeRtmpClient();
    virtual ~MockEdgeRtmpClient();

public:
    virtual srs_error_t connect();
    virtual void close();
    virtual srs_error_t publish(int chunk_size, bool with_vhost = true, std::string *pstream = NULL);
    virtual srs_error_t play(int chunk_size, bool with_vhost = true, std::string *pstream = NULL);
    virtual void kbps_sample(const char *label, srs_utime_t age);
    virtual int sid();
    virtual srs_error_t recv_message(SrsRtmpCommonMessage **pmsg);
    virtual srs_error_t decode_message(SrsRtmpCommonMessage *msg, SrsRtmpCommand **ppacket);
    virtual srs_error_t send_and_free_messages(SrsMediaPacket **msgs, int nb_msgs);
    virtual srs_error_t send_and_free_message(SrsMediaPacket *msg);
    virtual void set_recv_timeout(srs_utime_t timeout);
};

// Mock app factory for testing edge upstream
class MockEdgeAppFactory : public SrsAppFactory
{
public:
    MockEdgeRtmpClient *mock_client_;

public:
    MockEdgeAppFactory();
    virtual ~MockEdgeAppFactory();

public:
    virtual ISrsBasicRtmpClient *create_rtmp_client(std::string url, srs_utime_t cto, srs_utime_t sto);
};

// Mock HTTP client for testing edge FLV upstream
class MockEdgeHttpClient : public ISrsHttpClient
{
public:
    bool initialize_called_;
    bool get_called_;
    bool set_recv_timeout_called_;
    bool kbps_sample_called_;
    srs_error_t initialize_error_;
    srs_error_t get_error_;
    ISrsHttpMessage *mock_response_;
    std::string schema_;
    std::string host_;
    int port_;
    std::string path_;
    std::string kbps_label_;
    srs_utime_t kbps_age_;

public:
    MockEdgeHttpClient();
    virtual ~MockEdgeHttpClient();

public:
    virtual srs_error_t initialize(std::string schema, std::string h, int p, srs_utime_t tm);
    virtual srs_error_t get(std::string path, std::string req, ISrsHttpMessage **ppmsg);
    virtual srs_error_t post(std::string path, std::string req, ISrsHttpMessage **ppmsg);
    virtual void set_recv_timeout(srs_utime_t tm);
    virtual void kbps_sample(const char *label, srs_utime_t age);
};

// Mock HTTP message for testing edge FLV upstream
class MockEdgeHttpMessage : public ISrsHttpMessage
{
public:
    int status_code_;
    SrsHttpHeader *header_;
    ISrsHttpResponseReader *body_reader_;

public:
    MockEdgeHttpMessage();
    virtual ~MockEdgeHttpMessage();

public:
    virtual uint8_t message_type();
    virtual uint8_t method();
    virtual uint16_t status_code();
    virtual std::string method_str();
    virtual bool is_http_get();
    virtual bool is_http_put();
    virtual bool is_http_post();
    virtual bool is_http_delete();
    virtual bool is_http_options();
    virtual std::string uri();
    virtual std::string url();
    virtual std::string host();
    virtual std::string path();
    virtual std::string query();
    virtual std::string ext();
    virtual srs_error_t body_read_all(std::string &body);
    virtual ISrsHttpResponseReader *body_reader();
    virtual int64_t content_length();
    virtual std::string query_get(std::string key);
    virtual SrsHttpHeader *header();
    virtual bool is_jsonp();
    virtual bool is_keep_alive();
    virtual std::string parse_rest_id(std::string pattern);
};

// Mock file reader for testing edge FLV upstream
class MockEdgeFileReader : public ISrsFileReader
{
public:
    char *data_;
    int size_;
    int pos_;

public:
    MockEdgeFileReader(const char *data, int size);
    virtual ~MockEdgeFileReader();

public:
    virtual srs_error_t open(std::string p);
    virtual void close();
    virtual bool is_open();
    virtual int64_t tellg();
    virtual void skip(int64_t size);
    virtual int64_t seek2(int64_t offset);
    virtual int64_t filesize();
    virtual srs_error_t read(void *buf, size_t count, ssize_t *pnread);
    virtual srs_error_t lseek(off_t offset, int whence, off_t *seeked);
};

// Mock FLV decoder for testing edge FLV upstream
class MockEdgeFlvDecoder : public ISrsFlvDecoder
{
public:
    bool initialize_called_;
    bool read_header_called_;
    bool read_previous_tag_size_called_;

public:
    MockEdgeFlvDecoder();
    virtual ~MockEdgeFlvDecoder();

public:
    virtual srs_error_t initialize(ISrsReader *fr);
    virtual srs_error_t read_header(char header[9]);
    virtual srs_error_t read_tag_header(char *ptype, int32_t *pdata_size, uint32_t *ptime);
    virtual srs_error_t read_tag_data(char *data, int32_t size);
    virtual srs_error_t read_previous_tag_size(char previous_tag_size[4]);
};

// Mock app factory for testing edge FLV upstream
class MockEdgeFlvAppFactory : public SrsAppFactory
{
public:
    MockEdgeHttpClient *mock_http_client_;
    MockEdgeFileReader *mock_file_reader_;
    MockEdgeFlvDecoder *mock_flv_decoder_;

public:
    MockEdgeFlvAppFactory();
    virtual ~MockEdgeFlvAppFactory();

public:
    virtual ISrsHttpClient *create_http_client();
    virtual ISrsFileReader *create_http_file_reader(ISrsHttpResponseReader *r);
    virtual ISrsFlvDecoder *create_flv_decoder();
};

// Mock play edge for testing SrsEdgeIngester
class MockPlayEdge : public ISrsPlayEdge
{
public:
    int on_ingest_play_count_;
    srs_error_t on_ingest_play_error_;

public:
    MockPlayEdge();
    virtual ~MockPlayEdge();

public:
    virtual srs_error_t on_ingest_play();
    void reset();
};

// Mock edge upstream for testing SrsEdgeIngester::process_publish_message
class MockEdgeUpstreamForIngester : public ISrsEdgeUpstream
{
public:
    bool decode_message_called_;
    SrsRtmpCommand *decode_message_packet_;
    srs_error_t decode_message_error_;

public:
    MockEdgeUpstreamForIngester();
    virtual ~MockEdgeUpstreamForIngester();

public:
    virtual srs_error_t connect(ISrsRequest *r, ISrsLbRoundRobin *lb);
    virtual srs_error_t recv_message(SrsRtmpCommonMessage **pmsg);
    virtual srs_error_t decode_message(SrsRtmpCommonMessage *msg, SrsRtmpCommand **ppacket);
    virtual void close();
    virtual void selected(std::string &server, int &port);
    virtual void set_recv_timeout(srs_utime_t tm);
    virtual void kbps_sample(const char *label, srs_utime_t age);
    void reset();
};

// Mock publish edge for testing SrsEdgeForwarder
class MockPublishEdge : public ISrsPublishEdge
{
public:
    MockPublishEdge();
    virtual ~MockPublishEdge();
};

// Mock edge ingester for testing SrsPlayEdge
class MockEdgeIngester : public ISrsEdgeIngester
{
public:
    bool initialize_called_;
    bool start_called_;
    bool stop_called_;
    srs_error_t initialize_error_;
    srs_error_t start_error_;

public:
    MockEdgeIngester();
    virtual ~MockEdgeIngester();

public:
    virtual srs_error_t initialize(SrsSharedPtr<SrsLiveSource> s, ISrsPlayEdge *e, ISrsRequest *r);
    virtual srs_error_t start();
    virtual void stop();
    void reset();
};

// Mock edge forwarder for testing SrsPublishEdge
class MockEdgeForwarder : public ISrsEdgeForwarder
{
public:
    bool initialize_called_;
    bool start_called_;
    bool stop_called_;
    bool set_queue_size_called_;
    bool proxy_called_;
    srs_utime_t queue_size_;
    srs_error_t initialize_error_;
    srs_error_t start_error_;
    srs_error_t proxy_error_;

public:
    MockEdgeForwarder();
    virtual ~MockEdgeForwarder();

public:
    virtual void set_queue_size(srs_utime_t queue_size);
    virtual srs_error_t initialize(SrsSharedPtr<SrsLiveSource> s, ISrsPublishEdge *e, ISrsRequest *r);
    virtual srs_error_t start();
    virtual void stop();
    virtual srs_error_t proxy(SrsRtmpCommonMessage *msg);
    void reset();
};

// Mock ISrsStatistic for testing SrsRtspPlayStream
class MockStatisticForRtspPlayStream : public ISrsStatistic
{
public:
    int on_client_count_;
    srs_error_t on_client_error_;

public:
    MockStatisticForRtspPlayStream();
    virtual ~MockStatisticForRtspPlayStream();

public:
    virtual void on_disconnect(std::string id, srs_error_t err);
    virtual srs_error_t on_client(std::string id, ISrsRequest *req, ISrsExpire *conn, SrsRtmpConnType type);
    virtual srs_error_t on_video_info(ISrsRequest *req, SrsVideoCodecId vcodec, int avc_profile, int avc_level, int width, int height);
    virtual srs_error_t on_audio_info(ISrsRequest *req, SrsAudioCodecId acodec, SrsAudioSampleRate asample_rate, SrsAudioChannels asound_type, SrsAacObjectType aac_object);
    virtual void on_stream_publish(ISrsRequest *req, std::string publisher_id);
    virtual void on_stream_close(ISrsRequest *req);
    virtual void kbps_add_delta(std::string id, ISrsKbpsDelta *delta);
    virtual void kbps_sample();
    virtual srs_error_t on_video_frames(ISrsRequest *req, int nb_frames);
    virtual std::string server_id();
    virtual std::string service_id();
    virtual std::string service_pid();
    virtual SrsStatisticVhost *find_vhost_by_id(std::string vid);
    virtual SrsStatisticStream *find_stream(std::string sid);
    virtual SrsStatisticClient *find_client(std::string client_id);
    virtual srs_error_t dumps_vhosts(SrsJsonArray *arr);
    virtual srs_error_t dumps_streams(SrsJsonArray *arr, int start, int count);
    virtual srs_error_t dumps_clients(SrsJsonArray *arr, int start, int count);
    virtual srs_error_t dumps_metrics(int64_t &send_bytes, int64_t &recv_bytes, int64_t &nstreams, int64_t &nclients, int64_t &total_nclients, int64_t &nerrs);
    void reset();
};

// Mock ISrsRtspSourceManager for testing SrsRtspPlayStream
class MockRtspSourceManager : public ISrsRtspSourceManager
{
public:
    int fetch_or_create_count_;
    srs_error_t fetch_or_create_error_;
    SrsSharedPtr<SrsRtspSource> mock_source_;

public:
    MockRtspSourceManager();
    virtual ~MockRtspSourceManager();

public:
    virtual srs_error_t initialize();
    virtual srs_error_t fetch_or_create(ISrsRequest *r, SrsSharedPtr<SrsRtspSource> &pps);
    virtual SrsSharedPtr<SrsRtspSource> fetch(ISrsRequest *r);
    void reset();
};

// Mock ISrsRtspSendTrack for testing SrsRtspPlayStream
class MockRtspSendTrack : public ISrsRtspSendTrack
{
public:
    std::string track_id_;
    SrsRtcTrackDescription *track_desc_;
    bool track_status_;
    int on_rtp_count_;
    uint32_t last_ssrc_;
    uint16_t last_sequence_;

public:
    MockRtspSendTrack(std::string track_id, SrsRtcTrackDescription *desc);
    virtual ~MockRtspSendTrack();

public:
    virtual bool set_track_status(bool active);
    virtual std::string get_track_id();
    virtual SrsRtcTrackDescription *track_desc();
    virtual srs_error_t on_rtp(SrsRtpPacket *pkt);
    void reset();
};

// Mock ISrsAppFactory for testing SrsRtspPlayStream
class MockAppFactoryForRtspPlayStream : public SrsAppFactory
{
public:
    int create_rtsp_audio_send_track_count_;
    int create_rtsp_video_send_track_count_;

public:
    MockAppFactoryForRtspPlayStream();
    virtual ~MockAppFactoryForRtspPlayStream();

public:
    virtual ISrsRtspSendTrack *create_rtsp_audio_send_track(ISrsRtspConnection *session, SrsRtcTrackDescription *track_desc);
    virtual ISrsRtspSendTrack *create_rtsp_video_send_track(ISrsRtspConnection *session, SrsRtcTrackDescription *track_desc);
    void reset();
};

// Mock ISrsRtspStack for testing SrsRtspConnection
class MockRtspStack : public ISrsRtspStack
{
public:
    bool send_message_called_;
    int last_response_seq_;
    std::string last_response_session_;
    std::string last_response_type_;  // "OPTIONS", "DESCRIBE", "SETUP", "PLAY", "TEARDOWN"
    srs_error_t send_message_error_;

public:
    MockRtspStack();
    virtual ~MockRtspStack();

public:
    virtual srs_error_t recv_message(SrsRtspRequest **preq);
    virtual srs_error_t send_message(SrsRtspResponse *res);
    void reset();
};

// Mock ISrsRtspPlayStream for testing SrsRtspConnection::do_play and do_teardown
class MockRtspPlayStream : public ISrsRtspPlayStream
{
public:
    bool initialize_called_;
    bool start_called_;
    bool stop_called_;
    bool set_all_tracks_status_called_;
    bool set_all_tracks_status_value_;
    srs_error_t initialize_error_;
    srs_error_t start_error_;

public:
    MockRtspPlayStream();
    virtual ~MockRtspPlayStream();

public:
    virtual srs_error_t initialize(ISrsRequest *request, std::map<uint32_t, SrsRtcTrackDescription *> sub_relations);
    virtual srs_error_t start();
    virtual void stop();
    virtual void set_all_tracks_status(bool status);
    void reset();
};

#endif
