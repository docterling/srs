//
// Copyright (c) 2013-2025 The SRS Authors
//
// SPDX-License-Identifier: MIT
//

#include <srs_utest_app16.hpp>

using namespace std;

#include <srs_app_factory.hpp>
#include <srs_app_listener.hpp>
#include <srs_kernel_error.hpp>
#include <srs_kernel_st.hpp>
#include <srs_kernel_utility.hpp>
#include <srs_protocol_utility.hpp>
#include <sstream>

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
    srs_usleep(100 * SRS_UTIME_MILLISECONDS);

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
    srs_usleep(100 * SRS_UTIME_MILLISECONDS);

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
    srs_usleep(10 * SRS_UTIME_MILLISECONDS);

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
    srs_usleep(10 * SRS_UTIME_MILLISECONDS);

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
    srs_usleep(10 * SRS_UTIME_MILLISECONDS);

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
    srs_usleep(10 * SRS_UTIME_MILLISECONDS);

    // Receive the packet with server socket - this populates from_ and fromlen_
    int nread = server_socket->recvfrom(100 * SRS_UTIME_MILLISECONDS);
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
    srs_usleep(10 * SRS_UTIME_MILLISECONDS);

    // Receive the reply on the client side
    char recv_buf[1024];
    sockaddr_in from_addr;
    int from_len = sizeof(from_addr);
    nread = srs_recvfrom(client_fd, recv_buf, sizeof(recv_buf), (sockaddr *)&from_addr, &from_len, 100 * SRS_UTIME_MILLISECONDS);

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
    srs_usleep(10 * SRS_UTIME_MILLISECONDS);
    int received_count = 0;
    while (received_count < 25) {
        nread = srs_recvfrom(client_fd, recv_buf, sizeof(recv_buf), (sockaddr *)&from_addr, &from_len, 10 * SRS_UTIME_MILLISECONDS);
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
    srs_usleep(10 * SRS_UTIME_MILLISECONDS);

    // Receive the packet with server socket - this sets address_changed_ to true
    int nread = server_socket->recvfrom(100 * SRS_UTIME_MILLISECONDS);
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
    srs_usleep(10 * SRS_UTIME_MILLISECONDS);

    // Receive the second packet - this should set address_changed_ to true again
    nread = server_socket->recvfrom(100 * SRS_UTIME_MILLISECONDS);
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
    srs_usleep(10 * SRS_UTIME_MILLISECONDS);

    // Receive packet from second client
    nread = server_socket->recvfrom(100 * SRS_UTIME_MILLISECONDS);
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
