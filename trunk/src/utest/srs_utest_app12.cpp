//
// Copyright (c) 2013-2025 The SRS Authors
//
// SPDX-License-Identifier: MIT
//
#include <srs_utest_app12.hpp>

using namespace std;

#include <srs_app_srt_source.hpp>
#include <srs_kernel_error.hpp>
#include <srs_kernel_packet.hpp>
#include <srs_core_autofree.hpp>
#include <srs_utest_app6.hpp>
#include <srs_kernel_utility.hpp>
#include <srs_kernel_ts.hpp>
#include <srs_kernel_buffer.hpp>
#include <srs_utest_coworkers.hpp>

// Mock frame target implementation
MockSrtFrameTarget::MockSrtFrameTarget()
{
    on_frame_count_ = 0;
    last_frame_ = NULL;
    frame_error_ = srs_success;
}

MockSrtFrameTarget::~MockSrtFrameTarget()
{
    srs_freep(last_frame_);
    srs_freep(frame_error_);
}

srs_error_t MockSrtFrameTarget::on_frame(SrsMediaPacket *frame)
{
    on_frame_count_++;

    // Store a copy of the frame for verification
    srs_freep(last_frame_);
    if (frame) {
        last_frame_ = frame->copy();
    }

    return srs_error_copy(frame_error_);
}

void MockSrtFrameTarget::reset()
{
    on_frame_count_ = 0;
    srs_freep(last_frame_);
    srs_freep(frame_error_);
}

void MockSrtFrameTarget::set_frame_error(srs_error_t err)
{
    srs_freep(frame_error_);
    frame_error_ = srs_error_copy(err);
}

// Test SrsSrtPacket wrap and copy functionality
// This test covers the major use scenario: wrapping data and copying packets
VOID TEST(SrsSrtPacketTest, WrapDataAndCopy)
{
    // Create an SRT packet
    SrsUniquePtr<SrsSrtPacket> pkt(new SrsSrtPacket());

    // Test wrap(char *data, int size) - wraps raw data into packet
    const char *test_data = "Hello SRT";
    int data_size = strlen(test_data);
    char *wrapped_buf = pkt->wrap((char *)test_data, data_size);

    // Verify the wrapped data
    EXPECT_TRUE(wrapped_buf != NULL);
    EXPECT_EQ(data_size, pkt->size());
    EXPECT_EQ(0, memcmp(wrapped_buf, test_data, data_size));

    // Test copy() - creates a copy of the packet
    SrsUniquePtr<SrsSrtPacket> copied_pkt(pkt->copy());

    // Verify the copied packet has the same data
    EXPECT_TRUE(copied_pkt.get() != NULL);
    EXPECT_EQ(pkt->size(), copied_pkt->size());
    EXPECT_EQ(0, memcmp(pkt->data(), copied_pkt->data(), pkt->size()));

    // Test wrap(SrsMediaPacket *msg) - wraps a media packet (RTMP to SRT scenario)
    SrsUniquePtr<SrsMediaPacket> msg(new SrsMediaPacket());
    const char *media_data = "Media Packet Data";
    int media_size = strlen(media_data);
    char *media_buf = new char[media_size];
    memcpy(media_buf, media_data, media_size);
    msg->wrap(media_buf, media_size);
    msg->timestamp_ = 12345;
    msg->message_type_ = SrsFrameTypeVideo;

    // Wrap the media packet into SRT packet
    SrsUniquePtr<SrsSrtPacket> pkt2(new SrsSrtPacket());
    char *wrapped_msg_buf = pkt2->wrap(msg.get());

    // Verify the wrapped message
    EXPECT_TRUE(wrapped_msg_buf != NULL);
    EXPECT_EQ(media_size, pkt2->size());
    EXPECT_EQ(0, memcmp(wrapped_msg_buf, media_data, media_size));

    // Copy the packet with wrapped message
    SrsUniquePtr<SrsSrtPacket> copied_pkt2(pkt2->copy());

    // Verify the copied packet
    EXPECT_TRUE(copied_pkt2.get() != NULL);
    EXPECT_EQ(pkt2->size(), copied_pkt2->size());
    EXPECT_EQ(0, memcmp(pkt2->data(), copied_pkt2->data(), pkt2->size()));
}

// Test SrsSrtConsumer update_source_id and enqueue functionality
// This test covers the major use scenario: updating source ID flag and enqueueing packets
VOID TEST(SrsSrtConsumerTest, UpdateSourceIdAndEnqueue)
{
    srs_error_t err;

    // Create a mock SRT source
    MockSrtSource mock_source;

    // Create an SRT consumer with the mock source
    SrsUniquePtr<SrsSrtConsumer> consumer(new SrsSrtConsumer(&mock_source));

    // Test update_source_id() - should set the flag
    consumer->update_source_id();

    // Test enqueue() - add packets to the queue
    // Create first SRT packet (consumer takes ownership)
    SrsSrtPacket *pkt1 = new SrsSrtPacket();
    const char *data1 = "Test SRT Packet 1";
    pkt1->wrap((char *)data1, strlen(data1));

    // Enqueue first packet (consumer takes ownership)
    HELPER_EXPECT_SUCCESS(consumer->enqueue(pkt1));

    // Create second SRT packet (consumer takes ownership)
    SrsSrtPacket *pkt2 = new SrsSrtPacket();
    const char *data2 = "Test SRT Packet 2";
    pkt2->wrap((char *)data2, strlen(data2));

    // Enqueue second packet (consumer takes ownership)
    HELPER_EXPECT_SUCCESS(consumer->enqueue(pkt2));

    // Dump packets to verify they were enqueued
    SrsSrtPacket *dumped_pkt1 = NULL;
    HELPER_EXPECT_SUCCESS(consumer->dump_packet(&dumped_pkt1));
    EXPECT_TRUE(dumped_pkt1 != NULL);
    EXPECT_EQ(strlen(data1), (size_t)dumped_pkt1->size());
    EXPECT_EQ(0, memcmp(dumped_pkt1->data(), data1, strlen(data1)));
    srs_freep(dumped_pkt1);

    SrsSrtPacket *dumped_pkt2 = NULL;
    HELPER_EXPECT_SUCCESS(consumer->dump_packet(&dumped_pkt2));
    EXPECT_TRUE(dumped_pkt2 != NULL);
    EXPECT_EQ(strlen(data2), (size_t)dumped_pkt2->size());
    EXPECT_EQ(0, memcmp(dumped_pkt2->data(), data2, strlen(data2)));
    srs_freep(dumped_pkt2);

    // Verify queue is now empty
    SrsSrtPacket *dumped_pkt3 = NULL;
    HELPER_EXPECT_SUCCESS(consumer->dump_packet(&dumped_pkt3));
    EXPECT_TRUE(dumped_pkt3 == NULL);
}

// Test SrsSrtConsumer dump_packet functionality
// This test covers the major use scenario: dumping packets from queue with source ID update
VOID TEST(SrsSrtConsumerTest, DumpPacket)
{
    srs_error_t err;

    // Create a mock SRT source
    MockSrtSource mock_source;

    // Create an SRT consumer with the mock source
    SrsUniquePtr<SrsSrtConsumer> consumer(new SrsSrtConsumer(&mock_source));

    // Enqueue a packet first
    SrsSrtPacket *pkt = new SrsSrtPacket();
    const char *data = "Test SRT Data";
    pkt->wrap((char *)data, strlen(data));
    HELPER_EXPECT_SUCCESS(consumer->enqueue(pkt));

    // Trigger source ID update
    consumer->update_source_id();

    // Dump packet - this should update source_id flag and return the packet
    SrsSrtPacket *dumped_pkt = NULL;
    HELPER_EXPECT_SUCCESS(consumer->dump_packet(&dumped_pkt));

    // Verify the packet was dumped correctly
    EXPECT_TRUE(dumped_pkt != NULL);
    EXPECT_EQ(strlen(data), (size_t)dumped_pkt->size());
    EXPECT_EQ(0, memcmp(dumped_pkt->data(), data, strlen(data)));
    srs_freep(dumped_pkt);

    // Dump again from empty queue - should return NULL
    SrsSrtPacket *empty_pkt = NULL;
    HELPER_EXPECT_SUCCESS(consumer->dump_packet(&empty_pkt));
    EXPECT_TRUE(empty_pkt == NULL);
}

// Test SrsSrtConsumer wait functionality
// This test covers the major use scenario: waiting for messages and being signaled when enough messages arrive
VOID TEST(SrsSrtConsumerTest, WaitForMessages)
{
    srs_error_t err;

    // Create a mock SRT source
    MockSrtSource mock_source;

    // Create an SRT consumer with the mock source
    SrsUniquePtr<SrsSrtConsumer> consumer(new SrsSrtConsumer(&mock_source));

    // Scenario 1: Queue already has enough messages - wait() should return immediately
    // Enqueue 3 packets first
    for (int i = 0; i < 3; i++) {
        SrsSrtPacket *pkt = new SrsSrtPacket();
        char data[32];
        snprintf(data, sizeof(data), "Packet %d", i);
        pkt->wrap(data, strlen(data));
        HELPER_EXPECT_SUCCESS(consumer->enqueue(pkt));
    }

    // Wait for 2 messages - should return immediately since queue has 3 packets (3 > 2)
    srs_utime_t start_time = srs_time_now_realtime();
    consumer->wait(2, 1 * SRS_UTIME_MILLISECONDS);
    srs_utime_t elapsed = srs_time_now_realtime() - start_time;

    // Should return immediately (elapsed time should be very small, less than 10ms)
    EXPECT_LT(elapsed, 1 * SRS_UTIME_MILLISECONDS);

    // Clean up the queue
    for (int i = 0; i < 3; i++) {
        SrsSrtPacket *pkt = NULL;
        HELPER_EXPECT_SUCCESS(consumer->dump_packet(&pkt));
        srs_freep(pkt);
    }

    // Scenario 2: Queue doesn't have enough messages - wait() should timeout
    // Enqueue only 1 packet
    SrsSrtPacket *pkt1 = new SrsSrtPacket();
    const char *data1 = "Single Packet";
    pkt1->wrap((char *)data1, strlen(data1));
    HELPER_EXPECT_SUCCESS(consumer->enqueue(pkt1));

    // Wait for 2 messages with 50ms timeout - should timeout since queue only has 1 packet (1 <= 2)
    start_time = srs_time_now_realtime();
    consumer->wait(2, 10 * SRS_UTIME_MILLISECONDS);
    elapsed = srs_time_now_realtime() - start_time;

    // Should wait for approximately the timeout duration (allow 20ms tolerance due to system scheduling)
    EXPECT_GE(elapsed, 0.1 * SRS_UTIME_MILLISECONDS);
    EXPECT_LT(elapsed, 50 * SRS_UTIME_MILLISECONDS);

    // Clean up
    SrsSrtPacket *pkt = NULL;
    HELPER_EXPECT_SUCCESS(consumer->dump_packet(&pkt));
    srs_freep(pkt);
}

// Test SrsSrtFrameBuilder::on_ts_message functionality
// This test covers the major use scenario: processing H.264 video TS message
VOID TEST(SrsSrtFrameBuilderTest, OnTsMessageH264Video)
{
    srs_error_t err;

    // Create mock frame target
    MockSrtFrameTarget mock_target;

    // Create SrsSrtFrameBuilder with mock target
    SrsUniquePtr<SrsSrtFrameBuilder> builder(new SrsSrtFrameBuilder(&mock_target));

    // Create a mock request for initialization
    MockRtcAsyncCallRequest mock_req("test.vhost", "live", "stream1");
    HELPER_EXPECT_SUCCESS(builder->initialize(&mock_req));

    // Create a TS channel for H.264 video
    SrsUniquePtr<SrsTsChannel> channel(new SrsTsChannel());
    channel->apply_ = SrsTsPidApplyVideo;
    channel->stream_ = SrsTsStreamVideoH264;

    // Create a TS message with H.264 video data (no packet needed for this test)
    SrsUniquePtr<SrsTsMessage> msg(new SrsTsMessage(channel.get(), NULL));
    msg->sid_ = SrsTsPESStreamIdVideoCommon;
    msg->dts_ = 90000;  // 1 second in 90kHz timebase
    msg->pts_ = 90000;

    // Create simple H.264 NAL unit data (IDR frame with SPS/PPS)
    // Format: [4-byte length][NAL unit data]
    // SPS NAL (0x67)
    uint8_t sps_data[] = {0x67, 0x42, 0x00, 0x1e, 0x8d, 0x8d, 0x40, 0x50};
    // PPS NAL (0x68)
    uint8_t pps_data[] = {0x68, 0xce, 0x3c, 0x80};
    // IDR NAL (0x65)
    uint8_t idr_data[] = {0x65, 0x88, 0x84, 0x00, 0x10};

    // Build AnnexB format: start_code + NAL
    int total_size = 4 + sizeof(sps_data) + 4 + sizeof(pps_data) + 4 + sizeof(idr_data);
    char *payload = new char[total_size];
    SrsBuffer stream(payload, total_size);

    // Write SPS with start code
    stream.write_4bytes(0x00000001);
    stream.write_bytes((char *)sps_data, sizeof(sps_data));

    // Write PPS with start code
    stream.write_4bytes(0x00000001);
    stream.write_bytes((char *)pps_data, sizeof(pps_data));

    // Write IDR with start code
    stream.write_4bytes(0x00000001);
    stream.write_bytes((char *)idr_data, sizeof(idr_data));

    // Wrap payload into message using SrsSimpleStream
    msg->payload_ = new SrsSimpleStream();
    msg->payload_->append(payload, total_size);

    // Call on_ts_message - this is the method under test
    HELPER_EXPECT_SUCCESS(builder->on_ts_message(msg.get()));

    // Verify that frame was delivered to target
    // Should have at least 1 frame (sequence header + video frame)
    EXPECT_GT(mock_target.on_frame_count_, 0);
}

// Test SrsSrtFrameBuilder::on_ts_video_avc functionality
// This test covers the major use scenario: demuxing H.264 annexb data with SPS/PPS/IDR frames
VOID TEST(SrsSrtFrameBuilderTest, OnTsVideoAvc)
{
    srs_error_t err;

    // Create mock frame target
    MockSrtFrameTarget mock_target;

    // Create SrsSrtFrameBuilder with mock target
    SrsUniquePtr<SrsSrtFrameBuilder> builder(new SrsSrtFrameBuilder(&mock_target));

    // Create a mock request for initialization
    MockRtcAsyncCallRequest mock_req("test.vhost", "live", "stream1");
    HELPER_EXPECT_SUCCESS(builder->initialize(&mock_req));

    // Create a TS message with H.264 video data
    SrsUniquePtr<SrsTsChannel> channel(new SrsTsChannel());
    channel->apply_ = SrsTsPidApplyVideo;
    channel->stream_ = SrsTsStreamVideoH264;

    SrsUniquePtr<SrsTsMessage> msg(new SrsTsMessage(channel.get(), NULL));
    msg->sid_ = SrsTsPESStreamIdVideoCommon;
    msg->dts_ = 90000;  // 1 second in 90kHz timebase
    msg->pts_ = 90000;

    // Create H.264 annexb format data with SPS, PPS, and IDR frame
    // SPS NAL (0x67 = type 7)
    uint8_t sps_data[] = {0x67, 0x42, 0x00, 0x1e, 0x8d, 0x8d, 0x40, 0x50};
    // PPS NAL (0x68 = type 8)
    uint8_t pps_data[] = {0x68, 0xce, 0x3c, 0x80};
    // IDR NAL (0x65 = type 5)
    uint8_t idr_data[] = {0x65, 0x88, 0x84, 0x00, 0x10};

    // Build annexb format: start_code (0x00000001) + NAL unit
    int total_size = 4 + sizeof(sps_data) + 4 + sizeof(pps_data) + 4 + sizeof(idr_data);
    char *payload = new char[total_size];
    SrsBuffer stream(payload, total_size);

    // Write SPS with 4-byte start code
    stream.write_4bytes(0x00000001);
    stream.write_bytes((char *)sps_data, sizeof(sps_data));

    // Write PPS with 4-byte start code
    stream.write_4bytes(0x00000001);
    stream.write_bytes((char *)pps_data, sizeof(pps_data));

    // Write IDR with 4-byte start code
    stream.write_4bytes(0x00000001);
    stream.write_bytes((char *)idr_data, sizeof(idr_data));

    // Create SrsBuffer for on_ts_video_avc to read from
    SrsBuffer avs(payload, total_size);

    // Call on_ts_video_avc - this is the method under test
    HELPER_EXPECT_SUCCESS(builder->on_ts_video_avc(msg.get(), &avs));

    // Verify that frames were delivered to target
    // Should have 2 frames: sequence header (from SPS/PPS change) + video frame (IDR)
    EXPECT_EQ(2, mock_target.on_frame_count_);

    // Verify the last frame is not NULL
    EXPECT_TRUE(mock_target.last_frame_ != NULL);

    // Verify the last frame is a video frame
    if (mock_target.last_frame_) {
        EXPECT_EQ(SrsFrameTypeVideo, mock_target.last_frame_->message_type_);
    }

    // Clean up payload
    delete[] payload;
}

// Test SrsSrtFrameBuilder::on_ts_video_hevc functionality
// This test covers the major use scenario: demuxing HEVC annexb data with VPS/SPS/PPS/IDR frames
VOID TEST(SrsSrtFrameBuilderTest, OnTsVideoHevc)
{
    srs_error_t err;

    // Create mock frame target
    MockSrtFrameTarget mock_target;

    // Create SrsSrtFrameBuilder with mock target
    SrsUniquePtr<SrsSrtFrameBuilder> builder(new SrsSrtFrameBuilder(&mock_target));

    // Create a mock request for initialization
    MockRtcAsyncCallRequest mock_req("test.vhost", "live", "stream1");
    HELPER_EXPECT_SUCCESS(builder->initialize(&mock_req));

    // Create a TS message with HEVC video data
    SrsUniquePtr<SrsTsChannel> channel(new SrsTsChannel());
    channel->apply_ = SrsTsPidApplyVideo;
    channel->stream_ = SrsTsStreamVideoHEVC;

    SrsUniquePtr<SrsTsMessage> msg(new SrsTsMessage(channel.get(), NULL));
    msg->sid_ = SrsTsPESStreamIdVideoCommon;
    msg->dts_ = 90000;  // 1 second in 90kHz timebase
    msg->pts_ = 90000;

    // Create HEVC annexb format data with VPS, SPS, PPS, and IDR frame
    // VPS NAL (0x40 = type 32)
    uint8_t vps_data[] = {0x40, 0x01, 0x0c, 0x01, 0xff, 0xff, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0x90, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x3d, 0x95, 0x98, 0x09};
    // SPS NAL (0x42 = type 33)
    uint8_t sps_data[] = {0x42, 0x01, 0x01, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0x90, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x3d, 0xa0, 0x02, 0x80, 0x80, 0x2d, 0x16, 0x59, 0x59, 0xa4, 0x93, 0x2b, 0xc0, 0x40, 0x40, 0x00, 0x00, 0xfa, 0x40, 0x00, 0x17, 0x70, 0x02};
    // PPS NAL (0x44 = type 34)
    uint8_t pps_data[] = {0x44, 0x01, 0xc1, 0x72, 0xb4, 0x62, 0x40};
    // IDR NAL (0x26 = type 19, IDR_W_RADL)
    uint8_t idr_data[] = {0x26, 0x01, 0xaf, 0x06, 0xb8, 0x63, 0xef, 0x3a, 0x7f, 0x3c, 0x00, 0x01, 0x00, 0x80};

    // Build annexb format: start_code (0x00000001) + NAL unit
    int total_size = 4 + sizeof(vps_data) + 4 + sizeof(sps_data) + 4 + sizeof(pps_data) + 4 + sizeof(idr_data);
    char *payload = new char[total_size];
    SrsBuffer stream(payload, total_size);

    // Write VPS with 4-byte start code
    stream.write_4bytes(0x00000001);
    stream.write_bytes((char *)vps_data, sizeof(vps_data));

    // Write SPS with 4-byte start code
    stream.write_4bytes(0x00000001);
    stream.write_bytes((char *)sps_data, sizeof(sps_data));

    // Write PPS with 4-byte start code
    stream.write_4bytes(0x00000001);
    stream.write_bytes((char *)pps_data, sizeof(pps_data));

    // Write IDR with 4-byte start code
    stream.write_4bytes(0x00000001);
    stream.write_bytes((char *)idr_data, sizeof(idr_data));

    // Create SrsBuffer for on_ts_video_hevc to read from
    SrsBuffer avs(payload, total_size);

    // Call on_ts_video_hevc - this is the method under test
    HELPER_EXPECT_SUCCESS(builder->on_ts_video_hevc(msg.get(), &avs));

    // Verify that frames were delivered to target
    // Should have 2 frames: sequence header (from VPS/SPS/PPS change) + video frame (IDR)
    EXPECT_EQ(2, mock_target.on_frame_count_);

    // Verify the last frame is not NULL
    EXPECT_TRUE(mock_target.last_frame_ != NULL);

    // Verify the last frame is a video frame
    if (mock_target.last_frame_) {
        EXPECT_EQ(SrsFrameTypeVideo, mock_target.last_frame_->message_type_);
    }

    // Clean up payload
    delete[] payload;
}

// Test SrsSrtFrameBuilder::check_sps_pps_change functionality
// This test covers the major use scenario: generating video sequence header when SPS/PPS change
VOID TEST(SrsSrtFrameBuilderTest, CheckSpsPpsChange)
{
    srs_error_t err;

    // Create mock frame target
    MockSrtFrameTarget mock_target;

    // Create SrsSrtFrameBuilder with mock target
    SrsUniquePtr<SrsSrtFrameBuilder> builder(new SrsSrtFrameBuilder(&mock_target));

    // Create a TsMessage with valid timestamp
    SrsUniquePtr<SrsTsMessage> msg(new SrsTsMessage());
    msg->dts_ = 90000;  // 1 second in 90kHz timebase (will be converted to 1000ms)
    msg->pts_ = 90000;

    // Set up SPS and PPS data in the builder
    // Use simple but valid SPS/PPS data
    uint8_t sps_data[] = {0x67, 0x42, 0x00, 0x1e, 0x8d, 0x8d, 0x40, 0x50};
    uint8_t pps_data[] = {0x68, 0xce, 0x3c, 0x80};

    // Access private members to set up the test scenario
    builder->sps_ = std::string((char*)sps_data, sizeof(sps_data));
    builder->pps_ = std::string((char*)pps_data, sizeof(pps_data));
    builder->sps_pps_change_ = true;  // Simulate SPS/PPS change detected

    // Call check_sps_pps_change - this should generate and send a sequence header frame
    HELPER_EXPECT_SUCCESS(builder->check_sps_pps_change(msg.get()));

    // Verify that a frame was delivered to target (the sequence header)
    EXPECT_EQ(1, mock_target.on_frame_count_);

    // Verify the frame is not NULL
    EXPECT_TRUE(mock_target.last_frame_ != NULL);

    // Verify the frame is a video frame
    if (mock_target.last_frame_) {
        EXPECT_EQ(SrsFrameTypeVideo, mock_target.last_frame_->message_type_);
        // Verify timestamp was converted correctly (90000 / 90 = 1000ms)
        EXPECT_EQ(1000, (int)mock_target.last_frame_->timestamp_);
    }

    // Verify that sps_pps_change_ flag was reset to false
    EXPECT_FALSE(builder->sps_pps_change_);
}

// Test SrsSrtFrameBuilder::on_h264_frame functionality
// This test covers the major use scenario: converting H.264 NAL units to RTMP video frame
VOID TEST(SrsSrtFrameBuilderTest, OnH264Frame)
{
    srs_error_t err;

    // Create mock frame target
    MockSrtFrameTarget mock_target;

    // Create SrsSrtFrameBuilder with mock target
    SrsUniquePtr<SrsSrtFrameBuilder> builder(new SrsSrtFrameBuilder(&mock_target));

    // Create a mock request for initialization
    MockRtcAsyncCallRequest mock_req("test.vhost", "live", "stream1");
    HELPER_EXPECT_SUCCESS(builder->initialize(&mock_req));

    // Create a TS message with H.264 video timing information
    SrsUniquePtr<SrsTsMessage> msg(new SrsTsMessage());
    msg->dts_ = 90000;  // 1 second in 90kHz timebase (will be converted to 1000ms)
    msg->pts_ = 99000;  // 1.1 seconds in 90kHz timebase (will be converted to 1100ms, CTS=100ms)

    // Create H.264 NAL units for an IDR frame
    // IDR NAL (0x65 = type 5, keyframe)
    uint8_t idr_nal[] = {0x65, 0x88, 0x84, 0x00, 0x10, 0x20, 0x30, 0x40};
    // Non-IDR NAL (0x41 = type 1, inter frame)
    uint8_t non_idr_nal[] = {0x41, 0x9a, 0x12, 0x34};

    // Build ipb_frames vector with NAL units
    vector<pair<char *, int> > ipb_frames;
    ipb_frames.push_back(make_pair((char*)idr_nal, sizeof(idr_nal)));
    ipb_frames.push_back(make_pair((char*)non_idr_nal, sizeof(non_idr_nal)));

    // Call on_h264_frame - should convert TS message to RTMP video frame
    HELPER_EXPECT_SUCCESS(builder->on_h264_frame(msg.get(), ipb_frames));

    // Verify that on_frame was called once
    EXPECT_EQ(1, mock_target.on_frame_count_);

    // Verify the frame is not NULL
    EXPECT_TRUE(mock_target.last_frame_ != NULL);

    if (mock_target.last_frame_) {
        // Verify the frame is a video frame
        EXPECT_EQ(SrsFrameTypeVideo, mock_target.last_frame_->message_type_);

        // Verify timestamp was converted correctly (90000 / 90 = 1000ms)
        EXPECT_EQ(1000, (int)mock_target.last_frame_->timestamp_);

        // Verify the payload structure
        // Expected structure: 5-byte video tag header + (4-byte length + NAL data) for each NAL
        // 5 + (4 + 8) + (4 + 4) = 5 + 12 + 8 = 25 bytes
        int expected_size = 5 + (4 + sizeof(idr_nal)) + (4 + sizeof(non_idr_nal));
        EXPECT_EQ(expected_size, mock_target.last_frame_->size());

        // Verify the video tag header
        SrsBuffer payload(mock_target.last_frame_->payload(), mock_target.last_frame_->size());

        // First byte: 0x17 for keyframe (type=1, codec=7 for AVC)
        uint8_t frame_type_codec = payload.read_1bytes();
        EXPECT_EQ(0x17, frame_type_codec);

        // Second byte: 0x01 for AVC NALU
        uint8_t avc_packet_type = payload.read_1bytes();
        EXPECT_EQ(0x01, avc_packet_type);

        // Next 3 bytes: composition time (CTS = PTS - DTS = 1100 - 1000 = 100ms)
        int32_t cts = payload.read_3bytes();
        EXPECT_EQ(100, cts);

        // Verify first NAL unit (IDR)
        int32_t nal1_size = payload.read_4bytes();
        EXPECT_EQ((int)sizeof(idr_nal), nal1_size);
        char nal1_data[sizeof(idr_nal)];
        payload.read_bytes(nal1_data, sizeof(idr_nal));
        EXPECT_EQ(0, memcmp(nal1_data, idr_nal, sizeof(idr_nal)));

        // Verify second NAL unit (non-IDR)
        int32_t nal2_size = payload.read_4bytes();
        EXPECT_EQ((int)sizeof(non_idr_nal), nal2_size);
        char nal2_data[sizeof(non_idr_nal)];
        payload.read_bytes(nal2_data, sizeof(non_idr_nal));
        EXPECT_EQ(0, memcmp(nal2_data, non_idr_nal, sizeof(non_idr_nal)));
    }
}

// Test SrsSrtFrameBuilder check_vps_sps_pps_change functionality
// This test covers the major use scenario: generating HEVC sequence header when VPS/SPS/PPS change
VOID TEST(SrsSrtFrameBuilderTest, CheckVpsSppsPpsChange)
{
    srs_error_t err;

    // Create mock frame target
    MockSrtFrameTarget mock_target;

    // Create SrsSrtFrameBuilder with mock target
    SrsUniquePtr<SrsSrtFrameBuilder> builder(new SrsSrtFrameBuilder(&mock_target));

    // Create a mock TsMessage with valid DTS/PTS (in 90kHz timebase)
    SrsUniquePtr<SrsTsMessage> msg(new SrsTsMessage());
    msg->dts_ = 90000;  // 1 second in 90kHz
    msg->pts_ = 90000;  // 1 second in 90kHz

    // Valid HEVC VPS/SPS/PPS data (same as used in OnTsVideoHevc test)
    // VPS NAL (0x40 = type 32)
    uint8_t vps_data[] = {0x40, 0x01, 0x0c, 0x01, 0xff, 0xff, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0x90, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x3d, 0x95, 0x98, 0x09};
    std::string vps((char *)vps_data, sizeof(vps_data));

    // SPS NAL (0x42 = type 33)
    uint8_t sps_data[] = {0x42, 0x01, 0x01, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0x90, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x3d, 0xa0, 0x02, 0x80, 0x80, 0x2d, 0x16, 0x59, 0x59, 0xa4, 0x93, 0x2b, 0xc0, 0x40, 0x40, 0x00, 0x00, 0xfa, 0x40, 0x00, 0x17, 0x70, 0x02};
    std::string sps((char *)sps_data, sizeof(sps_data));

    // PPS NAL (0x44 = type 34)
    uint8_t pps_data[] = {0x44, 0x01, 0xc1, 0x72, 0xb4, 0x62, 0x40};
    std::string pps((char *)pps_data, sizeof(pps_data));

    // Set the HEVC VPS/SPS/PPS in the builder (accessing private members via test macro)
    builder->vps_sps_pps_change_ = true;
    builder->hevc_vps_ = vps;
    builder->hevc_sps_ = sps;
    builder->hevc_pps_.clear();
    builder->hevc_pps_.push_back(pps);

    // Call check_vps_sps_pps_change - should generate sequence header and call on_frame
    HELPER_EXPECT_SUCCESS(builder->check_vps_sps_pps_change(msg.get()));

    // Verify that on_frame was called once
    EXPECT_EQ(1, mock_target.on_frame_count_);

    // Verify that vps_sps_pps_change_ flag was reset to false
    EXPECT_FALSE(builder->vps_sps_pps_change_);

    // Verify the frame was generated correctly
    EXPECT_TRUE(mock_target.last_frame_ != NULL);
    if (mock_target.last_frame_) {
        // Verify it's a video frame
        EXPECT_EQ(SrsFrameTypeVideo, mock_target.last_frame_->message_type_);

        // Verify timestamp conversion from 90kHz to milliseconds (90000 / 90 = 1000ms)
        EXPECT_EQ(1000u, (uint32_t)mock_target.last_frame_->timestamp_);
    }

    // Test scenario 2: vps_sps_pps_change_ is false - should return immediately without calling on_frame
    mock_target.reset();
    builder->vps_sps_pps_change_ = false;

    HELPER_EXPECT_SUCCESS(builder->check_vps_sps_pps_change(msg.get()));

    // Verify on_frame was NOT called
    EXPECT_EQ(0, mock_target.on_frame_count_);

    // Test scenario 3: vps_sps_pps_change_ is true but VPS is empty - should return without calling on_frame
    mock_target.reset();
    builder->vps_sps_pps_change_ = true;
    builder->hevc_vps_ = "";  // Empty VPS

    HELPER_EXPECT_SUCCESS(builder->check_vps_sps_pps_change(msg.get()));

    // Verify on_frame was NOT called
    EXPECT_EQ(0, mock_target.on_frame_count_);
}

// Test SrsSrtFrameBuilder::on_hevc_frame functionality
// This test covers the major use scenario: processing HEVC video frame with multiple NALUs including IDR frame
VOID TEST(SrsSrtFrameBuilderTest, OnHevcFrameWithIDR)
{
    srs_error_t err;

    // Create mock frame target
    MockSrtFrameTarget mock_target;

    // Create SrsSrtFrameBuilder with mock target
    SrsUniquePtr<SrsSrtFrameBuilder> builder(new SrsSrtFrameBuilder(&mock_target));

    // Create a mock request for initialization
    MockRtcAsyncCallRequest mock_req("test.vhost", "live", "stream1");
    HELPER_EXPECT_SUCCESS(builder->initialize(&mock_req));

    // Create a TS channel for HEVC video
    SrsUniquePtr<SrsTsChannel> channel(new SrsTsChannel());
    channel->apply_ = SrsTsPidApplyVideo;
    channel->stream_ = SrsTsStreamVideoHEVC;

    // Create a TS message with HEVC video data
    SrsUniquePtr<SrsTsMessage> msg(new SrsTsMessage(channel.get(), NULL));
    msg->sid_ = SrsTsPESStreamIdVideoCommon;
    msg->dts_ = 90000;  // 1 second in 90kHz timebase (will be converted to 1000ms in FLV)
    msg->pts_ = 90000;

    // Create HEVC NAL units for testing
    // VPS NAL (type 32, 0x40 in first byte: (32 << 1) = 0x40)
    uint8_t vps_data[] = {0x40, 0x01, 0x0c, 0x01, 0xff, 0xff, 0x01, 0x60};
    // SPS NAL (type 33, 0x42 in first byte: (33 << 1) = 0x42)
    uint8_t sps_data[] = {0x42, 0x01, 0x01, 0x01, 0x60, 0x00, 0x00, 0x03};
    // PPS NAL (type 34, 0x44 in first byte: (34 << 1) = 0x44)
    uint8_t pps_data[] = {0x44, 0x01, 0xc1, 0x73, 0xd1, 0x89};
    // IDR NAL (type 19, 0x26 in first byte: (19 << 1) = 0x26) - this is an IRAP frame
    uint8_t idr_data[] = {0x26, 0x01, 0xaf, 0x08, 0x40, 0x00, 0x00, 0x10};

    // Build ipb_frames vector with NAL units
    std::vector<std::pair<char *, int> > ipb_frames;
    ipb_frames.push_back(std::make_pair((char *)vps_data, sizeof(vps_data)));
    ipb_frames.push_back(std::make_pair((char *)sps_data, sizeof(sps_data)));
    ipb_frames.push_back(std::make_pair((char *)pps_data, sizeof(pps_data)));
    ipb_frames.push_back(std::make_pair((char *)idr_data, sizeof(idr_data)));

    // Call on_hevc_frame
    HELPER_EXPECT_SUCCESS(builder->on_hevc_frame(msg.get(), ipb_frames));

    // Verify the frame was delivered to the target
    EXPECT_EQ(1, mock_target.on_frame_count_);
    EXPECT_TRUE(mock_target.last_frame_ != NULL);

    // Verify the frame properties
    SrsMediaPacket *frame = mock_target.last_frame_;
    EXPECT_TRUE(frame->payload() != NULL);
    EXPECT_GT(frame->size(), 0);

    // Expected frame size: 5 bytes header + (4 + vps_size) + (4 + sps_size) + (4 + pps_size) + (4 + idr_size)
    int expected_size = 5 + (4 + sizeof(vps_data)) + (4 + sizeof(sps_data)) + (4 + sizeof(pps_data)) + (4 + sizeof(idr_data));
    EXPECT_EQ(expected_size, frame->size());

    // Verify the timestamp (90000 / 90 = 1000ms)
    EXPECT_EQ(1000, frame->timestamp_);

    // Verify the message type is video
    EXPECT_EQ(SrsFrameTypeVideo, frame->message_type_);

    // Verify the enhanced RTMP header format
    SrsUniquePtr<SrsBuffer> buffer(new SrsBuffer(frame->payload(), frame->size()));

    // Read and verify the 5-byte video tag header
    uint8_t header_byte = buffer->read_1bytes();
    // Check SRS_FLV_IS_EX_HEADER bit is set (0x80)
    EXPECT_TRUE((header_byte & 0x80) != 0);
    // Check frame type is keyframe (1 << 4 = 0x10, shifted to bits 4-6)
    uint8_t frame_type = (header_byte >> 4) & 0x07;
    EXPECT_EQ(SrsVideoAvcFrameTypeKeyFrame, frame_type);
    // Check packet type is CodedFramesX (3, in bits 0-3)
    uint8_t packet_type = header_byte & 0x0f;
    EXPECT_EQ(SrsVideoHEVCFrameTraitPacketTypeCodedFramesX, packet_type);

    // Verify HEVC fourcc 'hvc1'
    uint32_t fourcc = buffer->read_4bytes();
    EXPECT_EQ(0x68766331, fourcc);  // 'h' 'v' 'c' '1'

    // Verify NAL units are written correctly with 4-byte length prefix
    // VPS
    uint32_t vps_length = buffer->read_4bytes();
    EXPECT_EQ(sizeof(vps_data), vps_length);
    EXPECT_EQ(0, memcmp(buffer->data() + buffer->pos(), vps_data, sizeof(vps_data)));
    buffer->skip(sizeof(vps_data));

    // SPS
    uint32_t sps_length = buffer->read_4bytes();
    EXPECT_EQ(sizeof(sps_data), sps_length);
    EXPECT_EQ(0, memcmp(buffer->data() + buffer->pos(), sps_data, sizeof(sps_data)));
    buffer->skip(sizeof(sps_data));

    // PPS
    uint32_t pps_length = buffer->read_4bytes();
    EXPECT_EQ(sizeof(pps_data), pps_length);
    EXPECT_EQ(0, memcmp(buffer->data() + buffer->pos(), pps_data, sizeof(pps_data)));
    buffer->skip(sizeof(pps_data));

    // IDR
    uint32_t idr_length = buffer->read_4bytes();
    EXPECT_EQ(sizeof(idr_data), idr_length);
    EXPECT_EQ(0, memcmp(buffer->data() + buffer->pos(), idr_data, sizeof(idr_data)));
}

// Test SrsSrtFrameBuilder::on_ts_audio functionality
// This test covers the major use scenario: processing AAC audio TS message with ADTS format
VOID TEST(SrsSrtFrameBuilderTest, OnTsAudioAAC)
{
    srs_error_t err;

    // Create mock frame target
    MockSrtFrameTarget mock_target;

    // Create SrsSrtFrameBuilder with mock target
    SrsUniquePtr<SrsSrtFrameBuilder> builder(new SrsSrtFrameBuilder(&mock_target));

    // Create a mock request for initialization
    MockRtcAsyncCallRequest mock_req("test.vhost", "live", "stream1");
    HELPER_EXPECT_SUCCESS(builder->initialize(&mock_req));

    // Create a TS channel for AAC audio
    SrsUniquePtr<SrsTsChannel> channel(new SrsTsChannel());
    channel->apply_ = SrsTsPidApplyAudio;
    channel->stream_ = SrsTsStreamAudioAAC;

    // Create ADTS AAC frame data first (before creating SrsTsMessage)
    // ADTS header format (7 bytes for protection_absent=1):
    // Based on working example from srs_utest_avc.cpp
    // Frame format: 0xff 0xf9 0x50 0x80 0x01 0x3f 0xfc [payload]
    // - syncword: 0xfff (12 bits)
    // - ID: 1 (MPEG-2 AAC)
    // - protection_absent: 1
    // - profile: 01 (AAC-LC)
    // - sampling_frequency_index: 0100 (44.1kHz, index 4)
    // - channel_configuration: 010 (stereo)
    // - frame_length: 10 bytes (7 header + 3 payload)

    // Build ADTS frame: 44.1kHz, stereo, AAC-LC, 10 bytes total (7 header + 3 payload)
    // frame_length = 10 = 0b0000000001010 (13 bits)
    // Bit layout: bits[12-11]=00, bits[10-3]=00000001, bits[2-0]=010
    uint8_t adts_frame[] = {
        0xff, 0xf9,             // syncword(0xfff) + ID(1) + layer(0) + protection_absent(1)
        0x50,                   // profile(01=AAC-LC) + sampling_frequency_index(0100=44.1kHz) + private_bit(0) + channel_config high bit(0)
        0x80,                   // channel_config low(10=stereo) + original_copy(0) + home(0) + copyright bits(00) + frame_length bits[12-11](00)
        0x01,                   // frame_length bits[10-3] (00000001)
        0x5f,                   // frame_length bits[2-0](010) + adts_buffer_fullness high 5 bits(11111)
        0xfc,                   // adts_buffer_fullness low 6 bits(111111) + number_of_raw_data_blocks(00)
        0xaa, 0xbb, 0xcc        // 3 bytes AAC raw data payload
    };

    int payload_size = sizeof(adts_frame);
    char *payload = new char[payload_size];
    memcpy(payload, adts_frame, payload_size);

    // Create a TS message with AAC audio data
    // Set up the payload in SrsSimpleStream
    SrsUniquePtr<SrsTsMessage> msg(new SrsTsMessage(channel.get(), NULL));
    msg->sid_ = SrsTsPESStreamIdAudioCommon;
    msg->dts_ = 90000;  // 1 second in 90kHz timebase
    msg->pts_ = 90000;

    // Append payload to the message's payload stream
    msg->payload_->append(payload, payload_size);

    // Call on_ts_message to process the AAC audio data (which internally calls on_ts_audio)
    HELPER_EXPECT_SUCCESS(builder->on_ts_message(msg.get()));

    // Verify that frames were sent to target
    // Should have 2 frames: 1 audio sequence header + 1 audio frame
    EXPECT_EQ(2, mock_target.on_frame_count_);
    EXPECT_TRUE(mock_target.last_frame_ != NULL);
    EXPECT_EQ(SrsFrameTypeAudio, mock_target.last_frame_->message_type_);

    // Verify the timestamp conversion from TS timebase (90kHz) to FLV timebase (1kHz)
    // pts = 90000 / 90 = 1000ms
    EXPECT_EQ(1000, (int)mock_target.last_frame_->timestamp_);

    srs_freepa(payload);
}

// Test SrsSrtFrameBuilder::check_audio_sh_change functionality
// This test covers the major use scenario: dispatching audio sequence header when audio config changes
VOID TEST(SrsSrtFrameBuilderTest, CheckAudioShChange)
{
    srs_error_t err;

    // Create mock frame target
    MockSrtFrameTarget mock_target;

    // Create SrsSrtFrameBuilder with mock target
    SrsUniquePtr<SrsSrtFrameBuilder> builder(new SrsSrtFrameBuilder(&mock_target));

    // Create a mock request for initialization
    MockRtcAsyncCallRequest mock_req("test.vhost", "live", "stream1");
    HELPER_EXPECT_SUCCESS(builder->initialize(&mock_req));

    // Set up audio sequence header change scenario
    // Simulate that audio_sh_ has been populated and audio_sh_change_ flag is set
    // This happens when audio specific config changes during stream processing
    builder->audio_sh_change_ = true;

    // Create a sample AAC audio specific config (2 bytes)
    // Format: profile(5 bits) + sampling_frequency_index(4 bits) + channel_configuration(4 bits) + other(3 bits)
    // AAC-LC (profile=2), 44.1kHz (index=4), stereo (channels=2)
    // Binary: 00010 0100 0010 000 = 0x1210
    uint8_t asc_data[] = {0x12, 0x10};
    builder->audio_sh_.assign((char*)asc_data, sizeof(asc_data));

    // Create a TS channel for AAC audio
    SrsUniquePtr<SrsTsChannel> channel(new SrsTsChannel());
    channel->apply_ = SrsTsPidApplyAudio;
    channel->stream_ = SrsTsStreamAudioAAC;

    // Create a TS message (the actual message content doesn't matter for this test)
    SrsUniquePtr<SrsTsMessage> msg(new SrsTsMessage(channel.get(), NULL));
    msg->sid_ = SrsTsPESStreamIdAudioCommon;
    msg->dts_ = 90000;  // 1 second in 90kHz timebase
    msg->pts_ = 90000;

    uint32_t pts = 1000;  // 1000ms in FLV timebase

    // Call check_audio_sh_change to dispatch the audio sequence header
    HELPER_EXPECT_SUCCESS(builder->check_audio_sh_change(msg.get(), pts));

    // Verify that audio sequence header frame was dispatched
    EXPECT_EQ(1, mock_target.on_frame_count_);
    EXPECT_TRUE(mock_target.last_frame_ != NULL);
    EXPECT_EQ(SrsFrameTypeAudio, mock_target.last_frame_->message_type_);
    EXPECT_EQ(pts, mock_target.last_frame_->timestamp_);

    // Verify the audio sequence header format
    SrsUniquePtr<SrsBuffer> buffer(new SrsBuffer(mock_target.last_frame_->payload(), mock_target.last_frame_->size()));

    // First byte: AAC codec flag
    // Format: codec(4 bits) + sample_rate(2 bits) + sample_bits(1 bit) + channels(1 bit)
    // AAC(10) + 44.1kHz(3) + 16bit(1) + stereo(1) = 0xAF
    uint8_t aac_flag = buffer->read_1bytes();
    EXPECT_EQ(0xAF, aac_flag);

    // Second byte: AAC packet type (0 = sequence header)
    uint8_t packet_type = buffer->read_1bytes();
    EXPECT_EQ(0, packet_type);

    // Remaining bytes: audio specific config
    EXPECT_EQ(sizeof(asc_data), (size_t)buffer->left());
    EXPECT_EQ(0, memcmp(buffer->data() + buffer->pos(), asc_data, sizeof(asc_data)));

    // Verify that audio_sh_change_ flag was reset to false
    EXPECT_FALSE(builder->audio_sh_change_);

    // Test that calling check_audio_sh_change again does nothing (flag is false)
    mock_target.reset();
    HELPER_EXPECT_SUCCESS(builder->check_audio_sh_change(msg.get(), pts));
    EXPECT_EQ(0, mock_target.on_frame_count_);
}

// Test SrsSrtFrameBuilder::on_aac_frame - converts AAC frame from TS to RTMP format
// This test covers the major use scenario: converting AAC audio data to RTMP format
VOID TEST(SrsSrtFrameBuilderTest, OnAacFrame)
{
    srs_error_t err;

    // Create mock frame target
    MockSrtFrameTarget mock_target;

    // Create SrsSrtFrameBuilder
    SrsUniquePtr<SrsSrtFrameBuilder> builder(new SrsSrtFrameBuilder(&mock_target));

    // Create a mock SrsTsMessage (only used for context, not directly accessed in on_aac_frame)
    SrsUniquePtr<SrsTsMessage> msg(new SrsTsMessage());

    // Create test AAC frame data (simulating raw AAC data)
    const char *aac_data = "AAC_FRAME_DATA_TEST";
    int data_size = strlen(aac_data);
    char *frame_data = new char[data_size];
    memcpy(frame_data, aac_data, data_size);

    // Set PTS (presentation timestamp) - convert from ms to 90kHz timebase for TS
    uint32_t pts = 1000; // 1000ms in RTMP timebase

    // Call on_aac_frame to convert AAC frame to RTMP format
    HELPER_EXPECT_SUCCESS(builder->on_aac_frame(msg.get(), pts, frame_data, data_size));

    // Verify that frame was sent to target
    EXPECT_EQ(1, mock_target.on_frame_count_);
    EXPECT_TRUE(mock_target.last_frame_ != NULL);

    // Verify the frame properties
    EXPECT_EQ(pts, mock_target.last_frame_->timestamp_);
    EXPECT_EQ(SrsFrameTypeAudio, mock_target.last_frame_->message_type_);

    // Verify the payload size: original data + 2 bytes FLV audio tag header
    int expected_size = data_size + 2;
    EXPECT_EQ(expected_size, mock_target.last_frame_->size());

    // Verify the audio tag header (first 2 bytes)
    char *payload = mock_target.last_frame_->payload();
    EXPECT_TRUE(payload != NULL);

    // First byte: audio flag = (codec << 4) | (sample_rate << 2) | (sample_bits << 1) | channels
    // Expected: (10 << 4) | (3 << 2) | (1 << 1) | 1 = 0xAF
    uint8_t expected_flag = (SrsAudioCodecIdAAC << 4) | (SrsAudioSampleRate44100 << 2) | (SrsAudioSampleBits16bit << 1) | SrsAudioChannelsStereo;
    EXPECT_EQ(expected_flag, (uint8_t)payload[0]);

    // Second byte: AAC packet type = 1 (AAC raw frame data)
    EXPECT_EQ(1, (uint8_t)payload[1]);

    // Verify the actual AAC data follows the 2-byte header
    EXPECT_EQ(0, memcmp(payload + 2, aac_data, data_size));

    srs_freepa(frame_data);
}

// Test SrsSrtSource::stream_is_dead and on_source_id_changed
// This test covers the major use scenario: stream lifecycle management and source ID changes
VOID TEST(SrsSrtSourceTest, StreamLifecycleAndSourceIdChange)
{
    srs_error_t err;

    // Create a mock request for SRT source initialization
    MockRtcAsyncCallRequest mock_req("test.vhost", "live", "stream1");

    // Create SrsSrtSource
    SrsUniquePtr<SrsSrtSource> source(new SrsSrtSource());

    // Initialize the source
    HELPER_EXPECT_SUCCESS(source->initialize(&mock_req));

    // Test 1: stream_is_dead() when can_publish is false (stream is publishing)
    // Simulate on_publish() which sets can_publish_ to false
    HELPER_EXPECT_SUCCESS(source->on_publish());
    EXPECT_FALSE(source->stream_is_dead());  // Should return false when publishing

    // Test 2: stream_is_dead() when can_publish is true but has consumers
    source->on_unpublish();  // Sets can_publish_ back to true
    EXPECT_TRUE(source->can_publish());

    // Create a consumer
    ISrsSrtConsumer *consumer = NULL;
    HELPER_EXPECT_SUCCESS(source->create_consumer(consumer));
    EXPECT_TRUE(consumer != NULL);

    // Should return false when has consumers
    EXPECT_FALSE(source->stream_is_dead());

    // Test 3: stream_is_dead() when can_publish is true, no consumers, but within cleanup delay
    // Destroy the consumer to trigger stream_die_at_ update
    srs_freep(consumer);

    // Should return false immediately after consumer destruction (within cleanup delay)
    EXPECT_FALSE(source->stream_is_dead());

    // Test 4: stream_is_dead() returns true after cleanup delay
    // Manually set stream_die_at_ to simulate time passing beyond cleanup delay
    // SRS_SRT_SOURCE_CLEANUP is 3 seconds
    source->stream_die_at_ = srs_time_now_cached() - (4 * SRS_UTIME_SECONDS);

    // Should return true after cleanup delay
    EXPECT_TRUE(source->stream_is_dead());

    // Test 5: on_source_id_changed() updates source ID and notifies consumers
    // Create a new source for testing source ID changes
    SrsUniquePtr<SrsSrtSource> source2(new SrsSrtSource());
    HELPER_EXPECT_SUCCESS(source2->initialize(&mock_req));

    // Create a new context ID
    SrsContextId new_id;
    new_id.set_value("test-source-id-123");

    // Change source ID
    HELPER_EXPECT_SUCCESS(source2->on_source_id_changed(new_id));

    // Verify source ID was updated
    EXPECT_EQ(0, source2->source_id().compare(new_id));

    // Verify pre_source_id was set to the first ID
    EXPECT_EQ(0, source2->pre_source_id().compare(new_id));

    // Test 6: on_source_id_changed() with same ID should do nothing
    SrsContextId same_id;
    same_id.set_value("test-source-id-123");
    HELPER_EXPECT_SUCCESS(source2->on_source_id_changed(same_id));

    // Source ID should remain unchanged
    EXPECT_EQ(0, source2->source_id().compare(new_id));

    // Test 7: on_source_id_changed() notifies consumers
    // Create consumers for the source
    ISrsSrtConsumer *consumer1 = NULL;
    ISrsSrtConsumer *consumer2 = NULL;
    HELPER_EXPECT_SUCCESS(source2->create_consumer(consumer1));
    HELPER_EXPECT_SUCCESS(source2->create_consumer(consumer2));

    // Change source ID again
    SrsContextId another_id;
    another_id.set_value("test-source-id-456");
    HELPER_EXPECT_SUCCESS(source2->on_source_id_changed(another_id));

    // Verify source ID was updated
    EXPECT_EQ(0, source2->source_id().compare(another_id));

    // Verify pre_source_id remains the first ID (not updated on subsequent changes)
    EXPECT_EQ(0, source2->pre_source_id().compare(new_id));

    // Verify consumers were notified (should_update_source_id_ flag set)
    SrsSrtConsumer *consumer1_impl = dynamic_cast<SrsSrtConsumer *>(consumer1);
    SrsSrtConsumer *consumer2_impl = dynamic_cast<SrsSrtConsumer *>(consumer2);
    EXPECT_TRUE(consumer1_impl != NULL);
    EXPECT_TRUE(consumer2_impl != NULL);
    EXPECT_TRUE(consumer1_impl->should_update_source_id_);
    EXPECT_TRUE(consumer2_impl->should_update_source_id_);

    // Cleanup consumers
    srs_freep(consumer1);
    srs_freep(consumer2);
}

// Test SrsSrtSource consumer management lifecycle
// This test covers the major use scenario: creating consumers, managing them, and destroying them
VOID TEST(SrsSrtSourceTest, ConsumerManagementLifecycle)
{
    srs_error_t err;

    // Create a mock request
    MockSrsRequest mock_req("__defaultVhost__", "live", "test_stream");

    // Create and initialize SRT source
    SrsUniquePtr<SrsSrtSource> source(new SrsSrtSource());
    HELPER_EXPECT_SUCCESS(source->initialize(&mock_req));

    // Test 1: source_id() and pre_source_id() - should return empty initially
    SrsContextId initial_source_id = source->source_id();
    SrsContextId initial_pre_source_id = source->pre_source_id();
    EXPECT_TRUE(initial_source_id.empty());
    EXPECT_TRUE(initial_pre_source_id.empty());

    // Test 2: create_consumer() - creates consumer and adds to list
    ISrsSrtConsumer *consumer1 = NULL;
    HELPER_EXPECT_SUCCESS(source->create_consumer(consumer1));
    EXPECT_TRUE(consumer1 != NULL);

    // Verify stream_die_at_ is reset to 0 when consumer is created
    EXPECT_EQ(0, source->stream_die_at_);

    // Test 3: consumer_dumps() - should succeed (just prints trace)
    HELPER_EXPECT_SUCCESS(source->consumer_dumps(consumer1));

    // Test 4: Create another consumer
    ISrsSrtConsumer *consumer2 = NULL;
    HELPER_EXPECT_SUCCESS(source->create_consumer(consumer2));
    EXPECT_TRUE(consumer2 != NULL);

    // Verify both consumers are in the list
    EXPECT_EQ(2, (int)source->consumers_.size());

    // Test 5: update_auth() - updates authentication info
    MockSrsRequest auth_req("__defaultVhost__", "live", "test_stream");
    auth_req.pageUrl_ = "http://example.com/page";
    auth_req.swfUrl_ = "http://example.com/swf";
    source->update_auth(&auth_req);

    // Verify auth was updated in the internal request
    EXPECT_STREQ(auth_req.pageUrl_.c_str(), source->req_->pageUrl_.c_str());
    EXPECT_STREQ(auth_req.swfUrl_.c_str(), source->req_->swfUrl_.c_str());

    // Test 6: set_bridge() - sets bridge and frees old one
    // Note: We don't create a real bridge here as it would require complex setup
    // Just verify the method can be called safely with NULL
    source->set_bridge(NULL);
    EXPECT_TRUE(source->srt_bridge_ == NULL);

    // Test 7: on_consumer_destroy() - removes consumer from list
    source->on_consumer_destroy(consumer1);

    // Verify consumer1 was removed
    EXPECT_EQ(1, (int)source->consumers_.size());

    // Verify stream_die_at_ is NOT set yet (still has one consumer)
    EXPECT_EQ(0, source->stream_die_at_);

    // Test 8: on_consumer_destroy() - removes last consumer and sets stream_die_at_
    source->on_consumer_destroy(consumer2);

    // Verify consumer2 was removed
    EXPECT_EQ(0, (int)source->consumers_.size());

    // Verify stream_die_at_ is set when last consumer is destroyed (and can_publish_ is true)
    EXPECT_TRUE(source->stream_die_at_ > 0);

    // Test 9: on_consumer_destroy() with non-existent consumer - should not crash
    ISrsSrtConsumer *fake_consumer = (ISrsSrtConsumer *)0x12345678;
    source->on_consumer_destroy(fake_consumer);

    // Should still have 0 consumers
    EXPECT_EQ(0, (int)source->consumers_.size());

    // Cleanup consumers
    srs_freep(consumer1);
    srs_freep(consumer2);
}

// Mock statistic implementation
MockSrtStatistic::MockSrtStatistic()
{
    on_stream_publish_count_ = 0;
    on_stream_close_count_ = 0;
    last_publisher_id_ = "";
    last_publish_req_ = NULL;
    last_close_req_ = NULL;
}

MockSrtStatistic::~MockSrtStatistic()
{
}

void MockSrtStatistic::on_disconnect(std::string id, srs_error_t err)
{
}

srs_error_t MockSrtStatistic::on_client(std::string id, ISrsRequest *req, ISrsExpire *conn, SrsRtmpConnType type)
{
    return srs_success;
}

srs_error_t MockSrtStatistic::on_video_info(ISrsRequest *req, SrsVideoCodecId vcodec, int avc_profile, int avc_level, int width, int height)
{
    return srs_success;
}

srs_error_t MockSrtStatistic::on_audio_info(ISrsRequest *req, SrsAudioCodecId acodec, SrsAudioSampleRate asample_rate, SrsAudioChannels asound_type, SrsAacObjectType aac_object)
{
    return srs_success;
}

void MockSrtStatistic::on_stream_publish(ISrsRequest *req, std::string publisher_id)
{
    on_stream_publish_count_++;
    last_publish_req_ = req;
    last_publisher_id_ = publisher_id;
}

void MockSrtStatistic::on_stream_close(ISrsRequest *req)
{
    on_stream_close_count_++;
    last_close_req_ = req;
}

void MockSrtStatistic::kbps_add_delta(std::string id, ISrsKbpsDelta *delta)
{
}

void MockSrtStatistic::kbps_sample()
{
}

srs_error_t MockSrtStatistic::on_video_frames(ISrsRequest *req, int nb_frames)
{
    return srs_success;
}

std::string MockSrtStatistic::server_id()
{
    return "mock_server_id";
}

std::string MockSrtStatistic::service_id()
{
    return "mock_service_id";
}

std::string MockSrtStatistic::service_pid()
{
    return "mock_pid";
}

SrsStatisticVhost *MockSrtStatistic::find_vhost_by_id(std::string vid)
{
    return NULL;
}

SrsStatisticStream *MockSrtStatistic::find_stream(std::string sid)
{
    return NULL;
}

SrsStatisticClient *MockSrtStatistic::find_client(std::string client_id)
{
    return NULL;
}

srs_error_t MockSrtStatistic::dumps_vhosts(SrsJsonArray *arr)
{
    return srs_success;
}

srs_error_t MockSrtStatistic::dumps_streams(SrsJsonArray *arr, int start, int count)
{
    return srs_success;
}

srs_error_t MockSrtStatistic::dumps_clients(SrsJsonArray *arr, int start, int count)
{
    return srs_success;
}

srs_error_t MockSrtStatistic::dumps_metrics(int64_t &send_bytes, int64_t &recv_bytes, int64_t &nstreams, int64_t &nclients, int64_t &total_nclients, int64_t &nerrs)
{
    send_bytes = 0;
    recv_bytes = 0;
    nstreams = 0;
    nclients = 0;
    total_nclients = 0;
    nerrs = 0;
    return srs_success;
}

void MockSrtStatistic::reset()
{
    on_stream_publish_count_ = 0;
    on_stream_close_count_ = 0;
    last_publisher_id_ = "";
    last_publish_req_ = NULL;
    last_close_req_ = NULL;
}

// Mock SRT bridge implementation
MockSrtBridge::MockSrtBridge()
{
    on_publish_count_ = 0;
    on_unpublish_count_ = 0;
    on_packet_count_ = 0;
    on_publish_error_ = srs_success;
    on_packet_error_ = srs_success;
}

MockSrtBridge::~MockSrtBridge()
{
    srs_freep(on_publish_error_);
    srs_freep(on_packet_error_);
}

srs_error_t MockSrtBridge::initialize(ISrsRequest *r)
{
    return srs_success;
}

srs_error_t MockSrtBridge::on_publish()
{
    on_publish_count_++;
    return srs_error_copy(on_publish_error_);
}

void MockSrtBridge::on_unpublish()
{
    on_unpublish_count_++;
}

srs_error_t MockSrtBridge::on_packet(SrsSrtPacket *packet)
{
    on_packet_count_++;
    return srs_error_copy(on_packet_error_);
}

void MockSrtBridge::set_on_publish_error(srs_error_t err)
{
    srs_freep(on_publish_error_);
    on_publish_error_ = srs_error_copy(err);
}

void MockSrtBridge::set_on_packet_error(srs_error_t err)
{
    srs_freep(on_packet_error_);
    on_packet_error_ = srs_error_copy(err);
}

void MockSrtBridge::reset()
{
    on_publish_count_ = 0;
    on_unpublish_count_ = 0;
    on_packet_count_ = 0;
    srs_freep(on_publish_error_);
    on_publish_error_ = srs_success;
    srs_freep(on_packet_error_);
    on_packet_error_ = srs_success;
}

// Mock SRT consumer implementation
MockSrtConsumer::MockSrtConsumer()
{
    enqueue_count_ = 0;
    enqueue_error_ = srs_success;
}

MockSrtConsumer::~MockSrtConsumer()
{
    srs_freep(enqueue_error_);
    for (int i = 0; i < (int)packets_.size(); i++) {
        srs_freep(packets_[i]);
    }
    packets_.clear();
}

srs_error_t MockSrtConsumer::enqueue(SrsSrtPacket *packet)
{
    enqueue_count_++;
    if (enqueue_error_ != srs_success) {
        srs_freep(packet);
        return srs_error_copy(enqueue_error_);
    }
    packets_.push_back(packet);
    return srs_success;
}

srs_error_t MockSrtConsumer::dump_packet(SrsSrtPacket **ppkt)
{
    return srs_success;
}

void MockSrtConsumer::wait(int nb_msgs, srs_utime_t timeout)
{
}

void MockSrtConsumer::set_enqueue_error(srs_error_t err)
{
    srs_freep(enqueue_error_);
    enqueue_error_ = srs_error_copy(err);
}

void MockSrtConsumer::reset()
{
    enqueue_count_ = 0;
    srs_freep(enqueue_error_);
    enqueue_error_ = srs_success;
    for (int i = 0; i < (int)packets_.size(); i++) {
        srs_freep(packets_[i]);
    }
    packets_.clear();
}

// Test SrsSrtSource publish/unpublish lifecycle
// This test covers the major use scenario: publishing a stream, then unpublishing it
VOID TEST(SrsSrtSourceTest, PublishUnpublishLifecycle)
{
    srs_error_t err;

    // Create a mock request
    MockSrsRequest req("test.vhost", "live", "livestream");

    // Create SRT source and initialize
    SrsUniquePtr<SrsSrtSource> source(new SrsSrtSource());
    HELPER_EXPECT_SUCCESS(source->initialize(&req));

    // Replace global stat with mock
    MockSrtStatistic mock_stat;
    ISrsStatistic *old_stat = source->stat_;
    source->stat_ = &mock_stat;

    // Test 1: can_publish() should return true initially
    EXPECT_TRUE(source->can_publish());

    // Test 2: on_publish() - should set can_publish_ to false and call stat->on_stream_publish
    HELPER_EXPECT_SUCCESS(source->on_publish());

    // Verify can_publish_ is now false
    EXPECT_FALSE(source->can_publish());

    // Verify stat->on_stream_publish was called
    EXPECT_EQ(1, mock_stat.on_stream_publish_count_);
    EXPECT_TRUE(mock_stat.last_publish_req_ != NULL);
    EXPECT_FALSE(mock_stat.last_publisher_id_.empty());

    // Test 3: on_unpublish() - should restore can_publish_ to true and call stat->on_stream_close
    source->on_unpublish();

    // Verify can_publish_ is now true
    EXPECT_TRUE(source->can_publish());

    // Verify stat->on_stream_close was called
    EXPECT_EQ(1, mock_stat.on_stream_close_count_);
    EXPECT_TRUE(mock_stat.last_close_req_ != NULL);

    // Verify stream_die_at_ is set (no consumers)
    EXPECT_TRUE(source->stream_die_at_ > 0);

    // Test 4: on_unpublish() when already unpublished - should be ignored
    srs_utime_t old_die_at = source->stream_die_at_;
    int old_close_count = mock_stat.on_stream_close_count_;

    source->on_unpublish();

    // Verify nothing changed
    EXPECT_EQ(old_close_count, mock_stat.on_stream_close_count_);
    EXPECT_EQ(old_die_at, source->stream_die_at_);

    // Restore global stat
    source->stat_ = old_stat;
}

// Test SrsSrtSource publish/unpublish with bridge
// This test covers the scenario with a bridge that needs to be notified
VOID TEST(SrsSrtSourceTest, PublishUnpublishWithBridge)
{
    srs_error_t err;

    // Create a mock request
    MockSrsRequest req("test.vhost", "live", "livestream");

    // Create SRT source and initialize
    SrsUniquePtr<SrsSrtSource> source(new SrsSrtSource());
    HELPER_EXPECT_SUCCESS(source->initialize(&req));

    // Replace global stat with mock
    MockSrtStatistic mock_stat;
    ISrsStatistic *old_stat = source->stat_;
    source->stat_ = &mock_stat;

    // Create and set mock bridge
    MockSrtBridge *mock_bridge = new MockSrtBridge();
    source->set_bridge(mock_bridge);

    // Test 1: on_publish() with bridge - should call bridge->on_publish()
    HELPER_EXPECT_SUCCESS(source->on_publish());

    // Verify bridge->on_publish was called
    EXPECT_EQ(1, mock_bridge->on_publish_count_);

    // Test 2: on_unpublish() with bridge - should call bridge->on_unpublish() and free bridge
    // Note: The bridge will be freed by on_unpublish(), so we can't check its state afterwards
    source->on_unpublish();

    // Verify bridge was freed (we can't check mock_bridge->on_unpublish_count_ because it's freed)
    EXPECT_TRUE(source->srt_bridge_ == NULL);

    // Restore global stat
    source->stat_ = old_stat;
}

// Test SrsSrtSource on_packet distribution to consumers and bridge
// This test covers the major use scenario: distributing packets to multiple consumers and bridge
VOID TEST(SrsSrtSourceTest, OnPacketDistribution)
{
    srs_error_t err;

    // Create a mock request
    MockSrsRequest req("test.vhost", "live", "livestream");

    // Create SRT source and initialize
    SrsUniquePtr<SrsSrtSource> source(new SrsSrtSource());
    HELPER_EXPECT_SUCCESS(source->initialize(&req));

    // Create mock consumers
    MockSrtConsumer *consumer1 = new MockSrtConsumer();
    MockSrtConsumer *consumer2 = new MockSrtConsumer();

    // Add consumers to source
    source->consumers_.push_back(consumer1);
    source->consumers_.push_back(consumer2);

    // Create and set mock bridge
    MockSrtBridge *mock_bridge = new MockSrtBridge();
    source->set_bridge(mock_bridge);

    // Create a test packet
    SrsUniquePtr<SrsSrtPacket> packet(new SrsSrtPacket());
    const char *test_data = "Test SRT Packet Data";
    packet->wrap((char *)test_data, strlen(test_data));

    // Test: on_packet should distribute to all consumers and bridge
    HELPER_EXPECT_SUCCESS(source->on_packet(packet.get()));

    // Verify both consumers received the packet
    EXPECT_EQ(1, consumer1->enqueue_count_);
    EXPECT_EQ(1, consumer2->enqueue_count_);
    EXPECT_EQ(1, (int)consumer1->packets_.size());
    EXPECT_EQ(1, (int)consumer2->packets_.size());

    // Verify packet data in consumers
    EXPECT_EQ(strlen(test_data), (size_t)consumer1->packets_[0]->size());
    EXPECT_EQ(0, memcmp(consumer1->packets_[0]->data(), test_data, strlen(test_data)));
    EXPECT_EQ(strlen(test_data), (size_t)consumer2->packets_[0]->size());
    EXPECT_EQ(0, memcmp(consumer2->packets_[0]->data(), test_data, strlen(test_data)));

    // Verify bridge received the packet
    EXPECT_EQ(1, mock_bridge->on_packet_count_);

    // Cleanup: Remove consumers from source before they are freed
    source->consumers_.clear();
    srs_freep(consumer1);
    srs_freep(consumer2);

    // Note: mock_bridge will be freed by source destructor
}

