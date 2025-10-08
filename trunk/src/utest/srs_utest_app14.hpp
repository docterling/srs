//
// Copyright (c) 2013-2025 The SRS Authors
//
// SPDX-License-Identifier: MIT
//

#ifndef SRS_UTEST_APP14_HPP
#define SRS_UTEST_APP14_HPP

/*
#include <srs_utest_app14.hpp>
*/
#include <srs_utest.hpp>

#include <srs_app_gb28181.hpp>
#include <srs_app_config.hpp>
#include <srs_app_server.hpp>
#include <srs_app_factory.hpp>
#include <srs_app_dvr.hpp>
#include <srs_protocol_http_stack.hpp>
#include <srs_protocol_rtmp_conn.hpp>
#include <srs_protocol_raw_avc.hpp>
#include <srs_protocol_http_client.hpp>
#include <srs_utest_app6.hpp>
#include <srs_utest_app11.hpp>

#ifdef SRS_RTSP
#include <srs_app_rtsp_conn.hpp>
#endif

// Mock ISrsGbMuxer for testing SrsGbSession
class MockGbMuxer : public ISrsGbMuxer
{
public:
    bool setup_called_;
    std::string setup_output_;
    bool on_ts_message_called_;
    srs_error_t on_ts_message_error_;

public:
    MockGbMuxer();
    virtual ~MockGbMuxer();

public:
    virtual void setup(std::string output);
    virtual srs_error_t on_ts_message(SrsTsMessage *msg);
    void reset();
};

// Mock ISrsAppConfig for testing SrsGbSession
class MockAppConfigForGbSession : public MockAppConfig
{
public:
    std::string stream_caster_output_;

public:
    MockAppConfigForGbSession();
    virtual ~MockAppConfigForGbSession();

public:
    virtual std::string get_stream_caster_output(SrsConfDirective *conf);
    void set_stream_caster_output(const std::string &output);
};

// Mock ISrsPackContext for testing SrsGbSession::on_ps_pack
class MockPackContext : public ISrsPackContext
{
public:
    MockPackContext();
    virtual ~MockPackContext();

public:
    virtual srs_error_t on_ts_message(SrsTsMessage *msg);
    virtual void on_recover_mode(int nn_recover);
};

// Mock ISrsGbMediaTcpConn for testing SrsGbSession::on_media_transport
class MockGbMediaTcpConn : public ISrsGbMediaTcpConn
{
public:
    bool set_cid_called_;
    SrsContextId received_cid_;
    bool is_connected_;

public:
    MockGbMediaTcpConn();
    virtual ~MockGbMediaTcpConn();

public:
    virtual void setup(srs_netfd_t stfd);
    virtual void setup_owner(SrsSharedResource<ISrsGbMediaTcpConn> *wrapper, ISrsInterruptable *owner_coroutine, ISrsContextIdSetter *owner_cid);
    virtual bool is_connected();
    virtual void interrupt();
    virtual void set_cid(const SrsContextId &cid);
    virtual const SrsContextId &get_id();
    virtual std::string desc();
    virtual srs_error_t cycle();
    virtual void on_executor_done(ISrsInterruptable *executor);
    void reset();
};

// Mock ISrsIpListener for testing SrsGbListener::initialize
class MockIpListener : public ISrsIpListener
{
public:
    std::string endpoint_ip_;
    int endpoint_port_;
    std::string label_;
    bool set_endpoint_called_;
    bool set_label_called_;

public:
    MockIpListener();
    virtual ~MockIpListener();

public:
    virtual ISrsListener *set_endpoint(const std::string &i, int p);
    virtual ISrsListener *set_label(const std::string &label);
    virtual srs_error_t listen();
    void reset();
};

// Mock ISrsAppConfig for testing SrsGbListener::initialize
class MockAppConfigForGbListener : public MockAppConfig
{
public:
    int stream_caster_listen_port_;

public:
    MockAppConfigForGbListener();
    virtual ~MockAppConfigForGbListener();

public:
    virtual int get_stream_caster_listen(SrsConfDirective *conf);
};

// Mock ISrsHttpServeMux for testing SrsGbListener::listen_api
class MockHttpServeMuxForGbListener : public ISrsHttpServeMux
{
public:
    bool handle_called_;
    std::string handle_pattern_;
    ISrsHttpHandler *handle_handler_;

public:
    MockHttpServeMuxForGbListener();
    virtual ~MockHttpServeMuxForGbListener();

public:
    virtual srs_error_t handle(std::string pattern, ISrsHttpHandler *handler);
    virtual srs_error_t serve_http(ISrsHttpResponseWriter *w, ISrsHttpMessage *r);
    void reset();
};

// Mock ISrsApiServerOwner for testing SrsGbListener::listen_api
class MockApiServerOwnerForGbListener : public ISrsApiServerOwner
{
public:
    ISrsHttpServeMux *mux_;

public:
    MockApiServerOwnerForGbListener();
    virtual ~MockApiServerOwnerForGbListener();

public:
    virtual ISrsHttpServeMux *api_server();
};

// Mock ISrsIpListener for testing SrsGbListener::listen
class MockIpListenerForGbListen : public ISrsIpListener
{
public:
    bool listen_called_;

public:
    MockIpListenerForGbListen();
    virtual ~MockIpListenerForGbListen();

public:
    virtual ISrsListener *set_endpoint(const std::string &i, int p);
    virtual ISrsListener *set_label(const std::string &label);
    virtual srs_error_t listen();
    void reset();
};

// Mock ISrsGbSession for testing SrsGbMediaTcpConn::on_ps_pack
class MockGbSessionForMediaConn : public ISrsGbSession
{
public:
    bool on_ps_pack_called_;
    ISrsPackContext *received_pack_;
    SrsPsPacket *received_ps_;
    std::vector<SrsTsMessage *> received_msgs_;
    bool on_media_transport_called_;
    SrsSharedResource<ISrsGbMediaTcpConn> received_media_;

public:
    MockGbSessionForMediaConn();
    virtual ~MockGbSessionForMediaConn();

public:
    virtual void setup(SrsConfDirective *conf);
    virtual void setup_owner(SrsSharedResource<ISrsGbSession> *wrapper, ISrsInterruptable *owner_coroutine, ISrsContextIdSetter *owner_cid);
    virtual void on_media_transport(SrsSharedResource<ISrsGbMediaTcpConn> media);
    virtual void on_ps_pack(ISrsPackContext *ctx, SrsPsPacket *ps, const std::vector<SrsTsMessage *> &msgs);
    virtual const SrsContextId &get_id();
    virtual std::string desc();
    virtual srs_error_t cycle();
    virtual void on_executor_done(ISrsInterruptable *executor);
    void reset();
};

// Mock ISrsResourceManager for testing SrsGbMediaTcpConn::bind_session
class MockResourceManagerForBindSession : public ISrsResourceManager
{
public:
    ISrsResource *session_to_return_;

public:
    MockResourceManagerForBindSession();
    virtual ~MockResourceManagerForBindSession();

public:
    virtual srs_error_t start();
    virtual bool empty();
    virtual size_t size();
    virtual void add(ISrsResource *conn, bool *exists = NULL);
    virtual void add_with_id(const std::string &id, ISrsResource *conn);
    virtual void add_with_fast_id(uint64_t id, ISrsResource *conn);
    virtual ISrsResource *at(int index);
    virtual ISrsResource *find_by_id(std::string id);
    virtual ISrsResource *find_by_fast_id(uint64_t id);
    virtual ISrsResource *find_by_name(std::string name);
    virtual void remove(ISrsResource *c);
    virtual void subscribe(ISrsDisposingHandler *h);
    virtual void unsubscribe(ISrsDisposingHandler *h);
    void reset();
};

// Mock ISrsBasicRtmpClient for testing SrsGbMuxer
class MockGbRtmpClient : public ISrsBasicRtmpClient
{
public:
    bool connect_called_;
    bool publish_called_;
    bool close_called_;
    srs_error_t connect_error_;
    srs_error_t publish_error_;
    int stream_id_;

public:
    MockGbRtmpClient();
    virtual ~MockGbRtmpClient();

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
    void reset();
};

// Mock ISrsRawAacStream for testing SrsGbMuxer
class MockGbRawAacStream : public ISrsRawAacStream
{
public:
    bool adts_demux_called_;
    bool mux_sequence_header_called_;
    bool mux_aac2flv_called_;
    srs_error_t adts_demux_error_;
    srs_error_t mux_sequence_header_error_;
    srs_error_t mux_aac2flv_error_;
    std::string sequence_header_output_;
    int demux_frame_size_;

public:
    MockGbRawAacStream();
    virtual ~MockGbRawAacStream();

public:
    virtual srs_error_t adts_demux(SrsBuffer *stream, char **pframe, int *pnb_frame, SrsRawAacStreamCodec &codec);
    virtual srs_error_t mux_sequence_header(SrsRawAacStreamCodec *codec, std::string &sh);
    virtual srs_error_t mux_aac2flv(char *frame, int nb_frame, SrsRawAacStreamCodec *codec, uint32_t dts, char **flv, int *nb_flv);
    void reset();
};

// Mock ISrsMpegpsQueue for testing SrsGbMuxer
class MockGbMpegpsQueue : public ISrsMpegpsQueue
{
public:
    bool push_called_;
    srs_error_t push_error_;
    int push_count_;

public:
    MockGbMpegpsQueue();
    virtual ~MockGbMpegpsQueue();

public:
    virtual srs_error_t push(SrsMediaPacket *msg);
    virtual SrsMediaPacket *dequeue();
    void reset();
};

// Mock ISrsGbSession for testing SrsGbMuxer
class MockGbSessionForMuxer : public ISrsGbSession
{
public:
    std::string device_id_;

public:
    MockGbSessionForMuxer();
    virtual ~MockGbSessionForMuxer();

public:
    virtual void setup(SrsConfDirective *conf);
    virtual void setup_owner(SrsSharedResource<ISrsGbSession> *wrapper, ISrsInterruptable *owner_coroutine, ISrsContextIdSetter *owner_cid);
    virtual void on_media_transport(SrsSharedResource<ISrsGbMediaTcpConn> media);
    virtual void on_ps_pack(ISrsPackContext *ctx, SrsPsPacket *ps, const std::vector<SrsTsMessage *> &msgs);
    virtual const SrsContextId &get_id();
    virtual std::string desc();
    virtual srs_error_t cycle();
    virtual void on_executor_done(ISrsInterruptable *executor);
};

// Mock ISrsPsPackHandler for testing SrsPackContext
class MockPsPackHandler : public ISrsPsPackHandler
{
public:
    bool on_ps_pack_called_;
    int on_ps_pack_count_;
    uint32_t last_pack_id_;
    int last_msgs_count_;
    srs_error_t on_ps_pack_error_;

public:
    MockPsPackHandler();
    virtual ~MockPsPackHandler();

public:
    virtual srs_error_t on_ps_pack(SrsPsPacket *ps, const std::vector<SrsTsMessage *> &msgs);
    void reset();
};

// Mock ISrsHttpMessage for testing SrsGoApiGbPublish
class MockHttpMessageForGbPublish : public SrsHttpMessage
{
public:
    std::string body_content_;
    MockHttpConn *mock_conn_;

public:
    MockHttpMessageForGbPublish();
    virtual ~MockHttpMessageForGbPublish();

public:
    virtual srs_error_t body_read_all(std::string &body);
};

// Mock ISrsResourceManager for testing SrsGoApiGbPublish
class MockResourceManagerForGbPublish : public ISrsResourceManager
{
public:
    std::map<std::string, ISrsResource*> id_map_;
    std::map<uint64_t, ISrsResource*> fast_id_map_;

public:
    MockResourceManagerForGbPublish();
    virtual ~MockResourceManagerForGbPublish();

public:
    virtual srs_error_t start();
    virtual bool empty();
    virtual size_t size();
    virtual void add(ISrsResource *conn, bool *exists = NULL);
    virtual void add_with_id(const std::string &id, ISrsResource *conn);
    virtual void add_with_fast_id(uint64_t id, ISrsResource *conn);
    virtual ISrsResource *at(int index);
    virtual ISrsResource *find_by_id(std::string id);
    virtual ISrsResource *find_by_fast_id(uint64_t id);
    virtual ISrsResource *find_by_name(std::string name);
    virtual void remove(ISrsResource *c);
    virtual void subscribe(ISrsDisposingHandler *h);
    virtual void unsubscribe(ISrsDisposingHandler *h);
    void reset();
};

// Mock ISrsGbSession for testing SrsGoApiGbPublish
class MockGbSessionForApiPublish : public ISrsGbSession
{
public:
    bool setup_called_;
    bool setup_owner_called_;
    SrsConfDirective *setup_conf_;
    ISrsInterruptable *owner_coroutine_;

public:
    MockGbSessionForApiPublish();
    virtual ~MockGbSessionForApiPublish();

public:
    virtual void setup(SrsConfDirective *conf);
    virtual void setup_owner(SrsSharedResource<ISrsGbSession> *wrapper, ISrsInterruptable *owner_coroutine, ISrsContextIdSetter *owner_cid);
    virtual void on_media_transport(SrsSharedResource<ISrsGbMediaTcpConn> media);
    virtual void on_ps_pack(ISrsPackContext *ctx, SrsPsPacket *ps, const std::vector<SrsTsMessage *> &msgs);
    virtual const SrsContextId &get_id();
    virtual std::string desc();
    virtual srs_error_t cycle();
    virtual void on_executor_done(ISrsInterruptable *executor);
    void reset();
};

// Mock ISrsAppFactory for testing SrsGoApiGbPublish
class MockAppFactoryForGbPublish : public ISrsAppFactory
{
public:
    MockGbSessionForApiPublish *mock_gb_session_;

public:
    MockAppFactoryForGbPublish();
    virtual ~MockAppFactoryForGbPublish();

public:
    virtual ISrsFileWriter *create_file_writer();
    virtual ISrsFileWriter *create_enc_file_writer();
    virtual ISrsFileReader *create_file_reader();
    virtual SrsPath *create_path();
    virtual SrsLiveSource *create_live_source();
    virtual ISrsOriginHub *create_origin_hub();
    virtual ISrsHourGlass *create_hourglass(const std::string &name, ISrsHourGlassHandler *handler, srs_utime_t interval);
    virtual ISrsBasicRtmpClient *create_rtmp_client(std::string url, srs_utime_t cto, srs_utime_t sto);
    virtual SrsHttpClient *create_http_client();
    virtual ISrsHttpResponseReader *create_http_response_reader(ISrsHttpResponseReader *r);
    virtual ISrsFileReader *create_http_file_reader(ISrsHttpResponseReader *r);
    virtual ISrsFlvDecoder *create_flv_decoder();
    virtual ISrsBasicRtmpClient *create_basic_rtmp_client(std::string url, srs_utime_t ctm, srs_utime_t stm);
#ifdef SRS_RTSP
    virtual ISrsRtspSendTrack *create_rtsp_audio_send_track(ISrsRtspConnection *session, SrsRtcTrackDescription *track_desc);
    virtual ISrsRtspSendTrack *create_rtsp_video_send_track(ISrsRtspConnection *session, SrsRtcTrackDescription *track_desc);
#endif
    virtual ISrsFlvTransmuxer *create_flv_transmuxer();
    virtual ISrsMp4Encoder *create_mp4_encoder();
    virtual SrsDvrFlvSegmenter *create_dvr_flv_segmenter();
    virtual SrsDvrMp4Segmenter *create_dvr_mp4_segmenter();
    virtual ISrsGbMediaTcpConn *create_gb_media_tcp_conn();
    virtual ISrsGbSession *create_gb_session();
    void reset();
};

#endif

