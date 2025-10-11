//
// Copyright (c) 2013-2025 The SRS Authors
//
// SPDX-License-Identifier: MIT
//

#include <srs_utest_app16.hpp>

using namespace std;

#include <srs_app_factory.hpp>
#include <srs_app_listener.hpp>
#include <srs_app_rtc_server.hpp>
#include <srs_app_srt_conn.hpp>
#include <srs_app_srt_source.hpp>
#include <srs_kernel_error.hpp>
#include <srs_kernel_st.hpp>
#include <srs_kernel_utility.hpp>
#include <srs_protocol_utility.hpp>
#include <srs_utest_app14.hpp>
#include <sstream>

// Mock ISrsSrtSocket implementation
MockSrtSocket::MockSrtSocket()
{
    recv_timeout_ = 1 * SRS_UTIME_SECONDS;
    send_timeout_ = 1 * SRS_UTIME_SECONDS;
    recv_bytes_ = 0;
    send_bytes_ = 0;
    recvmsg_error_ = srs_success;
    sendmsg_error_ = srs_success;
    recvmsg_called_count_ = 0;
    sendmsg_called_count_ = 0;
    last_recv_data_ = "";
    last_send_data_ = "";
}

MockSrtSocket::~MockSrtSocket()
{
    srs_freep(recvmsg_error_);
    srs_freep(sendmsg_error_);
}

srs_error_t MockSrtSocket::recvmsg(void *buf, size_t size, ssize_t *nread)
{
    recvmsg_called_count_++;
    if (recvmsg_error_ != srs_success) {
        return srs_error_copy(recvmsg_error_);
    }

    // Simulate receiving data
    string test_data = "test data";
    size_t copy_size = srs_min(size, test_data.size());
    memcpy(buf, test_data.c_str(), copy_size);
    *nread = copy_size;
    recv_bytes_ += copy_size;
    last_recv_data_ = string((char *)buf, copy_size);
    return srs_success;
}

srs_error_t MockSrtSocket::sendmsg(void *buf, size_t size, ssize_t *nwrite)
{
    sendmsg_called_count_++;
    if (sendmsg_error_ != srs_success) {
        return srs_error_copy(sendmsg_error_);
    }

    // Simulate sending data
    *nwrite = size;
    send_bytes_ += size;
    last_send_data_ = string((char *)buf, size);
    return srs_success;
}

void MockSrtSocket::set_recv_timeout(srs_utime_t tm)
{
    recv_timeout_ = tm;
}

void MockSrtSocket::set_send_timeout(srs_utime_t tm)
{
    send_timeout_ = tm;
}

srs_utime_t MockSrtSocket::get_send_timeout()
{
    return send_timeout_;
}

srs_utime_t MockSrtSocket::get_recv_timeout()
{
    return recv_timeout_;
}

int64_t MockSrtSocket::get_send_bytes()
{
    return send_bytes_;
}

int64_t MockSrtSocket::get_recv_bytes()
{
    return recv_bytes_;
}

// Mock ISrsUdpHandler implementation
MockUdpHandler::MockUdpHandler()
{
    on_udp_packet_called_ = false;
    packet_count_ = 0;
    last_packet_data_ = "";
    last_packet_size_ = 0;
}

MockUdpHandler::~MockUdpHandler()
{
}

srs_error_t MockUdpHandler::on_udp_packet(const sockaddr *from, const int fromlen, char *buf, int nb_buf)
{
    on_udp_packet_called_ = true;
    packet_count_++;
    last_packet_data_ = string(buf, nb_buf);
    last_packet_size_ = nb_buf;
    return srs_success;
}

// Mock ISrsUdpMuxHandler implementation
MockUdpMuxHandler::MockUdpMuxHandler()
{
    on_udp_packet_called_ = false;
    packet_count_ = 0;
    last_peer_ip_ = "";
    last_peer_port_ = 0;
    last_packet_data_ = "";
    last_packet_size_ = 0;
}

MockUdpMuxHandler::~MockUdpMuxHandler()
{
}

srs_error_t MockUdpMuxHandler::on_udp_packet(ISrsUdpMuxSocket *skt)
{
    on_udp_packet_called_ = true;
    packet_count_++;
    last_peer_ip_ = skt->get_peer_ip();
    last_peer_port_ = skt->get_peer_port();
    last_packet_data_ = string(skt->data(), skt->size());
    last_packet_size_ = skt->size();
    return srs_success;
}

VOID TEST(UdpListenerTest, ListenAndReceivePacket)
{
    srs_error_t err;

    // Generate random port in range [30000, 60000]
    SrsRand rand;
    int port = rand.integer(30000, 60000);

    // Create mock UDP handler
    SrsUniquePtr<MockUdpHandler> mock_handler(new MockUdpHandler());

    // Create UDP listener with mock handler
    SrsUniquePtr<SrsUdpListener> listener(new SrsUdpListener(mock_handler.get()));

    // Set endpoint and label
    listener->set_endpoint("127.0.0.1", port);
    listener->set_label("TEST-UDP");

    // Start listening - this should create UDP socket and start coroutine
    HELPER_EXPECT_SUCCESS(listener->listen());

    // Verify that the listener has a valid file descriptor
    EXPECT_TRUE(listener->stfd() != NULL);

    // Create a client UDP socket to send test packet
    srs_netfd_t client_fd = NULL;
    HELPER_EXPECT_SUCCESS(srs_udp_listen("127.0.0.1", 0, &client_fd));
    EXPECT_TRUE(client_fd != NULL);
    SrsUniquePtr<srs_netfd_t> client_fd_ptr(&client_fd, srs_close_stfd_ptr);

    // Prepare test packet data
    string test_data = "Hello UDP Listener Test";

    // Send packet to the listener
    sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int sent = srs_sendto(client_fd, (void *)test_data.c_str(), test_data.size(),
                          (sockaddr *)&dest_addr, sizeof(dest_addr), SRS_UTIME_NO_TIMEOUT);
    EXPECT_EQ(sent, (int)test_data.size());

    // Wait a bit for the listener to receive and process the packet
    srs_usleep(10 * SRS_UTIME_MILLISECONDS);

    // Verify that the mock handler received the packet
    EXPECT_TRUE(mock_handler->on_udp_packet_called_);
    EXPECT_EQ(mock_handler->packet_count_, 1);
    EXPECT_EQ(mock_handler->last_packet_size_, (int)test_data.size());
    EXPECT_EQ(mock_handler->last_packet_data_, test_data);

    // Clean up - close the listener
    listener->close();
}

VOID TEST(UdpListenerTest, SetEndpointAndSocketBuffer)
{
    srs_error_t err;

    // Generate random port in range [30000, 60000]
    SrsRand rand;
    int port = rand.integer(30000, 60000);

    // Create mock UDP handler
    SrsUniquePtr<MockUdpHandler> mock_handler(new MockUdpHandler());

    // Create UDP listener with mock handler
    SrsUniquePtr<SrsUdpListener> listener(new SrsUdpListener(mock_handler.get()));

    // Test set_label - should return this for chaining
    ISrsListener *result = listener->set_label("TEST-LABEL");
    EXPECT_EQ(result, listener.get());

    // Test set_endpoint - should return this for chaining
    result = listener->set_endpoint("127.0.0.1", port);
    EXPECT_EQ(result, listener.get());

    // Start listening to create the socket
    HELPER_EXPECT_SUCCESS(listener->listen());

    // Test fd() - should return valid file descriptor
    int fd = listener->fd();
    EXPECT_GT(fd, 0);

    // Test stfd() - should return valid state threads file descriptor
    srs_netfd_t stfd = listener->stfd();
    EXPECT_TRUE(stfd != NULL);
    EXPECT_EQ(srs_netfd_fileno(stfd), fd);

    // Verify socket buffer settings by checking socket options
    int sndbuf = 0;
    socklen_t opt_len = sizeof(sndbuf);
    getsockopt(fd, SOL_SOCKET, SO_SNDBUF, (void *)&sndbuf, &opt_len);
    // Socket buffer should be set (may not be exactly 10M due to OS limits, but should be > 0)
    EXPECT_GT(sndbuf, 0);

    int rcvbuf = 0;
    opt_len = sizeof(rcvbuf);
    getsockopt(fd, SOL_SOCKET, SO_RCVBUF, (void *)&rcvbuf, &opt_len);
    // Socket buffer should be set (may not be exactly 10M due to OS limits, but should be > 0)
    EXPECT_GT(rcvbuf, 0);

    // Clean up - close the listener
    listener->close();
}

VOID TEST(UdpMuxListenerTest, ListenAndCreateSocket)
{
    srs_error_t err;

    // Generate random port in range [30000, 60000]
    SrsRand rand;
    int port = rand.integer(30000, 60000);

    // Create mock UDP mux handler
    SrsUniquePtr<MockUdpMuxHandler> mock_handler(new MockUdpMuxHandler());

    // Create UDP mux listener with mock handler
    SrsUniquePtr<SrsUdpMuxListener> listener(new SrsUdpMuxListener(mock_handler.get(), "127.0.0.1", port));

    // Start listening - this should create UDP socket and start coroutine
    // Note: factory_ is already set to _srs_app_factory in constructor
    HELPER_EXPECT_SUCCESS(listener->listen());

    // Verify that the listener has a valid file descriptor
    EXPECT_TRUE(listener->stfd() != NULL);
    EXPECT_GT(listener->fd(), 0);

    // Verify that we can get the file descriptor
    int fd = listener->fd();
    EXPECT_GT(fd, 0);

    // Verify that the socket is bound to the correct port by checking socket name
    sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    int ret = getsockname(fd, (sockaddr *)&addr, &addr_len);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(ntohs(addr.sin_port), port);
    EXPECT_EQ(addr.sin_family, AF_INET);
}

VOID TEST(UdpMuxListenerTest, SetSocketBuffer)
{
    srs_error_t err;

    // Generate random port in range [30000, 60000]
    SrsRand rand;
    int port = rand.integer(30000, 60000);

    // Create mock UDP mux handler
    SrsUniquePtr<MockUdpMuxHandler> mock_handler(new MockUdpMuxHandler());

    // Create UDP mux listener with mock handler
    SrsUniquePtr<SrsUdpMuxListener> listener(new SrsUdpMuxListener(mock_handler.get(), "127.0.0.1", port));

    // Start listening - this should create UDP socket
    HELPER_EXPECT_SUCCESS(listener->listen());

    // Get the file descriptor
    int fd = listener->fd();
    EXPECT_GT(fd, 0);

    // Wait a bit for the cycle() to start and call set_socket_buffer()
    srs_usleep(1 * SRS_UTIME_MILLISECONDS);

    // Verify SO_SNDBUF is set - should be greater than default
    int sndbuf = 0;
    socklen_t opt_len = sizeof(sndbuf);
    int ret = getsockopt(fd, SOL_SOCKET, SO_SNDBUF, (void *)&sndbuf, &opt_len);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(sndbuf, 0);
    // The actual buffer size may be less than 10M due to OS limits, but should be reasonably large
    // On most systems, it should be at least 1KB if the 10MB request was processed
    EXPECT_GT(sndbuf, 1024);

    // Verify SO_RCVBUF is set - should be greater than default
    int rcvbuf = 0;
    opt_len = sizeof(rcvbuf);
    ret = getsockopt(fd, SOL_SOCKET, SO_RCVBUF, (void *)&rcvbuf, &opt_len);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(rcvbuf, 0);
    // The actual buffer size may be less than 10M due to OS limits, but should be reasonably large
    // On most systems, it should be at least 1KB if the 10MB request was processed
    EXPECT_GT(rcvbuf, 1024);
}

VOID TEST(UdpMuxListenerTest, ReceivePacketFromClient)
{
    srs_error_t err;

    // Generate random port in range [30000, 60000]
    SrsRand rand;
    int port = rand.integer(30000, 60000);

    // Create mock UDP mux handler
    SrsUniquePtr<MockUdpMuxHandler> mock_handler(new MockUdpMuxHandler());

    // Create UDP mux listener with mock handler - this is the UDP server
    SrsUniquePtr<SrsUdpMuxListener> listener(new SrsUdpMuxListener(mock_handler.get(), "127.0.0.1", port));

    // Start listening - this creates the UDP socket and starts the coroutine
    HELPER_EXPECT_SUCCESS(listener->listen());

    // Verify that the listener has a valid file descriptor
    EXPECT_TRUE(listener->stfd() != NULL);
    EXPECT_GT(listener->fd(), 0);

    // Yield to allow the listener coroutine to start and initialize
    srs_usleep(1 * SRS_UTIME_MILLISECONDS);

    // Create a UDP client socket to send test packets
    srs_netfd_t client_fd = NULL;
    HELPER_EXPECT_SUCCESS(srs_udp_listen("127.0.0.1", 0, &client_fd));
    EXPECT_TRUE(client_fd != NULL);
    SrsUniquePtr<srs_netfd_t> client_fd_ptr(&client_fd, srs_close_stfd_ptr);

    // Prepare test packet data
    string test_data = "Hello UDP Mux Listener Test";

    // Send packet from client to the UDP server
    sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int sent = srs_sendto(client_fd, (void *)test_data.c_str(), test_data.size(),
                          (sockaddr *)&dest_addr, sizeof(dest_addr), SRS_UTIME_NO_TIMEOUT);
    EXPECT_EQ(sent, (int)test_data.size());

    // Yield to allow the listener coroutine to start and initialize
    srs_usleep(1 * SRS_UTIME_MILLISECONDS);

    // Verify that the mock handler received the packet via SrsUdpMuxSocket
    EXPECT_TRUE(mock_handler->on_udp_packet_called_);
    EXPECT_EQ(mock_handler->packet_count_, 1);
    EXPECT_EQ(mock_handler->last_packet_size_, (int)test_data.size());
    EXPECT_EQ(mock_handler->last_packet_data_, test_data);

    // Send another packet to verify multiple packets work
    string test_data2 = "Second packet";
    sent = srs_sendto(client_fd, (void *)test_data2.c_str(), test_data2.size(),
                      (sockaddr *)&dest_addr, sizeof(dest_addr), SRS_UTIME_NO_TIMEOUT);
    EXPECT_EQ(sent, (int)test_data2.size());

    // Yield to allow the listener coroutine to start and initialize
    srs_usleep(1 * SRS_UTIME_MILLISECONDS);

    // Verify the second packet was received
    EXPECT_EQ(mock_handler->packet_count_, 2);
    EXPECT_EQ(mock_handler->last_packet_size_, (int)test_data2.size());
    EXPECT_EQ(mock_handler->last_packet_data_, test_data2);
}

VOID TEST(UdpMuxSocketTest, SendtoReplyToClient)
{
    srs_error_t err;

    // Generate random ports in range [30000, 60000] for server and client
    SrsRand rand;
    int server_port = rand.integer(30000, 60000);
    int client_port = rand.integer(30000, 60000);
    while (client_port == server_port) {
        client_port = rand.integer(30000, 60000);
    }

    // Create a standalone UDP server socket (not using listener to avoid interference)
    srs_netfd_t server_fd = NULL;
    HELPER_EXPECT_SUCCESS(srs_udp_listen("127.0.0.1", server_port, &server_fd));
    EXPECT_TRUE(server_fd != NULL);
    SrsUniquePtr<srs_netfd_t> server_fd_ptr(&server_fd, srs_close_stfd_ptr);

    // Create a UDP client socket
    srs_netfd_t client_fd = NULL;
    HELPER_EXPECT_SUCCESS(srs_udp_listen("127.0.0.1", client_port, &client_fd));
    EXPECT_TRUE(client_fd != NULL);
    SrsUniquePtr<srs_netfd_t> client_fd_ptr(&client_fd, srs_close_stfd_ptr);

    // Create SrsUdpMuxSocket wrapping the server socket
    SrsUniquePtr<SrsUdpMuxSocket> server_socket(new SrsUdpMuxSocket(server_fd));

    // Prepare test packet data
    string test_data = "Hello from client";

    // Send packet from client to the server
    sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(server_port);
    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int sent = srs_sendto(client_fd, (void *)test_data.c_str(), test_data.size(),
                          (sockaddr *)&dest_addr, sizeof(dest_addr), SRS_UTIME_NO_TIMEOUT);
    EXPECT_EQ(sent, (int)test_data.size());

    // Yield to allow packet to arrive
    srs_usleep(1 * SRS_UTIME_MILLISECONDS);

    // Receive the packet with server socket - this populates from_ and fromlen_
    int nread = server_socket->recvfrom(1 * SRS_UTIME_MILLISECONDS);
    EXPECT_EQ(nread, (int)test_data.size());
    EXPECT_EQ(string(server_socket->data(), nread), test_data);

    // Verify the peer information is correctly captured
    // Note: peer_id() must be called first to populate peer_ip_ and peer_port_
    string peer_id = server_socket->peer_id();
    EXPECT_FALSE(peer_id.empty());
    EXPECT_EQ(server_socket->get_peer_port(), client_port);
    EXPECT_EQ(server_socket->get_peer_ip(), "127.0.0.1");

    // Now test the sendto functionality - send a reply back to the client
    string reply_data = "Hello from server";
    HELPER_EXPECT_SUCCESS(server_socket->sendto((void *)reply_data.c_str(), reply_data.size(), SRS_UTIME_NO_TIMEOUT));

    // Yield to allow the packet to be sent
    srs_usleep(1 * SRS_UTIME_MILLISECONDS);

    // Receive the reply on the client side
    char recv_buf[1024];
    sockaddr_in from_addr;
    int from_len = sizeof(from_addr);
    nread = srs_recvfrom(client_fd, recv_buf, sizeof(recv_buf), (sockaddr *)&from_addr, &from_len, 1 * SRS_UTIME_MILLISECONDS);

    // Verify the reply was received
    EXPECT_EQ(nread, (int)reply_data.size());
    EXPECT_EQ(string(recv_buf, nread), reply_data);
    EXPECT_EQ(ntohs(from_addr.sin_port), server_port);

    // Test multiple sendto calls to verify yield behavior (nn_msgs_for_yield_ > 20)
    // Send 25 packets to trigger the yield logic
    for (int i = 0; i < 25; i++) {
        std::stringstream ss;
        ss << "msg" << i;
        string msg = ss.str();
        HELPER_EXPECT_SUCCESS(server_socket->sendto((void *)msg.c_str(), msg.size(), SRS_UTIME_NO_TIMEOUT));
    }

    // Verify at least some packets were received (may not be all due to UDP nature)
    srs_usleep(1 * SRS_UTIME_MILLISECONDS);
    int received_count = 0;
    while (received_count < 25) {
        nread = srs_recvfrom(client_fd, recv_buf, sizeof(recv_buf), (sockaddr *)&from_addr, &from_len, 1 * SRS_UTIME_MILLISECONDS);
        if (nread <= 0)
            break;
        received_count++;
    }
    // Should receive at least some packets (UDP may drop some, but most should arrive on localhost)
    EXPECT_GT(received_count, 0);
}

VOID TEST(UdpMuxSocketTest, PeerIdGenerationAndCaching)
{
    srs_error_t err;

    // Generate random ports in range [30000, 60000] for server and client
    SrsRand rand;
    int server_port = rand.integer(30000, 60000);
    int client_port = rand.integer(30000, 60000);
    while (client_port == server_port) {
        client_port = rand.integer(30000, 60000);
    }

    // Create a standalone UDP server socket
    srs_netfd_t server_fd = NULL;
    HELPER_EXPECT_SUCCESS(srs_udp_listen("127.0.0.1", server_port, &server_fd));
    EXPECT_TRUE(server_fd != NULL);
    SrsUniquePtr<srs_netfd_t> server_fd_ptr(&server_fd, srs_close_stfd_ptr);

    // Create a UDP client socket
    srs_netfd_t client_fd = NULL;
    HELPER_EXPECT_SUCCESS(srs_udp_listen("127.0.0.1", client_port, &client_fd));
    EXPECT_TRUE(client_fd != NULL);
    SrsUniquePtr<srs_netfd_t> client_fd_ptr(&client_fd, srs_close_stfd_ptr);

    // Create SrsUdpMuxSocket wrapping the server socket
    SrsUniquePtr<SrsUdpMuxSocket> server_socket(new SrsUdpMuxSocket(server_fd));

    // Prepare test packet data
    string test_data = "Test packet for peer_id";

    // Send packet from client to the server
    sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(server_port);
    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int sent = srs_sendto(client_fd, (void *)test_data.c_str(), test_data.size(),
                          (sockaddr *)&dest_addr, sizeof(dest_addr), SRS_UTIME_NO_TIMEOUT);
    EXPECT_EQ(sent, (int)test_data.size());

    // Yield to allow packet to arrive
    srs_usleep(1 * SRS_UTIME_MILLISECONDS);

    // Receive the packet with server socket - this sets address_changed_ to true
    int nread = server_socket->recvfrom(1 * SRS_UTIME_MILLISECONDS);
    EXPECT_EQ(nread, (int)test_data.size());
    EXPECT_EQ(string(server_socket->data(), nread), test_data);

    // Test peer_id() - first call should generate the peer ID
    string peer_id = server_socket->peer_id();
    EXPECT_FALSE(peer_id.empty());

    // Verify peer_id format is "ip:port"
    std::stringstream expected_peer_id;
    expected_peer_id << "127.0.0.1:" << client_port;
    EXPECT_EQ(peer_id, expected_peer_id.str());

    // Verify get_peer_ip() and get_peer_port() return correct values
    EXPECT_EQ(server_socket->get_peer_ip(), "127.0.0.1");
    EXPECT_EQ(server_socket->get_peer_port(), client_port);

    // Test peer_id() caching - second call should return cached value without regeneration
    string peer_id2 = server_socket->peer_id();
    EXPECT_EQ(peer_id2, peer_id);

    // Send another packet from the same client
    string test_data2 = "Second packet";
    sent = srs_sendto(client_fd, (void *)test_data2.c_str(), test_data2.size(),
                      (sockaddr *)&dest_addr, sizeof(dest_addr), SRS_UTIME_NO_TIMEOUT);
    EXPECT_EQ(sent, (int)test_data2.size());

    // Yield to allow packet to arrive
    srs_usleep(1 * SRS_UTIME_MILLISECONDS);

    // Receive the second packet - this should set address_changed_ to true again
    nread = server_socket->recvfrom(1 * SRS_UTIME_MILLISECONDS);
    EXPECT_EQ(nread, (int)test_data2.size());

    // Call peer_id() again - should regenerate but return the same value (same client)
    string peer_id3 = server_socket->peer_id();
    EXPECT_EQ(peer_id3, peer_id);

    // Test fast_id() - should return non-zero for IPv4
    uint64_t fast_id = server_socket->fast_id();
    EXPECT_GT(fast_id, 0ULL);

    // Verify IP address caching by sending from a different client port
    int client_port2 = rand.integer(30000, 60000);
    while (client_port2 == server_port || client_port2 == client_port) {
        client_port2 = rand.integer(30000, 60000);
    }

    srs_netfd_t client_fd2 = NULL;
    HELPER_EXPECT_SUCCESS(srs_udp_listen("127.0.0.1", client_port2, &client_fd2));
    EXPECT_TRUE(client_fd2 != NULL);
    SrsUniquePtr<srs_netfd_t> client_fd2_ptr(&client_fd2, srs_close_stfd_ptr);

    // Send packet from second client
    string test_data3 = "Third packet from different client";
    sent = srs_sendto(client_fd2, (void *)test_data3.c_str(), test_data3.size(),
                      (sockaddr *)&dest_addr, sizeof(dest_addr), SRS_UTIME_NO_TIMEOUT);
    EXPECT_EQ(sent, (int)test_data3.size());

    // Yield to allow packet to arrive
    srs_usleep(1 * SRS_UTIME_MILLISECONDS);

    // Receive packet from second client
    nread = server_socket->recvfrom(1 * SRS_UTIME_MILLISECONDS);
    EXPECT_EQ(nread, (int)test_data3.size());

    // Call peer_id() - should generate new peer ID with different port
    string peer_id4 = server_socket->peer_id();
    EXPECT_FALSE(peer_id4.empty());

    // Verify new peer_id has different port but same IP (IP should be cached)
    std::stringstream expected_peer_id2;
    expected_peer_id2 << "127.0.0.1:" << client_port2;
    EXPECT_EQ(peer_id4, expected_peer_id2.str());
    EXPECT_NE(peer_id4, peer_id);

    // Verify get_peer_port() returns the new port
    EXPECT_EQ(server_socket->get_peer_port(), client_port2);
    EXPECT_EQ(server_socket->get_peer_ip(), "127.0.0.1");
}

VOID TEST(SrtConnectionTest, ReadWriteAndTimeouts)
{
    srs_error_t err;

    // Create SrsSrtConnection with a dummy SRT file descriptor
    srs_srt_t dummy_fd = 1;
    SrsUniquePtr<SrsSrtConnection> conn(new SrsSrtConnection(dummy_fd));

    // Create mock SRT socket
    MockSrtSocket *mock_socket = new MockSrtSocket();

    // Inject mock socket into the connection
    conn->srt_skt_ = mock_socket;

    // Test initialize - should return success
    HELPER_EXPECT_SUCCESS(conn->initialize());

    // Test set_recv_timeout and get_recv_timeout
    srs_utime_t recv_timeout = 1 * SRS_UTIME_SECONDS;
    conn->set_recv_timeout(recv_timeout);
    EXPECT_EQ(conn->get_recv_timeout(), recv_timeout);
    EXPECT_EQ(mock_socket->recv_timeout_, recv_timeout);

    // Test set_send_timeout and get_send_timeout
    srs_utime_t send_timeout = 1 * SRS_UTIME_SECONDS;
    conn->set_send_timeout(send_timeout);
    EXPECT_EQ(conn->get_send_timeout(), send_timeout);
    EXPECT_EQ(mock_socket->send_timeout_, send_timeout);

    // Test read - should call mock socket's recvmsg
    char read_buf[1024];
    ssize_t nread = 0;
    HELPER_EXPECT_SUCCESS(conn->read(read_buf, sizeof(read_buf), &nread));
    EXPECT_EQ(mock_socket->recvmsg_called_count_, 1);
    EXPECT_GT(nread, 0);
    EXPECT_EQ(mock_socket->recv_bytes_, nread);

    // Test get_recv_bytes
    EXPECT_EQ(conn->get_recv_bytes(), mock_socket->recv_bytes_);

    // Test write - should call mock socket's sendmsg
    string test_data = "Hello SRT";
    ssize_t nwrite = 0;
    HELPER_EXPECT_SUCCESS(conn->write((void *)test_data.c_str(), test_data.size(), &nwrite));
    EXPECT_EQ(mock_socket->sendmsg_called_count_, 1);
    EXPECT_EQ(nwrite, (ssize_t)test_data.size());
    EXPECT_EQ(mock_socket->send_bytes_, (int64_t)test_data.size());
    EXPECT_EQ(mock_socket->last_send_data_, test_data);

    // Test get_send_bytes
    EXPECT_EQ(conn->get_send_bytes(), mock_socket->send_bytes_);

    // Test read_fully - should return error (unsupported method)
    HELPER_EXPECT_FAILED(conn->read_fully(read_buf, sizeof(read_buf), &nread));

    // Test writev - should return error (unsupported method)
    iovec iov[1];
    iov[0].iov_base = (void *)test_data.c_str();
    iov[0].iov_len = test_data.size();
    HELPER_EXPECT_FAILED(conn->writev(iov, 1, &nwrite));

    // Clean up - set to NULL to avoid double-free
    conn->srt_skt_ = NULL;
    srs_freep(mock_socket);
}

// Mock ISrsProtocolReadWriter implementation for SrsSrtRecvThread
MockSrtProtocolReadWriter::MockSrtProtocolReadWriter()
{
    read_error_ = srs_success;
    read_count_ = 0;
    simulate_timeout_ = false;
    test_data_ = "test srt data";
    recv_timeout_ = 1 * SRS_UTIME_SECONDS;
    send_timeout_ = 1 * SRS_UTIME_SECONDS;
    recv_bytes_ = 0;
    send_bytes_ = 0;
}

MockSrtProtocolReadWriter::~MockSrtProtocolReadWriter()
{
    srs_freep(read_error_);
}

srs_error_t MockSrtProtocolReadWriter::read(void *buf, size_t size, ssize_t *nread)
{
    read_count_++;

    // Simulate timeout error
    if (simulate_timeout_) {
        return srs_error_new(ERROR_SRT_TIMEOUT, "srt timeout");
    }

    // Return error if set
    if (read_error_ != srs_success) {
        return srs_error_copy(read_error_);
    }

    // Simulate reading data
    size_t copy_size = srs_min(size, test_data_.size());
    memcpy(buf, test_data_.c_str(), copy_size);
    *nread = copy_size;
    recv_bytes_ += copy_size;
    return srs_success;
}

srs_error_t MockSrtProtocolReadWriter::read_fully(void *buf, size_t size, ssize_t *nread)
{
    return srs_error_new(ERROR_NOT_SUPPORTED, "not supported");
}

srs_error_t MockSrtProtocolReadWriter::write(void *buf, size_t size, ssize_t *nwrite)
{
    *nwrite = size;
    send_bytes_ += size;
    return srs_success;
}

srs_error_t MockSrtProtocolReadWriter::writev(const iovec *iov, int iov_size, ssize_t *nwrite)
{
    return srs_error_new(ERROR_NOT_SUPPORTED, "not supported");
}

void MockSrtProtocolReadWriter::set_recv_timeout(srs_utime_t tm)
{
    recv_timeout_ = tm;
}

srs_utime_t MockSrtProtocolReadWriter::get_recv_timeout()
{
    return recv_timeout_;
}

int64_t MockSrtProtocolReadWriter::get_recv_bytes()
{
    return recv_bytes_;
}

void MockSrtProtocolReadWriter::set_send_timeout(srs_utime_t tm)
{
    send_timeout_ = tm;
}

srs_utime_t MockSrtProtocolReadWriter::get_send_timeout()
{
    return send_timeout_;
}

int64_t MockSrtProtocolReadWriter::get_send_bytes()
{
    return send_bytes_;
}

// Mock ISrsCoroutine implementation for SrsSrtRecvThread
MockSrtCoroutine::MockSrtCoroutine()
{
    pull_error_ = srs_success;
    pull_count_ = 0;
    started_ = false;
}

MockSrtCoroutine::~MockSrtCoroutine()
{
    srs_freep(pull_error_);
}

srs_error_t MockSrtCoroutine::start()
{
    started_ = true;
    return srs_success;
}

void MockSrtCoroutine::stop()
{
}

void MockSrtCoroutine::interrupt()
{
}

srs_error_t MockSrtCoroutine::pull()
{
    pull_count_++;

    // Return error after 2 calls to allow at least one read iteration
    if (pull_error_ != srs_success && pull_count_ > 2) {
        return srs_error_copy(pull_error_);
    }

    return srs_success;
}

const SrsContextId &MockSrtCoroutine::cid()
{
    return cid_;
}

void MockSrtCoroutine::set_cid(const SrsContextId &cid)
{
    cid_ = cid;
}

VOID TEST(SrtRecvThreadTest, StartAndReadData)
{
    srs_error_t err;

    // Create mock SRT connection
    MockSrtProtocolReadWriter *mock_conn = new MockSrtProtocolReadWriter();

    // Create SrsSrtRecvThread with mock connection
    SrsUniquePtr<SrsSrtRecvThread> recv_thread(new SrsSrtRecvThread(mock_conn));

    // Create mock coroutine
    MockSrtCoroutine *mock_trd = new MockSrtCoroutine();

    // Inject mock coroutine into recv thread
    srs_freep(recv_thread->trd_);
    recv_thread->trd_ = mock_trd;

    // Test start - should call mock coroutine's start()
    HELPER_EXPECT_SUCCESS(recv_thread->start());
    EXPECT_TRUE(mock_trd->started_);

    // Test the major use scenario: reading data successfully with timeout handling
    // The do_cycle() method has an infinite loop, so we need to make pull() return error after a few iterations
    // to break the loop. We'll simulate: timeout -> timeout -> pull error (thread quit)

    // Set pull to return error after 3 calls to break the infinite loop
    mock_trd->pull_error_ = srs_error_new(ERROR_THREAD_INTERRUPED, "thread interrupted");

    // Simulate timeout on first read
    mock_conn->simulate_timeout_ = true;

    // Call cycle() which calls do_cycle() - should handle timeout and then exit on pull error
    HELPER_EXPECT_FAILED(recv_thread->cycle());

    // Verify that pull was called and read was called
    EXPECT_GT(mock_trd->pull_count_, 0);
    EXPECT_GT(mock_conn->read_count_, 0);

    // Verify that recv_err_ was set by cycle() when do_cycle() failed
    HELPER_EXPECT_FAILED(recv_thread->get_recv_err());

    // Clean up - set to NULL to avoid double-free
    recv_thread->trd_ = NULL;
    srs_freep(mock_trd);
    srs_freep(mock_conn);
}

VOID TEST(MpegtsSrtConnTest, BasicConnectionInfo)
{
    // Create a dummy SRT file descriptor
    srs_srt_t dummy_fd = 1;
    std::string test_ip = "192.168.1.100";
    int test_port = 9000;

    // Create SrsMpegtsSrtConn with test parameters
    SrsUniquePtr<SrsMpegtsSrtConn> conn(new SrsMpegtsSrtConn(NULL, dummy_fd, test_ip, test_port));

    // Test desc() - should return "srt-ts-conn"
    EXPECT_EQ(conn->desc(), "srt-ts-conn");

    // Test delta() - should return non-NULL delta_ member
    ISrsKbpsDelta *delta = conn->delta();
    EXPECT_TRUE(delta != NULL);

    // Test remote_ip() - should return the IP address passed in constructor
    EXPECT_EQ(conn->remote_ip(), test_ip);

    // Test get_id() - should return the context ID from the thread
    const SrsContextId &cid = conn->get_id();
    EXPECT_TRUE(!cid.empty());

    // Clean up - set members to NULL to avoid double-free of global references
    conn->stat_ = NULL;
    conn->config_ = NULL;
    conn->stream_publish_tokens_ = NULL;
    conn->srt_sources_ = NULL;
    conn->live_sources_ = NULL;
    conn->rtc_sources_ = NULL;
    conn->hooks_ = NULL;
}

// Mock SrsSrtSource implementation for testing on_srt_packet
MockSrtSourceForPacket::MockSrtSourceForPacket()
{
    on_packet_called_count_ = 0;
    on_packet_error_ = srs_success;
    last_packet_ = NULL;
}

MockSrtSourceForPacket::~MockSrtSourceForPacket()
{
    srs_freep(on_packet_error_);
    srs_freep(last_packet_);
}

srs_error_t MockSrtSourceForPacket::on_packet(SrsSrtPacket *packet)
{
    on_packet_called_count_++;

    // Store a copy of the packet for verification
    srs_freep(last_packet_);
    last_packet_ = packet->copy();

    if (on_packet_error_ != srs_success) {
        return srs_error_copy(on_packet_error_);
    }

    return srs_success;
}

VOID TEST(MpegtsSrtConnTest, OnSrtPacketValidTsPacket)
{
    srs_error_t err;

    // Create a dummy SRT file descriptor
    srs_srt_t dummy_fd = 1;
    std::string test_ip = "192.168.1.100";
    int test_port = 9000;

    // Create SrsMpegtsSrtConn with test parameters
    SrsUniquePtr<SrsMpegtsSrtConn> conn(new SrsMpegtsSrtConn(NULL, dummy_fd, test_ip, test_port));

    // Create mock SRT source
    MockSrtSourceForPacket *mock_source = new MockSrtSourceForPacket();

    // Inject mock source into the connection
    conn->srt_source_ = SrsSharedPtr<SrsSrtSource>(mock_source);

    // Test 1: Valid TS packet with correct size (188 bytes) and sync byte (0x47)
    char valid_ts_packet[188];
    memset(valid_ts_packet, 0, sizeof(valid_ts_packet));
    valid_ts_packet[0] = 0x47; // TS sync byte

    HELPER_EXPECT_SUCCESS(conn->on_srt_packet(valid_ts_packet, 188));
    EXPECT_EQ(mock_source->on_packet_called_count_, 1);
    EXPECT_TRUE(mock_source->last_packet_ != NULL);
    EXPECT_EQ(mock_source->last_packet_->size(), 188);
    EXPECT_EQ(mock_source->last_packet_->data()[0], 0x47);

    // Test 2: Valid TS packet with multiple TS packets (376 bytes = 2 * 188)
    char valid_ts_packets[376];
    memset(valid_ts_packets, 0, sizeof(valid_ts_packets));
    valid_ts_packets[0] = 0x47;   // First TS packet sync byte
    valid_ts_packets[188] = 0x47; // Second TS packet sync byte

    HELPER_EXPECT_SUCCESS(conn->on_srt_packet(valid_ts_packets, 376));
    EXPECT_EQ(mock_source->on_packet_called_count_, 2);
    EXPECT_TRUE(mock_source->last_packet_ != NULL);
    EXPECT_EQ(mock_source->last_packet_->size(), 376);

    // Test 3: Invalid length (0 bytes) - should return success but not call on_packet
    int prev_count = mock_source->on_packet_called_count_;
    HELPER_EXPECT_SUCCESS(conn->on_srt_packet(valid_ts_packet, 0));
    EXPECT_EQ(mock_source->on_packet_called_count_, prev_count); // No change

    // Test 4: Invalid length (negative) - should return success but not call on_packet
    HELPER_EXPECT_SUCCESS(conn->on_srt_packet(valid_ts_packet, -1));
    EXPECT_EQ(mock_source->on_packet_called_count_, prev_count); // No change

    // Test 5: Invalid size (not multiple of 188) - should return error
    HELPER_EXPECT_FAILED(conn->on_srt_packet(valid_ts_packet, 100));
    EXPECT_EQ(mock_source->on_packet_called_count_, prev_count); // No change

    // Test 6: Invalid sync byte (not 0x47) - should return error
    char invalid_sync_packet[188];
    memset(invalid_sync_packet, 0, sizeof(invalid_sync_packet));
    invalid_sync_packet[0] = 0x48; // Wrong sync byte

    HELPER_EXPECT_FAILED(conn->on_srt_packet(invalid_sync_packet, 188));
    EXPECT_EQ(mock_source->on_packet_called_count_, prev_count); // No change

    // Test 7: Source returns error - should propagate error
    mock_source->on_packet_error_ = srs_error_new(ERROR_SRT_CONN, "mock error");
    HELPER_EXPECT_FAILED(conn->on_srt_packet(valid_ts_packet, 188));

    // Clean up - set members to NULL to avoid double-free of global references
    conn->stat_ = NULL;
    conn->config_ = NULL;
    conn->stream_publish_tokens_ = NULL;
    conn->srt_sources_ = NULL;
    conn->live_sources_ = NULL;
    conn->rtc_sources_ = NULL;
    conn->hooks_ = NULL;
}

// Mock ISrsAppConfig for SRT HTTP hooks implementation
MockAppConfigForSrtHooks::MockAppConfigForSrtHooks()
{
    on_connect_directive_ = NULL;
    on_close_directive_ = NULL;
    on_publish_directive_ = NULL;
    on_unpublish_directive_ = NULL;
    on_play_directive_ = NULL;
    on_stop_directive_ = NULL;
}

MockAppConfigForSrtHooks::~MockAppConfigForSrtHooks()
{
    clear_on_connect_directive();
    clear_on_close_directive();
    clear_on_publish_directive();
    clear_on_unpublish_directive();
    clear_on_play_directive();
    clear_on_stop_directive();
}

SrsConfDirective *MockAppConfigForSrtHooks::get_vhost_on_connect(std::string vhost)
{
    return on_connect_directive_;
}

SrsConfDirective *MockAppConfigForSrtHooks::get_vhost_on_close(std::string vhost)
{
    return on_close_directive_;
}

SrsConfDirective *MockAppConfigForSrtHooks::get_vhost_on_publish(std::string vhost)
{
    return on_publish_directive_;
}

SrsConfDirective *MockAppConfigForSrtHooks::get_vhost_on_unpublish(std::string vhost)
{
    return on_unpublish_directive_;
}

SrsConfDirective *MockAppConfigForSrtHooks::get_vhost_on_play(std::string vhost)
{
    return on_play_directive_;
}

SrsConfDirective *MockAppConfigForSrtHooks::get_vhost_on_stop(std::string vhost)
{
    return on_stop_directive_;
}

void MockAppConfigForSrtHooks::set_on_connect_urls(const std::vector<std::string> &urls)
{
    clear_on_connect_directive();
    if (!urls.empty()) {
        on_connect_directive_ = new SrsConfDirective();
        on_connect_directive_->name_ = "on_connect";
        on_connect_directive_->args_ = urls;
    }
}

void MockAppConfigForSrtHooks::set_on_close_urls(const std::vector<std::string> &urls)
{
    clear_on_close_directive();
    if (!urls.empty()) {
        on_close_directive_ = new SrsConfDirective();
        on_close_directive_->name_ = "on_close";
        on_close_directive_->args_ = urls;
    }
}

void MockAppConfigForSrtHooks::set_on_publish_urls(const std::vector<std::string> &urls)
{
    clear_on_publish_directive();
    if (!urls.empty()) {
        on_publish_directive_ = new SrsConfDirective();
        on_publish_directive_->name_ = "on_publish";
        on_publish_directive_->args_ = urls;
    }
}

void MockAppConfigForSrtHooks::set_on_unpublish_urls(const std::vector<std::string> &urls)
{
    clear_on_unpublish_directive();
    if (!urls.empty()) {
        on_unpublish_directive_ = new SrsConfDirective();
        on_unpublish_directive_->name_ = "on_unpublish";
        on_unpublish_directive_->args_ = urls;
    }
}

void MockAppConfigForSrtHooks::set_on_play_urls(const std::vector<std::string> &urls)
{
    clear_on_play_directive();
    if (!urls.empty()) {
        on_play_directive_ = new SrsConfDirective();
        on_play_directive_->name_ = "on_play";
        on_play_directive_->args_ = urls;
    }
}

void MockAppConfigForSrtHooks::set_on_stop_urls(const std::vector<std::string> &urls)
{
    clear_on_stop_directive();
    if (!urls.empty()) {
        on_stop_directive_ = new SrsConfDirective();
        on_stop_directive_->name_ = "on_stop";
        on_stop_directive_->args_ = urls;
    }
}

void MockAppConfigForSrtHooks::clear_on_connect_directive()
{
    srs_freep(on_connect_directive_);
}

void MockAppConfigForSrtHooks::clear_on_close_directive()
{
    srs_freep(on_close_directive_);
}

void MockAppConfigForSrtHooks::clear_on_publish_directive()
{
    srs_freep(on_publish_directive_);
}

void MockAppConfigForSrtHooks::clear_on_unpublish_directive()
{
    srs_freep(on_unpublish_directive_);
}

void MockAppConfigForSrtHooks::clear_on_play_directive()
{
    srs_freep(on_play_directive_);
}

void MockAppConfigForSrtHooks::clear_on_stop_directive()
{
    srs_freep(on_stop_directive_);
}

// Mock ISrsHttpHooks for SRT implementation
MockHttpHooksForSrt::MockHttpHooksForSrt()
{
    on_connect_count_ = 0;
    on_connect_error_ = srs_success;
    on_close_count_ = 0;
    on_publish_count_ = 0;
    on_publish_error_ = srs_success;
    on_unpublish_count_ = 0;
    on_play_count_ = 0;
    on_play_error_ = srs_success;
    on_stop_count_ = 0;
}

MockHttpHooksForSrt::~MockHttpHooksForSrt()
{
    srs_freep(on_connect_error_);
    srs_freep(on_publish_error_);
    srs_freep(on_play_error_);
    clear_calls();
}

srs_error_t MockHttpHooksForSrt::on_connect(std::string url, ISrsRequest *req)
{
    on_connect_count_++;
    on_connect_calls_.push_back(std::make_pair(url, req));
    if (on_connect_error_ != srs_success) {
        return srs_error_copy(on_connect_error_);
    }
    return srs_success;
}

void MockHttpHooksForSrt::on_close(std::string url, ISrsRequest *req, int64_t send_bytes, int64_t recv_bytes)
{
    on_close_count_++;
    on_close_calls_.push_back(std::make_tuple(url, req, send_bytes, recv_bytes));
}

srs_error_t MockHttpHooksForSrt::on_publish(std::string url, ISrsRequest *req)
{
    on_publish_count_++;
    on_publish_calls_.push_back(std::make_pair(url, req));
    if (on_publish_error_ != srs_success) {
        return srs_error_copy(on_publish_error_);
    }
    return srs_success;
}

void MockHttpHooksForSrt::on_unpublish(std::string url, ISrsRequest *req)
{
    on_unpublish_count_++;
    on_unpublish_calls_.push_back(std::make_pair(url, req));
}

srs_error_t MockHttpHooksForSrt::on_play(std::string url, ISrsRequest *req)
{
    on_play_count_++;
    on_play_calls_.push_back(std::make_pair(url, req));
    if (on_play_error_ != srs_success) {
        return srs_error_copy(on_play_error_);
    }
    return srs_success;
}

void MockHttpHooksForSrt::on_stop(std::string url, ISrsRequest *req)
{
    on_stop_count_++;
    on_stop_calls_.push_back(std::make_pair(url, req));
}

srs_error_t MockHttpHooksForSrt::on_dvr(SrsContextId cid, std::string url, ISrsRequest *req, std::string file)
{
    return srs_success;
}

srs_error_t MockHttpHooksForSrt::on_hls(SrsContextId cid, std::string url, ISrsRequest *req, std::string file, std::string ts_url,
                                        std::string m3u8, std::string m3u8_url, int sn, srs_utime_t duration)
{
    return srs_success;
}

srs_error_t MockHttpHooksForSrt::on_hls_notify(SrsContextId cid, std::string url, ISrsRequest *req, std::string ts_url, int nb_notify)
{
    return srs_success;
}

srs_error_t MockHttpHooksForSrt::discover_co_workers(std::string url, std::string &host, int &port)
{
    return srs_success;
}

srs_error_t MockHttpHooksForSrt::on_forward_backend(std::string url, ISrsRequest *req, std::vector<std::string> &rtmp_urls)
{
    return srs_success;
}

void MockHttpHooksForSrt::clear_calls()
{
    on_connect_calls_.clear();
    on_connect_count_ = 0;
    on_close_calls_.clear();
    on_close_count_ = 0;
    on_publish_calls_.clear();
    on_publish_count_ = 0;
    on_unpublish_calls_.clear();
    on_unpublish_count_ = 0;
    on_play_calls_.clear();
    on_play_count_ = 0;
    on_stop_calls_.clear();
    on_stop_count_ = 0;
}

VOID TEST(MpegtsSrtConnTest, HttpHooksOnConnect)
{
    srs_error_t err;

    // Create a dummy SRT file descriptor
    srs_srt_t dummy_fd = 1;
    std::string test_ip = "192.168.1.100";
    int test_port = 9000;

    // Create SrsMpegtsSrtConn with test parameters
    SrsUniquePtr<SrsMpegtsSrtConn> conn(new SrsMpegtsSrtConn(NULL, dummy_fd, test_ip, test_port));

    // Create mock config
    SrsUniquePtr<MockAppConfigForSrtHooks> mock_config(new MockAppConfigForSrtHooks());

    // Create mock hooks
    SrsUniquePtr<MockHttpHooksForSrt> mock_hooks(new MockHttpHooksForSrt());

    // Create mock request
    SrsUniquePtr<MockRtcAsyncCallRequest> mock_req(new MockRtcAsyncCallRequest("test.vhost", "live", "stream1"));

    // Inject mocks into connection
    conn->config_ = mock_config.get();
    conn->hooks_ = mock_hooks.get();
    conn->req_ = mock_req.get();

    // Test 1: HTTP hooks disabled - should return success without calling hooks
    mock_config->set_http_hooks_enabled(false);
    HELPER_EXPECT_SUCCESS(conn->http_hooks_on_connect());
    EXPECT_EQ(mock_hooks->on_connect_count_, 0);

    // Test 2: HTTP hooks enabled but no on_connect URLs configured - should return success
    mock_config->set_http_hooks_enabled(true);
    HELPER_EXPECT_SUCCESS(conn->http_hooks_on_connect());
    EXPECT_EQ(mock_hooks->on_connect_count_, 0);

    // Test 3: HTTP hooks enabled with on_connect URLs - should call hooks
    std::vector<std::string> urls;
    urls.push_back("http://localhost:8080/on_connect");
    urls.push_back("http://localhost:8080/on_connect2");
    mock_config->set_on_connect_urls(urls);

    HELPER_EXPECT_SUCCESS(conn->http_hooks_on_connect());
    EXPECT_EQ(mock_hooks->on_connect_count_, 2);
    EXPECT_EQ(mock_hooks->on_connect_calls_.size(), 2);
    EXPECT_EQ(mock_hooks->on_connect_calls_[0].first, "http://localhost:8080/on_connect");
    EXPECT_EQ(mock_hooks->on_connect_calls_[0].second, mock_req.get());
    EXPECT_EQ(mock_hooks->on_connect_calls_[1].first, "http://localhost:8080/on_connect2");

    // Test 4: Hook returns error - should propagate error
    mock_hooks->clear_calls();
    mock_hooks->on_connect_error_ = srs_error_new(ERROR_SYSTEM_STREAM_BUSY, "hook failed");
    HELPER_EXPECT_FAILED(conn->http_hooks_on_connect());

    // Clean up - set to NULL to avoid double-free
    conn->config_ = NULL;
    conn->hooks_ = NULL;
    conn->req_ = NULL;
    conn->stat_ = NULL;
    conn->stream_publish_tokens_ = NULL;
    conn->srt_sources_ = NULL;
    conn->live_sources_ = NULL;
    conn->rtc_sources_ = NULL;
}

VOID TEST(MpegtsSrtConnTest, HttpHooksOnClose)
{
    // Create a dummy SRT file descriptor
    srs_srt_t dummy_fd = 1;
    std::string test_ip = "192.168.1.100";
    int test_port = 9000;

    // Create SrsMpegtsSrtConn with test parameters
    SrsUniquePtr<SrsMpegtsSrtConn> conn(new SrsMpegtsSrtConn(NULL, dummy_fd, test_ip, test_port));

    // Create mock config
    SrsUniquePtr<MockAppConfigForSrtHooks> mock_config(new MockAppConfigForSrtHooks());

    // Create mock hooks
    SrsUniquePtr<MockHttpHooksForSrt> mock_hooks(new MockHttpHooksForSrt());

    // Create mock request
    SrsUniquePtr<MockRtcAsyncCallRequest> mock_req(new MockRtcAsyncCallRequest("test.vhost", "live", "stream1"));

    // Create mock SRT protocol read/writer to track bytes
    MockSrtProtocolReadWriter *mock_srt_conn = new MockSrtProtocolReadWriter();
    mock_srt_conn->send_bytes_ = 1000;
    mock_srt_conn->recv_bytes_ = 2000;

    // Inject mocks into connection
    conn->config_ = mock_config.get();
    conn->hooks_ = mock_hooks.get();
    conn->req_ = mock_req.get();
    conn->srt_conn_ = mock_srt_conn;

    // Test 1: HTTP hooks disabled - should not call hooks
    mock_config->set_http_hooks_enabled(false);
    conn->http_hooks_on_close();
    EXPECT_EQ(mock_hooks->on_close_count_, 0);

    // Test 2: HTTP hooks enabled but no on_close URLs configured - should not call hooks
    mock_config->set_http_hooks_enabled(true);
    conn->http_hooks_on_close();
    EXPECT_EQ(mock_hooks->on_close_count_, 0);

    // Test 3: HTTP hooks enabled with on_close URLs - should call hooks with byte counts
    std::vector<std::string> urls;
    urls.push_back("http://localhost:8080/on_close");
    urls.push_back("http://localhost:8080/on_close2");
    mock_config->set_on_close_urls(urls);

    conn->http_hooks_on_close();
    EXPECT_EQ(mock_hooks->on_close_count_, 2);
    EXPECT_EQ(mock_hooks->on_close_calls_.size(), 2);
    EXPECT_EQ(std::get<0>(mock_hooks->on_close_calls_[0]), "http://localhost:8080/on_close");
    EXPECT_EQ(std::get<1>(mock_hooks->on_close_calls_[0]), mock_req.get());
    EXPECT_EQ(std::get<2>(mock_hooks->on_close_calls_[0]), 1000); // send_bytes
    EXPECT_EQ(std::get<3>(mock_hooks->on_close_calls_[0]), 2000); // recv_bytes

    // Clean up - set to NULL to avoid double-free
    conn->srt_conn_ = NULL;
    srs_freep(mock_srt_conn);
    conn->config_ = NULL;
    conn->hooks_ = NULL;
    conn->req_ = NULL;
    conn->stat_ = NULL;
    conn->stream_publish_tokens_ = NULL;
    conn->srt_sources_ = NULL;
    conn->live_sources_ = NULL;
    conn->rtc_sources_ = NULL;
}

VOID TEST(MpegtsSrtConnTest, HttpHooksOnPublish)
{
    srs_error_t err;

    // Create a dummy SRT file descriptor
    srs_srt_t dummy_fd = 1;
    std::string test_ip = "192.168.1.100";
    int test_port = 9000;

    // Create SrsMpegtsSrtConn with test parameters
    SrsUniquePtr<SrsMpegtsSrtConn> conn(new SrsMpegtsSrtConn(NULL, dummy_fd, test_ip, test_port));

    // Create mock config
    SrsUniquePtr<MockAppConfigForSrtHooks> mock_config(new MockAppConfigForSrtHooks());

    // Create mock hooks
    SrsUniquePtr<MockHttpHooksForSrt> mock_hooks(new MockHttpHooksForSrt());

    // Create mock request
    SrsUniquePtr<MockRtcAsyncCallRequest> mock_req(new MockRtcAsyncCallRequest("test.vhost", "live", "stream1"));

    // Inject mocks into connection
    conn->config_ = mock_config.get();
    conn->hooks_ = mock_hooks.get();
    conn->req_ = mock_req.get();

    // Test 1: HTTP hooks disabled - should return success without calling hooks
    mock_config->set_http_hooks_enabled(false);
    HELPER_EXPECT_SUCCESS(conn->http_hooks_on_publish());
    EXPECT_EQ(mock_hooks->on_publish_count_, 0);

    // Test 2: HTTP hooks enabled but no on_publish URLs configured - should return success
    mock_config->set_http_hooks_enabled(true);
    HELPER_EXPECT_SUCCESS(conn->http_hooks_on_publish());
    EXPECT_EQ(mock_hooks->on_publish_count_, 0);

    // Test 3: HTTP hooks enabled with on_publish URLs - should call hooks
    std::vector<std::string> urls;
    urls.push_back("http://localhost:8080/on_publish");
    urls.push_back("http://localhost:8080/on_publish2");
    mock_config->set_on_publish_urls(urls);

    HELPER_EXPECT_SUCCESS(conn->http_hooks_on_publish());
    EXPECT_EQ(mock_hooks->on_publish_count_, 2);
    EXPECT_EQ(mock_hooks->on_publish_calls_.size(), 2);
    EXPECT_EQ(mock_hooks->on_publish_calls_[0].first, "http://localhost:8080/on_publish");
    EXPECT_EQ(mock_hooks->on_publish_calls_[0].second, mock_req.get());

    // Test 4: Hook returns error - should propagate error
    mock_hooks->clear_calls();
    mock_hooks->on_publish_error_ = srs_error_new(ERROR_SYSTEM_STREAM_BUSY, "publish hook failed");
    HELPER_EXPECT_FAILED(conn->http_hooks_on_publish());

    // Clean up - set to NULL to avoid double-free
    conn->config_ = NULL;
    conn->hooks_ = NULL;
    conn->req_ = NULL;
    conn->stat_ = NULL;
    conn->stream_publish_tokens_ = NULL;
    conn->srt_sources_ = NULL;
    conn->live_sources_ = NULL;
    conn->rtc_sources_ = NULL;
}

VOID TEST(MpegtsSrtConnTest, HttpHooksOnUnpublish)
{
    // Create a dummy SRT file descriptor
    srs_srt_t dummy_fd = 1;
    std::string test_ip = "192.168.1.100";
    int test_port = 9000;

    // Create SrsMpegtsSrtConn with test parameters
    SrsUniquePtr<SrsMpegtsSrtConn> conn(new SrsMpegtsSrtConn(NULL, dummy_fd, test_ip, test_port));

    // Create mock config
    SrsUniquePtr<MockAppConfigForSrtHooks> mock_config(new MockAppConfigForSrtHooks());

    // Create mock hooks
    SrsUniquePtr<MockHttpHooksForSrt> mock_hooks(new MockHttpHooksForSrt());

    // Create mock request
    SrsUniquePtr<MockRtcAsyncCallRequest> mock_req(new MockRtcAsyncCallRequest("test.vhost", "live", "stream1"));

    // Inject mocks into connection
    conn->config_ = mock_config.get();
    conn->hooks_ = mock_hooks.get();
    conn->req_ = mock_req.get();

    // Test 1: HTTP hooks disabled - should not call hooks
    mock_config->set_http_hooks_enabled(false);
    conn->http_hooks_on_unpublish();
    EXPECT_EQ(mock_hooks->on_unpublish_count_, 0);

    // Test 2: HTTP hooks enabled but no on_unpublish URLs configured - should not call hooks
    mock_config->set_http_hooks_enabled(true);
    conn->http_hooks_on_unpublish();
    EXPECT_EQ(mock_hooks->on_unpublish_count_, 0);

    // Test 3: HTTP hooks enabled with on_unpublish URLs - should call hooks
    std::vector<std::string> urls;
    urls.push_back("http://localhost:8080/on_unpublish");
    urls.push_back("http://localhost:8080/on_unpublish2");
    mock_config->set_on_unpublish_urls(urls);

    conn->http_hooks_on_unpublish();
    EXPECT_EQ(mock_hooks->on_unpublish_count_, 2);
    EXPECT_EQ(mock_hooks->on_unpublish_calls_.size(), 2);
    EXPECT_EQ(mock_hooks->on_unpublish_calls_[0].first, "http://localhost:8080/on_unpublish");
    EXPECT_EQ(mock_hooks->on_unpublish_calls_[0].second, mock_req.get());

    // Clean up - set to NULL to avoid double-free
    conn->config_ = NULL;
    conn->hooks_ = NULL;
    conn->req_ = NULL;
    conn->stat_ = NULL;
    conn->stream_publish_tokens_ = NULL;
    conn->srt_sources_ = NULL;
    conn->live_sources_ = NULL;
    conn->rtc_sources_ = NULL;
}

VOID TEST(MpegtsSrtConnTest, HttpHooksOnPlay)
{
    srs_error_t err;

    // Create a dummy SRT file descriptor
    srs_srt_t dummy_fd = 1;
    std::string test_ip = "192.168.1.100";
    int test_port = 9000;

    // Create SrsMpegtsSrtConn with test parameters
    SrsUniquePtr<SrsMpegtsSrtConn> conn(new SrsMpegtsSrtConn(NULL, dummy_fd, test_ip, test_port));

    // Create mock config
    SrsUniquePtr<MockAppConfigForSrtHooks> mock_config(new MockAppConfigForSrtHooks());

    // Create mock hooks
    SrsUniquePtr<MockHttpHooksForSrt> mock_hooks(new MockHttpHooksForSrt());

    // Create mock request
    SrsUniquePtr<MockRtcAsyncCallRequest> mock_req(new MockRtcAsyncCallRequest("test.vhost", "live", "stream1"));

    // Inject mocks into connection
    conn->config_ = mock_config.get();
    conn->hooks_ = mock_hooks.get();
    conn->req_ = mock_req.get();

    // Test 1: HTTP hooks disabled - should return success without calling hooks
    mock_config->set_http_hooks_enabled(false);
    HELPER_EXPECT_SUCCESS(conn->http_hooks_on_play());
    EXPECT_EQ(mock_hooks->on_play_count_, 0);

    // Test 2: HTTP hooks enabled but no on_play URLs configured - should return success
    mock_config->set_http_hooks_enabled(true);
    HELPER_EXPECT_SUCCESS(conn->http_hooks_on_play());
    EXPECT_EQ(mock_hooks->on_play_count_, 0);

    // Test 3: HTTP hooks enabled with on_play URLs - should call hooks
    std::vector<std::string> urls;
    urls.push_back("http://localhost:8080/on_play");
    urls.push_back("http://localhost:8080/on_play2");
    mock_config->set_on_play_urls(urls);

    HELPER_EXPECT_SUCCESS(conn->http_hooks_on_play());
    EXPECT_EQ(mock_hooks->on_play_count_, 2);
    EXPECT_EQ(mock_hooks->on_play_calls_.size(), 2);
    EXPECT_EQ(mock_hooks->on_play_calls_[0].first, "http://localhost:8080/on_play");
    EXPECT_EQ(mock_hooks->on_play_calls_[0].second, mock_req.get());

    // Test 4: Hook returns error - should propagate error
    mock_hooks->clear_calls();
    mock_hooks->on_play_error_ = srs_error_new(ERROR_SYSTEM_STREAM_BUSY, "play hook failed");
    HELPER_EXPECT_FAILED(conn->http_hooks_on_play());

    // Clean up - set to NULL to avoid double-free
    conn->config_ = NULL;
    conn->hooks_ = NULL;
    conn->req_ = NULL;
    conn->stat_ = NULL;
    conn->stream_publish_tokens_ = NULL;
    conn->srt_sources_ = NULL;
    conn->live_sources_ = NULL;
    conn->rtc_sources_ = NULL;
}

VOID TEST(MpegtsSrtConnTest, HttpHooksOnStop)
{
    // Create a dummy SRT file descriptor
    srs_srt_t dummy_fd = 1;
    std::string test_ip = "192.168.1.100";
    int test_port = 9000;

    // Create SrsMpegtsSrtConn with test parameters
    SrsUniquePtr<SrsMpegtsSrtConn> conn(new SrsMpegtsSrtConn(NULL, dummy_fd, test_ip, test_port));

    // Create mock config
    SrsUniquePtr<MockAppConfigForSrtHooks> mock_config(new MockAppConfigForSrtHooks());

    // Create mock hooks
    SrsUniquePtr<MockHttpHooksForSrt> mock_hooks(new MockHttpHooksForSrt());

    // Create mock request
    SrsUniquePtr<MockRtcAsyncCallRequest> mock_req(new MockRtcAsyncCallRequest("test.vhost", "live", "stream1"));

    // Inject mocks into connection
    conn->config_ = mock_config.get();
    conn->hooks_ = mock_hooks.get();
    conn->req_ = mock_req.get();

    // Test 1: HTTP hooks disabled - should not call hooks
    mock_config->set_http_hooks_enabled(false);
    conn->http_hooks_on_stop();
    EXPECT_EQ(mock_hooks->on_stop_count_, 0);

    // Test 2: HTTP hooks enabled but no on_stop URLs configured - should not call hooks
    mock_config->set_http_hooks_enabled(true);
    conn->http_hooks_on_stop();
    EXPECT_EQ(mock_hooks->on_stop_count_, 0);

    // Test 3: HTTP hooks enabled with on_stop URLs - should call hooks
    std::vector<std::string> urls;
    urls.push_back("http://localhost:8080/on_stop");
    urls.push_back("http://localhost:8080/on_stop2");
    mock_config->set_on_stop_urls(urls);

    conn->http_hooks_on_stop();
    EXPECT_EQ(mock_hooks->on_stop_count_, 2);
    EXPECT_EQ(mock_hooks->on_stop_calls_.size(), 2);
    EXPECT_EQ(mock_hooks->on_stop_calls_[0].first, "http://localhost:8080/on_stop");
    EXPECT_EQ(mock_hooks->on_stop_calls_[0].second, mock_req.get());

    // Clean up - set to NULL to avoid double-free
    conn->config_ = NULL;
    conn->hooks_ = NULL;
    conn->req_ = NULL;
    conn->stat_ = NULL;
    conn->stream_publish_tokens_ = NULL;
    conn->srt_sources_ = NULL;
    conn->live_sources_ = NULL;
    conn->rtc_sources_ = NULL;
}

VOID TEST(ApiServerAsCandidatesTest, MajorUseScenario)
{
    srs_error_t err;

    // Create mock config
    SrsUniquePtr<MockAppConfig> mock_config(new MockAppConfig());

    // Test 1: API as candidates disabled - should return success without adding candidates
    mock_config->set_api_as_candidates(false);
    set<string> candidate_ips;
    HELPER_EXPECT_SUCCESS(api_server_as_candidates(mock_config.get(), "192.168.1.100", candidate_ips));
    EXPECT_EQ(candidate_ips.size(), 0);

    // Test 2: Empty API string - should return success without adding candidates
    mock_config->set_api_as_candidates(true);
    candidate_ips.clear();
    HELPER_EXPECT_SUCCESS(api_server_as_candidates(mock_config.get(), "", candidate_ips));
    EXPECT_EQ(candidate_ips.size(), 0);

    // Test 3: Localhost name - should return success without adding candidates
    candidate_ips.clear();
    HELPER_EXPECT_SUCCESS(api_server_as_candidates(mock_config.get(), "localhost", candidate_ips));
    EXPECT_EQ(candidate_ips.size(), 0);

    // Test 4: Loopback addresses - should return success without adding candidates
    candidate_ips.clear();
    HELPER_EXPECT_SUCCESS(api_server_as_candidates(mock_config.get(), "127.0.0.1", candidate_ips));
    EXPECT_EQ(candidate_ips.size(), 0);

    candidate_ips.clear();
    HELPER_EXPECT_SUCCESS(api_server_as_candidates(mock_config.get(), "0.0.0.0", candidate_ips));
    EXPECT_EQ(candidate_ips.size(), 0);

    candidate_ips.clear();
    HELPER_EXPECT_SUCCESS(api_server_as_candidates(mock_config.get(), "::", candidate_ips));
    EXPECT_EQ(candidate_ips.size(), 0);

    // Test 5: Valid IPv4 address - should add to candidates
    candidate_ips.clear();
    HELPER_EXPECT_SUCCESS(api_server_as_candidates(mock_config.get(), "192.168.1.100", candidate_ips));
    EXPECT_EQ(candidate_ips.size(), 1);
    EXPECT_TRUE(candidate_ips.find("192.168.1.100") != candidate_ips.end());

    // Test 6: Domain name with keep_api_domain enabled - should add domain to candidates
    mock_config->set_keep_api_domain(true);
    mock_config->set_resolve_api_domain(false);
    candidate_ips.clear();
    HELPER_EXPECT_SUCCESS(api_server_as_candidates(mock_config.get(), "example.com", candidate_ips));
    EXPECT_EQ(candidate_ips.size(), 1);
    EXPECT_TRUE(candidate_ips.find("example.com") != candidate_ips.end());

    // Test 7: Domain name with resolve_api_domain enabled - should resolve and add IP
    // Note: DNS resolution may fail in test environment, so we test with a domain that resolves to loopback
    // which should be filtered out, resulting in no candidates added
    mock_config->set_keep_api_domain(false);
    mock_config->set_resolve_api_domain(true);
    candidate_ips.clear();
    HELPER_EXPECT_SUCCESS(api_server_as_candidates(mock_config.get(), "localhost", candidate_ips));
    // localhost resolves to 127.0.0.1 which is filtered out
    EXPECT_EQ(candidate_ips.size(), 0);

    // Test 8: Multiple calls should accumulate candidates in the set
    candidate_ips.clear();
    HELPER_EXPECT_SUCCESS(api_server_as_candidates(mock_config.get(), "192.168.1.100", candidate_ips));
    HELPER_EXPECT_SUCCESS(api_server_as_candidates(mock_config.get(), "192.168.1.101", candidate_ips));
    EXPECT_EQ(candidate_ips.size(), 2);
    EXPECT_TRUE(candidate_ips.find("192.168.1.100") != candidate_ips.end());
    EXPECT_TRUE(candidate_ips.find("192.168.1.101") != candidate_ips.end());

    // Test 9: Duplicate IPs should not be added twice (set behavior)
    candidate_ips.clear();
    HELPER_EXPECT_SUCCESS(api_server_as_candidates(mock_config.get(), "192.168.1.100", candidate_ips));
    HELPER_EXPECT_SUCCESS(api_server_as_candidates(mock_config.get(), "192.168.1.100", candidate_ips));
    EXPECT_EQ(candidate_ips.size(), 1);
    EXPECT_TRUE(candidate_ips.find("192.168.1.100") != candidate_ips.end());
}

// Mock SrsProtocolUtility implementation
MockProtocolUtility::MockProtocolUtility()
{
}

MockProtocolUtility::~MockProtocolUtility()
{
    clear_ips();
}

vector<SrsIPAddress *> &MockProtocolUtility::local_ips()
{
    return mock_ips_;
}

void MockProtocolUtility::add_ip(string ip, string ifname, bool is_ipv4, bool is_loopback, bool is_internet)
{
    SrsIPAddress *addr = new SrsIPAddress();
    addr->ip_ = ip;
    addr->ifname_ = ifname;
    addr->is_ipv4_ = is_ipv4;
    addr->is_loopback_ = is_loopback;
    addr->is_internet_ = is_internet;
    mock_ips_.push_back(addr);
}

void MockProtocolUtility::clear_ips()
{
    for (size_t i = 0; i < mock_ips_.size(); i++) {
        srs_freep(mock_ips_[i]);
    }
    mock_ips_.clear();
}

// Mock ISrsAppConfig for discover_candidates implementation
MockAppConfigForDiscoverCandidates::MockAppConfigForDiscoverCandidates()
{
    rtc_server_candidates_ = "*";
    use_auto_detect_network_ip_ = true;
    rtc_server_ip_family_ = "ipv4";
    api_as_candidates_ = false;
    resolve_api_domain_ = false;
    keep_api_domain_ = false;
}

MockAppConfigForDiscoverCandidates::~MockAppConfigForDiscoverCandidates()
{
}

string MockAppConfigForDiscoverCandidates::get_rtc_server_candidates()
{
    return rtc_server_candidates_;
}

bool MockAppConfigForDiscoverCandidates::get_use_auto_detect_network_ip()
{
    return use_auto_detect_network_ip_;
}

string MockAppConfigForDiscoverCandidates::get_rtc_server_ip_family()
{
    return rtc_server_ip_family_;
}

VOID TEST(RtcServerTest, DiscoverCandidates_AutoDetectIPv4)
{
    // Create mock utility with multiple network interfaces
    SrsUniquePtr<MockProtocolUtility> mock_utility(new MockProtocolUtility());
    mock_utility->add_ip("127.0.0.1", "lo", true, true, false);      // loopback
    mock_utility->add_ip("192.168.1.100", "eth0", true, false, false); // private IPv4
    mock_utility->add_ip("10.0.0.50", "eth1", true, false, false);     // private IPv4
    mock_utility->add_ip("fe80::1", "eth0", false, false, false);      // IPv6

    // Create mock config with auto-detect enabled
    SrsUniquePtr<MockAppConfigForDiscoverCandidates> mock_config(new MockAppConfigForDiscoverCandidates());
    mock_config->rtc_server_candidates_ = "*";
    mock_config->use_auto_detect_network_ip_ = true;
    mock_config->rtc_server_ip_family_ = "ipv4";

    // Create RTC user config
    SrsUniquePtr<SrsRtcUserConfig> ruc(new SrsRtcUserConfig());
    ruc->req_->host_ = "example.com";

    // Test: Auto-detect should return non-loopback IPv4 addresses
    set<string> candidates = discover_candidates(mock_utility.get(), mock_config.get(), ruc.get());

    EXPECT_EQ(candidates.size(), 2);
    EXPECT_TRUE(candidates.find("192.168.1.100") != candidates.end());
    EXPECT_TRUE(candidates.find("10.0.0.50") != candidates.end());
    EXPECT_TRUE(candidates.find("127.0.0.1") == candidates.end()); // loopback excluded
    EXPECT_TRUE(candidates.find("fe80::1") == candidates.end());   // IPv6 excluded when family=ipv4
}

VOID TEST(RtcServerTest, DiscoverCandidates_ExplicitCandidate)
{
    // Create mock utility with network interfaces
    SrsUniquePtr<MockProtocolUtility> mock_utility(new MockProtocolUtility());
    mock_utility->add_ip("192.168.1.100", "eth0", true, false, false);

    // Create mock config with explicit candidate
    SrsUniquePtr<MockAppConfigForDiscoverCandidates> mock_config(new MockAppConfigForDiscoverCandidates());
    mock_config->rtc_server_candidates_ = "203.0.113.10"; // explicit public IP
    mock_config->use_auto_detect_network_ip_ = true;

    // Create RTC user config
    SrsUniquePtr<SrsRtcUserConfig> ruc(new SrsRtcUserConfig());
    ruc->req_->host_ = "example.com";

    // Test: Explicit candidate should override auto-detection
    set<string> candidates = discover_candidates(mock_utility.get(), mock_config.get(), ruc.get());

    EXPECT_EQ(candidates.size(), 1);
    EXPECT_TRUE(candidates.find("203.0.113.10") != candidates.end());
    EXPECT_TRUE(candidates.find("192.168.1.100") == candidates.end()); // auto-detected IP not included
}

VOID TEST(RtcServerTest, DiscoverCandidates_EipOverride)
{
    // Create mock utility
    SrsUniquePtr<MockProtocolUtility> mock_utility(new MockProtocolUtility());
    mock_utility->add_ip("192.168.1.100", "eth0", true, false, false);

    // Create mock config
    SrsUniquePtr<MockAppConfigForDiscoverCandidates> mock_config(new MockAppConfigForDiscoverCandidates());
    mock_config->rtc_server_candidates_ = "*";

    // Create RTC user config with eip specified
    SrsUniquePtr<SrsRtcUserConfig> ruc(new SrsRtcUserConfig());
    ruc->req_->host_ = "example.com";
    ruc->eip_ = "198.51.100.20"; // user-specified external IP

    // Test: User-specified eip should be included in candidates
    set<string> candidates = discover_candidates(mock_utility.get(), mock_config.get(), ruc.get());

    EXPECT_TRUE(candidates.find("198.51.100.20") != candidates.end());
    EXPECT_TRUE(candidates.find("192.168.1.100") != candidates.end());
}

// Mock ISrsStreamPublishTokenManager implementation
MockStreamPublishTokenManager::MockStreamPublishTokenManager()
{
    acquire_token_error_ = srs_success;
    acquire_token_count_ = 0;
    release_token_count_ = 0;
    token_to_return_ = NULL;
}

MockStreamPublishTokenManager::~MockStreamPublishTokenManager()
{
    srs_freep(acquire_token_error_);
    // Note: Don't free token_to_return_ because it's managed by SrsSharedPtr in the caller
}

srs_error_t MockStreamPublishTokenManager::acquire_token(ISrsRequest *req, SrsStreamPublishToken *&token)
{
    acquire_token_count_++;
    if (acquire_token_error_ != srs_success) {
        token = NULL;
        return srs_error_copy(acquire_token_error_);
    }

    // Create a new token if not already created
    if (!token_to_return_) {
        token_to_return_ = new SrsStreamPublishToken(req->get_stream_url(), NULL);
    }
    token = token_to_return_;
    return srs_success;
}

void MockStreamPublishTokenManager::release_token(const std::string &stream_url)
{
    release_token_count_++;
}

void MockStreamPublishTokenManager::set_acquire_token_error(srs_error_t err)
{
    srs_freep(acquire_token_error_);
    acquire_token_error_ = srs_error_copy(err);
}

void MockStreamPublishTokenManager::reset()
{
    srs_freep(acquire_token_error_);
    // Note: Don't free token_to_return_ here because it may have been freed by SrsSharedPtr
    // Just set it to NULL
    acquire_token_error_ = srs_success;
    acquire_token_count_ = 0;
    release_token_count_ = 0;
    token_to_return_ = NULL;
}

// Mock ISrsRtcConnection implementation
MockRtcConnectionForSessionManager::MockRtcConnectionForSessionManager()
{
    add_publisher_called_ = false;
    add_player_called_ = false;
    set_all_tracks_status_called_ = false;
    set_publish_token_called_ = false;
    add_publisher_error_ = srs_success;
    add_player_error_ = srs_success;
    username_ = "test-username";
    token_ = "test-token";
}

MockRtcConnectionForSessionManager::~MockRtcConnectionForSessionManager()
{
    srs_freep(add_publisher_error_);
    srs_freep(add_player_error_);
}

srs_error_t MockRtcConnectionForSessionManager::add_publisher(SrsRtcUserConfig *ruc, SrsSdp &local_sdp)
{
    add_publisher_called_ = true;
    return srs_error_copy(add_publisher_error_);
}

srs_error_t MockRtcConnectionForSessionManager::add_player(SrsRtcUserConfig *ruc, SrsSdp &local_sdp)
{
    add_player_called_ = true;
    return srs_error_copy(add_player_error_);
}

void MockRtcConnectionForSessionManager::set_all_tracks_status(std::string stream_uri, bool is_publish, bool status)
{
    set_all_tracks_status_called_ = true;
}

void MockRtcConnectionForSessionManager::set_publish_token(SrsSharedPtr<SrsStreamPublishToken> publish_token)
{
    set_publish_token_called_ = true;
    publish_token_ = publish_token;
}

void MockRtcConnectionForSessionManager::reset()
{
    add_publisher_called_ = false;
    add_player_called_ = false;
    set_all_tracks_status_called_ = false;
    set_publish_token_called_ = false;
    srs_freep(add_publisher_error_);
    srs_freep(add_player_error_);
    add_publisher_error_ = srs_success;
    add_player_error_ = srs_success;
}

// Mock ISrsAppFactory implementation
MockAppFactoryForSessionManager::MockAppFactoryForSessionManager()
{
    mock_connection_ = new MockRtcConnectionForSessionManager();
    create_rtc_connection_count_ = 0;
}

MockAppFactoryForSessionManager::~MockAppFactoryForSessionManager()
{
    srs_freep(mock_connection_);
}

ISrsRtcConnection *MockAppFactoryForSessionManager::create_rtc_connection(ISrsExecRtcAsyncTask *exec, const SrsContextId &cid)
{
    create_rtc_connection_count_++;
    // Create a real SrsRtcConnection object for testing
    // The test will inject mock_connection_ methods into it
    SrsRtcConnection *session = new SrsRtcConnection(exec, cid);
    return session;
}

void MockAppFactoryForSessionManager::reset()
{
    create_rtc_connection_count_ = 0;
}

// Unit test for SrsRtcSessionManager::create_rtc_session
// This test verifies the major use scenario: token acquisition and error handling
// Note: Full session creation requires complex initialization, so we test the key logic paths
VOID TEST(RtcSessionManagerTest, CreateRtcSession_TokenAcquisitionAndErrorHandling)
{
    srs_error_t err;

    // Create mock dependencies
    MockResourceManagerForBindSession mock_conn_manager;
    MockStreamPublishTokenManager mock_token_manager;
    MockRtcSourceManager mock_rtc_sources;

    // Create SrsRtcSessionManager
    SrsUniquePtr<SrsRtcSessionManager> session_manager(new SrsRtcSessionManager());

    // Inject mock dependencies
    session_manager->conn_manager_ = &mock_conn_manager;
    session_manager->stream_publish_tokens_ = &mock_token_manager;
    session_manager->rtc_sources_ = &mock_rtc_sources;

    // Test 1: Verify error handling when token acquisition fails
    {
        // Set token acquisition to fail
        mock_token_manager.set_acquire_token_error(srs_error_new(ERROR_SYSTEM_STREAM_BUSY, "stream busy"));

        // Create RTC user config for publishing
        SrsUniquePtr<SrsRtcUserConfig> ruc(new SrsRtcUserConfig());
        ruc->publish_ = true;
        ruc->req_ = new MockRtcAsyncCallRequest("test.vhost", "live", "stream1");

        // Create local SDP
        SrsSdp local_sdp;

        // Test: Create RTC session should fail due to token acquisition error
        ISrsRtcConnection *session = NULL;
        HELPER_EXPECT_FAILED(session_manager->create_rtc_session(ruc.get(), local_sdp, &session));

        // Verify: Token acquisition was attempted
        EXPECT_EQ(mock_token_manager.acquire_token_count_, 1);

        // Verify: RTC source was NOT fetched/created (because token acquisition failed first)
        EXPECT_EQ(mock_rtc_sources.fetch_or_create_count_, 0);

        // Clean up
        srs_freep(session);
        srs_freep(ruc->req_);
    }

    // Test 2: Verify error handling when source fetch/create fails
    {
        mock_token_manager.reset();
        mock_rtc_sources.reset();

        // Set source fetch/create to fail
        mock_rtc_sources.set_fetch_or_create_error(srs_error_new(ERROR_SYSTEM_CREATE_PIPE, "create source failed"));

        // Create RTC user config for publishing
        SrsUniquePtr<SrsRtcUserConfig> ruc2(new SrsRtcUserConfig());
        ruc2->publish_ = true;
        ruc2->req_ = new MockRtcAsyncCallRequest("test.vhost", "live", "stream2");

        // Create local SDP
        SrsSdp local_sdp2;

        // Test: Create RTC session should fail due to source creation error
        ISrsRtcConnection *session2 = NULL;
        HELPER_EXPECT_FAILED(session_manager->create_rtc_session(ruc2.get(), local_sdp2, &session2));

        // Verify: Token acquisition was attempted
        EXPECT_EQ(mock_token_manager.acquire_token_count_, 1);

        // Verify: RTC source fetch/create was attempted
        EXPECT_EQ(mock_rtc_sources.fetch_or_create_count_, 1);

        // Clean up
        srs_freep(session2);
        srs_freep(ruc2->req_);
    }

    // Test 3: Verify error handling when source cannot publish (stream busy)
    {
        mock_token_manager.reset();
        mock_rtc_sources.reset();

        // Set source to not allow publishing (simulate stream busy)
        // can_publish() returns !is_created_, so set is_created_ to true
        mock_rtc_sources.mock_source_->is_created_ = true;

        // Create RTC user config for publishing
        SrsUniquePtr<SrsRtcUserConfig> ruc3(new SrsRtcUserConfig());
        ruc3->publish_ = true;
        ruc3->req_ = new MockRtcAsyncCallRequest("test.vhost", "live", "stream3");

        // Create local SDP
        SrsSdp local_sdp3;

        // Test: Create RTC session should fail because source is busy
        ISrsRtcConnection *session3 = NULL;
        HELPER_EXPECT_FAILED(session_manager->create_rtc_session(ruc3.get(), local_sdp3, &session3));

        // Verify: Token acquisition was attempted
        EXPECT_EQ(mock_token_manager.acquire_token_count_, 1);

        // Verify: RTC source fetch/create was attempted
        EXPECT_EQ(mock_rtc_sources.fetch_or_create_count_, 1);

        // Clean up
        srs_freep(session3);
        srs_freep(ruc3->req_);
    }

    // Clean up - set to NULL to avoid double-free
    session_manager->conn_manager_ = NULL;
    session_manager->stream_publish_tokens_ = NULL;
    session_manager->rtc_sources_ = NULL;
}

// Mock ISrsRtcConnection implementation for srs_update_rtc_sessions test
MockRtcConnectionForUpdateSessions::MockRtcConnectionForUpdateSessions()
{
    is_alive_ = true;
    is_disposing_ = false;
    username_ = "test-user";
    switch_to_context_called_ = false;
    alive_called_ = false;
    udp_network_ = NULL;
}

MockRtcConnectionForUpdateSessions::~MockRtcConnectionForUpdateSessions()
{
    udp_network_ = NULL;
}

const SrsContextId &MockRtcConnectionForUpdateSessions::get_id()
{
    return cid_;
}

std::string MockRtcConnectionForUpdateSessions::desc()
{
    return "MockRtcConnection";
}

void MockRtcConnectionForUpdateSessions::on_disposing(ISrsResource *c)
{
}

void MockRtcConnectionForUpdateSessions::on_before_dispose(ISrsResource *c)
{
}

void MockRtcConnectionForUpdateSessions::expire()
{
}

srs_error_t MockRtcConnectionForUpdateSessions::send_rtcp(char *data, int nb_data)
{
    return srs_success;
}

srs_error_t MockRtcConnectionForUpdateSessions::send_rtcp_rr(uint32_t ssrc, SrsRtpRingBuffer *rtp_queue, const uint64_t &last_send_systime, const SrsNtp &last_send_ntp)
{
    return srs_success;
}

srs_error_t MockRtcConnectionForUpdateSessions::send_rtcp_xr_rrtr(uint32_t ssrc)
{
    return srs_success;
}

void MockRtcConnectionForUpdateSessions::check_send_nacks(SrsRtpNackForReceiver *nack, uint32_t ssrc, uint32_t &sent_nacks, uint32_t &timeout_nacks)
{
}

srs_error_t MockRtcConnectionForUpdateSessions::send_rtcp_fb_pli(uint32_t ssrc, const SrsContextId &cid_of_subscriber)
{
    return srs_success;
}

srs_error_t MockRtcConnectionForUpdateSessions::do_send_packet(SrsRtpPacket *pkt)
{
    return srs_success;
}

srs_error_t MockRtcConnectionForUpdateSessions::do_check_send_nacks()
{
    return srs_success;
}

void MockRtcConnectionForUpdateSessions::on_timer_nack()
{
}

srs_error_t MockRtcConnectionForUpdateSessions::on_dtls_handshake_done()
{
    return srs_success;
}

srs_error_t MockRtcConnectionForUpdateSessions::on_dtls_alert(std::string type, std::string desc)
{
    return srs_success;
}

srs_error_t MockRtcConnectionForUpdateSessions::on_rtp_cipher(char *data, int nb_data)
{
    return srs_success;
}

srs_error_t MockRtcConnectionForUpdateSessions::on_rtp_plaintext(char *data, int nb_data)
{
    return srs_success;
}

srs_error_t MockRtcConnectionForUpdateSessions::on_rtcp(char *data, int nb_data)
{
    return srs_success;
}

srs_error_t MockRtcConnectionForUpdateSessions::on_binding_request(SrsStunPacket *r, std::string &ice_pwd)
{
    return srs_success;
}

ISrsRtcNetwork *MockRtcConnectionForUpdateSessions::udp()
{
    return udp_network_;
}

ISrsRtcNetwork *MockRtcConnectionForUpdateSessions::tcp()
{
    return NULL;
}

void MockRtcConnectionForUpdateSessions::alive()
{
    alive_called_ = true;
}

bool MockRtcConnectionForUpdateSessions::is_alive()
{
    return is_alive_;
}

bool MockRtcConnectionForUpdateSessions::is_disposing()
{
    return is_disposing_;
}

void MockRtcConnectionForUpdateSessions::switch_to_context()
{
    switch_to_context_called_ = true;
}

srs_error_t MockRtcConnectionForUpdateSessions::add_publisher(SrsRtcUserConfig *ruc, SrsSdp &local_sdp)
{
    return srs_success;
}

srs_error_t MockRtcConnectionForUpdateSessions::add_player(SrsRtcUserConfig *ruc, SrsSdp &local_sdp)
{
    return srs_success;
}

void MockRtcConnectionForUpdateSessions::set_all_tracks_status(std::string stream_uri, bool is_publish, bool status)
{
}

void MockRtcConnectionForUpdateSessions::set_remote_sdp(const SrsSdp &sdp)
{
}

void MockRtcConnectionForUpdateSessions::set_local_sdp(const SrsSdp &sdp)
{
}

void MockRtcConnectionForUpdateSessions::set_state_as_waiting_stun()
{
}

srs_error_t MockRtcConnectionForUpdateSessions::initialize(ISrsRequest *r, bool dtls, bool srtp, std::string username)
{
    return srs_success;
}

std::string MockRtcConnectionForUpdateSessions::username()
{
    return username_;
}

std::string MockRtcConnectionForUpdateSessions::token()
{
    return "test-token";
}

void MockRtcConnectionForUpdateSessions::set_publish_token(SrsSharedPtr<SrsStreamPublishToken> publish_token)
{
}

void MockRtcConnectionForUpdateSessions::simulate_drop_packet(bool v, int nn)
{
}

void MockRtcConnectionForUpdateSessions::simulate_nack_drop(int nn)
{
}

// Mock ISrsResourceManager implementation for srs_update_rtc_sessions test
MockResourceManagerForUpdateSessions::MockResourceManagerForUpdateSessions()
{
}

MockResourceManagerForUpdateSessions::~MockResourceManagerForUpdateSessions()
{
    reset();
}

srs_error_t MockResourceManagerForUpdateSessions::start()
{
    return srs_success;
}

bool MockResourceManagerForUpdateSessions::empty()
{
    return resources_.empty();
}

size_t MockResourceManagerForUpdateSessions::size()
{
    return resources_.size();
}

void MockResourceManagerForUpdateSessions::add(ISrsResource *conn, bool *exists)
{
    resources_.push_back(conn);
}

void MockResourceManagerForUpdateSessions::add_with_id(const std::string &id, ISrsResource *conn)
{
    resources_.push_back(conn);
    id_map_[id] = conn;
}

void MockResourceManagerForUpdateSessions::add_with_fast_id(uint64_t id, ISrsResource *conn)
{
    resources_.push_back(conn);
    fast_id_map_[id] = conn;
}

void MockResourceManagerForUpdateSessions::add_with_name(const std::string &name, ISrsResource *conn)
{
    resources_.push_back(conn);
    name_map_[name] = conn;
}

ISrsResource *MockResourceManagerForUpdateSessions::at(int index)
{
    if (index < 0 || index >= (int)resources_.size()) {
        return NULL;
    }
    return resources_[index];
}

ISrsResource *MockResourceManagerForUpdateSessions::find_by_id(std::string id)
{
    std::map<std::string, ISrsResource *>::iterator it = id_map_.find(id);
    if (it != id_map_.end()) {
        return it->second;
    }
    return NULL;
}

ISrsResource *MockResourceManagerForUpdateSessions::find_by_fast_id(uint64_t id)
{
    std::map<uint64_t, ISrsResource *>::iterator it = fast_id_map_.find(id);
    if (it != fast_id_map_.end()) {
        return it->second;
    }
    return NULL;
}

ISrsResource *MockResourceManagerForUpdateSessions::find_by_name(std::string name)
{
    std::map<std::string, ISrsResource *>::iterator it = name_map_.find(name);
    if (it != name_map_.end()) {
        return it->second;
    }
    return NULL;
}

void MockResourceManagerForUpdateSessions::remove(ISrsResource *c)
{
    removed_resources_.push_back(c);
    // Remove from resources_ vector
    for (std::vector<ISrsResource *>::iterator it = resources_.begin(); it != resources_.end(); ++it) {
        if (*it == c) {
            resources_.erase(it);
            break;
        }
    }
}

void MockResourceManagerForUpdateSessions::subscribe(ISrsDisposingHandler *h)
{
}

void MockResourceManagerForUpdateSessions::unsubscribe(ISrsDisposingHandler *h)
{
}

void MockResourceManagerForUpdateSessions::reset()
{
    resources_.clear();
    removed_resources_.clear();
    id_map_.clear();
    fast_id_map_.clear();
    name_map_.clear();
}

// Unit test for SrsRtcSessionManager::srs_update_rtc_sessions
// This test verifies the major use scenario: checking sessions and disposing dead sessions
VOID TEST(RtcSessionManagerTest, UpdateRtcSessions_CheckAndDisposeDeadSessions)
{
    // Create mock connection manager
    SrsUniquePtr<MockResourceManagerForUpdateSessions> mock_conn_manager(new MockResourceManagerForUpdateSessions());

    // Create SrsRtcSessionManager
    SrsUniquePtr<SrsRtcSessionManager> session_manager(new SrsRtcSessionManager());

    // Inject mock connection manager
    session_manager->conn_manager_ = mock_conn_manager.get();

    // Test scenario: Multiple sessions with different states
    // - Session 1: Alive session (should be counted, not removed)
    // - Session 2: Dead session (not alive, should be removed)
    // - Session 3: Disposing session (should be ignored)
    // - Session 4: Alive session (should be counted, not removed)

    // Create mock sessions
    MockRtcConnectionForUpdateSessions *session1 = new MockRtcConnectionForUpdateSessions();
    session1->is_alive_ = true;
    session1->is_disposing_ = false;
    session1->username_ = "user1";

    MockRtcConnectionForUpdateSessions *session2 = new MockRtcConnectionForUpdateSessions();
    session2->is_alive_ = false; // Dead session
    session2->is_disposing_ = false;
    session2->username_ = "user2";

    MockRtcConnectionForUpdateSessions *session3 = new MockRtcConnectionForUpdateSessions();
    session3->is_alive_ = false;
    session3->is_disposing_ = true; // Already disposing
    session3->username_ = "user3";

    MockRtcConnectionForUpdateSessions *session4 = new MockRtcConnectionForUpdateSessions();
    session4->is_alive_ = true;
    session4->is_disposing_ = false;
    session4->username_ = "user4";

    // Add sessions to mock connection manager
    mock_conn_manager->add(session1);
    mock_conn_manager->add(session2);
    mock_conn_manager->add(session3);
    mock_conn_manager->add(session4);

    // Verify initial state
    EXPECT_EQ(mock_conn_manager->size(), 4);
    EXPECT_EQ(mock_conn_manager->removed_resources_.size(), 0);

    // Call srs_update_rtc_sessions
    session_manager->srs_update_rtc_sessions();

    // Verify results:
    // 1. Dead session (session2) should be removed
    EXPECT_EQ(mock_conn_manager->removed_resources_.size(), 1);
    EXPECT_EQ(mock_conn_manager->removed_resources_[0], session2);

    // 2. switch_to_context should be called for dead session
    EXPECT_TRUE(session2->switch_to_context_called_);

    // 3. Alive sessions should NOT be removed
    EXPECT_FALSE(session1->switch_to_context_called_);
    EXPECT_FALSE(session4->switch_to_context_called_);

    // 4. Disposing session should be ignored (not removed again)
    EXPECT_FALSE(session3->switch_to_context_called_);

    // 5. Connection manager should have 3 sessions left (session1, session3, session4)
    EXPECT_EQ(mock_conn_manager->size(), 3);

    // Clean up - set to NULL to avoid double-free
    session_manager->conn_manager_ = NULL;

    // Free mock sessions
    srs_freep(session1);
    srs_freep(session2);
    srs_freep(session3);
    srs_freep(session4);
}

// Mock ISrsRtcNetwork implementation for on_udp_packet tests
MockRtcNetworkForUdpNetwork::MockRtcNetworkForUdpNetwork()
{
    on_stun_called_ = false;
    on_rtp_called_ = false;
    on_rtcp_called_ = false;
    on_dtls_called_ = false;
}

MockRtcNetworkForUdpNetwork::~MockRtcNetworkForUdpNetwork()
{
}

srs_error_t MockRtcNetworkForUdpNetwork::initialize(SrsSessionConfig *cfg, bool dtls, bool srtp)
{
    return srs_success;
}

void MockRtcNetworkForUdpNetwork::set_state(SrsRtcNetworkState state)
{
}

srs_error_t MockRtcNetworkForUdpNetwork::on_dtls_handshake_done()
{
    return srs_success;
}

srs_error_t MockRtcNetworkForUdpNetwork::on_dtls_alert(std::string type, std::string desc)
{
    return srs_success;
}

srs_error_t MockRtcNetworkForUdpNetwork::on_dtls(char *data, int nb_data)
{
    on_dtls_called_ = true;
    return srs_success;
}

srs_error_t MockRtcNetworkForUdpNetwork::on_stun(SrsStunPacket *r, char *data, int nb_data)
{
    on_stun_called_ = true;
    return srs_success;
}

srs_error_t MockRtcNetworkForUdpNetwork::on_rtp(char *data, int nb_data)
{
    on_rtp_called_ = true;
    return srs_success;
}

srs_error_t MockRtcNetworkForUdpNetwork::on_rtcp(char *data, int nb_data)
{
    on_rtcp_called_ = true;
    return srs_success;
}

srs_error_t MockRtcNetworkForUdpNetwork::protect_rtp(void *packet, int *nb_cipher)
{
    return srs_success;
}

srs_error_t MockRtcNetworkForUdpNetwork::protect_rtcp(void *packet, int *nb_cipher)
{
    return srs_success;
}

bool MockRtcNetworkForUdpNetwork::is_establelished()
{
    return true;
}

srs_error_t MockRtcNetworkForUdpNetwork::write(void *buf, size_t size, ssize_t *nwrite)
{
    return srs_success;
}

// Test SrsRtcSessionManager::on_udp_packet with no session found (error case)
VOID TEST(RtcSessionManagerTest, OnUdpPacket_NoSessionFound)
{
    srs_error_t err = srs_success;

    // Create session manager
    SrsUniquePtr<SrsRtcSessionManager> session_manager(new SrsRtcSessionManager());

    // Create mock connection manager (empty - no sessions)
    SrsUniquePtr<MockResourceManagerForUpdateSessions> mock_conn_manager(new MockResourceManagerForUpdateSessions());

    // Inject mock connection manager
    session_manager->conn_manager_ = mock_conn_manager.get();

    // Create RTP packet (V=2, PT=96 for RTP)
    char rtp_packet[20];
    memset(rtp_packet, 0, sizeof(rtp_packet));
    rtp_packet[0] = (char)0x80; // V=2 (10000000)
    rtp_packet[1] = 96;   // PT=96 (RTP payload type)

    // Create mock UDP socket
    SrsUniquePtr<MockUdpMuxSocket> mock_socket(new MockUdpMuxSocket());
    mock_socket->data_ = rtp_packet;
    mock_socket->size_ = 20;
    mock_socket->fast_id_ = 12345; // Non-existent session
    mock_socket->peer_id_ = "192.168.1.100:8000";

    // Call on_udp_packet
    err = session_manager->on_udp_packet(mock_socket.get());

    // Verify: Should fail when no session is found
    HELPER_EXPECT_FAILED(err);

    // Clean up
    session_manager->conn_manager_ = NULL;
}

// Note: The remaining tests for RTP, RTCP, DTLS, and STUN packets require more complex setup
// including proper mock UDP networks and session initialization. The above test covers the
// basic error path when no session is found, which is a key scenario in on_udp_packet.
