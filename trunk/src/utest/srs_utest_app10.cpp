//
// Copyright (c) 2013-2025 The SRS Authors
//
// SPDX-License-Identifier: MIT
//
#include <srs_utest_app10.hpp>

using namespace std;

#include <srs_app_server.hpp>
#include <srs_app_rtmp_conn.hpp>
#include <srs_kernel_error.hpp>
#include <srs_kernel_utility.hpp>
#include <srs_kernel_consts.hpp>
#include <srs_kernel_hourglass.hpp>
#include <srs_app_factory.hpp>
#include <algorithm>

// Mock config implementation for SrsServer::listen() testing
MockAppConfigForServerListen::MockAppConfigForServerListen()
{
    rtmps_enabled_ = false;
    http_api_enabled_ = false;
    https_api_enabled_ = false;
    http_stream_enabled_ = false;
    https_stream_enabled_ = false;
    rtc_server_tcp_enabled_ = false;
    rtc_server_protocol_ = "udp";
    rtsp_server_enabled_ = false;
    exporter_enabled_ = false;
}

MockAppConfigForServerListen::~MockAppConfigForServerListen()
{
}

std::vector<std::string> MockAppConfigForServerListen::get_listens()
{
    return rtmp_listens_;
}

bool MockAppConfigForServerListen::get_rtmps_enabled()
{
    return rtmps_enabled_;
}

std::vector<std::string> MockAppConfigForServerListen::get_rtmps_listen()
{
    return rtmps_listens_;
}

bool MockAppConfigForServerListen::get_http_api_enabled()
{
    return http_api_enabled_;
}

std::vector<std::string> MockAppConfigForServerListen::get_http_api_listens()
{
    return http_api_listens_;
}

bool MockAppConfigForServerListen::get_https_api_enabled()
{
    return https_api_enabled_;
}

std::vector<std::string> MockAppConfigForServerListen::get_https_api_listens()
{
    return https_api_listens_;
}

bool MockAppConfigForServerListen::get_http_stream_enabled()
{
    return http_stream_enabled_;
}

std::vector<std::string> MockAppConfigForServerListen::get_http_stream_listens()
{
    return http_stream_listens_;
}

bool MockAppConfigForServerListen::get_https_stream_enabled()
{
    return https_stream_enabled_;
}

std::vector<std::string> MockAppConfigForServerListen::get_https_stream_listens()
{
    return https_stream_listens_;
}

bool MockAppConfigForServerListen::get_rtc_server_tcp_enabled()
{
    return rtc_server_tcp_enabled_;
}

std::vector<std::string> MockAppConfigForServerListen::get_rtc_server_tcp_listens()
{
    return rtc_server_tcp_listens_;
}

std::string MockAppConfigForServerListen::get_rtc_server_protocol()
{
    return rtc_server_protocol_;
}

bool MockAppConfigForServerListen::get_rtsp_server_enabled()
{
    return rtsp_server_enabled_;
}

std::vector<std::string> MockAppConfigForServerListen::get_rtsp_server_listens()
{
    return rtsp_server_listens_;
}

bool MockAppConfigForServerListen::get_exporter_enabled()
{
    return exporter_enabled_;
}

std::string MockAppConfigForServerListen::get_exporter_listen()
{
    return exporter_listen_;
}

// Test SrsServer constructor and destructor to ensure proper initialization
// and cleanup of all server components including listeners, managers, and
// WebRTC session management.
VOID TEST(SrsServerTest, ConstructorAndDestructor)
{
    // Create SrsServer instance - tests constructor initialization
    SrsUniquePtr<SrsServer> server(new SrsServer());

    // Verify that the server object was created successfully
    EXPECT_TRUE(server.get() != NULL);

    // The destructor will be called automatically when server goes out of scope
    // This tests proper cleanup of all components:
    // - Signal manager
    // - HTTP API mux
    // - All protocol listeners (RTMP, RTMPS, HTTP, HTTPS, WebRTC, RTSP, etc.)
    // - Stream casters
    // - HTTP server and heartbeat
    // - Ingester and timer
    // - WebRTC session manager
    // - PID file locker
}

// Test SrsServer::initialize() method to verify proper initialization sequence
// including PID file locking, SRT event loop, DTLS certificate, DVR async worker,
// config subscription, HTTP API/server initialization, blackhole, and WebRTC
// session manager. This test covers the major use scenario of server initialization.
VOID TEST(SrsServerTest, InitializeSuccess)
{
    srs_error_t err = srs_success;

    // Create SrsServer instance
    SrsUniquePtr<SrsServer> server(new SrsServer());
    EXPECT_TRUE(server.get() != NULL);

    // Call initialize() - this is the main test
    // This will initialize all server components in the correct sequence:
    // 1. PID file locker acquisition
    // 2. SRT log and event loop initialization
    // 3. WebRTC DTLS certificate initialization
    // 4. DVR async call worker start
    // 5. Config subscription
    // 6. HTTP stream/API configuration check and port reuse logic
    // 7. HTTP API mux initialization (if not reusing)
    // 8. HTTP server initialization
    // 9. WebRTC blackhole initialization
    // 10. WebRTC session manager initialization
    HELPER_EXPECT_SUCCESS(server->initialize());

    // Verify that the server was initialized successfully
    // The fact that initialize() returned success means all components
    // were initialized properly without errors
    EXPECT_TRUE(server.get() != NULL);
}

// Test SrsServer::listen() method to verify proper listener setup for RTMP protocol.
// This test covers the major use scenario of starting RTMP listener on a random port.
VOID TEST(SrsServerTest, ListenRtmpSuccess)
{
    srs_error_t err = srs_success;

    // Generate random port in range [30000, 60000]
    SrsRand rand;
    int port = rand.integer(30000, 60000);
    std::string listen_addr = srs_strconv_format_int(port);

    // Create mock config with RTMP listening enabled
    MockAppConfigForServerListen mock_config;
    mock_config.rtmp_listens_.push_back(listen_addr);

    // Create SrsServer instance
    SrsUniquePtr<SrsServer> server(new SrsServer());
    EXPECT_TRUE(server.get() != NULL);

    // Inject mock config
    server->config_ = &mock_config;

    // Initialize server first (required before listen)
    HELPER_EXPECT_SUCCESS(server->initialize());

    // Call listen() - this is the main test
    // This will:
    // 1. Create RTMP listener on the configured port
    // 2. Start listening for incoming RTMP connections
    // 3. Start connection manager
    HELPER_EXPECT_SUCCESS(server->listen());

    // Verify that the server is listening
    // The fact that listen() returned success means:
    // - RTMP listener was created successfully
    // - Socket binding succeeded on the random port
    // - Connection manager started successfully
    EXPECT_TRUE(server.get() != NULL);

    // Cleanup: restore original config to avoid side effects
    server->config_ = _srs_config;
}

MockHttpServeMux::MockHttpServeMux()
{
    handle_count_ = 0;
}

MockHttpServeMux::~MockHttpServeMux()
{
}

srs_error_t MockHttpServeMux::handle(std::string pattern, ISrsHttpHandler *handler)
{
    handle_count_++;
    patterns_.push_back(pattern);
    // Free the handler since we're not actually using it
    srs_freep(handler);
    return srs_success;
}

srs_error_t MockHttpServeMux::serve_http(ISrsHttpResponseWriter *w, ISrsHttpMessage *r)
{
    return srs_success;
}

// Test SrsServer::http_handle() method to verify proper HTTP API handler registration.
// This test covers the major use scenario of registering all HTTP API endpoints including
// root API, versioning, summaries, rusages, stats, meminfos, authors, features, vhosts,
// streams, clients, raw API, clusters, test endpoints, metrics, console, and WebRTC APIs.
VOID TEST(SrsServerTest, HttpHandleSuccess)
{
    srs_error_t err = srs_success;

    // Create mock HTTP API mux
    MockHttpServeMux* mock_mux = new MockHttpServeMux();

    // Create SrsServer instance
    SrsUniquePtr<SrsServer> server(new SrsServer());
    EXPECT_TRUE(server.get() != NULL);

    // Inject mock HTTP API mux
    ISrsHttpServeMux* original_mux = server->http_api_mux_;
    server->http_api_mux_ = mock_mux;

    // Set reuse_api_over_server_ to false to test all handler registrations
    server->reuse_api_over_server_ = false;

    // Call http_handle() - this is the main test
    // This will register all HTTP API handlers:
    // 1. Root API (/)
    // 2. API version (/api/v1/versions)
    // 3. API endpoints (/api/, /api/v1/)
    // 4. Server stats (/api/v1/summaries, /api/v1/rusages, etc.)
    // 5. System info (/api/v1/meminfos, /api/v1/authors, /api/v1/features)
    // 6. Stream management (/api/v1/vhosts/, /api/v1/streams/, /api/v1/clients/)
    // 7. Raw API (/api/v1/raw)
    // 8. Cluster API (/api/v1/clusters)
    // 9. Test endpoints (/api/v1/tests/*)
    // 10. Metrics (/metrics)
    // 11. Console (/console/)
    // 12. WebRTC APIs (via listen_rtc_api())
    HELPER_EXPECT_SUCCESS(server->http_handle());

    // Verify that handlers were registered
    // The exact count depends on conditional compilation flags (SRS_GPERF, SRS_VALGRIND, SRS_SIGNAL_API)
    // but should be at least 20 handlers (basic APIs + WebRTC APIs)
    EXPECT_TRUE(mock_mux->handle_count_ >= 20);

    // Verify some key patterns were registered
    bool has_root = false;
    bool has_api = false;
    bool has_summaries = false;
    bool has_metrics = false;
    bool has_rtc_play = false;

    for (size_t i = 0; i < mock_mux->patterns_.size(); i++) {
        if (mock_mux->patterns_[i] == "/") has_root = true;
        if (mock_mux->patterns_[i] == "/api/") has_api = true;
        if (mock_mux->patterns_[i] == "/api/v1/summaries") has_summaries = true;
        if (mock_mux->patterns_[i] == "/metrics") has_metrics = true;
        if (mock_mux->patterns_[i] == "/rtc/v1/play/") has_rtc_play = true;
    }

    EXPECT_TRUE(has_root);
    EXPECT_TRUE(has_api);
    EXPECT_TRUE(has_summaries);
    EXPECT_TRUE(has_metrics);
    EXPECT_TRUE(has_rtc_play);

    // Cleanup: restore original mux to avoid side effects
    server->http_api_mux_ = original_mux;
    srs_freep(mock_mux);
}

MockLogForSignal::MockLogForSignal()
{
    reopen_count_ = 0;
}

MockLogForSignal::~MockLogForSignal()
{
}

srs_error_t MockLogForSignal::initialize()
{
    return srs_success;
}

void MockLogForSignal::reopen()
{
    reopen_count_++;
}

void MockLogForSignal::log(SrsLogLevel level, const char *tag, const SrsContextId &context_id, const char *fmt, va_list args)
{
    // Do nothing for mock
}

MockAppConfigForSignal::MockAppConfigForSignal()
{
    force_grace_quit_ = false;
}

MockAppConfigForSignal::~MockAppConfigForSignal()
{
}

bool MockAppConfigForSignal::is_force_grace_quit()
{
    return force_grace_quit_;
}

VOID TEST(ServerTest, OnSignalHandling)
{
    // Create SrsServer instance
    SrsUniquePtr<SrsServer> server(new SrsServer());
    EXPECT_TRUE(server.get() != NULL);

    // Create and inject mock config
    MockAppConfigForSignal* mock_config = new MockAppConfigForSignal();
    ISrsAppConfig* original_config = server->config_;
    server->config_ = mock_config;

    // Create and inject mock log
    MockLogForSignal* mock_log = new MockLogForSignal();
    ISrsLog* original_log = server->log_;
    server->log_ = mock_log;

    // Test 1: SRS_SIGNAL_RELOAD should set signal_reload_ flag
    server->signal_reload_ = false;
    server->on_signal(SRS_SIGNAL_RELOAD);
    EXPECT_TRUE(server->signal_reload_);

    // Test 2: SRS_SIGNAL_REOPEN_LOG should call log_->reopen()
    EXPECT_EQ(0, mock_log->reopen_count_);
    server->on_signal(SRS_SIGNAL_REOPEN_LOG);
    EXPECT_EQ(1, mock_log->reopen_count_);

    // Test 3: SRS_SIGNAL_PERSISTENCE_CONFIG should set signal_persistence_config_ flag
    server->signal_persistence_config_ = false;
    server->on_signal(SRS_SIGNAL_PERSISTENCE_CONFIG);
    EXPECT_TRUE(server->signal_persistence_config_);

    // Test 4: SRS_SIGNAL_FAST_QUIT should set signal_fast_quit_ flag
    server->signal_fast_quit_ = false;
    server->on_signal(SRS_SIGNAL_FAST_QUIT);
    EXPECT_TRUE(server->signal_fast_quit_);

    // Test 5: SIGINT should set signal_fast_quit_ flag
    server->signal_fast_quit_ = false;
    server->on_signal(SIGINT);
    EXPECT_TRUE(server->signal_fast_quit_);

    // Test 6: SRS_SIGNAL_GRACEFULLY_QUIT should set signal_gracefully_quit_ flag
    server->signal_gracefully_quit_ = false;
    server->on_signal(SRS_SIGNAL_GRACEFULLY_QUIT);
    EXPECT_TRUE(server->signal_gracefully_quit_);

    // Test 7: SRS_SIGNAL_FAST_QUIT with force_grace_quit enabled should convert to gracefully quit
    mock_config->force_grace_quit_ = true;
    server->signal_gracefully_quit_ = false;
    server->signal_fast_quit_ = false;
    server->on_signal(SRS_SIGNAL_FAST_QUIT);
    EXPECT_TRUE(server->signal_gracefully_quit_);
    EXPECT_FALSE(server->signal_fast_quit_);

    // Test 8: Repeated signals should not change already-set flags
    server->signal_fast_quit_ = true;
    server->on_signal(SRS_SIGNAL_FAST_QUIT);
    EXPECT_TRUE(server->signal_fast_quit_);

    server->signal_gracefully_quit_ = true;
    server->on_signal(SRS_SIGNAL_GRACEFULLY_QUIT);
    EXPECT_TRUE(server->signal_gracefully_quit_);

    // Cleanup: restore original config and log
    server->config_ = original_config;
    server->log_ = original_log;
    srs_freep(mock_config);
    srs_freep(mock_log);
}

// Mock config implementation for SrsServer::do2_cycle() testing
MockAppConfigForDo2Cycle::MockAppConfigForDo2Cycle()
{
    reload_error_ = srs_success;
    persistence_error_ = srs_success;
    reload_count_ = 0;
    persistence_count_ = 0;
    reload_state_ = SrsReloadStateInit;
}

MockAppConfigForDo2Cycle::~MockAppConfigForDo2Cycle()
{
    srs_freep(reload_error_);
    srs_freep(persistence_error_);
}

srs_error_t MockAppConfigForDo2Cycle::reload(SrsReloadState *pstate)
{
    reload_count_++;
    if (pstate) {
        *pstate = reload_state_;
    }
    return srs_error_copy(reload_error_);
}

srs_error_t MockAppConfigForDo2Cycle::persistence()
{
    persistence_count_++;
    return srs_error_copy(persistence_error_);
}

void MockAppConfigForDo2Cycle::reset()
{
    srs_freep(reload_error_);
    srs_freep(persistence_error_);
    reload_error_ = srs_success;
    persistence_error_ = srs_success;
    reload_count_ = 0;
    persistence_count_ = 0;
    reload_state_ = SrsReloadStateInit;
}

VOID TEST(ServerTest, Do2CycleReloadSuccess)
{
    srs_error_t err;

    // Create SrsServer instance
    SrsUniquePtr<SrsServer> server(new SrsServer());
    EXPECT_TRUE(server.get() != NULL);

    // Create and inject mock config
    MockAppConfigForDo2Cycle* mock_config = new MockAppConfigForDo2Cycle();
    ISrsAppConfig* original_config = server->config_;
    server->config_ = mock_config;

    // Test major use scenario: signal_reload_ triggers config reload with success
    // This covers the main code path where reload signal is processed successfully
    server->signal_reload_ = true;
    server->signal_fast_quit_ = false;
    server->signal_gracefully_quit_ = false;
    mock_config->reload_state_ = SrsReloadStateFinished;
    HELPER_EXPECT_SUCCESS(server->do2_cycle());
    EXPECT_FALSE(server->signal_reload_);  // Flag should be cleared after processing
    EXPECT_EQ(1, mock_config->reload_count_);  // Config reload should be called once

    // Cleanup: restore original config
    server->config_ = original_config;
    srs_freep(mock_config);
}

// Mock hourglass implementation for SrsServer::setup_ticks() testing
MockHourGlassForSetupTicks::MockHourGlassForSetupTicks()
{
    tick_count_ = 0;
    start_count_ = 0;
}

MockHourGlassForSetupTicks::~MockHourGlassForSetupTicks()
{
}

srs_error_t MockHourGlassForSetupTicks::start()
{
    start_count_++;
    return srs_success;
}

void MockHourGlassForSetupTicks::stop()
{
}

srs_error_t MockHourGlassForSetupTicks::tick(srs_utime_t interval)
{
    return tick(0, interval);
}

srs_error_t MockHourGlassForSetupTicks::tick(int event, srs_utime_t interval)
{
    tick_count_++;
    tick_events_.push_back(event);
    tick_intervals_.push_back(interval);
    return srs_success;
}

void MockHourGlassForSetupTicks::untick(int event)
{
}

// Mock app factory implementation for SrsServer::setup_ticks() testing
MockAppFactoryForSetupTicks::MockAppFactoryForSetupTicks()
{
    mock_hourglass_ = new MockHourGlassForSetupTicks();
}

MockAppFactoryForSetupTicks::~MockAppFactoryForSetupTicks()
{
    // Do NOT free mock_hourglass_ here because it's owned by SrsServer::timer_
    // and will be freed by SrsServer destructor
    mock_hourglass_ = NULL;
}

ISrsHourGlass *MockAppFactoryForSetupTicks::create_hourglass(const std::string &name, ISrsHourGlassHandler *handler, srs_utime_t interval)
{
    return mock_hourglass_;
}

// Mock config implementation for SrsServer::setup_ticks() testing
MockAppConfigForSetupTicks::MockAppConfigForSetupTicks()
{
    stats_enabled_ = false;
    heartbeat_enabled_ = false;
    heartbeat_interval_ = 1 * SRS_UTIME_MILLISECONDS;
}

MockAppConfigForSetupTicks::~MockAppConfigForSetupTicks()
{
}

bool MockAppConfigForSetupTicks::get_stats_enabled()
{
    return stats_enabled_;
}

bool MockAppConfigForSetupTicks::get_heartbeat_enabled()
{
    return heartbeat_enabled_;
}

srs_utime_t MockAppConfigForSetupTicks::get_heartbeat_interval()
{
    return heartbeat_interval_;
}

VOID TEST(ServerTest, SetupTicksWithStatsAndHeartbeat)
{
    srs_error_t err;

    // Create SrsServer instance
    SrsUniquePtr<SrsServer> server(new SrsServer());
    EXPECT_TRUE(server.get() != NULL);

    // Create and inject mock config
    MockAppConfigForSetupTicks* mock_config = new MockAppConfigForSetupTicks();
    ISrsAppConfig* original_config = server->config_;
    server->config_ = mock_config;

    // Create and inject mock app factory
    MockAppFactoryForSetupTicks* mock_factory = new MockAppFactoryForSetupTicks();
    SrsAppFactory* original_factory = server->app_factory_;
    server->app_factory_ = mock_factory;

    // Test major use scenario: setup_ticks with stats and heartbeat enabled
    // This covers the main code path where both stats and heartbeat are enabled
    mock_config->stats_enabled_ = true;
    mock_config->heartbeat_enabled_ = true;
    mock_config->heartbeat_interval_ = 1 * SRS_UTIME_MILLISECONDS;

    HELPER_EXPECT_SUCCESS(server->setup_ticks());

    // Verify that timer was created and started
    EXPECT_EQ(1, mock_factory->mock_hourglass_->start_count_);

    // Verify that ticks were registered
    // When stats_enabled: events 2,4,5,6,7,8,10,11,12 (9 ticks)
    // When heartbeat_enabled: event 9 (1 tick)
    // Total: 10 ticks
    EXPECT_EQ(10, mock_factory->mock_hourglass_->tick_count_);

    // Verify specific tick events are registered
    std::vector<int> &events = mock_factory->mock_hourglass_->tick_events_;
    EXPECT_TRUE(std::find(events.begin(), events.end(), 2) != events.end());  // rusage
    EXPECT_TRUE(std::find(events.begin(), events.end(), 4) != events.end());  // disk
    EXPECT_TRUE(std::find(events.begin(), events.end(), 5) != events.end());  // meminfo
    EXPECT_TRUE(std::find(events.begin(), events.end(), 6) != events.end());  // platform
    EXPECT_TRUE(std::find(events.begin(), events.end(), 7) != events.end());  // network
    EXPECT_TRUE(std::find(events.begin(), events.end(), 8) != events.end());  // kbps
    EXPECT_TRUE(std::find(events.begin(), events.end(), 9) != events.end());  // heartbeat
    EXPECT_TRUE(std::find(events.begin(), events.end(), 10) != events.end()); // udp snmp
    EXPECT_TRUE(std::find(events.begin(), events.end(), 11) != events.end()); // rtc sessions
    EXPECT_TRUE(std::find(events.begin(), events.end(), 12) != events.end()); // server stats

    // Cleanup: restore original config and factory
    server->config_ = original_config;
    server->app_factory_ = original_factory;
    srs_freep(mock_config);
    srs_freep(mock_factory);
}

MockRtcSessionManagerForNotify::MockRtcSessionManagerForNotify()
{
    update_rtc_sessions_count_ = 0;
}

MockRtcSessionManagerForNotify::~MockRtcSessionManagerForNotify()
{
}

void MockRtcSessionManagerForNotify::srs_update_rtc_sessions()
{
    update_rtc_sessions_count_++;
}

MockHttpHeartbeatForNotify::MockHttpHeartbeatForNotify()
{
    heartbeat_count_ = 0;
}

MockHttpHeartbeatForNotify::~MockHttpHeartbeatForNotify()
{
}

void MockHttpHeartbeatForNotify::heartbeat()
{
    heartbeat_count_++;
}

MockConnectionManagerForResampleKbps::MockConnectionManagerForResampleKbps()
{
}

MockConnectionManagerForResampleKbps::~MockConnectionManagerForResampleKbps()
{
}

srs_error_t MockConnectionManagerForResampleKbps::start()
{
    return srs_success;
}

bool MockConnectionManagerForResampleKbps::empty()
{
    return connections_.empty();
}

size_t MockConnectionManagerForResampleKbps::size()
{
    return connections_.size();
}

void MockConnectionManagerForResampleKbps::add(ISrsResource *conn, bool *exists)
{
    connections_.push_back(conn);
}

ISrsResource *MockConnectionManagerForResampleKbps::at(int index)
{
    if (index < 0 || index >= (int)connections_.size()) {
        return NULL;
    }
    return connections_[index];
}

void MockConnectionManagerForResampleKbps::remove(ISrsResource *c)
{
}

void MockConnectionManagerForResampleKbps::subscribe(ISrsDisposingHandler *h)
{
}

void MockConnectionManagerForResampleKbps::unsubscribe(ISrsDisposingHandler *h)
{
}

MockStatisticForResampleKbps::MockStatisticForResampleKbps()
{
    kbps_add_delta_count_ = 0;
    kbps_sample_count_ = 0;
}

MockStatisticForResampleKbps::~MockStatisticForResampleKbps()
{
}

void MockStatisticForResampleKbps::on_disconnect(std::string id, srs_error_t err)
{
}

srs_error_t MockStatisticForResampleKbps::on_client(std::string id, ISrsRequest *req, ISrsExpire *conn, SrsRtmpConnType type)
{
    return srs_success;
}

srs_error_t MockStatisticForResampleKbps::on_video_info(ISrsRequest *req, SrsVideoCodecId vcodec, int avc_profile, int avc_level, int width, int height)
{
    return srs_success;
}

srs_error_t MockStatisticForResampleKbps::on_audio_info(ISrsRequest *req, SrsAudioCodecId acodec, SrsAudioSampleRate asample_rate, SrsAudioChannels asound_type, SrsAacObjectType aac_object)
{
    return srs_success;
}

void MockStatisticForResampleKbps::on_stream_publish(ISrsRequest *req, std::string publisher_id)
{
}

void MockStatisticForResampleKbps::on_stream_close(ISrsRequest *req)
{
}

void MockStatisticForResampleKbps::kbps_add_delta(std::string id, ISrsKbpsDelta *delta)
{
    kbps_add_delta_count_++;
}

void MockStatisticForResampleKbps::kbps_sample()
{
    kbps_sample_count_++;
}

srs_error_t MockStatisticForResampleKbps::on_video_frames(ISrsRequest *req, int nb_frames)
{
    return srs_success;
}

void MockStatisticForResampleKbps::reset()
{
    kbps_add_delta_count_ = 0;
    kbps_sample_count_ = 0;
}

VOID TEST(SrsServerTest, NotifyEventDispatch)
{
    srs_error_t err = srs_success;

    // Create server instance
    SrsUniquePtr<SrsServer> server(new SrsServer());

    // Create mock objects
    MockRtcSessionManagerForNotify* mock_rtc_manager = new MockRtcSessionManagerForNotify();
    MockHttpHeartbeatForNotify* mock_heartbeat = new MockHttpHeartbeatForNotify();

    // Save original pointers
    SrsRtcSessionManager* original_rtc_manager = server->rtc_session_manager_;
    SrsHttpHeartbeat* original_heartbeat = server->http_heartbeat_;

    // Inject mock objects (no cast needed since they inherit from the base classes)
    server->rtc_session_manager_ = mock_rtc_manager;
    server->http_heartbeat_ = mock_heartbeat;

    // Test event 11 - RTC session update
    HELPER_EXPECT_SUCCESS(server->notify(11, 0, 0));
    EXPECT_EQ(1, mock_rtc_manager->update_rtc_sessions_count_);

    // Test event 9 - HTTP heartbeat
    HELPER_EXPECT_SUCCESS(server->notify(9, 0, 0));
    EXPECT_EQ(1, mock_heartbeat->heartbeat_count_);

    // Test event 8 - resample_kbps (should not crash)
    HELPER_EXPECT_SUCCESS(server->notify(8, 0, 0));

    // Test multiple calls to same event
    HELPER_EXPECT_SUCCESS(server->notify(11, 0, 0));
    EXPECT_EQ(2, mock_rtc_manager->update_rtc_sessions_count_);

    HELPER_EXPECT_SUCCESS(server->notify(9, 0, 0));
    EXPECT_EQ(2, mock_heartbeat->heartbeat_count_);

    // Test other events (should not crash)
    HELPER_EXPECT_SUCCESS(server->notify(2, 0, 0));  // srs_update_system_rusage
    HELPER_EXPECT_SUCCESS(server->notify(4, 0, 0));  // srs_update_disk_stat
    HELPER_EXPECT_SUCCESS(server->notify(5, 0, 0));  // srs_update_meminfo
    HELPER_EXPECT_SUCCESS(server->notify(6, 0, 0));  // srs_update_platform_info
    HELPER_EXPECT_SUCCESS(server->notify(7, 0, 0));  // srs_update_network_devices
    HELPER_EXPECT_SUCCESS(server->notify(10, 0, 0)); // srs_update_udp_snmp_statistic
    HELPER_EXPECT_SUCCESS(server->notify(12, 0, 0)); // srs_update_server_statistics

    // Restore original pointers
    server->rtc_session_manager_ = original_rtc_manager;
    server->http_heartbeat_ = original_heartbeat;

    // Cleanup mock objects
    srs_freep(mock_rtc_manager);
    srs_freep(mock_heartbeat);
}

VOID TEST(SrsServerTest, ResampleKbps)
{
    // Create server instance
    SrsUniquePtr<SrsServer> server(new SrsServer());

    // Create mock objects
    MockConnectionManagerForResampleKbps *mock_conn_manager = new MockConnectionManagerForResampleKbps();
    MockStatisticForResampleKbps *mock_stat = new MockStatisticForResampleKbps();

    // Save original pointers
    ISrsResourceManager *original_conn_manager = server->conn_manager_;
    ISrsStatistic *original_stat = server->stat_;

    // Inject mock objects
    server->conn_manager_ = mock_conn_manager;
    server->stat_ = mock_stat;

    // Test case: Empty connection manager - verifies kbps_sample is called
    // This is the major use scenario: resample_kbps() should always call
    // stat_->kbps_sample() to update global server statistics, even when
    // there are no connections.
    if (true) {
        server->resample_kbps();
        EXPECT_EQ(0, mock_stat->kbps_add_delta_count_);
        EXPECT_EQ(1, mock_stat->kbps_sample_count_);
    }

    // Restore original pointers
    server->conn_manager_ = original_conn_manager;
    server->stat_ = original_stat;

    // Cleanup mock objects
    srs_freep(mock_conn_manager);
    srs_freep(mock_stat);
}

MockAppConfigForConnectionLimit::MockAppConfigForConnectionLimit()
{
    max_connections_ = 10;
}

MockAppConfigForConnectionLimit::~MockAppConfigForConnectionLimit()
{
}

int MockAppConfigForConnectionLimit::get_max_connections()
{
    return max_connections_;
}

MockConnectionManagerForConnectionLimit::MockConnectionManagerForConnectionLimit()
{
    connection_count_ = 0;
}

MockConnectionManagerForConnectionLimit::~MockConnectionManagerForConnectionLimit()
{
}

srs_error_t MockConnectionManagerForConnectionLimit::start()
{
    return srs_success;
}

bool MockConnectionManagerForConnectionLimit::empty()
{
    return connection_count_ == 0;
}

size_t MockConnectionManagerForConnectionLimit::size()
{
    return connection_count_;
}

void MockConnectionManagerForConnectionLimit::add(ISrsResource *conn, bool *exists)
{
}

ISrsResource *MockConnectionManagerForConnectionLimit::at(int index)
{
    return NULL;
}

void MockConnectionManagerForConnectionLimit::remove(ISrsResource *c)
{
}

void MockConnectionManagerForConnectionLimit::subscribe(ISrsDisposingHandler *h)
{
}

void MockConnectionManagerForConnectionLimit::unsubscribe(ISrsDisposingHandler *h)
{
}

VOID TEST(SrsServerTest, OnBeforeConnectionExceedLimit)
{
    srs_error_t err = srs_success;

    // Create server instance
    SrsUniquePtr<SrsServer> server(new SrsServer());

    // Create mock objects
    MockAppConfigForConnectionLimit *mock_config = new MockAppConfigForConnectionLimit();
    MockConnectionManagerForConnectionLimit *mock_conn_manager = new MockConnectionManagerForConnectionLimit();

    // Save original pointers
    ISrsAppConfig *original_config = server->config_;
    ISrsResourceManager *original_conn_manager = server->conn_manager_;

    // Inject mock objects
    server->config_ = mock_config;
    server->conn_manager_ = mock_conn_manager;

    // Test case: Connection limit exceeded
    // This is the major use scenario: when current connections >= max_connections,
    // on_before_connection() should return ERROR_EXCEED_CONNECTIONS error.
    if (true) {
        // Set max connections to 10
        mock_config->max_connections_ = 10;
        // Set current connections to 10 (at limit)
        mock_conn_manager->connection_count_ = 10;

        // Try to accept a new connection - should fail
        err = server->on_before_connection("RTMP", 100, "192.168.1.100", 1935);
        EXPECT_TRUE(err != srs_success);
        EXPECT_EQ(ERROR_EXCEED_CONNECTIONS, srs_error_code(err));
        srs_freep(err);
    }

    // Test case: Connection limit not exceeded
    if (true) {
        // Set max connections to 10
        mock_config->max_connections_ = 10;
        // Set current connections to 9 (below limit)
        mock_conn_manager->connection_count_ = 9;

        // Try to accept a new connection - should succeed
        HELPER_EXPECT_SUCCESS(server->on_before_connection("RTMP", 101, "192.168.1.101", 1935));
    }

    // Restore original pointers
    server->config_ = original_config;
    server->conn_manager_ = original_conn_manager;

    // Cleanup mock objects
    srs_freep(mock_config);
    srs_freep(mock_conn_manager);
}

VOID TEST(SrsRtmpTransportTest, BasicOperations)
{
    srs_error_t err = srs_success;

    // Create a dummy file descriptor (cast from int for testing)
    // Note: We won't actually use this for I/O, just testing the wrapper methods
    srs_netfd_t dummy_fd = (srs_netfd_t)((void*)0x1234);

    // Create SrsRtmpTransport instance
    SrsUniquePtr<SrsRtmpTransport> transport(new SrsRtmpTransport(dummy_fd));

    // Test fd() - should return the file descriptor we passed
    EXPECT_EQ(dummy_fd, transport->fd());

    // Test io() - should return the socket interface (not NULL)
    EXPECT_TRUE(transport->io() != NULL);

    // Test handshake() - should return success (no-op for plain RTMP)
    HELPER_EXPECT_SUCCESS(transport->handshake());

    // Test transport_type() - should return "plaintext"
    EXPECT_STREQ("plaintext", transport->transport_type());

    // Test get_recv_bytes() - should return 0 for new connection
    EXPECT_EQ(0, transport->get_recv_bytes());

    // Test get_send_bytes() - should return 0 for new connection
    EXPECT_EQ(0, transport->get_send_bytes());

    // Prevent the destructor from trying to close the dummy fd
    // by setting the internal stfd_ to NULL before destruction
    transport->skt_->stfd_ = NULL;
}
