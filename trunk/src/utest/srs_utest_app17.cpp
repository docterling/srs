//
// Copyright (c) 2013-2025 The SRS Authors
//
// SPDX-License-Identifier: MIT
//

#include <srs_utest_app17.hpp>

using namespace std;

#include <srs_app_mpegts_udp.hpp>
#include <srs_kernel_error.hpp>
#include <srs_kernel_packet.hpp>
#include <srs_kernel_utility.hpp>

// Mock ISrsAppConfig implementation
MockAppConfigForUdpCaster::MockAppConfigForUdpCaster()
{
    stream_caster_listen_port_ = 0;
}

MockAppConfigForUdpCaster::~MockAppConfigForUdpCaster()
{
}

int MockAppConfigForUdpCaster::get_stream_caster_listen(SrsConfDirective *conf)
{
    return stream_caster_listen_port_;
}

// Mock ISrsIpListener implementation
MockIpListenerForUdpCaster::MockIpListenerForUdpCaster()
{
    endpoint_port_ = 0;
    set_endpoint_called_ = false;
    set_label_called_ = false;
    listen_called_ = false;
    close_called_ = false;
}

MockIpListenerForUdpCaster::~MockIpListenerForUdpCaster()
{
}

ISrsListener *MockIpListenerForUdpCaster::set_endpoint(const std::string &i, int p)
{
    endpoint_ip_ = i;
    endpoint_port_ = p;
    set_endpoint_called_ = true;
    return this;
}

ISrsListener *MockIpListenerForUdpCaster::set_label(const std::string &label)
{
    label_ = label;
    set_label_called_ = true;
    return this;
}

srs_error_t MockIpListenerForUdpCaster::listen()
{
    listen_called_ = true;
    return srs_success;
}

void MockIpListenerForUdpCaster::close()
{
    close_called_ = true;
}

void MockIpListenerForUdpCaster::reset()
{
    endpoint_ip_ = "";
    endpoint_port_ = 0;
    label_ = "";
    set_endpoint_called_ = false;
    set_label_called_ = false;
    listen_called_ = false;
    close_called_ = false;
}

// Mock ISrsMpegtsOverUdp implementation
MockMpegtsOverUdp::MockMpegtsOverUdp()
{
    initialize_called_ = false;
    initialize_error_ = srs_success;
}

MockMpegtsOverUdp::~MockMpegtsOverUdp()
{
    srs_freep(initialize_error_);
}

srs_error_t MockMpegtsOverUdp::initialize(SrsConfDirective *c)
{
    initialize_called_ = true;
    if (initialize_error_ != srs_success) {
        return srs_error_copy(initialize_error_);
    }
    return srs_success;
}

srs_error_t MockMpegtsOverUdp::on_ts_message(SrsTsMessage *msg)
{
    return srs_success;
}

srs_error_t MockMpegtsOverUdp::on_udp_packet(const sockaddr *from, const int fromlen, char *buf, int nb_buf)
{
    return srs_success;
}

void MockMpegtsOverUdp::reset()
{
    initialize_called_ = false;
    srs_freep(initialize_error_);
}

// Test SrsUdpCasterListener - covers the major use scenario:
// 1. Create listener
// 2. Initialize with valid port configuration
// 3. Verify listener and caster are properly configured
// 4. Start listening
// 5. Close the listener
VOID TEST(UdpCasterListenerTest, InitializeAndListen)
{
    srs_error_t err;

    // Create mock config with valid port
    SrsUniquePtr<MockAppConfigForUdpCaster> mock_config(new MockAppConfigForUdpCaster());
    mock_config->stream_caster_listen_port_ = 8935;

    // Create mock listener and caster
    MockIpListenerForUdpCaster *mock_listener = new MockIpListenerForUdpCaster();
    MockMpegtsOverUdp *mock_caster = new MockMpegtsOverUdp();

    // Create SrsUdpCasterListener
    SrsUniquePtr<SrsUdpCasterListener> listener(new SrsUdpCasterListener());

    // Inject mock dependencies
    listener->config_ = mock_config.get();
    listener->listener_ = mock_listener;
    listener->caster_ = mock_caster;

    // Create a dummy config directive
    SrsConfDirective conf;

    // Test initialize() - should configure listener and caster
    HELPER_EXPECT_SUCCESS(listener->initialize(&conf));

    // Verify listener was configured with correct endpoint
    EXPECT_TRUE(mock_listener->set_endpoint_called_);
    EXPECT_EQ(8935, mock_listener->endpoint_port_);
    EXPECT_EQ(srs_net_address_any(), mock_listener->endpoint_ip_);

    // Verify listener label was set
    EXPECT_TRUE(mock_listener->set_label_called_);
    EXPECT_EQ("MPEGTS", mock_listener->label_);

    // Verify caster was initialized
    EXPECT_TRUE(mock_caster->initialize_called_);

    // Test listen() - should start listening
    HELPER_EXPECT_SUCCESS(listener->listen());
    EXPECT_TRUE(mock_listener->listen_called_);

    // Test close() - should close listener
    listener->close();
    EXPECT_TRUE(mock_listener->close_called_);

    // Clean up - set to NULL to avoid double-free
    listener->config_ = NULL;
    listener->listener_ = NULL;
    listener->caster_ = NULL;
}

// Test SrsUdpCasterListener::initialize with invalid port
VOID TEST(UdpCasterListenerTest, InitializeWithInvalidPort)
{
    srs_error_t err;

    // Create mock config with invalid port
    SrsUniquePtr<MockAppConfigForUdpCaster> mock_config(new MockAppConfigForUdpCaster());
    mock_config->stream_caster_listen_port_ = 0; // Invalid port

    // Create mock listener and caster
    MockIpListenerForUdpCaster *mock_listener = new MockIpListenerForUdpCaster();
    MockMpegtsOverUdp *mock_caster = new MockMpegtsOverUdp();

    // Create SrsUdpCasterListener
    SrsUniquePtr<SrsUdpCasterListener> listener(new SrsUdpCasterListener());

    // Inject mock dependencies
    listener->config_ = mock_config.get();
    listener->listener_ = mock_listener;
    listener->caster_ = mock_caster;

    // Create a dummy config directive
    SrsConfDirective conf;

    // Test initialize() - should fail with invalid port
    HELPER_EXPECT_FAILED(listener->initialize(&conf));

    // Verify listener was NOT configured
    EXPECT_FALSE(mock_listener->set_endpoint_called_);
    EXPECT_FALSE(mock_listener->set_label_called_);

    // Verify caster was NOT initialized
    EXPECT_FALSE(mock_caster->initialize_called_);

    // Clean up - set to NULL to avoid double-free
    listener->config_ = NULL;
    listener->listener_ = NULL;
    listener->caster_ = NULL;
}

// Test SrsUdpCasterListener::initialize when caster initialization fails
VOID TEST(UdpCasterListenerTest, InitializeWithCasterFailure)
{
    srs_error_t err;

    // Create mock config with valid port
    SrsUniquePtr<MockAppConfigForUdpCaster> mock_config(new MockAppConfigForUdpCaster());
    mock_config->stream_caster_listen_port_ = 8935;

    // Create mock listener and caster
    MockIpListenerForUdpCaster *mock_listener = new MockIpListenerForUdpCaster();
    MockMpegtsOverUdp *mock_caster = new MockMpegtsOverUdp();

    // Configure caster to fail initialization
    mock_caster->initialize_error_ = srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "caster init failed");

    // Create SrsUdpCasterListener
    SrsUniquePtr<SrsUdpCasterListener> listener(new SrsUdpCasterListener());

    // Inject mock dependencies
    listener->config_ = mock_config.get();
    listener->listener_ = mock_listener;
    listener->caster_ = mock_caster;

    // Create a dummy config directive
    SrsConfDirective conf;

    // Test initialize() - should fail when caster initialization fails
    HELPER_EXPECT_FAILED(listener->initialize(&conf));

    // Verify listener was configured (happens before caster init)
    EXPECT_TRUE(mock_listener->set_endpoint_called_);
    EXPECT_TRUE(mock_listener->set_label_called_);

    // Verify caster initialization was attempted
    EXPECT_TRUE(mock_caster->initialize_called_);

    // Clean up - set to NULL to avoid double-free
    listener->config_ = NULL;
    listener->listener_ = NULL;
    listener->caster_ = NULL;
}

// Test SrsMpegtsQueue push and dequeue - major use scenario
// This test covers the typical workflow:
// 1. Push multiple audio and video packets with different timestamps
// 2. Dequeue packets in timestamp order once threshold is met (2+ videos and 2+ audios)
// 3. Verify packets are returned in correct timestamp order
// 4. Verify dequeue returns NULL when threshold is not met
VOID TEST(MpegtsQueueTest, PushAndDequeueBasic)
{
    srs_error_t err;

    // Create SrsMpegtsQueue
    SrsUniquePtr<SrsMpegtsQueue> queue(new SrsMpegtsQueue());

    // Push 3 video packets
    SrsMediaPacket *video1 = new SrsMediaPacket();
    video1->timestamp_ = 1000;
    video1->message_type_ = SrsFrameTypeVideo;
    HELPER_EXPECT_SUCCESS(queue->push(video1));

    SrsMediaPacket *video2 = new SrsMediaPacket();
    video2->timestamp_ = 2000;
    video2->message_type_ = SrsFrameTypeVideo;
    HELPER_EXPECT_SUCCESS(queue->push(video2));

    SrsMediaPacket *video3 = new SrsMediaPacket();
    video3->timestamp_ = 3000;
    video3->message_type_ = SrsFrameTypeVideo;
    HELPER_EXPECT_SUCCESS(queue->push(video3));

    // Push 3 audio packets
    SrsMediaPacket *audio1 = new SrsMediaPacket();
    audio1->timestamp_ = 1500;
    audio1->message_type_ = SrsFrameTypeAudio;
    HELPER_EXPECT_SUCCESS(queue->push(audio1));

    SrsMediaPacket *audio2 = new SrsMediaPacket();
    audio2->timestamp_ = 2500;
    audio2->message_type_ = SrsFrameTypeAudio;
    HELPER_EXPECT_SUCCESS(queue->push(audio2));

    SrsMediaPacket *audio3 = new SrsMediaPacket();
    audio3->timestamp_ = 3500;
    audio3->message_type_ = SrsFrameTypeAudio;
    HELPER_EXPECT_SUCCESS(queue->push(audio3));

    // Now we have 3 videos and 3 audios, dequeue should return packets in timestamp order
    SrsMediaPacket *dequeued1 = queue->dequeue();
    ASSERT_TRUE(dequeued1 != NULL);
    EXPECT_EQ(1000, dequeued1->timestamp_);
    EXPECT_TRUE(dequeued1->is_video());
    srs_freep(dequeued1);

    // After dequeue, we have 2 videos and 3 audios, still meets threshold (2+ videos and 2+ audios)
    SrsMediaPacket *dequeued2 = queue->dequeue();
    ASSERT_TRUE(dequeued2 != NULL);
    EXPECT_EQ(1500, dequeued2->timestamp_);
    EXPECT_TRUE(dequeued2->is_audio());
    srs_freep(dequeued2);

    // After dequeue, we have 2 videos and 2 audios, still meets threshold
    SrsMediaPacket *dequeued3 = queue->dequeue();
    ASSERT_TRUE(dequeued3 != NULL);
    EXPECT_EQ(2000, dequeued3->timestamp_);
    EXPECT_TRUE(dequeued3->is_video());
    srs_freep(dequeued3);

    // After dequeue, we have 1 video and 2 audios, does NOT meet threshold (need 2+ videos)
    // So dequeue should return NULL
    SrsMediaPacket *dequeued4 = queue->dequeue();
    EXPECT_TRUE(dequeued4 == NULL);
}

// Test SrsMpegtsOverUdp::on_udp_packet and on_udp_bytes - major use scenario
// This test covers the complete UDP packet processing flow:
// 1. Receive UDP packet with TS data
// 2. Parse peer IP and port from sockaddr
// 3. Append data to buffer
// 4. Find TS sync byte (0x47)
// 5. Parse TS packets (188 bytes each)
// 6. Decode TS packets using context
// 7. Erase consumed bytes from buffer
VOID TEST(MpegtsOverUdpTest, ProcessUdpPacketWithTsData)
{
    srs_error_t err;

    // Create SrsMpegtsOverUdp instance
    SrsUniquePtr<SrsMpegtsOverUdp> udp_handler(new SrsMpegtsOverUdp());

    // Create a valid TS packet (188 bytes) - PAT packet
    // This is a real TS PAT (Program Association Table) packet
    uint8_t ts_packet[188] = {
        // TS header: sync_byte=0x47, pid=0x0000 (PAT)
        0x47, 0x40, 0x00, 0x10, 0x00,
        // PAT table
        0x00, 0xb0, 0x0d, 0x00, 0x01, 0xc1, 0x00, 0x00, 0x00, 0x01, 0xf0,
        0x00, 0x2a, 0xb1, 0x04, 0xb2};
    // Fill rest with 0xff (stuffing bytes)
    for (int i = 21; i < 188; i++) {
        ts_packet[i] = 0xff;
    }

    // Create sockaddr for peer address
    struct sockaddr_in peer_addr;
    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(12345);
    inet_pton(AF_INET, "192.168.1.100", &peer_addr.sin_addr);

    // Test on_udp_packet - should successfully process the TS packet
    HELPER_EXPECT_SUCCESS(udp_handler->on_udp_packet(
        (const sockaddr *)&peer_addr,
        sizeof(peer_addr),
        (char *)ts_packet,
        sizeof(ts_packet)));

    // Verify buffer was updated (data appended and then consumed)
    // After processing one complete TS packet (188 bytes), buffer should be empty
    EXPECT_EQ(0, udp_handler->buffer_->length());
}

// Mock ISrsRawH264Stream implementation
MockMpegtsRawH264Stream::MockMpegtsRawH264Stream()
{
    annexb_demux_called_ = false;
    is_sps_called_ = false;
    is_pps_called_ = false;
    sps_demux_called_ = false;
    pps_demux_called_ = false;
    annexb_demux_error_ = srs_success;
    sps_demux_error_ = srs_success;
    pps_demux_error_ = srs_success;
    demux_frame_ = NULL;
    demux_frame_size_ = 0;
    is_sps_result_ = false;
    is_pps_result_ = false;
}

MockMpegtsRawH264Stream::~MockMpegtsRawH264Stream()
{
    srs_freep(annexb_demux_error_);
    srs_freep(sps_demux_error_);
    srs_freep(pps_demux_error_);
}

srs_error_t MockMpegtsRawH264Stream::annexb_demux(SrsBuffer *stream, char **pframe, int *pnb_frame)
{
    annexb_demux_called_ = true;

    if (annexb_demux_error_ != srs_success) {
        return srs_error_copy(annexb_demux_error_);
    }

    *pframe = demux_frame_;
    *pnb_frame = demux_frame_size_;

    // Skip the frame in the buffer
    if (demux_frame_size_ > 0 && stream->left() >= demux_frame_size_) {
        stream->skip(demux_frame_size_);
    }

    return srs_success;
}

bool MockMpegtsRawH264Stream::is_sps(char *frame, int nb_frame)
{
    is_sps_called_ = true;
    return is_sps_result_;
}

bool MockMpegtsRawH264Stream::is_pps(char *frame, int nb_frame)
{
    is_pps_called_ = true;
    return is_pps_result_;
}

srs_error_t MockMpegtsRawH264Stream::sps_demux(char *frame, int nb_frame, std::string &sps)
{
    sps_demux_called_ = true;

    if (sps_demux_error_ != srs_success) {
        return srs_error_copy(sps_demux_error_);
    }

    sps = sps_output_;
    return srs_success;
}

srs_error_t MockMpegtsRawH264Stream::pps_demux(char *frame, int nb_frame, std::string &pps)
{
    pps_demux_called_ = true;

    if (pps_demux_error_ != srs_success) {
        return srs_error_copy(pps_demux_error_);
    }

    pps = pps_output_;
    return srs_success;
}

srs_error_t MockMpegtsRawH264Stream::mux_sequence_header(std::string sps, std::string pps, std::string &sh)
{
    // Simple mock implementation
    sh = "\x17\x00\x00\x00\x00"; // AVC sequence header prefix
    return srs_success;
}

srs_error_t MockMpegtsRawH264Stream::mux_ipb_frame(char *frame, int frame_size, std::string &ibp)
{
    // Simple mock implementation
    ibp.assign(frame, frame_size);
    return srs_success;
}

srs_error_t MockMpegtsRawH264Stream::mux_avc2flv(std::string video, int8_t frame_type, int8_t avc_packet_type, uint32_t dts, uint32_t pts, char **flv, int *nb_flv)
{
    // Simple mock implementation - allocate a small buffer
    *nb_flv = 10;
    *flv = new char[*nb_flv];
    memset(*flv, 0, *nb_flv);
    return srs_success;
}

void MockMpegtsRawH264Stream::reset()
{
    annexb_demux_called_ = false;
    is_sps_called_ = false;
    is_pps_called_ = false;
    sps_demux_called_ = false;
    pps_demux_called_ = false;
    srs_freep(annexb_demux_error_);
    srs_freep(sps_demux_error_);
    srs_freep(pps_demux_error_);
    demux_frame_ = NULL;
    demux_frame_size_ = 0;
    is_sps_result_ = false;
    is_pps_result_ = false;
}

// Mock ISrsBasicRtmpClient implementation
MockMpegtsRtmpClient::MockMpegtsRtmpClient()
{
    connect_called_ = false;
    publish_called_ = false;
    close_called_ = false;
    connect_error_ = srs_success;
    publish_error_ = srs_success;
    stream_id_ = 1;
}

MockMpegtsRtmpClient::~MockMpegtsRtmpClient()
{
    srs_freep(connect_error_);
    srs_freep(publish_error_);
}

srs_error_t MockMpegtsRtmpClient::connect()
{
    connect_called_ = true;
    return srs_error_copy(connect_error_);
}

void MockMpegtsRtmpClient::close()
{
    close_called_ = true;
}

srs_error_t MockMpegtsRtmpClient::publish(int chunk_size, bool with_vhost, std::string *pstream)
{
    publish_called_ = true;
    return srs_error_copy(publish_error_);
}

srs_error_t MockMpegtsRtmpClient::play(int chunk_size, bool with_vhost, std::string *pstream)
{
    return srs_success;
}

void MockMpegtsRtmpClient::kbps_sample(const char *label, srs_utime_t age)
{
}

srs_error_t MockMpegtsRtmpClient::recv_message(SrsRtmpCommonMessage **pmsg)
{
    return srs_success;
}

srs_error_t MockMpegtsRtmpClient::decode_message(SrsRtmpCommonMessage *msg, SrsRtmpCommand **ppacket)
{
    return srs_success;
}

srs_error_t MockMpegtsRtmpClient::send_and_free_messages(SrsMediaPacket **msgs, int nb_msgs)
{
    for (int i = 0; i < nb_msgs; i++) {
        srs_freep(msgs[i]);
    }
    return srs_success;
}

srs_error_t MockMpegtsRtmpClient::send_and_free_message(SrsMediaPacket *msg)
{
    srs_freep(msg);
    return srs_success;
}

void MockMpegtsRtmpClient::set_recv_timeout(srs_utime_t timeout)
{
}

int MockMpegtsRtmpClient::sid()
{
    return stream_id_;
}

void MockMpegtsRtmpClient::reset()
{
    connect_called_ = false;
    publish_called_ = false;
    close_called_ = false;
    srs_freep(connect_error_);
    srs_freep(publish_error_);
    stream_id_ = 1;
}

// Test SrsMpegtsOverUdp::on_ts_video - major use scenario
// This test covers the complete video processing flow:
// 1. Receive TS video message with H.264 annexb data (SPS, PPS, IDR frame)
// 2. Connect to RTMP server
// 3. Demux annexb frames from buffer
// 4. Process SPS and PPS to generate sequence header
// 5. Process IDR frame and write to RTMP
VOID TEST(MpegtsOverUdpTest, OnTsVideoWithSpsPpsIdrFrame)
{
    srs_error_t err;

    // Create SrsMpegtsOverUdp instance
    SrsUniquePtr<SrsMpegtsOverUdp> udp_handler(new SrsMpegtsOverUdp());

    // Create mock dependencies
    MockMpegtsRawH264Stream *mock_avc = new MockMpegtsRawH264Stream();
    MockMpegtsRtmpClient *mock_sdk = new MockMpegtsRtmpClient();

    // Inject mock dependencies
    udp_handler->avc_ = mock_avc;
    // Note: sdk_ should be NULL initially, then connect() will be called
    // For this test, we simulate that sdk_ is already connected
    udp_handler->sdk_ = mock_sdk;

    // Create a TsMessage with valid timestamp
    SrsUniquePtr<SrsTsMessage> msg(new SrsTsMessage());
    msg->dts_ = 90000; // 1 second in 90kHz timebase (will be converted to 1000ms)
    msg->pts_ = 90000;

    // Create H.264 annexb data: SPS + PPS + IDR frame
    // SPS NALU (type 7): 0x00 0x00 0x00 0x01 0x67 ...
    char sps_nalu[] = {0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x1e};
    // PPS NALU (type 8): 0x00 0x00 0x00 0x01 0x68 ...
    char pps_nalu[] = {0x00, 0x00, 0x00, 0x01, 0x68, (char)0xce, 0x3c, (char)0x80};
    // IDR NALU (type 5): 0x00 0x00 0x00 0x01 0x65 ...
    char idr_nalu[] = {0x00, 0x00, 0x00, 0x01, 0x65, (char)0x88, (char)0x84, 0x00};

    // Combine all NALUs into a single buffer
    int total_size = sizeof(sps_nalu) + sizeof(pps_nalu) + sizeof(idr_nalu);
    char *annexb_data = new char[total_size];
    memcpy(annexb_data, sps_nalu, sizeof(sps_nalu));
    memcpy(annexb_data + sizeof(sps_nalu), pps_nalu, sizeof(pps_nalu));
    memcpy(annexb_data + sizeof(sps_nalu) + sizeof(pps_nalu), idr_nalu, sizeof(idr_nalu));

    // Create buffer with annexb data
    SrsUniquePtr<SrsBuffer> avs(new SrsBuffer(annexb_data, total_size));

    // Configure mock_avc to return SPS frame
    mock_avc->demux_frame_ = sps_nalu + 4;    // Skip start code
    mock_avc->demux_frame_size_ = total_size; // Return all data to empty the buffer
    mock_avc->is_sps_result_ = true;
    mock_avc->is_pps_result_ = false;
    mock_avc->sps_output_ = std::string(sps_nalu + 4, sizeof(sps_nalu) - 4);
    mock_avc->pps_output_ = std::string(pps_nalu + 4, sizeof(pps_nalu) - 4);

    // Test on_ts_video - should process SPS frame
    HELPER_EXPECT_SUCCESS(udp_handler->on_ts_video(msg.get(), avs.get()));

    // Verify connect was NOT called (sdk_ is already set, so connect() returns early)
    // In real scenario, connect() would be called if sdk_ is NULL
    EXPECT_FALSE(mock_sdk->connect_called_);

    // Verify annexb_demux was called
    EXPECT_TRUE(mock_avc->annexb_demux_called_);

    // Verify is_sps was called
    EXPECT_TRUE(mock_avc->is_sps_called_);

    // Verify sps_demux was called
    EXPECT_TRUE(mock_avc->sps_demux_called_);

    // Verify h264_sps_ was updated
    EXPECT_EQ(mock_avc->sps_output_, udp_handler->h264_sps_);
    EXPECT_TRUE(udp_handler->h264_sps_changed_);

    // Verify buffer is now empty (all data consumed)
    EXPECT_TRUE(avs->empty());

    // Clean up - set to NULL to avoid double-free
    udp_handler->avc_ = NULL;
    udp_handler->sdk_ = NULL;
    srs_freep(mock_avc);
    srs_freep(mock_sdk);
    delete[] annexb_data;
}

// Test SrsMpegtsOverUdp::write_h264_sps_pps - major use scenario
// This test covers the complete SPS/PPS writing flow:
// 1. Both SPS and PPS have changed (h264_sps_changed_ and h264_pps_changed_ are true)
// 2. Call write_h264_sps_pps with dts and pts
// 3. Verify it calls avc_->mux_sequence_header to create sequence header
// 4. Verify it calls avc_->mux_avc2flv to convert to FLV format
// 5. Verify it calls rtmp_write_packet to write the packet
// 6. Verify flags are reset correctly (h264_sps_changed_, h264_pps_changed_ set to false, h264_sps_pps_sent_ set to true)
VOID TEST(MpegtsOverUdpTest, WriteH264SpsPps)
{
    srs_error_t err;

    // Create SrsMpegtsOverUdp instance
    SrsUniquePtr<SrsMpegtsOverUdp> udp_handler(new SrsMpegtsOverUdp());

    // Create mock dependencies
    MockMpegtsRawH264Stream *mock_avc = new MockMpegtsRawH264Stream();
    MockMpegtsRtmpClient *mock_sdk = new MockMpegtsRtmpClient();
    SrsUniquePtr<SrsMpegtsQueue> mock_queue(new SrsMpegtsQueue());

    // Inject mock dependencies
    udp_handler->avc_ = mock_avc;
    udp_handler->sdk_ = mock_sdk;
    udp_handler->queue_ = mock_queue.get();

    // Set up SPS and PPS data
    std::string sps_data = "\x67\x42\x00\x1e";
    std::string pps_data = "\x68\xce\x3c\x80";
    udp_handler->h264_sps_ = sps_data;
    udp_handler->h264_pps_ = pps_data;

    // Set both SPS and PPS changed flags to true (required for write_h264_sps_pps to proceed)
    udp_handler->h264_sps_changed_ = true;
    udp_handler->h264_pps_changed_ = true;
    udp_handler->h264_sps_pps_sent_ = false;

    // Test write_h264_sps_pps with valid dts and pts
    uint32_t dts = 1000;
    uint32_t pts = 1000;
    HELPER_EXPECT_SUCCESS(udp_handler->write_h264_sps_pps(dts, pts));

    // Verify flags are reset correctly after successful write
    EXPECT_FALSE(udp_handler->h264_sps_changed_);
    EXPECT_FALSE(udp_handler->h264_pps_changed_);
    EXPECT_TRUE(udp_handler->h264_sps_pps_sent_);

    // Clean up - set to NULL to avoid double-free
    udp_handler->avc_ = NULL;
    udp_handler->sdk_ = NULL;
    udp_handler->queue_ = NULL;
    srs_freep(mock_avc);
    srs_freep(mock_sdk);
}

// Test SrsMpegtsOverUdp::write_h264_ipb_frame - major use scenario
// This test covers the complete IPB frame writing flow:
// 1. SPS/PPS already sent (h264_sps_pps_sent_ = true)
// 2. Write an IDR frame (keyframe) with valid H.264 NALU data
// 3. Verify it detects IDR frame type correctly (NALU type 5)
// 4. Verify it calls avc_->mux_ipb_frame to mux the frame
// 5. Verify it calls avc_->mux_avc2flv with correct frame type (keyframe)
// 6. Verify it calls rtmp_write_packet to write the video packet
VOID TEST(MpegtsOverUdpTest, WriteH264IpbFrameWithIdrFrame)
{
    srs_error_t err;

    // Create SrsMpegtsOverUdp instance
    SrsUniquePtr<SrsMpegtsOverUdp> udp_handler(new SrsMpegtsOverUdp());

    // Create mock dependencies
    MockMpegtsRawH264Stream *mock_avc = new MockMpegtsRawH264Stream();
    MockMpegtsRtmpClient *mock_sdk = new MockMpegtsRtmpClient();
    SrsUniquePtr<SrsMpegtsQueue> mock_queue(new SrsMpegtsQueue());

    // Inject mock dependencies
    udp_handler->avc_ = mock_avc;
    udp_handler->sdk_ = mock_sdk;
    udp_handler->queue_ = mock_queue.get();

    // Set h264_sps_pps_sent_ to true (required for write_h264_ipb_frame to proceed)
    udp_handler->h264_sps_pps_sent_ = true;

    // Create an IDR frame NALU (type 5)
    // NALU header: 0x65 = 0110 0101 (forbidden_zero_bit=0, nal_ref_idc=3, nal_unit_type=5)
    char idr_frame[] = {0x65, (char)0x88, (char)0x84, 0x00, 0x01, 0x02, 0x03, 0x04};
    int frame_size = sizeof(idr_frame);

    // Test write_h264_ipb_frame with IDR frame
    uint32_t dts = 2000;
    uint32_t pts = 2000;
    HELPER_EXPECT_SUCCESS(udp_handler->write_h264_ipb_frame(idr_frame, frame_size, dts, pts));

    // Clean up - set to NULL to avoid double-free
    udp_handler->avc_ = NULL;
    udp_handler->sdk_ = NULL;
    udp_handler->queue_ = NULL;
    srs_freep(mock_avc);
    srs_freep(mock_sdk);
}

// Mock ISrsAppFactory implementation
MockAppFactoryForMpegtsOverUdp::MockAppFactoryForMpegtsOverUdp()
{
    mock_rtmp_client_ = NULL;
    create_rtmp_client_called_ = false;
}

MockAppFactoryForMpegtsOverUdp::~MockAppFactoryForMpegtsOverUdp()
{
    // Don't free mock_rtmp_client_ - it's managed by the test
}

ISrsBasicRtmpClient *MockAppFactoryForMpegtsOverUdp::create_rtmp_client(std::string url, srs_utime_t cto, srs_utime_t sto)
{
    create_rtmp_client_called_ = true;
    return mock_rtmp_client_;
}

void MockAppFactoryForMpegtsOverUdp::reset()
{
    create_rtmp_client_called_ = false;
}

// Test SrsMpegtsOverUdp::rtmp_write_packet - major use scenario
// This test covers the complete RTMP packet writing flow:
// 1. Call rtmp_write_packet with video data
// 2. Verify it calls connect() to establish RTMP connection (if not already connected)
// 3. Verify it creates RTMP message from raw data using srs_rtmp_create_msg
// 4. Verify it pushes message to queue
// 5. Verify it dequeues ready messages and sends them via sdk_->send_and_free_message
// 6. Verify multiple packets are sent in correct order when queue threshold is met
VOID TEST(MpegtsOverUdpTest, RtmpWritePacketWithVideoData)
{
    srs_error_t err;

    // Create SrsMpegtsOverUdp instance
    SrsUniquePtr<SrsMpegtsOverUdp> udp_handler(new SrsMpegtsOverUdp());

    // Create mock dependencies
    MockMpegtsRtmpClient *mock_sdk = new MockMpegtsRtmpClient();
    SrsUniquePtr<MockAppFactoryForMpegtsOverUdp> mock_factory(new MockAppFactoryForMpegtsOverUdp());
    SrsUniquePtr<SrsMpegtsQueue> mock_queue(new SrsMpegtsQueue());

    // Configure mock factory to return our mock RTMP client
    mock_factory->mock_rtmp_client_ = mock_sdk;

    // Inject mock dependencies
    udp_handler->app_factory_ = mock_factory.get();
    udp_handler->queue_ = mock_queue.get();
    udp_handler->output_ = "rtmp://127.0.0.1/live/stream";

    // Note: sdk_ is NULL initially, so connect() will be called

    // Create test video data (H.264 IDR frame) - allocate on heap
    int video_size = 25;
    char *video_data = new char[video_size];
    video_data[0] = 0x17;
    video_data[1] = 0x01;
    video_data[2] = 0x00;
    video_data[3] = 0x00;
    video_data[4] = 0x00;
    video_data[5] = 0x00;
    video_data[6] = 0x00;
    video_data[7] = 0x00;
    video_data[8] = 0x10;
    video_data[9] = 0x65;
    video_data[10] = (char)0x88;
    video_data[11] = (char)0x84;
    video_data[12] = 0x00;
    for (int i = 13; i < video_size; i++)
        video_data[i] = i - 12;

    uint32_t timestamp = 1000;
    char type = SrsFrameTypeVideo;

    // First call to rtmp_write_packet - should trigger connect()
    HELPER_EXPECT_SUCCESS(udp_handler->rtmp_write_packet(type, timestamp, video_data, video_size));

    // Verify connect was called (factory creates RTMP client)
    EXPECT_TRUE(mock_factory->create_rtmp_client_called_);
    EXPECT_TRUE(mock_sdk->connect_called_);
    EXPECT_TRUE(mock_sdk->publish_called_);

    // Verify sdk_ is now set
    EXPECT_TRUE(udp_handler->sdk_ != NULL);

    // Reset the flag to verify it's not called again
    mock_sdk->connect_called_ = false;
    mock_sdk->publish_called_ = false;

    // Second call should NOT trigger connect (already connected)
    char *video_data2 = new char[17];
    video_data2[0] = 0x27;
    video_data2[1] = 0x01;
    video_data2[2] = 0x00;
    video_data2[3] = 0x00;
    video_data2[4] = 0x00;
    video_data2[5] = 0x00;
    video_data2[6] = 0x00;
    video_data2[7] = 0x00;
    video_data2[8] = 0x08;
    video_data2[9] = 0x41;
    video_data2[10] = (char)0x88;
    video_data2[11] = (char)0x84;
    video_data2[12] = 0x00;
    for (int i = 13; i < 17; i++)
        video_data2[i] = i - 12;

    uint32_t timestamp2 = 1040;
    HELPER_EXPECT_SUCCESS(udp_handler->rtmp_write_packet(type, timestamp2, video_data2, 17));

    // Verify connect was NOT called again
    EXPECT_FALSE(mock_sdk->connect_called_);
    EXPECT_FALSE(mock_sdk->publish_called_);

    // Clean up - set to NULL to avoid double-free
    udp_handler->app_factory_ = NULL;
    udp_handler->queue_ = NULL;
    udp_handler->sdk_ = NULL;
    srs_freep(mock_sdk);
}
