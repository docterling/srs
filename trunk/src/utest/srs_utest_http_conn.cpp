/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2013-2025 Winlin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <srs_utest_http_conn.hpp>

#include <srs_app_http_conn.hpp>
#include <srs_protocol_conn.hpp>
#include <srs_protocol_http_conn.hpp>
#include <srs_protocol_http_stack.hpp>
#include <srs_protocol_st.hpp>
#include <srs_utest_ai13.hpp>
#include <srs_utest_ai15.hpp>
#include <srs_utest_ai19.hpp>
#include <srs_utest_http.hpp>
#include <srs_utest_mock.hpp>
#include <srs_utest_service.hpp>

// This test is used to verify the basic workflow of the HTTP connection.
// It's finished with the help of AI, but each step is manually designed
// and verified. So this is not dominated by AI, but by humanbeing.
VOID TEST(HttpConnTest, ManuallyVerifyBasicWorkflowForHttpRequest)
{
    srs_error_t err;

    // Create mock objects for dependencies
    SrsUniquePtr<MockAppConfig> mock_config(new MockAppConfig());
    SrsUniquePtr<MockAppFactory> mock_app_factory(new MockAppFactory());
    SrsUniquePtr<MockHttpxConn> mock_handler(new MockHttpxConn());
    MockProtocolReadWriter *mock_io = new MockProtocolReadWriter();
    MockHttpParser *mock_parser = new MockHttpParser();
    SrsUniquePtr<MockHttpServeMux> mock_http_mux(new MockHttpServeMux());

    // Setup mock config
    mock_config->default_vhost_ = new SrsConfDirective();
    mock_config->default_vhost_->name_ = "vhost";
    mock_config->default_vhost_->args_.push_back("__defaultVhost__");

    // Create SrsHttpConn - it takes ownership of mock_io
    SrsUniquePtr<SrsHttpConn> conn(new SrsHttpConn(mock_handler.get(), mock_io, mock_http_mux.get(), "192.168.1.100", 8080));

    // Inject mock dependencies into private fields
    conn->config_ = mock_config.get();
    conn->app_factory_ = mock_app_factory.get();
    srs_freep(conn->parser_);
    conn->parser_ = mock_parser;

    // Start the HTTP connection
    HELPER_EXPECT_SUCCESS(conn->start());

    // Wait for coroutine to start
    srs_usleep(1 * SRS_UTIME_MILLISECONDS);

    // Verify that http_mux->handle() was called
    EXPECT_TRUE(mock_parser->initialize_called_);
    EXPECT_TRUE(mock_handler->on_start_called_);
    EXPECT_TRUE(mock_parser->parse_message_called_);

    // Create a mock HTTP message for the parser to return
    // Note: The parser will take ownership of this message
    MockHttpMessage *mock_message = new MockHttpMessage();
    mock_message->set_url("/api/v1/versions", false);
    mock_parser->messages_.push_back(mock_message);
    mock_parser->cond_->signal();

    // Wait for message processing
    srs_usleep(1 * SRS_UTIME_MILLISECONDS);

    // Verify that http_mux->handle() and serve_http() were called
    EXPECT_EQ(1, mock_http_mux->serve_http_count_);
}

// This test is used to verify the basic workflow of the HTTPx connection.
// It's finished with the help of AI, but each step is manually designed
// and verified. So this is not dominated by AI, but by humanbeing.
VOID TEST(HttpConnTest, ManuallyVerifyBasicWorkflowForHttpxRequest)
{
    srs_error_t err;

    // Create mock objects for dependencies
    SrsUniquePtr<MockAppConfig> mock_config(new MockAppConfig());
    SrsUniquePtr<MockAppFactory> mock_app_factory(new MockAppFactory());
    MockProtocolReadWriter *mock_io = new MockProtocolReadWriter();
    MockHttpParser *mock_parser = new MockHttpParser();
    SrsUniquePtr<MockHttpServeMux> mock_http_mux(new MockHttpServeMux());
    MockSslConnection *mock_ssl = new MockSslConnection();

    // Setup mock config
    mock_config->default_vhost_ = new SrsConfDirective();
    mock_config->default_vhost_->name_ = "vhost";
    mock_config->default_vhost_->args_.push_back("__defaultVhost__");

    // Create mock resource manager
    SrsUniquePtr<MockConnectionManager> mock_manager(new MockConnectionManager());

    // Create SrsHttpxConn - it takes ownership of mock_io
    // Set key and cert to empty to disable SSL
    SrsUniquePtr<SrsHttpxConn> conn(new SrsHttpxConn(mock_manager.get(), mock_io, mock_http_mux.get(), "192.168.1.100", 8080, "", ""));

    // Inject mock dependencies into private fields
    conn->config_ = mock_config.get();
    // Inject mock SSL connection
    srs_freep(conn->ssl_);
    conn->ssl_ = mock_ssl;

    // Access the internal SrsHttpConn through conn_ field (cast from ISrsHttpConn* to SrsHttpConn*)
    SrsHttpConn *http_conn = new SrsHttpConn(conn.get(), mock_ssl, mock_http_mux.get(), "192.168.1.100", 8080);
    http_conn->config_ = mock_config.get();
    http_conn->app_factory_ = mock_app_factory.get();
    srs_freep(http_conn->parser_);
    http_conn->parser_ = mock_parser;

    // Inject the mock SrsHttpConn into conn_ field
    srs_freep(conn->conn_);
    conn->conn_ = http_conn;

    // Start the HTTPx connection
    HELPER_EXPECT_SUCCESS(conn->start());

    // Wait for coroutine to start
    srs_usleep(1 * SRS_UTIME_MILLISECONDS);

    // Verify that parser->initialize() was called
    EXPECT_TRUE(mock_parser->initialize_called_);
    EXPECT_TRUE(mock_parser->parse_message_called_);

    // Verify HTTPS handshake was called
    EXPECT_TRUE(mock_ssl->handshake_called_);

    // Create a mock HTTP message for the parser to return
    // Note: The parser will take ownership of this message
    MockHttpMessage *mock_message = new MockHttpMessage();
    mock_message->set_url("/api/v1/versions", false);
    mock_parser->messages_.push_back(mock_message);
    mock_parser->cond_->signal();

    // Wait for message processing
    srs_usleep(1 * SRS_UTIME_MILLISECONDS);

    // Verify that http_mux->serve_http() was called
    EXPECT_EQ(1, mock_http_mux->serve_http_count_);
}

