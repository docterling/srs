//
// Copyright (c) 2013-2025 The SRS Authors
//
// SPDX-License-Identifier: MIT
//

#ifndef SRS_UTEST_APP10_HPP
#define SRS_UTEST_APP10_HPP

/*
#include <srs_utest_app10.hpp>
*/
#include <srs_utest.hpp>
#include <srs_utest_app6.hpp>
#include <srs_protocol_http_stack.hpp>
#include <srs_kernel_hourglass.hpp>
#include <srs_app_factory.hpp>
#include <srs_app_rtc_server.hpp>
#include <srs_app_heartbeat.hpp>

// Mock config for testing SrsServer::listen()
class MockAppConfigForServerListen : public MockAppConfig
{
public:
    std::vector<std::string> rtmp_listens_;
    bool rtmps_enabled_;
    std::vector<std::string> rtmps_listens_;
    bool http_api_enabled_;
    std::vector<std::string> http_api_listens_;
    bool https_api_enabled_;
    std::vector<std::string> https_api_listens_;
    bool http_stream_enabled_;
    std::vector<std::string> http_stream_listens_;
    bool https_stream_enabled_;
    std::vector<std::string> https_stream_listens_;
    bool rtc_server_tcp_enabled_;
    std::vector<std::string> rtc_server_tcp_listens_;
    std::string rtc_server_protocol_;
    bool rtsp_server_enabled_;
    std::vector<std::string> rtsp_server_listens_;
    bool exporter_enabled_;
    std::string exporter_listen_;

public:
    MockAppConfigForServerListen();
    virtual ~MockAppConfigForServerListen();

public:
    virtual std::vector<std::string> get_listens();
    virtual bool get_rtmps_enabled();
    virtual std::vector<std::string> get_rtmps_listen();
    virtual bool get_http_api_enabled();
    virtual std::vector<std::string> get_http_api_listens();
    virtual bool get_https_api_enabled();
    virtual std::vector<std::string> get_https_api_listens();
    virtual bool get_http_stream_enabled();
    virtual std::vector<std::string> get_http_stream_listens();
    virtual bool get_https_stream_enabled();
    virtual std::vector<std::string> get_https_stream_listens();
    virtual bool get_rtc_server_tcp_enabled();
    virtual std::vector<std::string> get_rtc_server_tcp_listens();
    virtual std::string get_rtc_server_protocol();
    virtual bool get_rtsp_server_enabled();
    virtual std::vector<std::string> get_rtsp_server_listens();
    virtual bool get_exporter_enabled();
    virtual std::string get_exporter_listen();
};

// Mock ISrsHttpServeMux for testing SrsServer::http_handle()
class MockHttpServeMux : public ISrsHttpServeMux
{
public:
    int handle_count_;
    std::vector<std::string> patterns_;

public:
    MockHttpServeMux();
    virtual ~MockHttpServeMux();

public:
    virtual srs_error_t handle(std::string pattern, ISrsHttpHandler *handler);
    virtual srs_error_t serve_http(ISrsHttpResponseWriter *w, ISrsHttpMessage *r);
};

// Mock ISrsLog for testing SrsServer::on_signal()
class MockLogForSignal : public ISrsLog
{
public:
    int reopen_count_;

public:
    MockLogForSignal();
    virtual ~MockLogForSignal();

public:
    virtual srs_error_t initialize();
    virtual void reopen();
    virtual void log(SrsLogLevel level, const char *tag, const SrsContextId &context_id, const char *fmt, va_list args);
};

// Mock ISrsAppConfig for testing SrsServer::on_signal()
class MockAppConfigForSignal : public MockAppConfig
{
public:
    bool force_grace_quit_;

public:
    MockAppConfigForSignal();
    virtual ~MockAppConfigForSignal();

public:
    virtual bool is_force_grace_quit();
};

// Mock ISrsAppConfig for testing SrsServer::do2_cycle()
class MockAppConfigForDo2Cycle : public MockAppConfig
{
public:
    srs_error_t reload_error_;
    srs_error_t persistence_error_;
    int reload_count_;
    int persistence_count_;
    SrsReloadState reload_state_;

public:
    MockAppConfigForDo2Cycle();
    virtual ~MockAppConfigForDo2Cycle();

public:
    virtual srs_error_t reload(SrsReloadState *pstate);
    virtual srs_error_t persistence();
    void reset();
};

// Mock ISrsHourGlass for testing SrsServer::setup_ticks()
class MockHourGlassForSetupTicks : public ISrsHourGlass
{
public:
    int tick_count_;
    int start_count_;
    std::vector<int> tick_events_;
    std::vector<srs_utime_t> tick_intervals_;

public:
    MockHourGlassForSetupTicks();
    virtual ~MockHourGlassForSetupTicks();

public:
    virtual srs_error_t start();
    virtual void stop();
    virtual srs_error_t tick(srs_utime_t interval);
    virtual srs_error_t tick(int event, srs_utime_t interval);
    virtual void untick(int event);
};

// Mock SrsAppFactory for testing SrsServer::setup_ticks()
class MockAppFactoryForSetupTicks : public SrsAppFactory
{
public:
    MockHourGlassForSetupTicks *mock_hourglass_;

public:
    MockAppFactoryForSetupTicks();
    virtual ~MockAppFactoryForSetupTicks();

public:
    virtual ISrsHourGlass *create_hourglass(const std::string &name, ISrsHourGlassHandler *handler, srs_utime_t interval);
};

// Mock ISrsAppConfig for testing SrsServer::setup_ticks()
class MockAppConfigForSetupTicks : public MockAppConfig
{
public:
    bool stats_enabled_;
    bool heartbeat_enabled_;
    srs_utime_t heartbeat_interval_;

public:
    MockAppConfigForSetupTicks();
    virtual ~MockAppConfigForSetupTicks();

public:
    virtual bool get_stats_enabled();
    virtual bool get_heartbeat_enabled();
    virtual srs_utime_t get_heartbeat_interval();
};

// Mock SrsRtcSessionManager for testing SrsServer::notify()
class MockRtcSessionManagerForNotify : public SrsRtcSessionManager
{
public:
    int update_rtc_sessions_count_;

public:
    MockRtcSessionManagerForNotify();
    virtual ~MockRtcSessionManagerForNotify();

public:
    virtual void srs_update_rtc_sessions();
};

// Mock SrsHttpHeartbeat for testing SrsServer::notify()
class MockHttpHeartbeatForNotify : public SrsHttpHeartbeat
{
public:
    int heartbeat_count_;

public:
    MockHttpHeartbeatForNotify();
    virtual ~MockHttpHeartbeatForNotify();

public:
    virtual void heartbeat();
};

// Mock connection manager for testing SrsServer::resample_kbps()
class MockConnectionManagerForResampleKbps : public ISrsResourceManager
{
public:
    std::vector<ISrsResource *> connections_;

public:
    MockConnectionManagerForResampleKbps();
    virtual ~MockConnectionManagerForResampleKbps();

public:
    virtual srs_error_t start();
    virtual bool empty();
    virtual size_t size();
    virtual void add(ISrsResource *conn, bool *exists = NULL);
    virtual ISrsResource *at(int index);
    virtual void remove(ISrsResource *c);
    virtual void subscribe(ISrsDisposingHandler *h);
    virtual void unsubscribe(ISrsDisposingHandler *h);
};

// Mock statistic for testing SrsServer::resample_kbps()
class MockStatisticForResampleKbps : public ISrsStatistic
{
public:
    int kbps_add_delta_count_;
    int kbps_sample_count_;

public:
    MockStatisticForResampleKbps();
    virtual ~MockStatisticForResampleKbps();

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
    void reset();
};

// Mock config for testing SrsServer::on_before_connection()
class MockAppConfigForConnectionLimit : public MockAppConfig
{
public:
    int max_connections_;

public:
    MockAppConfigForConnectionLimit();
    virtual ~MockAppConfigForConnectionLimit();

public:
    virtual int get_max_connections();
};

// Mock connection manager for testing SrsServer::on_before_connection()
class MockConnectionManagerForConnectionLimit : public ISrsResourceManager
{
public:
    size_t connection_count_;

public:
    MockConnectionManagerForConnectionLimit();
    virtual ~MockConnectionManagerForConnectionLimit();

public:
    virtual srs_error_t start();
    virtual bool empty();
    virtual size_t size();
    virtual void add(ISrsResource *conn, bool *exists = NULL);
    virtual ISrsResource *at(int index);
    virtual void remove(ISrsResource *c);
    virtual void subscribe(ISrsDisposingHandler *h);
    virtual void unsubscribe(ISrsDisposingHandler *h);
};

#endif
