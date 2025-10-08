//
// Copyright (c) 2013-2025 The SRS Authors
//
// SPDX-License-Identifier: MIT
//
#include <srs_utest_app14.hpp>

using namespace std;

#include <srs_app_gb28181.hpp>
#include <srs_app_listener.hpp>
#include <srs_kernel_error.hpp>
#include <srs_kernel_utility.hpp>
#include <srs_app_config.hpp>
#include <srs_kernel_ts.hpp>
#include <srs_kernel_ps.hpp>
#include <srs_protocol_raw_avc.hpp>
#include <srs_protocol_rtmp_conn.hpp>
#include <srs_protocol_json.hpp>
#include <srs_utest_kernel.hpp>
#include <srs_utest_app11.hpp>
#include <srs_utest_http.hpp>

// Mock ISrsGbMuxer implementation
MockGbMuxer::MockGbMuxer()
{
    setup_called_ = false;
    setup_output_ = "";
    on_ts_message_called_ = false;
    on_ts_message_error_ = srs_success;
}

MockGbMuxer::~MockGbMuxer()
{
}

void MockGbMuxer::setup(std::string output)
{
    setup_called_ = true;
    setup_output_ = output;
}

srs_error_t MockGbMuxer::on_ts_message(SrsTsMessage *msg)
{
    on_ts_message_called_ = true;
    return srs_error_copy(on_ts_message_error_);
}

void MockGbMuxer::reset()
{
    setup_called_ = false;
    setup_output_ = "";
    on_ts_message_called_ = false;
    srs_freep(on_ts_message_error_);
}

// Mock ISrsAppConfig implementation
MockAppConfigForGbSession::MockAppConfigForGbSession()
{
    stream_caster_output_ = "";
}

MockAppConfigForGbSession::~MockAppConfigForGbSession()
{
}

std::string MockAppConfigForGbSession::get_stream_caster_output(SrsConfDirective *conf)
{
    return stream_caster_output_;
}

void MockAppConfigForGbSession::set_stream_caster_output(const std::string &output)
{
    stream_caster_output_ = output;
}

// Test GB28181 session state conversion functions
VOID TEST(GB28181Test, SessionStateConversion)
{
    // Test srs_gb_session_state() for all valid states
    EXPECT_STREQ("Init", srs_gb_session_state(SrsGbSessionStateInit).c_str());
    EXPECT_STREQ("Connecting", srs_gb_session_state(SrsGbSessionStateConnecting).c_str());
    EXPECT_STREQ("Established", srs_gb_session_state(SrsGbSessionStateEstablished).c_str());
    EXPECT_STREQ("Invalid", srs_gb_session_state((SrsGbSessionState)999).c_str());

    // Test srs_gb_state() for state transitions
    EXPECT_STREQ("Init->Connecting", srs_gb_state(SrsGbSessionStateInit, SrsGbSessionStateConnecting).c_str());
    EXPECT_STREQ("Connecting->Established", srs_gb_state(SrsGbSessionStateConnecting, SrsGbSessionStateEstablished).c_str());
    EXPECT_STREQ("Established->Init", srs_gb_state(SrsGbSessionStateEstablished, SrsGbSessionStateInit).c_str());
}

// Test SrsGbSession setup and setup_owner methods
VOID TEST(GB28181Test, SessionSetupAndOwner)
{
    // Create mock dependencies
    SrsUniquePtr<MockAppConfigForGbSession> mock_config(new MockAppConfigForGbSession());
    MockGbMuxer *mock_muxer = new MockGbMuxer();

    // Create session
    SrsUniquePtr<SrsGbSession> session(new SrsGbSession());

    // Inject mock dependencies
    session->config_ = mock_config.get();
    session->muxer_ = mock_muxer;

    // Setup mock config to return test output
    mock_config->set_stream_caster_output("rtmp://127.0.0.1/live/test_stream");

    // Test setup() method
    SrsConfDirective *conf = NULL;
    session->setup(conf);

    // Verify muxer->setup() was called with correct output
    EXPECT_TRUE(mock_muxer->setup_called_);
    EXPECT_STREQ("rtmp://127.0.0.1/live/test_stream", mock_muxer->setup_output_.c_str());

    // Test setup_owner() method
    SrsSharedResource<ISrsGbSession> *wrapper = NULL;
    ISrsInterruptable *owner_coroutine = (ISrsInterruptable *)0x1234;
    ISrsContextIdSetter *owner_cid = (ISrsContextIdSetter *)0x5678;

    session->setup_owner(wrapper, owner_coroutine, owner_cid);

    // Verify owner fields are set correctly
    EXPECT_EQ(wrapper, session->wrapper_);
    EXPECT_EQ(owner_coroutine, session->owner_coroutine_);
    EXPECT_EQ(owner_cid, session->owner_cid_);

    // Test on_executor_done() method
    session->on_executor_done(NULL);

    // Verify owner_coroutine_ is cleared
    EXPECT_EQ((ISrsInterruptable *)NULL, session->owner_coroutine_);

    // Clean up - set to NULL to avoid double-free
    session->muxer_ = NULL;
    srs_freep(mock_muxer);
}

// Mock ISrsPackContext implementation
MockPackContext::MockPackContext()
{
}

MockPackContext::~MockPackContext()
{
}

srs_error_t MockPackContext::on_ts_message(SrsTsMessage *msg)
{
    return srs_success;
}

void MockPackContext::on_recover_mode(int nn_recover)
{
}

// Test SrsGbSession::on_ps_pack method - major use scenario
VOID TEST(GB28181Test, SessionOnPsPack)
{
    // Create mock dependencies
    SrsUniquePtr<MockAppConfigForGbSession> mock_config(new MockAppConfigForGbSession());
    MockGbMuxer *mock_muxer = new MockGbMuxer();

    // Create session
    SrsUniquePtr<SrsGbSession> session(new SrsGbSession());

    // Inject mock dependencies
    session->config_ = mock_config.get();
    session->muxer_ = mock_muxer;

    // Create mock pack context
    SrsUniquePtr<MockPackContext> ctx(new MockPackContext());
    ctx->media_id_ = 1;
    ctx->media_startime_ = srs_time_now_realtime();
    ctx->media_nn_recovered_ = 5;
    ctx->media_nn_msgs_dropped_ = 3;
    ctx->media_reserved_ = 10;

    // Create test messages: 2 video messages and 1 audio message
    std::vector<SrsTsMessage *> msgs;

    // Create first video message
    SrsUniquePtr<SrsTsMessage> video1(new SrsTsMessage());
    video1->sid_ = SrsTsPESStreamIdVideoCommon;
    video1->dts_ = 90000;
    video1->pts_ = 90000;
    video1->payload_->append("video1", 6);
    msgs.push_back(video1.get());

    // Create audio message
    SrsUniquePtr<SrsTsMessage> audio(new SrsTsMessage());
    audio->sid_ = SrsTsPESStreamIdAudioCommon;
    audio->dts_ = 90000;
    audio->pts_ = 90000;
    audio->payload_->append("audio1", 6);
    msgs.push_back(audio.get());

    // Create second video message
    SrsUniquePtr<SrsTsMessage> video2(new SrsTsMessage());
    video2->sid_ = SrsTsPESStreamIdVideoCommon;
    video2->dts_ = 90000;
    video2->pts_ = 90000;
    video2->payload_->append("video2", 6);
    msgs.push_back(video2.get());

    // Call on_ps_pack
    session->on_ps_pack(ctx.get(), NULL, msgs);

    // Verify statistics are updated correctly
    EXPECT_EQ(1u, ctx->media_id_);
    EXPECT_EQ(1u, session->media_id_);
    EXPECT_EQ(1u, session->media_packs_);
    EXPECT_EQ(3u, session->media_msgs_);
    EXPECT_EQ(5u, session->media_recovered_);
    EXPECT_EQ(3u, session->media_msgs_dropped_);
    EXPECT_EQ(10u, session->media_reserved_);

    // Verify muxer was called (should be called twice: once for audio, once for grouped video)
    EXPECT_TRUE(mock_muxer->on_ts_message_called_);

    // Reset mock for second test
    mock_muxer->reset();

    // Test context change (new media transport)
    SrsUniquePtr<MockPackContext> ctx2(new MockPackContext());
    ctx2->media_id_ = 2;  // Different media_id
    ctx2->media_startime_ = srs_time_now_realtime();
    ctx2->media_nn_recovered_ = 2;
    ctx2->media_nn_msgs_dropped_ = 1;
    ctx2->media_reserved_ = 5;

    // Create new messages
    std::vector<SrsTsMessage *> msgs2;
    SrsUniquePtr<SrsTsMessage> video3(new SrsTsMessage());
    video3->sid_ = SrsTsPESStreamIdVideoCommon;
    video3->dts_ = 180000;
    video3->pts_ = 180000;
    video3->payload_->append("video3", 6);
    msgs2.push_back(video3.get());

    // Call on_ps_pack with new context
    session->on_ps_pack(ctx2.get(), NULL, msgs2);

    // Verify statistics are accumulated for old context
    EXPECT_EQ(1u, session->total_packs_);
    EXPECT_EQ(3u, session->total_msgs_);
    EXPECT_EQ(5u, session->total_recovered_);
    EXPECT_EQ(3u, session->total_msgs_dropped_);
    EXPECT_EQ(10u, session->total_reserved_);

    // Verify new context statistics
    EXPECT_EQ(2u, session->media_id_);
    EXPECT_EQ(1u, session->media_packs_);
    EXPECT_EQ(1u, session->media_msgs_);
    EXPECT_EQ(2u, session->media_recovered_);
    EXPECT_EQ(1u, session->media_msgs_dropped_);
    EXPECT_EQ(5u, session->media_reserved_);

    // Clean up - set to NULL to avoid double-free
    session->muxer_ = NULL;
    srs_freep(mock_muxer);
}

// Mock ISrsGbMediaTcpConn implementation
MockGbMediaTcpConn::MockGbMediaTcpConn()
{
    set_cid_called_ = false;
    is_connected_ = false;
}

MockGbMediaTcpConn::~MockGbMediaTcpConn()
{
}

void MockGbMediaTcpConn::setup(srs_netfd_t stfd)
{
}

void MockGbMediaTcpConn::setup_owner(SrsSharedResource<ISrsGbMediaTcpConn> *wrapper, ISrsInterruptable *owner_coroutine, ISrsContextIdSetter *owner_cid)
{
}

bool MockGbMediaTcpConn::is_connected()
{
    return is_connected_;
}

void MockGbMediaTcpConn::interrupt()
{
}

void MockGbMediaTcpConn::set_cid(const SrsContextId &cid)
{
    set_cid_called_ = true;
    received_cid_ = cid;
}

const SrsContextId &MockGbMediaTcpConn::get_id()
{
    return received_cid_;
}

std::string MockGbMediaTcpConn::desc()
{
    return "MockGbMediaTcpConn";
}

srs_error_t MockGbMediaTcpConn::cycle()
{
    return srs_success;
}

void MockGbMediaTcpConn::on_executor_done(ISrsInterruptable *executor)
{
}

void MockGbMediaTcpConn::reset()
{
    set_cid_called_ = false;
    received_cid_ = SrsContextId();
}

// Test SrsGbSession::on_media_transport - covers the major use scenario:
// 1. Session receives a media transport connection
// 2. Session stores the media transport reference
// 3. Session propagates its context ID to the media transport via set_cid()
VOID TEST(GB28181Test, SessionOnMediaTransport)
{
    // Create mock dependencies
    SrsUniquePtr<MockAppConfigForGbSession> mock_config(new MockAppConfigForGbSession());
    MockGbMuxer *mock_muxer = new MockGbMuxer();

    // Create session
    SrsUniquePtr<SrsGbSession> session(new SrsGbSession());

    // Inject mock dependencies
    session->config_ = mock_config.get();
    session->muxer_ = mock_muxer;

    // Set a specific context ID for the session
    SrsContextId session_cid;
    session_cid.set_value("test-session-123");
    session->cid_ = session_cid;

    // Create mock media transport wrapped in SrsSharedResource
    MockGbMediaTcpConn *mock_media = new MockGbMediaTcpConn();
    SrsSharedResource<ISrsGbMediaTcpConn> media_resource(mock_media);

    // Call on_media_transport
    session->on_media_transport(media_resource);

    // Verify that set_cid was called on the media transport
    EXPECT_TRUE(mock_media->set_cid_called_);

    // Verify that the session's context ID was passed to the media transport
    EXPECT_EQ(0, mock_media->received_cid_.compare(session_cid));

    // Clean up - set to NULL to avoid double-free
    session->muxer_ = NULL;
    srs_freep(mock_muxer);
}

// Test SrsGbSession::drive_state - covers the major use scenario:
// 1. Session starts in Init state with media disconnected
// 2. When media connects, session transitions to Established state
// 3. When media disconnects, session transitions back to Init state
VOID TEST(GB28181Test, SessionDriveState)
{
    srs_error_t err;

    // Create mock dependencies
    SrsUniquePtr<MockAppConfigForGbSession> mock_config(new MockAppConfigForGbSession());
    MockGbMuxer *mock_muxer = new MockGbMuxer();
    MockGbMediaTcpConn *mock_media = new MockGbMediaTcpConn();

    // Create session
    SrsUniquePtr<SrsGbSession> session(new SrsGbSession());

    // Inject mock dependencies
    session->config_ = mock_config.get();
    session->muxer_ = mock_muxer;
    session->media_ = SrsSharedResource<ISrsGbMediaTcpConn>(mock_media);
    session->device_id_ = "test-device-123";

    // Verify initial state is Init
    EXPECT_EQ(SrsGbSessionStateInit, session->state_);

    // Test 1: Init state with media disconnected - should remain in Init
    mock_media->is_connected_ = false;
    HELPER_EXPECT_SUCCESS(session->drive_state());
    EXPECT_EQ(SrsGbSessionStateInit, session->state_);

    // Test 2: Init state with media connected - should transition to Established
    mock_media->is_connected_ = true;
    HELPER_EXPECT_SUCCESS(session->drive_state());
    EXPECT_EQ(SrsGbSessionStateEstablished, session->state_);

    // Test 3: Established state with media still connected - should remain in Established
    mock_media->is_connected_ = true;
    HELPER_EXPECT_SUCCESS(session->drive_state());
    EXPECT_EQ(SrsGbSessionStateEstablished, session->state_);

    // Test 4: Established state with media disconnected - should transition back to Init
    mock_media->is_connected_ = false;
    HELPER_EXPECT_SUCCESS(session->drive_state());
    EXPECT_EQ(SrsGbSessionStateInit, session->state_);

    // Clean up - set to NULL to avoid double-free
    session->muxer_ = NULL;
    srs_freep(mock_muxer);
}

// Mock ISrsIpListener implementation
MockIpListener::MockIpListener()
{
    endpoint_ip_ = "";
    endpoint_port_ = 0;
    label_ = "";
    set_endpoint_called_ = false;
    set_label_called_ = false;
}

MockIpListener::~MockIpListener()
{
}

ISrsListener *MockIpListener::set_endpoint(const std::string &i, int p)
{
    endpoint_ip_ = i;
    endpoint_port_ = p;
    set_endpoint_called_ = true;
    return this;
}

ISrsListener *MockIpListener::set_label(const std::string &label)
{
    label_ = label;
    set_label_called_ = true;
    return this;
}

srs_error_t MockIpListener::listen()
{
    return srs_success;
}

void MockIpListener::reset()
{
    endpoint_ip_ = "";
    endpoint_port_ = 0;
    label_ = "";
    set_endpoint_called_ = false;
    set_label_called_ = false;
}

// Mock ISrsAppConfig for GbListener implementation
MockAppConfigForGbListener::MockAppConfigForGbListener()
{
    stream_caster_listen_port_ = 0;
}

MockAppConfigForGbListener::~MockAppConfigForGbListener()
{
}

int MockAppConfigForGbListener::get_stream_caster_listen(SrsConfDirective *conf)
{
    return stream_caster_listen_port_;
}

// Test SrsGbListener::initialize - covers the major use scenario:
// 1. Listener receives a configuration directive
// 2. Listener copies the configuration
// 3. Listener retrieves the listen port from config
// 4. Listener sets the endpoint (IP and port) on the media listener
// 5. Listener sets the label on the media listener
VOID TEST(GB28181Test, ListenerInitialize)
{
    srs_error_t err = srs_success;

    // Create mock dependencies
    SrsUniquePtr<MockAppConfigForGbListener> mock_config(new MockAppConfigForGbListener());
    MockIpListener *mock_listener = new MockIpListener();

    // Setup mock config to return test port
    mock_config->stream_caster_listen_port_ = 9000;

    // Create listener
    SrsUniquePtr<SrsGbListener> listener(new SrsGbListener());

    // Inject mock dependencies
    listener->config_ = mock_config.get();
    listener->media_listener_ = mock_listener;

    // Create test configuration directive
    SrsUniquePtr<SrsConfDirective> conf(new SrsConfDirective());
    conf->name_ = "stream_caster";
    conf->args_.push_back("gb28181");

    // Add listen directive
    SrsConfDirective *listen_directive = new SrsConfDirective();
    listen_directive->name_ = "listen";
    listen_directive->args_.push_back("9000");
    conf->directives_.push_back(listen_directive);

    // Test initialize() method
    HELPER_EXPECT_SUCCESS(listener->initialize(conf.get()));

    // Verify configuration was copied
    EXPECT_TRUE(listener->conf_ != NULL);
    EXPECT_STREQ("stream_caster", listener->conf_->name_.c_str());

    // Verify set_endpoint was called with correct parameters
    EXPECT_TRUE(mock_listener->set_endpoint_called_);
    EXPECT_STREQ("0.0.0.0", mock_listener->endpoint_ip_.c_str());
    EXPECT_EQ(9000, mock_listener->endpoint_port_);

    // Verify set_label was called with correct label
    EXPECT_TRUE(mock_listener->set_label_called_);
    EXPECT_STREQ("GB-TCP", mock_listener->label_.c_str());

    // Clean up - set to NULL to avoid double-free
    listener->media_listener_ = NULL;
    srs_freep(mock_listener);
}

// Mock ISrsHttpServeMux implementation
MockHttpServeMuxForGbListener::MockHttpServeMuxForGbListener()
{
    handle_called_ = false;
    handle_pattern_ = "";
    handle_handler_ = NULL;
}

MockHttpServeMuxForGbListener::~MockHttpServeMuxForGbListener()
{
}

srs_error_t MockHttpServeMuxForGbListener::handle(std::string pattern, ISrsHttpHandler *handler)
{
    handle_called_ = true;
    handle_pattern_ = pattern;
    handle_handler_ = handler;
    return srs_success;
}

srs_error_t MockHttpServeMuxForGbListener::serve_http(ISrsHttpResponseWriter *w, ISrsHttpMessage *r)
{
    return srs_success;
}

void MockHttpServeMuxForGbListener::reset()
{
    handle_called_ = false;
    handle_pattern_ = "";
    handle_handler_ = NULL;
}

// Mock ISrsApiServerOwner implementation
MockApiServerOwnerForGbListener::MockApiServerOwnerForGbListener()
{
    mux_ = NULL;
}

MockApiServerOwnerForGbListener::~MockApiServerOwnerForGbListener()
{
    mux_ = NULL;
}

ISrsHttpServeMux *MockApiServerOwnerForGbListener::api_server()
{
    return mux_;
}

// Mock ISrsIpListener implementation
MockIpListenerForGbListen::MockIpListenerForGbListen()
{
    listen_called_ = false;
}

MockIpListenerForGbListen::~MockIpListenerForGbListen()
{
}

ISrsListener *MockIpListenerForGbListen::set_endpoint(const std::string &i, int p)
{
    return this;
}

ISrsListener *MockIpListenerForGbListen::set_label(const std::string &label)
{
    return this;
}

srs_error_t MockIpListenerForGbListen::listen()
{
    listen_called_ = true;
    return srs_success;
}

void MockIpListenerForGbListen::reset()
{
    listen_called_ = false;
}

// Test SrsGbListener::listen - covers the major use scenario:
// 1. Listener calls media_listener_->listen() to start listening for TCP connections
// 2. Listener calls listen_api() to register HTTP API handlers
// 3. listen_api() retrieves the HTTP mux from api_server_owner_
// 4. listen_api() registers the /gb/v1/publish/ endpoint with the mux
VOID TEST(GB28181Test, ListenerListen)
{
    srs_error_t err;

    // Create mock dependencies
    MockIpListenerForGbListen *mock_media_listener = new MockIpListenerForGbListen();
    SrsUniquePtr<MockHttpServeMuxForGbListener> mock_mux(new MockHttpServeMuxForGbListener());
    SrsUniquePtr<MockApiServerOwnerForGbListener> mock_api_owner(new MockApiServerOwnerForGbListener());

    // Setup mock api_server_owner to return mock mux
    mock_api_owner->mux_ = mock_mux.get();

    // Create listener
    SrsUniquePtr<SrsGbListener> listener(new SrsGbListener());

    // Inject mock dependencies
    listener->media_listener_ = mock_media_listener;
    listener->api_server_owner_ = mock_api_owner.get();

    // Create test configuration directive
    SrsUniquePtr<SrsConfDirective> conf(new SrsConfDirective());
    conf->name_ = "stream_caster";
    conf->args_.push_back("gb28181");
    listener->conf_ = conf->copy();

    // Test listen() method
    HELPER_EXPECT_SUCCESS(listener->listen());

    // Verify media_listener_->listen() was called
    EXPECT_TRUE(mock_media_listener->listen_called_);

    // Verify mux->handle() was called with correct pattern
    EXPECT_TRUE(mock_mux->handle_called_);
    EXPECT_STREQ("/gb/v1/publish/", mock_mux->handle_pattern_.c_str());
    EXPECT_TRUE(mock_mux->handle_handler_ != NULL);

    // Clean up - set to NULL to avoid double-free
    listener->media_listener_ = NULL;
    srs_freep(mock_media_listener);
}

// Mock ISrsInterruptable for testing SrsGbMediaTcpConn::setup_owner
class MockInterruptableForGbMediaTcpConn : public ISrsInterruptable
{
public:
    bool interrupt_called_;
    srs_error_t pull_error_;

public:
    MockInterruptableForGbMediaTcpConn()
    {
        interrupt_called_ = false;
        pull_error_ = srs_success;
    }
    virtual ~MockInterruptableForGbMediaTcpConn()
    {
        srs_freep(pull_error_);
    }

public:
    virtual void interrupt()
    {
        interrupt_called_ = true;
    }
    virtual srs_error_t pull()
    {
        return srs_error_copy(pull_error_);
    }
};

// Mock ISrsContextIdSetter for testing SrsGbMediaTcpConn::setup_owner
class MockContextIdSetterForGbMediaTcpConn : public ISrsContextIdSetter
{
public:
    bool set_cid_called_;
    SrsContextId received_cid_;

public:
    MockContextIdSetterForGbMediaTcpConn()
    {
        set_cid_called_ = false;
    }
    virtual ~MockContextIdSetterForGbMediaTcpConn()
    {
    }

public:
    virtual void set_cid(const SrsContextId &cid)
    {
        set_cid_called_ = true;
        received_cid_ = cid;
    }
};

// Test SrsGbMediaTcpConn::setup_owner, on_executor_done, is_connected, set_cid, get_id, desc
// This test covers the major use scenario:
// 1. Create SrsGbMediaTcpConn and setup owner with wrapper, coroutine, and cid setter
// 2. Verify is_connected() returns false initially
// 3. Call set_cid() and verify it propagates to owner_cid_
// 4. Verify get_id() returns the correct context id
// 5. Verify desc() returns correct description
// 6. Call on_executor_done() and verify owner_coroutine_ is cleared
VOID TEST(GbMediaTcpConnTest, SetupOwnerAndLifecycle)
{
    // Create mock dependencies
    SrsUniquePtr<MockInterruptableForGbMediaTcpConn> mock_coroutine(new MockInterruptableForGbMediaTcpConn());
    SrsUniquePtr<MockContextIdSetterForGbMediaTcpConn> mock_cid_setter(new MockContextIdSetterForGbMediaTcpConn());

    // Create SrsGbMediaTcpConn - wrapper will own it
    SrsGbMediaTcpConn *conn = new SrsGbMediaTcpConn();

    // Create a wrapper that owns the conn
    SrsUniquePtr<SrsSharedResource<ISrsGbMediaTcpConn> > wrapper(new SrsSharedResource<ISrsGbMediaTcpConn>(conn));

    // Test setup_owner
    conn->setup_owner(wrapper.get(), mock_coroutine.get(), mock_cid_setter.get());

    // Verify is_connected() returns false initially
    EXPECT_FALSE(conn->is_connected());

    // Test set_cid() - should propagate to owner_cid_
    SrsContextId test_cid;
    conn->set_cid(test_cid);

    // Verify owner_cid_->set_cid() was called
    EXPECT_TRUE(mock_cid_setter->set_cid_called_);
    EXPECT_EQ(0, test_cid.compare(mock_cid_setter->received_cid_));

    // Test get_id() - should return the cid we set
    const SrsContextId &returned_cid = conn->get_id();
    EXPECT_EQ(0, test_cid.compare(returned_cid));

    // Test desc() - should return "GB-Media-TCP"
    EXPECT_STREQ("GB-Media-TCP", conn->desc().c_str());

    // Test on_executor_done() - should clear owner_coroutine_
    conn->on_executor_done(mock_coroutine.get());
    // After on_executor_done, owner_coroutine_ should be NULL
    // We can't directly verify this, but we can verify the method doesn't crash

    // wrapper will automatically clean up conn when it goes out of scope
}

// Mock ISrsGbSession implementation
MockGbSessionForMediaConn::MockGbSessionForMediaConn()
{
    on_ps_pack_called_ = false;
    received_pack_ = NULL;
    received_ps_ = NULL;
    on_media_transport_called_ = false;
}

MockGbSessionForMediaConn::~MockGbSessionForMediaConn()
{
}

void MockGbSessionForMediaConn::setup(SrsConfDirective *conf)
{
}

void MockGbSessionForMediaConn::setup_owner(SrsSharedResource<ISrsGbSession> *wrapper, ISrsInterruptable *owner_coroutine, ISrsContextIdSetter *owner_cid)
{
}

void MockGbSessionForMediaConn::on_media_transport(SrsSharedResource<ISrsGbMediaTcpConn> media)
{
    on_media_transport_called_ = true;
    received_media_ = media;
}

void MockGbSessionForMediaConn::on_ps_pack(ISrsPackContext *ctx, SrsPsPacket *ps, const std::vector<SrsTsMessage *> &msgs)
{
    on_ps_pack_called_ = true;
    received_pack_ = ctx;
    received_ps_ = ps;
    received_msgs_ = msgs;
}

const SrsContextId &MockGbSessionForMediaConn::get_id()
{
    static SrsContextId cid;
    return cid;
}

std::string MockGbSessionForMediaConn::desc()
{
    return "MockGbSessionForMediaConn";
}

srs_error_t MockGbSessionForMediaConn::cycle()
{
    return srs_success;
}

void MockGbSessionForMediaConn::on_executor_done(ISrsInterruptable *executor)
{
}

void MockGbSessionForMediaConn::reset()
{
    on_ps_pack_called_ = false;
    received_pack_ = NULL;
    received_ps_ = NULL;
    received_msgs_.clear();
    on_media_transport_called_ = false;
}

// Mock ISrsResourceManager implementation
MockResourceManagerForBindSession::MockResourceManagerForBindSession()
{
    session_to_return_ = NULL;
}

MockResourceManagerForBindSession::~MockResourceManagerForBindSession()
{
}

srs_error_t MockResourceManagerForBindSession::start()
{
    return srs_success;
}

bool MockResourceManagerForBindSession::empty()
{
    return true;
}

size_t MockResourceManagerForBindSession::size()
{
    return 0;
}

void MockResourceManagerForBindSession::add(ISrsResource *conn, bool *exists)
{
}

void MockResourceManagerForBindSession::add_with_id(const std::string &id, ISrsResource *conn)
{
}

void MockResourceManagerForBindSession::add_with_fast_id(uint64_t id, ISrsResource *conn)
{
}

ISrsResource *MockResourceManagerForBindSession::at(int index)
{
    return NULL;
}

ISrsResource *MockResourceManagerForBindSession::find_by_id(std::string id)
{
    return NULL;
}

ISrsResource *MockResourceManagerForBindSession::find_by_fast_id(uint64_t id)
{
    return session_to_return_;
}

ISrsResource *MockResourceManagerForBindSession::find_by_name(std::string /*name*/)
{
    return NULL;
}

void MockResourceManagerForBindSession::remove(ISrsResource *c)
{
}

void MockResourceManagerForBindSession::subscribe(ISrsDisposingHandler *h)
{
}

void MockResourceManagerForBindSession::unsubscribe(ISrsDisposingHandler *h)
{
}

void MockResourceManagerForBindSession::reset()
{
    session_to_return_ = NULL;
}

// Test SrsGbMediaTcpConn::on_ps_pack - covers the major use scenario:
// 1. Media connection receives PS pack with messages
// 2. Connection state changes from disconnected to connected
// 3. Session is notified about the PS pack via on_ps_pack callback
VOID TEST(GbMediaTcpConnTest, OnPsPack)
{
    // Create mock dependencies
    MockGbSessionForMediaConn *mock_session = new MockGbSessionForMediaConn();
    MockPackContext *mock_pack = new MockPackContext();

    // Create SrsGbMediaTcpConn - wrapper will own it
    SrsGbMediaTcpConn *conn = new SrsGbMediaTcpConn();

    // Create a wrapper that owns the conn
    SrsUniquePtr<SrsSharedResource<ISrsGbMediaTcpConn> > wrapper(new SrsSharedResource<ISrsGbMediaTcpConn>(conn));

    // Inject mock dependencies
    conn->session_ = mock_session;
    conn->pack_ = mock_pack;

    // Verify initial state - not connected
    EXPECT_FALSE(conn->is_connected());

    // Create test messages: 1 video message and 1 audio message
    std::vector<SrsTsMessage *> msgs;

    // Create video message
    SrsUniquePtr<SrsTsMessage> video(new SrsTsMessage());
    video->sid_ = SrsTsPESStreamIdVideoCommon;
    video->dts_ = 90000;
    video->pts_ = 90000;
    video->payload_->append("video_data", 10);
    msgs.push_back(video.get());

    // Create audio message
    SrsUniquePtr<SrsTsMessage> audio(new SrsTsMessage());
    audio->sid_ = SrsTsPESStreamIdAudioCommon;
    audio->dts_ = 90000;
    audio->pts_ = 90000;
    audio->payload_->append("audio_data", 10);
    msgs.push_back(audio.get());

    // Call on_ps_pack
    srs_error_t err = conn->on_ps_pack(NULL, msgs);
    HELPER_EXPECT_SUCCESS(err);

    // Verify connection state changed to connected
    EXPECT_TRUE(conn->is_connected());

    // Verify session->on_ps_pack was called
    EXPECT_TRUE(mock_session->on_ps_pack_called_);
    EXPECT_EQ(mock_pack, mock_session->received_pack_);
    EXPECT_EQ(NULL, mock_session->received_ps_);
    EXPECT_EQ(2u, mock_session->received_msgs_.size());

    // Clean up - set to NULL to avoid double-free
    // Note: conn destructor will free pack_ and session_, so we set them to NULL
    conn->session_ = NULL;
    conn->pack_ = NULL;
    srs_freep(mock_session);
    srs_freep(mock_pack);
}

// Test SrsGbMediaTcpConn::bind_session - covers the major use scenario:
// 1. Media connection binds to an existing session by SSRC
// 2. Session is found in the resource manager by fast_id (SSRC)
// 3. Session's on_media_transport is called with the media connection wrapper
// 4. The session pointer is returned to the caller
VOID TEST(GbMediaTcpConnTest, BindSession)
{
    srs_error_t err = srs_success;

    // Create mock session wrapped in SrsSharedResource
    MockGbSessionForMediaConn *mock_session = new MockGbSessionForMediaConn();
    SrsUniquePtr<SrsSharedResource<ISrsGbSession> > session_wrapper(new SrsSharedResource<ISrsGbSession>(mock_session));

    // Create mock resource manager
    SrsUniquePtr<MockResourceManagerForBindSession> mock_manager(new MockResourceManagerForBindSession());
    mock_manager->session_to_return_ = session_wrapper.get();

    // Create SrsGbMediaTcpConn - wrapper will own it
    SrsGbMediaTcpConn *conn = new SrsGbMediaTcpConn();
    SrsUniquePtr<SrsSharedResource<ISrsGbMediaTcpConn> > wrapper(new SrsSharedResource<ISrsGbMediaTcpConn>(conn));

    // Inject mock dependencies
    conn->gb_manager_ = mock_manager.get();
    conn->wrapper_ = wrapper.get();

    // Test bind_session with valid SSRC
    uint32_t ssrc = 12345;
    ISrsGbSession *bound_session = NULL;
    err = conn->bind_session(ssrc, &bound_session);
    HELPER_EXPECT_SUCCESS(err);

    // Verify session was found and bound
    EXPECT_TRUE(bound_session != NULL);
    EXPECT_EQ(mock_session, bound_session);

    // Verify session's on_media_transport was called with the wrapper
    EXPECT_TRUE(mock_session->on_media_transport_called_);
    EXPECT_EQ(conn, mock_session->received_media_.get());

    // Test bind_session with zero SSRC (should return success but do nothing)
    mock_session->reset();
    bound_session = NULL;
    err = conn->bind_session(0, &bound_session);
    HELPER_EXPECT_SUCCESS(err);
    EXPECT_TRUE(bound_session == NULL);
    EXPECT_FALSE(mock_session->on_media_transport_called_);

    // Test bind_session when session not found (should return success but do nothing)
    mock_session->reset();
    mock_manager->session_to_return_ = NULL;
    bound_session = NULL;
    err = conn->bind_session(ssrc, &bound_session);
    HELPER_EXPECT_SUCCESS(err);
    EXPECT_TRUE(bound_session == NULL);
    EXPECT_FALSE(mock_session->on_media_transport_called_);

    // Clean up - set to NULL to avoid double-free
    conn->gb_manager_ = NULL;
    conn->wrapper_ = NULL;
}

// Test SrsMpegpsQueue::push - covers the major use scenario:
// 1. Push audio and video packets with unique timestamps
// 2. Handle timestamp collision by adjusting timestamp (+1ms)
// 3. Verify audio/video counters are updated correctly
VOID TEST(MpegpsQueueTest, PushMediaPackets)
{
    srs_error_t err = srs_success;

    // Create SrsMpegpsQueue
    SrsUniquePtr<SrsMpegpsQueue> queue(new SrsMpegpsQueue());

    // Test 1: Push video packet with unique timestamp
    SrsMediaPacket *video1 = new SrsMediaPacket();
    video1->timestamp_ = 1000;
    video1->message_type_ = SrsFrameTypeVideo;
    char *video_data1 = new char[10];
    memset(video_data1, 0x01, 10);
    video1->wrap(video_data1, 10);

    err = queue->push(video1);
    HELPER_EXPECT_SUCCESS(err);
    EXPECT_EQ(1, queue->nb_videos_);
    EXPECT_EQ(0, queue->nb_audios_);
    EXPECT_EQ(1u, queue->msgs_.size());
    EXPECT_EQ(video1, queue->msgs_[1000]);

    // Test 2: Push audio packet with unique timestamp
    SrsMediaPacket *audio1 = new SrsMediaPacket();
    audio1->timestamp_ = 1020;
    audio1->message_type_ = SrsFrameTypeAudio;
    char *audio_data1 = new char[8];
    memset(audio_data1, 0xAF, 8);
    audio1->wrap(audio_data1, 8);

    err = queue->push(audio1);
    HELPER_EXPECT_SUCCESS(err);
    EXPECT_EQ(1, queue->nb_videos_);
    EXPECT_EQ(1, queue->nb_audios_);
    EXPECT_EQ(2u, queue->msgs_.size());
    EXPECT_EQ(audio1, queue->msgs_[1020]);

    // Test 3: Push video packet with timestamp collision - should adjust to 1001
    SrsMediaPacket *video2 = new SrsMediaPacket();
    video2->timestamp_ = 1000; // Same as video1
    video2->message_type_ = SrsFrameTypeVideo;
    char *video_data2 = new char[10];
    memset(video_data2, 0x02, 10);
    video2->wrap(video_data2, 10);

    err = queue->push(video2);
    HELPER_EXPECT_SUCCESS(err);
    EXPECT_EQ(2, queue->nb_videos_);
    EXPECT_EQ(1, queue->nb_audios_);
    EXPECT_EQ(3u, queue->msgs_.size());
    // Timestamp should be adjusted to 1001
    EXPECT_EQ(1001, video2->timestamp_);
    EXPECT_EQ(video2, queue->msgs_[1001]);

    // Test 4: Push audio packet with timestamp collision - should adjust to 1021
    SrsMediaPacket *audio2 = new SrsMediaPacket();
    audio2->timestamp_ = 1020; // Same as audio1
    audio2->message_type_ = SrsFrameTypeAudio;
    char *audio_data2 = new char[8];
    memset(audio_data2, 0xAE, 8);
    audio2->wrap(audio_data2, 8);

    err = queue->push(audio2);
    HELPER_EXPECT_SUCCESS(err);
    EXPECT_EQ(2, queue->nb_videos_);
    EXPECT_EQ(2, queue->nb_audios_);
    EXPECT_EQ(4u, queue->msgs_.size());
    // Timestamp should be adjusted to 1021
    EXPECT_EQ(1021, audio2->timestamp_);
    EXPECT_EQ(audio2, queue->msgs_[1021]);

    // Test 5: Push video packet with multiple collisions - should adjust to 1002
    SrsMediaPacket *video3 = new SrsMediaPacket();
    video3->timestamp_ = 1000; // Will collide with 1000 and 1001
    video3->message_type_ = SrsFrameTypeVideo;
    char *video_data3 = new char[10];
    memset(video_data3, 0x03, 10);
    video3->wrap(video_data3, 10);

    err = queue->push(video3);
    HELPER_EXPECT_SUCCESS(err);
    EXPECT_EQ(3, queue->nb_videos_);
    EXPECT_EQ(2, queue->nb_audios_);
    EXPECT_EQ(5u, queue->msgs_.size());
    // Timestamp should be adjusted to 1002
    EXPECT_EQ(1002, video3->timestamp_);
    EXPECT_EQ(video3, queue->msgs_[1002]);

    // Verify all packets are in the queue with correct timestamps
    EXPECT_EQ(video1, queue->msgs_[1000]);
    EXPECT_EQ(video2, queue->msgs_[1001]);
    EXPECT_EQ(video3, queue->msgs_[1002]);
    EXPECT_EQ(audio1, queue->msgs_[1020]);
    EXPECT_EQ(audio2, queue->msgs_[1021]);
}

// Test SrsMpegpsQueue::dequeue - covers the major use scenario:
// 1. Dequeue returns NULL when there are insufficient packets (< 2 videos or < 2 audios)
// 2. Dequeue returns packets in timestamp order when there are 2+ videos and 2+ audios
// 3. Verify audio/video counters are decremented correctly
// 4. Verify packets are removed from the queue in correct order
VOID TEST(MpegpsQueueTest, DequeueMediaPackets)
{
    srs_error_t err = srs_success;

    // Create SrsMpegpsQueue
    SrsUniquePtr<SrsMpegpsQueue> queue(new SrsMpegpsQueue());

    // Test 1: Dequeue returns NULL when queue is empty
    SrsMediaPacket *msg = queue->dequeue();
    EXPECT_TRUE(msg == NULL);

    // Test 2: Push 1 video and 1 audio - should not dequeue (need 2+ of each)
    SrsMediaPacket *video1 = new SrsMediaPacket();
    video1->timestamp_ = 1000;
    video1->message_type_ = SrsFrameTypeVideo;
    char *video_data1 = new char[10];
    memset(video_data1, 0x01, 10);
    video1->wrap(video_data1, 10);
    err = queue->push(video1);
    HELPER_EXPECT_SUCCESS(err);

    SrsMediaPacket *audio1 = new SrsMediaPacket();
    audio1->timestamp_ = 1020;
    audio1->message_type_ = SrsFrameTypeAudio;
    char *audio_data1 = new char[8];
    memset(audio_data1, 0xAF, 8);
    audio1->wrap(audio_data1, 8);
    err = queue->push(audio1);
    HELPER_EXPECT_SUCCESS(err);

    // Should return NULL - need 2+ videos and 2+ audios
    msg = queue->dequeue();
    EXPECT_TRUE(msg == NULL);
    EXPECT_EQ(1, queue->nb_videos_);
    EXPECT_EQ(1, queue->nb_audios_);

    // Test 3: Push another video and audio - now should dequeue
    SrsMediaPacket *video2 = new SrsMediaPacket();
    video2->timestamp_ = 1040;
    video2->message_type_ = SrsFrameTypeVideo;
    char *video_data2 = new char[10];
    memset(video_data2, 0x02, 10);
    video2->wrap(video_data2, 10);
    err = queue->push(video2);
    HELPER_EXPECT_SUCCESS(err);

    SrsMediaPacket *audio2 = new SrsMediaPacket();
    audio2->timestamp_ = 1060;
    audio2->message_type_ = SrsFrameTypeAudio;
    char *audio_data2 = new char[8];
    memset(audio_data2, 0xAE, 8);
    audio2->wrap(audio_data2, 8);
    err = queue->push(audio2);
    HELPER_EXPECT_SUCCESS(err);

    // Now we have 2 videos and 2 audios - should dequeue in timestamp order
    EXPECT_EQ(2, queue->nb_videos_);
    EXPECT_EQ(2, queue->nb_audios_);
    EXPECT_EQ(4u, queue->msgs_.size());

    // Test 4: Dequeue first packet (video1 at timestamp 1000)
    msg = queue->dequeue();
    EXPECT_TRUE(msg != NULL);
    EXPECT_EQ(1000, msg->timestamp_);
    EXPECT_TRUE(msg->is_video());
    EXPECT_EQ(1, queue->nb_videos_);
    EXPECT_EQ(2, queue->nb_audios_);
    EXPECT_EQ(3u, queue->msgs_.size());
    srs_freep(msg);

    // Test 5: After first dequeue, we have 1 video and 2 audios - should return NULL
    msg = queue->dequeue();
    EXPECT_TRUE(msg == NULL);
    EXPECT_EQ(1, queue->nb_videos_);
    EXPECT_EQ(2, queue->nb_audios_);
    EXPECT_EQ(3u, queue->msgs_.size());

    // Test 6: Push more videos to reach 2+ videos again
    SrsMediaPacket *video3 = new SrsMediaPacket();
    video3->timestamp_ = 1080;
    video3->message_type_ = SrsFrameTypeVideo;
    char *video_data3 = new char[10];
    memset(video_data3, 0x03, 10);
    video3->wrap(video_data3, 10);
    err = queue->push(video3);
    HELPER_EXPECT_SUCCESS(err);

    // Now we have 2 videos and 2 audios again - should dequeue
    EXPECT_EQ(2, queue->nb_videos_);
    EXPECT_EQ(2, queue->nb_audios_);
    EXPECT_EQ(4u, queue->msgs_.size());

    // Dequeue second packet (audio1 at timestamp 1020)
    msg = queue->dequeue();
    EXPECT_TRUE(msg != NULL);
    EXPECT_EQ(1020, msg->timestamp_);
    EXPECT_TRUE(msg->is_audio());
    EXPECT_EQ(2, queue->nb_videos_);
    EXPECT_EQ(1, queue->nb_audios_);
    EXPECT_EQ(3u, queue->msgs_.size());
    srs_freep(msg);
}

// Mock ISrsBasicRtmpClient implementation
MockGbRtmpClient::MockGbRtmpClient()
{
    connect_called_ = false;
    publish_called_ = false;
    close_called_ = false;
    connect_error_ = srs_success;
    publish_error_ = srs_success;
    stream_id_ = 1;
}

MockGbRtmpClient::~MockGbRtmpClient()
{
    srs_freep(connect_error_);
    srs_freep(publish_error_);
}

srs_error_t MockGbRtmpClient::connect()
{
    connect_called_ = true;
    return srs_error_copy(connect_error_);
}

void MockGbRtmpClient::close()
{
    close_called_ = true;
}

srs_error_t MockGbRtmpClient::publish(int chunk_size, bool with_vhost, std::string *pstream)
{
    publish_called_ = true;
    return srs_error_copy(publish_error_);
}

srs_error_t MockGbRtmpClient::play(int chunk_size, bool with_vhost, std::string *pstream)
{
    return srs_success;
}

void MockGbRtmpClient::kbps_sample(const char *label, srs_utime_t age)
{
}

int MockGbRtmpClient::sid()
{
    return stream_id_;
}

srs_error_t MockGbRtmpClient::recv_message(SrsRtmpCommonMessage **pmsg)
{
    return srs_success;
}

srs_error_t MockGbRtmpClient::decode_message(SrsRtmpCommonMessage *msg, SrsRtmpCommand **ppacket)
{
    return srs_success;
}

srs_error_t MockGbRtmpClient::send_and_free_messages(SrsMediaPacket **msgs, int nb_msgs)
{
    return srs_success;
}

srs_error_t MockGbRtmpClient::send_and_free_message(SrsMediaPacket *msg)
{
    return srs_success;
}

void MockGbRtmpClient::set_recv_timeout(srs_utime_t timeout)
{
}

void MockGbRtmpClient::reset()
{
    connect_called_ = false;
    publish_called_ = false;
    close_called_ = false;
    srs_freep(connect_error_);
    srs_freep(publish_error_);
}

// Mock ISrsRawAacStream implementation
MockGbRawAacStream::MockGbRawAacStream()
{
    adts_demux_called_ = false;
    mux_sequence_header_called_ = false;
    mux_aac2flv_called_ = false;
    adts_demux_error_ = srs_success;
    mux_sequence_header_error_ = srs_success;
    mux_aac2flv_error_ = srs_success;
    sequence_header_output_ = "\xAF\x00\x12\x10";  // AAC sequence header
    demux_frame_size_ = 100;
}

MockGbRawAacStream::~MockGbRawAacStream()
{
    srs_freep(adts_demux_error_);
    srs_freep(mux_sequence_header_error_);
    srs_freep(mux_aac2flv_error_);
}

srs_error_t MockGbRawAacStream::adts_demux(SrsBuffer *stream, char **pframe, int *pnb_frame, SrsRawAacStreamCodec &codec)
{
    adts_demux_called_ = true;

    if (adts_demux_error_ != srs_success) {
        return srs_error_copy(adts_demux_error_);
    }

    // Simulate demuxing AAC frame
    if (!stream->empty()) {
        *pframe = stream->data() + stream->pos();
        *pnb_frame = srs_min(demux_frame_size_, stream->left());
        stream->skip(*pnb_frame);

        // Set codec info
        codec.aac_object_ = SrsAacObjectTypeAacLC;
        codec.sampling_frequency_index_ = 4;  // 44100Hz
        codec.channel_configuration_ = 2;     // Stereo
        codec.sound_format_ = 10;             // AAC
        codec.sound_rate_ = 3;                // 44kHz
        codec.sound_size_ = 1;                // 16-bit
        codec.sound_type_ = 1;                // Stereo
    } else {
        *pframe = NULL;
        *pnb_frame = 0;
    }

    return srs_success;
}

srs_error_t MockGbRawAacStream::mux_sequence_header(SrsRawAacStreamCodec *codec, std::string &sh)
{
    mux_sequence_header_called_ = true;

    if (mux_sequence_header_error_ != srs_success) {
        return srs_error_copy(mux_sequence_header_error_);
    }

    sh = sequence_header_output_;
    return srs_success;
}

srs_error_t MockGbRawAacStream::mux_aac2flv(char *frame, int nb_frame, SrsRawAacStreamCodec *codec, uint32_t dts, char **flv, int *nb_flv)
{
    mux_aac2flv_called_ = true;

    if (mux_aac2flv_error_ != srs_success) {
        return srs_error_copy(mux_aac2flv_error_);
    }

    // Simulate muxing AAC to FLV
    *nb_flv = nb_frame + 2;  // 2 bytes for AAC header
    *flv = new char[*nb_flv];
    (*flv)[0] = 0xAF;  // AAC, 44kHz, 16-bit, stereo
    (*flv)[1] = codec->aac_packet_type_;  // 0 for sequence header, 1 for raw data
    if (nb_frame > 0) {
        memcpy(*flv + 2, frame, nb_frame);
    }

    return srs_success;
}

void MockGbRawAacStream::reset()
{
    adts_demux_called_ = false;
    mux_sequence_header_called_ = false;
    mux_aac2flv_called_ = false;
    srs_freep(adts_demux_error_);
    srs_freep(mux_sequence_header_error_);
    srs_freep(mux_aac2flv_error_);
}

// Mock ISrsMpegpsQueue implementation
MockGbMpegpsQueue::MockGbMpegpsQueue()
{
    push_called_ = false;
    push_error_ = srs_success;
    push_count_ = 0;
}

MockGbMpegpsQueue::~MockGbMpegpsQueue()
{
    srs_freep(push_error_);
}

srs_error_t MockGbMpegpsQueue::push(SrsMediaPacket *msg)
{
    push_called_ = true;
    push_count_++;

    if (push_error_ != srs_success) {
        return srs_error_copy(push_error_);
    }

    return srs_success;
}

SrsMediaPacket *MockGbMpegpsQueue::dequeue()
{
    return NULL;
}

void MockGbMpegpsQueue::reset()
{
    push_called_ = false;
    push_count_ = 0;
    srs_freep(push_error_);
}

// Mock ISrsGbSession implementation
MockGbSessionForMuxer::MockGbSessionForMuxer()
{
    device_id_ = "34020000001320000001";
}

MockGbSessionForMuxer::~MockGbSessionForMuxer()
{
}

void MockGbSessionForMuxer::setup(SrsConfDirective *conf)
{
}

void MockGbSessionForMuxer::setup_owner(SrsSharedResource<ISrsGbSession> *wrapper, ISrsInterruptable *owner_coroutine, ISrsContextIdSetter *owner_cid)
{
}

void MockGbSessionForMuxer::on_media_transport(SrsSharedResource<ISrsGbMediaTcpConn> media)
{
}

void MockGbSessionForMuxer::on_ps_pack(ISrsPackContext *ctx, SrsPsPacket *ps, const std::vector<SrsTsMessage *> &msgs)
{
}

const SrsContextId &MockGbSessionForMuxer::get_id()
{
    static SrsContextId cid;
    return cid;
}

std::string MockGbSessionForMuxer::desc()
{
    return "MockGbSessionForMuxer";
}

srs_error_t MockGbSessionForMuxer::cycle()
{
    return srs_success;
}

void MockGbSessionForMuxer::on_executor_done(ISrsInterruptable *executor)
{
}

// Mock ISrsPsPackHandler implementation
MockPsPackHandler::MockPsPackHandler()
{
    on_ps_pack_called_ = false;
    on_ps_pack_count_ = 0;
    last_pack_id_ = 0;
    last_msgs_count_ = 0;
    on_ps_pack_error_ = srs_success;
}

MockPsPackHandler::~MockPsPackHandler()
{
    srs_freep(on_ps_pack_error_);
}

srs_error_t MockPsPackHandler::on_ps_pack(SrsPsPacket *ps, const std::vector<SrsTsMessage *> &msgs)
{
    on_ps_pack_called_ = true;
    on_ps_pack_count_++;
    last_pack_id_ = ps->id_;
    last_msgs_count_ = (int)msgs.size();

    if (on_ps_pack_error_ != srs_success) {
        return srs_error_copy(on_ps_pack_error_);
    }

    return srs_success;
}

void MockPsPackHandler::reset()
{
    on_ps_pack_called_ = false;
    on_ps_pack_count_ = 0;
    last_pack_id_ = 0;
    last_msgs_count_ = 0;
    srs_freep(on_ps_pack_error_);
}

// Test SrsGbMuxer::on_ts_message - covers the major use scenario:
// 1. Process audio TS message with AAC ADTS data
// 2. Connect to RTMP server on first message
// 3. Generate AAC sequence header on first audio frame
// 4. Mux audio frames to FLV and push to queue
VOID TEST(GbMuxerTest, OnTsMessageAudio)
{
    srs_error_t err = srs_success;

    // Create mock session
    SrsUniquePtr<MockGbSessionForMuxer> mock_session(new MockGbSessionForMuxer());
    mock_session->device_id_ = "34020000001320000001";

    // Create SrsGbMuxer
    SrsUniquePtr<SrsGbMuxer> muxer(new SrsGbMuxer(mock_session.get()));
    muxer->setup("rtmp://127.0.0.1/live/[stream]");

    // Create mock dependencies
    MockGbRawAacStream *mock_aac = new MockGbRawAacStream();
    MockGbMpegpsQueue *mock_queue = new MockGbMpegpsQueue();

    // Inject mocks into muxer (but not sdk_ yet - let connect() create it)
    muxer->aac_ = mock_aac;
    muxer->queue_ = mock_queue;

    // We need to mock the app_factory to return our mock SDK
    // For now, let's just test without RTMP connection by setting sdk_ to a mock
    // that's already "connected"
    MockGbRtmpClient *mock_sdk = new MockGbRtmpClient();
    muxer->sdk_ = mock_sdk;  // Simulate already connected

    // Create TS message with audio data (AAC ADTS format)
    SrsUniquePtr<SrsTsMessage> ts_msg(new SrsTsMessage());
    ts_msg->sid_ = SrsTsPESStreamIdAudioCommon;  // Audio stream
    ts_msg->dts_ = 90000;  // 1 second in 90kHz timebase

    // Create AAC ADTS frame data (simplified)
    // ADTS header (7 bytes) + AAC raw data
    char adts_data[200];
    memset(adts_data, 0, sizeof(adts_data));
    // ADTS sync word (12 bits = 0xFFF)
    adts_data[0] = 0xFF;
    adts_data[1] = 0xF1;  // MPEG-4, no CRC
    // Profile, sample rate, channel config (simplified)
    adts_data[2] = 0x50;  // AAC LC, 44.1kHz
    adts_data[3] = 0x80;
    adts_data[4] = 0x00;
    adts_data[5] = 0x1F;
    adts_data[6] = 0xFC;
    // Fill with some audio data
    for (int i = 7; i < 200; i++) {
        adts_data[i] = (char)(i & 0xFF);
    }

    ts_msg->payload_->append(adts_data, 200);

    // Test: Process audio TS message
    err = muxer->on_ts_message(ts_msg.get());
    HELPER_EXPECT_SUCCESS(err);

    // Verify: AAC demux was called
    EXPECT_TRUE(mock_aac->adts_demux_called_);

    // Verify: AAC sequence header was generated
    EXPECT_TRUE(mock_aac->mux_sequence_header_called_);

    // Verify: AAC to FLV muxing was called (sequence header + raw data)
    EXPECT_TRUE(mock_aac->mux_aac2flv_called_);

    // Verify: Messages were pushed to queue (sequence header + raw data)
    EXPECT_TRUE(mock_queue->push_called_);
    EXPECT_GE(mock_queue->push_count_, 2);  // At least sequence header + one raw frame

    // Clean up - set to NULL to avoid double-free
    muxer->sdk_ = NULL;
    muxer->aac_ = NULL;
    muxer->queue_ = NULL;
    srs_freep(mock_sdk);
    srs_freep(mock_aac);
    srs_freep(mock_queue);
}

// Test SrsPackContext::on_ts_message - covers the major use scenario:
// 1. Accumulate multiple TS messages in the same PS pack
// 2. Trigger on_ps_pack callback when a new pack ID is detected
// 3. Correct DTS/PTS timestamps when they are zero
VOID TEST(PackContextTest, OnTsMessageMultiplePacksWithTimestampCorrection)
{
    srs_error_t err = srs_success;

    // Create mock handler
    SrsUniquePtr<MockPsPackHandler> mock_handler(new MockPsPackHandler());

    // Create SrsPackContext
    SrsUniquePtr<SrsPackContext> ctx(new SrsPackContext(mock_handler.get()));

    // Create PS context and packet for first pack
    SrsUniquePtr<SrsPsContext> ps_ctx1(new SrsPsContext());
    SrsUniquePtr<SrsPsPacket> ps_packet1(new SrsPsPacket(ps_ctx1.get()));
    ps_packet1->id_ = 0x10000001;  // First pack ID

    // Create first TS message (video) with valid timestamps
    SrsUniquePtr<SrsTsMessage> msg1(new SrsTsMessage());
    msg1->sid_ = SrsTsPESStreamIdVideoCommon;
    msg1->dts_ = 90000;  // 1 second in 90kHz
    msg1->pts_ = 90000;
    msg1->ps_helper_ = &ps_ctx1->helper_;
    ps_ctx1->helper_.ctx_ = ps_ctx1.get();
    ps_ctx1->helper_.ps_ = ps_packet1.get();

    // Test: Process first message
    err = ctx->on_ts_message(msg1.get());
    HELPER_EXPECT_SUCCESS(err);

    // Verify: Handler not called yet (still accumulating messages in same pack)
    EXPECT_FALSE(mock_handler->on_ps_pack_called_);

    // Create second TS message (audio) in same pack with zero timestamps
    SrsUniquePtr<SrsTsMessage> msg2(new SrsTsMessage());
    msg2->sid_ = SrsTsPESStreamIdAudioCommon;
    msg2->dts_ = 0;  // Zero timestamp - should be corrected
    msg2->pts_ = 0;  // Zero timestamp - should be corrected
    msg2->ps_helper_ = &ps_ctx1->helper_;

    // Test: Process second message in same pack
    err = ctx->on_ts_message(msg2.get());
    HELPER_EXPECT_SUCCESS(err);

    // Verify: Handler still not called (same pack ID)
    EXPECT_FALSE(mock_handler->on_ps_pack_called_);

    // Create PS context and packet for second pack (different ID)
    SrsUniquePtr<SrsPsContext> ps_ctx2(new SrsPsContext());
    SrsUniquePtr<SrsPsPacket> ps_packet2(new SrsPsPacket(ps_ctx2.get()));
    ps_packet2->id_ = 0x10000002;  // Different pack ID

    // Create third TS message (video) in new pack
    SrsUniquePtr<SrsTsMessage> msg3(new SrsTsMessage());
    msg3->sid_ = SrsTsPESStreamIdVideoCommon;
    msg3->dts_ = 180000;  // 2 seconds in 90kHz
    msg3->pts_ = 180000;
    msg3->ps_helper_ = &ps_ctx2->helper_;
    ps_ctx2->helper_.ctx_ = ps_ctx2.get();
    ps_ctx2->helper_.ps_ = ps_packet2.get();

    // Test: Process third message with new pack ID
    err = ctx->on_ts_message(msg3.get());
    HELPER_EXPECT_SUCCESS(err);

    // Verify: Handler was called once (pack ID changed)
    EXPECT_TRUE(mock_handler->on_ps_pack_called_);
    EXPECT_EQ(1, mock_handler->on_ps_pack_count_);
    EXPECT_EQ(0x10000001u, mock_handler->last_pack_id_);
    EXPECT_EQ(2, mock_handler->last_msgs_count_);  // msg1 and msg2
}

// Test SrsPackContext::on_recover_mode - covers recovery statistics and message dropping
VOID TEST(PackContextTest, OnRecoverMode)
{
    srs_error_t err;

    // Create mock handler
    SrsUniquePtr<MockPsPackHandler> mock_handler(new MockPsPackHandler());

    // Create SrsPackContext
    SrsUniquePtr<SrsPackContext> ctx(new SrsPackContext(mock_handler.get()));

    // Verify initial state
    EXPECT_EQ(0u, ctx->media_nn_recovered_);
    EXPECT_EQ(0u, ctx->media_nn_msgs_dropped_);

    // Test: First recovery with empty message queue (nn_recover = 1)
    ctx->on_recover_mode(1);

    // Verify: Recovery counter incremented, no messages dropped
    EXPECT_EQ(1u, ctx->media_nn_recovered_);
    EXPECT_EQ(0u, ctx->media_nn_msgs_dropped_);

    // Setup: Add messages to the queue
    SrsUniquePtr<SrsPsContext> ps_ctx(new SrsPsContext());
    SrsUniquePtr<SrsPsPacket> ps_packet(new SrsPsPacket(ps_ctx.get()));
    ps_packet->id_ = 0x10000001;

    // Create and add first message
    SrsUniquePtr<SrsTsMessage> msg1(new SrsTsMessage());
    msg1->sid_ = SrsTsPESStreamIdVideoCommon;
    msg1->dts_ = 90000;
    msg1->pts_ = 90000;
    msg1->ps_helper_ = &ps_ctx->helper_;
    ps_ctx->helper_.ctx_ = ps_ctx.get();
    ps_ctx->helper_.ps_ = ps_packet.get();

    err = ctx->on_ts_message(msg1.get());
    HELPER_EXPECT_SUCCESS(err);

    // Create and add second message
    SrsUniquePtr<SrsTsMessage> msg2(new SrsTsMessage());
    msg2->sid_ = SrsTsPESStreamIdAudioCommon;
    msg2->dts_ = 90000;
    msg2->pts_ = 90000;
    msg2->ps_helper_ = &ps_ctx->helper_;

    err = ctx->on_ts_message(msg2.get());
    HELPER_EXPECT_SUCCESS(err);

    // Test: Second recovery with messages in queue (nn_recover = 2)
    ctx->on_recover_mode(2);

    // Verify: Recovery counter NOT incremented (nn_recover > 1), but messages dropped
    EXPECT_EQ(1u, ctx->media_nn_recovered_);  // Still 1, not incremented
    EXPECT_EQ(2u, ctx->media_nn_msgs_dropped_);  // 2 messages dropped

    // Add more messages to test another recovery
    SrsUniquePtr<SrsTsMessage> msg3(new SrsTsMessage());
    msg3->sid_ = SrsTsPESStreamIdVideoCommon;
    msg3->dts_ = 180000;
    msg3->pts_ = 180000;
    msg3->ps_helper_ = &ps_ctx->helper_;

    err = ctx->on_ts_message(msg3.get());
    HELPER_EXPECT_SUCCESS(err);

    // Test: Third recovery with one message (nn_recover = 0, should increment)
    ctx->on_recover_mode(0);

    // Verify: Recovery counter incremented (nn_recover <= 1), message dropped
    EXPECT_EQ(2u, ctx->media_nn_recovered_);  // Incremented to 2
    EXPECT_EQ(3u, ctx->media_nn_msgs_dropped_);  // Total 3 messages dropped
}

// Test SrsRecoverablePsContext::decode_rtp with valid RTP packet containing PS data
VOID TEST(RecoverablePsContextTest, DecodeRtpWithValidPacket)
{
    srs_error_t err;

    // Create mock handler
    MockPsHandler handler;

    // Create context under test
    SrsUniquePtr<SrsRecoverablePsContext> context(new SrsRecoverablePsContext());

    // PT=DynamicRTP-Type-96, SSRC=0xBEBD135, Seq=31916, Time=95652000
    // This is a valid RTP packet containing PS data with audio PES packet
    string raw = string(
        "\x80\x60\x7c\xac\x05\xb3\x88\xa0\x0b\xeb\xd1\x35\x00\x00\x01\xc0"
        "\x00\x6e\x8c\x80\x07\x25\x8a\x6d\xa9\xfd\xff\xf8\xff\xf9\x50\x40"
        "\x0c\x9f\xfc\x01\x3a\x2e\x98\x28\x18\x0a\x09\x84\x81\x60\xc0\x50"
        "\x2a\x12\x13\x05\x02\x22\x00\x88\x4c\x40\x11\x09\x85\x02\x61\x10"
        "\xa8\x40\x00\x00\x00\x1f\xa6\x8d\xef\x03\xca\xf0\x63\x7f\x02\xe2"
        "\x1d\x7f\xbf\x3e\x22\xbe\x3d\xf7\xa2\x7c\xba\xe6\xc8\xfb\x35\x9f"
        "\xd1\xa2\xc4\xaa\xc5\x3d\xf6\x67\xfd\xc6\x39\x06\x9f\x9e\xdf\x9b"
        "\x10\xd7\x4f\x59\xfd\xef\xea\xee\xc8\x4c\x40\xe5\xd9\xed\x00\x1c",
        128);
    SrsUniquePtr<SrsBuffer> buffer(new SrsBuffer((char *)raw.data(), raw.length()));

    // Decode RTP packet with no reserved bytes
    HELPER_EXPECT_SUCCESS(context->decode_rtp(buffer.get(), 0, &handler));

    // Verify successful decoding - should get one audio message
    ASSERT_EQ((size_t)1, handler.msgs_.size());
    EXPECT_EQ(0, context->recover_);

    // Verify message properties
    SrsTsMessage *msg = handler.msgs_.front();
    EXPECT_EQ(SrsTsPESStreamIdAudioCommon, msg->sid_);
    EXPECT_EQ(100, msg->PES_packet_length_);

    // Verify RTP header fields were extracted correctly
    EXPECT_EQ(31916, context->ctx_->helper()->rtp_seq_);
    EXPECT_EQ(95652000, context->ctx_->helper()->rtp_ts_);
    EXPECT_EQ(96, context->ctx_->helper()->rtp_pt_);
}

// Test SrsRecoverablePsContext::enter_recover_mode with normal packet size
VOID TEST(RecoverablePsContextTest, EnterRecoverModeWithNormalPacket)
{
    srs_error_t err;

    // Create mock handler
    MockPsHandler handler;

    // Create context under test
    SrsUniquePtr<SrsRecoverablePsContext> context(new SrsRecoverablePsContext());

    // Create a buffer with normal packet size (less than SRS_GB_LARGE_PACKET=1500)
    char data[1000];
    memset(data, 0xFF, sizeof(data));
    SrsUniquePtr<SrsBuffer> stream(new SrsBuffer(data, sizeof(data)));

    // Initialize helper with some test values
    SrsPsDecodeHelper *h = context->ctx_->helper();
    h->rtp_seq_ = 12345;
    h->rtp_ts_ = 90000;
    h->rtp_pt_ = 96;
    h->pack_first_seq_ = 100;
    h->pack_nn_msgs_ = 5;
    h->pack_pre_msg_last_seq_ = 99;
    h->pack_id_ = 1;

    // Create a last message to be reaped
    SrsTsMessage *last = context->ctx_->last();
    last->PES_packet_length_ = 200;

    // Simulate an error that triggers recovery
    srs_error_t test_error = srs_error_new(ERROR_GB_PS_MEDIA, "test decode error");

    // Record initial state
    int initial_recover = context->recover_;
    int initial_pos = stream->pos();

    // Call enter_recover_mode
    err = context->enter_recover_mode(stream.get(), &handler, initial_pos, test_error);

    // Verify: Should succeed (return srs_success) because packet size is normal
    HELPER_EXPECT_SUCCESS(err);

    // Verify: Recovery counter incremented
    EXPECT_EQ(initial_recover + 1, context->recover_);

    // Verify: Stream buffer fully consumed (all bytes skipped)
    EXPECT_EQ(0, stream->left());

    // Verify: Handler's on_recover_mode was called
    // Note: MockPsHandler doesn't track this, but the method was invoked
}

// Test SrsRecoverablePsContext recovery flow: enter recover mode, skip to pack header, quit recover mode
VOID TEST(RecoverablePsContextTest, RecoverModeSkipToPackHeaderAndQuit)
{
    // Create mock handler
    MockPsHandler handler;

    // Create context under test
    SrsUniquePtr<SrsRecoverablePsContext> context(new SrsRecoverablePsContext());

    // Create a buffer with corrupted data followed by pack header (00 00 01 ba)
    // Simulate real scenario: garbage bytes, then pack start code
    char data[100];
    memset(data, 0xFF, sizeof(data)); // Fill with garbage

    // Place pack header at offset 50: 00 00 01 ba
    data[50] = 0x00;
    data[51] = 0x00;
    data[52] = 0x01;
    data[53] = 0xba;

    SrsUniquePtr<SrsBuffer> stream(new SrsBuffer(data, sizeof(data)));

    // Initialize helper with test values
    SrsPsDecodeHelper *h = context->ctx_->helper();
    h->rtp_seq_ = 54321;
    h->rtp_ts_ = 180000;
    h->rtp_pt_ = 96;

    // Simulate entering recover mode first
    context->recover_ = 1;

    // Record initial position
    int initial_pos = stream->pos();
    EXPECT_EQ(0, initial_pos);

    // Call srs_skip_util_pack to find pack header
    bool found = srs_skip_util_pack(stream.get());

    // Verify: Pack header was found
    EXPECT_TRUE(found);

    // Verify: Stream position advanced to pack header (offset 50)
    EXPECT_EQ(50, stream->pos());

    // Verify: Still in recover mode
    EXPECT_EQ(1, context->recover_);

    // Now call quit_recover_mode to exit recover mode
    context->quit_recover_mode(stream.get(), &handler);

    // Verify: Exited recover mode (recover_ reset to 0)
    EXPECT_EQ(0, context->recover_);

    // Verify: Stream position unchanged (still at pack header)
    EXPECT_EQ(50, stream->pos());

    // Verify: Remaining bytes available for decoding
    EXPECT_EQ(50, stream->left());
}

// Mock ISrsHttpMessage implementation for GB publish API
MockHttpMessageForGbPublish::MockHttpMessageForGbPublish()
{
    mock_conn_ = new MockHttpConn();
    set_connection(mock_conn_);
    body_content_ = "";
}

MockHttpMessageForGbPublish::~MockHttpMessageForGbPublish()
{
    srs_freep(mock_conn_);
}

srs_error_t MockHttpMessageForGbPublish::body_read_all(std::string &body)
{
    body = body_content_;
    return srs_success;
}

// Mock ISrsResourceManager implementation for GB publish API
MockResourceManagerForGbPublish::MockResourceManagerForGbPublish()
{
}

MockResourceManagerForGbPublish::~MockResourceManagerForGbPublish()
{
}

srs_error_t MockResourceManagerForGbPublish::start()
{
    return srs_success;
}

bool MockResourceManagerForGbPublish::empty()
{
    return id_map_.empty() && fast_id_map_.empty();
}

size_t MockResourceManagerForGbPublish::size()
{
    return id_map_.size();
}

void MockResourceManagerForGbPublish::add(ISrsResource *conn, bool *exists)
{
}

void MockResourceManagerForGbPublish::add_with_id(const std::string &id, ISrsResource *conn)
{
    id_map_[id] = conn;
}

void MockResourceManagerForGbPublish::add_with_fast_id(uint64_t id, ISrsResource *conn)
{
    fast_id_map_[id] = conn;
}

ISrsResource *MockResourceManagerForGbPublish::at(int index)
{
    return NULL;
}

ISrsResource *MockResourceManagerForGbPublish::find_by_id(std::string id)
{
    if (id_map_.find(id) != id_map_.end()) {
        return id_map_[id];
    }
    return NULL;
}

ISrsResource *MockResourceManagerForGbPublish::find_by_fast_id(uint64_t id)
{
    if (fast_id_map_.find(id) != fast_id_map_.end()) {
        return fast_id_map_[id];
    }
    return NULL;
}

ISrsResource *MockResourceManagerForGbPublish::find_by_name(std::string /*name*/)
{
    return NULL;
}

void MockResourceManagerForGbPublish::remove(ISrsResource *c)
{
}

void MockResourceManagerForGbPublish::subscribe(ISrsDisposingHandler *h)
{
}

void MockResourceManagerForGbPublish::unsubscribe(ISrsDisposingHandler *h)
{
}

void MockResourceManagerForGbPublish::reset()
{
    id_map_.clear();
    fast_id_map_.clear();
}

// Mock ISrsGbSession implementation for GB publish API
MockGbSessionForApiPublish::MockGbSessionForApiPublish()
{
    setup_called_ = false;
    setup_owner_called_ = false;
    setup_conf_ = NULL;
    owner_coroutine_ = NULL;
}

MockGbSessionForApiPublish::~MockGbSessionForApiPublish()
{
    owner_coroutine_ = NULL;
}

void MockGbSessionForApiPublish::setup(SrsConfDirective *conf)
{
    setup_called_ = true;
    setup_conf_ = conf;
}

void MockGbSessionForApiPublish::setup_owner(SrsSharedResource<ISrsGbSession> *wrapper, ISrsInterruptable *owner_coroutine, ISrsContextIdSetter *owner_cid)
{
    setup_owner_called_ = true;
    owner_coroutine_ = owner_coroutine;
}

void MockGbSessionForApiPublish::on_media_transport(SrsSharedResource<ISrsGbMediaTcpConn> media)
{
}

void MockGbSessionForApiPublish::on_ps_pack(ISrsPackContext *ctx, SrsPsPacket *ps, const std::vector<SrsTsMessage *> &msgs)
{
}

const SrsContextId &MockGbSessionForApiPublish::get_id()
{
    static SrsContextId cid;
    return cid;
}

std::string MockGbSessionForApiPublish::desc()
{
    return "MockGbSessionForApiPublish";
}

srs_error_t MockGbSessionForApiPublish::cycle()
{
    srs_error_t err = srs_success;

    // Block like a real session would, until interrupted
    while (true) {
        if (!owner_coroutine_) {
            return err;
        }
        if ((err = owner_coroutine_->pull()) != srs_success) {
            return srs_error_wrap(err, "pull");
        }

        // Sleep to simulate work and allow interruption
        srs_usleep(1 * SRS_UTIME_MILLISECONDS);
    }

    return err;
}

void MockGbSessionForApiPublish::on_executor_done(ISrsInterruptable *executor)
{
    owner_coroutine_ = NULL;
}

void MockGbSessionForApiPublish::reset()
{
    setup_called_ = false;
    setup_owner_called_ = false;
    setup_conf_ = NULL;
    owner_coroutine_ = NULL;
}

// Mock ISrsAppFactory implementation for GB publish API
MockAppFactoryForGbPublish::MockAppFactoryForGbPublish()
{
    mock_gb_session_ = new MockGbSessionForApiPublish();
}

MockAppFactoryForGbPublish::~MockAppFactoryForGbPublish()
{
    srs_freep(mock_gb_session_);
}

ISrsFileWriter *MockAppFactoryForGbPublish::create_file_writer()
{
    return NULL;
}

ISrsFileWriter *MockAppFactoryForGbPublish::create_enc_file_writer()
{
    return NULL;
}

ISrsFileReader *MockAppFactoryForGbPublish::create_file_reader()
{
    return NULL;
}

SrsPath *MockAppFactoryForGbPublish::create_path()
{
    return NULL;
}

SrsLiveSource *MockAppFactoryForGbPublish::create_live_source()
{
    return NULL;
}

ISrsOriginHub *MockAppFactoryForGbPublish::create_origin_hub()
{
    return NULL;
}

ISrsHourGlass *MockAppFactoryForGbPublish::create_hourglass(const std::string &name, ISrsHourGlassHandler *handler, srs_utime_t interval)
{
    return NULL;
}

ISrsBasicRtmpClient *MockAppFactoryForGbPublish::create_rtmp_client(std::string url, srs_utime_t cto, srs_utime_t sto)
{
    return NULL;
}

SrsHttpClient *MockAppFactoryForGbPublish::create_http_client()
{
    return NULL;
}

ISrsHttpResponseReader *MockAppFactoryForGbPublish::create_http_response_reader(ISrsHttpResponseReader *r)
{
    return NULL;
}

ISrsFileReader *MockAppFactoryForGbPublish::create_http_file_reader(ISrsHttpResponseReader *r)
{
    return NULL;
}

ISrsFlvDecoder *MockAppFactoryForGbPublish::create_flv_decoder()
{
    return NULL;
}

ISrsBasicRtmpClient *MockAppFactoryForGbPublish::create_basic_rtmp_client(std::string url, srs_utime_t ctm, srs_utime_t stm)
{
    return NULL;
}

#ifdef SRS_RTSP
ISrsRtspSendTrack *MockAppFactoryForGbPublish::create_rtsp_audio_send_track(ISrsRtspConnection *session, SrsRtcTrackDescription *track_desc)
{
    return NULL;
}

ISrsRtspSendTrack *MockAppFactoryForGbPublish::create_rtsp_video_send_track(ISrsRtspConnection *session, SrsRtcTrackDescription *track_desc)
{
    return NULL;
}
#endif

ISrsFlvTransmuxer *MockAppFactoryForGbPublish::create_flv_transmuxer()
{
    return NULL;
}

ISrsMp4Encoder *MockAppFactoryForGbPublish::create_mp4_encoder()
{
    return NULL;
}

SrsDvrFlvSegmenter *MockAppFactoryForGbPublish::create_dvr_flv_segmenter()
{
    return NULL;
}

SrsDvrMp4Segmenter *MockAppFactoryForGbPublish::create_dvr_mp4_segmenter()
{
    return NULL;
}

ISrsGbMediaTcpConn *MockAppFactoryForGbPublish::create_gb_media_tcp_conn()
{
    return NULL;
}

ISrsGbSession *MockAppFactoryForGbPublish::create_gb_session()
{
    // Return the mock session (ownership transferred to caller)
    MockGbSessionForApiPublish *session = mock_gb_session_;
    mock_gb_session_ = new MockGbSessionForApiPublish();
    return session;
}

void MockAppFactoryForGbPublish::reset()
{
    srs_freep(mock_gb_session_);
    mock_gb_session_ = new MockGbSessionForApiPublish();
}

// Test SrsGoApiGbPublish::do_serve_http - successful GB session creation
VOID TEST(GB28181Test, GoApiGbPublishSuccess)
{
    srs_error_t err;

    // Create mock dependencies
    SrsUniquePtr<MockResponseWriter> mock_writer(new MockResponseWriter());
    SrsUniquePtr<MockHttpMessageForGbPublish> mock_request(new MockHttpMessageForGbPublish());
    SrsUniquePtr<MockResourceManagerForGbPublish> mock_manager(new MockResourceManagerForGbPublish());
    SrsUniquePtr<MockAppFactoryForGbPublish> mock_factory(new MockAppFactoryForGbPublish());
    SrsUniquePtr<MockAppConfigForGbListener> mock_config(new MockAppConfigForGbListener());

    // Set up test configuration
    SrsConfDirective *conf = new SrsConfDirective();
    conf->name_ = "stream_caster";
    mock_config->stream_caster_listen_port_ = 9000;

    // Create API handler
    SrsUniquePtr<SrsGoApiGbPublish> api(new SrsGoApiGbPublish(conf));

    // Inject mock dependencies
    api->config_ = mock_config.get();
    api->gb_manager_ = mock_manager.get();
    api->app_factory_ = mock_factory.get();

    // Set up request body with valid JSON
    mock_request->body_content_ = "{\"id\":\"34020000001320000001\",\"ssrc\":\"1234567890\"}";

    // Create response object
    SrsUniquePtr<SrsJsonObject> res(SrsJsonAny::object());

    // Call do_serve_http
    err = api->do_serve_http(mock_writer.get(), mock_request.get(), res.get());
    HELPER_EXPECT_SUCCESS(err);

    // Verify response contains expected fields
    SrsJsonAny *code = res->get_property("code");
    EXPECT_TRUE(code != NULL);
    EXPECT_TRUE(code->is_integer());
    EXPECT_EQ(ERROR_SUCCESS, code->to_integer());

    SrsJsonAny *port = res->get_property("port");
    EXPECT_TRUE(port != NULL);
    EXPECT_TRUE(port->is_integer());
    EXPECT_EQ(9000, port->to_integer());

    SrsJsonAny *is_tcp = res->get_property("is_tcp");
    EXPECT_TRUE(is_tcp != NULL);
    EXPECT_TRUE(is_tcp->is_boolean());
    EXPECT_TRUE(is_tcp->to_boolean());

    // Verify session was created and registered
    ISrsResource *session_by_id = mock_manager->find_by_id("34020000001320000001");
    EXPECT_TRUE(session_by_id != NULL);

    ISrsResource *session_by_ssrc = mock_manager->find_by_fast_id(1234567890);
    EXPECT_TRUE(session_by_ssrc != NULL);

    // Verify both lookups return the same session
    EXPECT_EQ(session_by_id, session_by_ssrc);

    // Verify Connection header was set to Close
    std::string connection_header = mock_writer->header()->get("Connection");
    EXPECT_STREQ("Close", connection_header.c_str());

    // Clean up - interrupt and remove the created session/executor
    // The session is wrapped in SrsSharedResource, and the executor manages it
    SrsSharedResource<ISrsGbSession> *session_wrapper = dynamic_cast<SrsSharedResource<ISrsGbSession>*>(session_by_id);
    if (session_wrapper) {
        ISrsGbSession *session = session_wrapper->get();
        MockGbSessionForApiPublish *mock_session = dynamic_cast<MockGbSessionForApiPublish*>(session);
        if (mock_session && mock_session->owner_coroutine_) {
            // Interrupt the executor coroutine to stop the cycle
            mock_session->owner_coroutine_->interrupt();
        }
    }

    // Wait a bit for the coroutine to finish
    srs_usleep(1 * SRS_UTIME_MILLISECONDS);

    // Clean up - set injected fields to NULL to avoid double-free
    api->config_ = NULL;
    api->gb_manager_ = NULL;
    api->app_factory_ = NULL;

    srs_freep(conf);
}

