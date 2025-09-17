//
// Copyright (c) 2013-2025 The SRS Authors
//
// SPDX-License-Identifier: MIT
//

#include <srs_utest_app2.hpp>

#include <srs_app_rtc_source.hpp>
#include <srs_core_autofree.hpp>
#include <srs_kernel_buffer.hpp>
#include <srs_kernel_codec.hpp>
#include <srs_kernel_rtc_rtp.hpp>
#include <srs_protocol_format.hpp>
#include <srs_utest.hpp>

// External function declaration from srs_app_rtc_source.cpp
extern srs_error_t aac_raw_append_adts_header(SrsMediaPacket *shared_audio, SrsFormat *format, char **pbuf, int *pnn_buf);

// Mock implementation of ISrsRtcSourceForConsumer for testing SrsRtcConsumer
class MockRtcSourceForConsumer : public ISrsRtcSourceForConsumer
{
public:
    SrsContextId source_id_;
    SrsContextId pre_source_id_;
    int consumer_destroy_count_;
    ISrsRtcConsumer *last_destroyed_consumer_;

    MockRtcSourceForConsumer()
    {
        source_id_.set_value("test-source-id");
        pre_source_id_.set_value("test-pre-source-id");
        consumer_destroy_count_ = 0;
        last_destroyed_consumer_ = NULL;
    }

    virtual ~MockRtcSourceForConsumer()
    {
    }

    virtual void on_consumer_destroy(ISrsRtcConsumer *consumer)
    {
        consumer_destroy_count_++;
        last_destroyed_consumer_ = consumer;
    }

    virtual SrsContextId source_id()
    {
        return source_id_;
    }

    virtual SrsContextId pre_source_id()
    {
        return pre_source_id_;
    }
};

// Mock implementation of ISrsRtcSourceChangeCallback for testing
class MockRtcSourceChangeCallback : public ISrsRtcSourceChangeCallback
{
public:
    int stream_change_count_;
    SrsRtcSourceDescription *last_stream_desc_;

    MockRtcSourceChangeCallback()
    {
        stream_change_count_ = 0;
        last_stream_desc_ = NULL;
    }

    virtual ~MockRtcSourceChangeCallback()
    {
    }

    virtual void on_stream_change(SrsRtcSourceDescription *desc)
    {
        stream_change_count_++;
        last_stream_desc_ = desc;
    }
};

VOID TEST(AppTest2, AacRawAppendAdtsHeaderSequenceHeader)
{
    srs_error_t err;

    // Create a mock AAC sequence header packet
    SrsUniquePtr<SrsMediaPacket> audio_packet(new SrsMediaPacket());
    audio_packet->message_type_ = SrsFrameTypeAudio;

    // Create format with AAC sequence header
    SrsUniquePtr<SrsFormat> format(new SrsFormat());
    HELPER_EXPECT_SUCCESS(format->initialize());

    // Set up AAC codec info to simulate sequence header
    format->acodec_ = new SrsAudioCodecConfig();
    format->acodec_->id_ = SrsAudioCodecIdAAC;
    format->acodec_->aac_object_ = SrsAacObjectTypeAacLC;
    format->acodec_->aac_sample_rate_ = 4; // 44100 Hz index
    format->acodec_->aac_channels_ = 2;

    // Create audio frame info to simulate sequence header
    format->audio_ = new SrsParsedAudioPacket();
    format->audio_->dts_ = 1000;
    format->audio_->cts_ = 0;
    format->audio_->nb_samples_ = 0; // Sequence header has 0 samples

    char *buf = NULL;
    int nn_buf = 0;

    // Test: sequence header should return success without creating buffer
    HELPER_EXPECT_SUCCESS(aac_raw_append_adts_header(audio_packet.get(), format.get(), &buf, &nn_buf));
    EXPECT_TRUE(buf == NULL);
    EXPECT_EQ(0, nn_buf);
}

VOID TEST(AppTest2, AacRawAppendAdtsHeaderNoSamples)
{
    srs_error_t err;

    // Create a mock AAC packet
    SrsUniquePtr<SrsMediaPacket> audio_packet(new SrsMediaPacket());
    audio_packet->message_type_ = SrsFrameTypeAudio;

    // Create format with AAC but no samples
    SrsUniquePtr<SrsFormat> format(new SrsFormat());
    HELPER_EXPECT_SUCCESS(format->initialize());

    // Set up AAC codec info
    format->acodec_ = new SrsAudioCodecConfig();
    format->acodec_->id_ = SrsAudioCodecIdAAC;
    format->acodec_->aac_object_ = SrsAacObjectTypeAacLC;
    format->acodec_->aac_sample_rate_ = 4; // 44100 Hz index
    format->acodec_->aac_channels_ = 2;

    // Create audio frame info with no samples
    format->audio_ = new SrsParsedAudioPacket();
    format->audio_->dts_ = 1000;
    format->audio_->cts_ = 0;
    format->audio_->nb_samples_ = 0; // No samples

    char *buf = NULL;
    int nn_buf = 0;

    // Test: no samples should return success without creating buffer
    HELPER_EXPECT_SUCCESS(aac_raw_append_adts_header(audio_packet.get(), format.get(), &buf, &nn_buf));
    EXPECT_TRUE(buf == NULL);
    EXPECT_EQ(0, nn_buf);
}

VOID TEST(AppTest2, AacRawAppendAdtsHeaderMultipleSamples)
{
    srs_error_t err;

    // Create a mock AAC packet
    SrsUniquePtr<SrsMediaPacket> audio_packet(new SrsMediaPacket());
    audio_packet->message_type_ = SrsFrameTypeAudio;

    // Create format with AAC
    SrsUniquePtr<SrsFormat> format(new SrsFormat());
    HELPER_EXPECT_SUCCESS(format->initialize());

    // Set up AAC codec info
    format->acodec_ = new SrsAudioCodecConfig();
    format->acodec_->id_ = SrsAudioCodecIdAAC;
    format->acodec_->aac_object_ = SrsAacObjectTypeAacLC;
    format->acodec_->aac_sample_rate_ = 4; // 44100 Hz index
    format->acodec_->aac_channels_ = 2;

    // Create audio frame info with multiple samples (should fail)
    format->audio_ = new SrsParsedAudioPacket();
    format->audio_->dts_ = 1000;
    format->audio_->cts_ = 0;
    format->audio_->nb_samples_ = 2; // Multiple samples should cause error

    char *buf = NULL;
    int nn_buf = 0;

    // Test: multiple samples should return error
    HELPER_EXPECT_FAILED(aac_raw_append_adts_header(audio_packet.get(), format.get(), &buf, &nn_buf));
}

VOID TEST(AppTest2, AacRawAppendAdtsHeaderValidSingleSample)
{
    srs_error_t err;

    // Create a mock AAC packet
    SrsUniquePtr<SrsMediaPacket> audio_packet(new SrsMediaPacket());
    audio_packet->message_type_ = SrsFrameTypeAudio;

    // Create format with AAC
    SrsUniquePtr<SrsFormat> format(new SrsFormat());
    HELPER_EXPECT_SUCCESS(format->initialize());

    // Set up AAC codec info
    format->acodec_ = new SrsAudioCodecConfig();
    format->acodec_->id_ = SrsAudioCodecIdAAC;
    format->acodec_->aac_object_ = SrsAacObjectTypeAacLC; // Object type 2
    format->acodec_->aac_sample_rate_ = 4;                // 44100 Hz index
    format->acodec_->aac_channels_ = 2;                   // Stereo

    // Create audio frame info with single sample
    format->audio_ = new SrsParsedAudioPacket();
    format->audio_->dts_ = 1000;
    format->audio_->cts_ = 0;
    format->audio_->nb_samples_ = 1; // Single sample

    // Create sample data
    const char sample_data[] = {0x01, 0x02, 0x03, 0x04, 0x05}; // 5 bytes of sample data
    format->audio_->samples_[0].bytes_ = (char *)sample_data;
    format->audio_->samples_[0].size_ = sizeof(sample_data);

    char *buf = NULL;
    int nn_buf = 0;

    // Test: valid single sample should create ADTS header + data
    HELPER_EXPECT_SUCCESS(aac_raw_append_adts_header(audio_packet.get(), format.get(), &buf, &nn_buf));

    // Verify buffer was created
    EXPECT_TRUE(buf != NULL);
    EXPECT_EQ(7 + sizeof(sample_data), nn_buf); // 7 bytes ADTS header + sample data

    if (!buf) {
        return;
    }
    SrsUniquePtr<char> buf_ptr(buf);

    // Verify ADTS header structure
    EXPECT_EQ((unsigned char)0xFF, (unsigned char)buf[0]); // Sync word high
    EXPECT_EQ((unsigned char)0xF9, (unsigned char)buf[1]); // Sync word low + layer + protection

    // Verify AAC object type, sample rate, and channels are encoded correctly
    // Byte 2: (object-1)<<6 | (sample_rate&0x0F)<<2 | (channels&0x04)>>2
    unsigned char expected_byte2 = ((SrsAacObjectTypeAacLC - 1) << 6) | ((4 & 0x0F) << 2) | ((2 & 0x04) >> 2);
    EXPECT_EQ(expected_byte2, (unsigned char)buf[2]);

    // Verify frame length is encoded correctly (total length = 7 + sample size)
    int expected_frame_len = 7 + sizeof(sample_data);
    // Frame length spans bytes 3-5
    unsigned char expected_byte3 = ((2 & 0x03) << 6) | ((expected_frame_len >> 11) & 0x03);
    unsigned char expected_byte4 = (expected_frame_len >> 3) & 0xFF;
    unsigned char expected_byte5 = ((expected_frame_len & 0x07) << 5) | 0x1F;

    EXPECT_EQ(expected_byte3, (unsigned char)buf[3]);
    EXPECT_EQ(expected_byte4, (unsigned char)buf[4]);
    EXPECT_EQ(expected_byte5, (unsigned char)buf[5]);

    EXPECT_EQ((unsigned char)0xFC, (unsigned char)buf[6]); // Buffer fullness + frame count

    // Verify sample data is copied correctly
    for (int i = 0; i < sizeof(sample_data); i++) {
        EXPECT_EQ(sample_data[i], buf[7 + i]);
    }
}

VOID TEST(AppTest2, AacRawAppendAdtsHeaderDifferentCodecParams)
{
    srs_error_t err;

    // Create a mock AAC packet
    SrsUniquePtr<SrsMediaPacket> audio_packet(new SrsMediaPacket());
    audio_packet->message_type_ = SrsFrameTypeAudio;

    // Create format with different AAC parameters
    SrsUniquePtr<SrsFormat> format(new SrsFormat());
    HELPER_EXPECT_SUCCESS(format->initialize());

    // Set up AAC codec info with different parameters
    format->acodec_ = new SrsAudioCodecConfig();
    format->acodec_->id_ = SrsAudioCodecIdAAC;
    format->acodec_->aac_object_ = SrsAacObjectTypeAacHE; // Object type 5
    format->acodec_->aac_sample_rate_ = 3;                // 48000 Hz index
    format->acodec_->aac_channels_ = 1;                   // Mono

    // Create audio frame info with single sample
    format->audio_ = new SrsParsedAudioPacket();
    format->audio_->dts_ = 2000;
    format->audio_->cts_ = 0;
    format->audio_->nb_samples_ = 1;

    // Create sample data
    const char sample_data[] = {(char)0xAA, (char)0xBB, (char)0xCC}; // 3 bytes of sample data
    format->audio_->samples_[0].bytes_ = (char *)sample_data;
    format->audio_->samples_[0].size_ = sizeof(sample_data);

    char *buf = NULL;
    int nn_buf = 0;

    // Test: different codec parameters should work
    HELPER_EXPECT_SUCCESS(aac_raw_append_adts_header(audio_packet.get(), format.get(), &buf, &nn_buf));

    // Verify buffer was created with correct size
    EXPECT_TRUE(buf != NULL);
    EXPECT_EQ(7 + sizeof(sample_data), nn_buf);

    if (!buf) {
        return;
    }
    SrsUniquePtr<char> buf_ptr(buf);

    // Verify ADTS header with different parameters
    EXPECT_EQ((unsigned char)0xFF, (unsigned char)buf[0]);
    EXPECT_EQ((unsigned char)0xF9, (unsigned char)buf[1]);

    // Verify different AAC object type, sample rate, and channels
    unsigned char expected_byte2 = (unsigned char)(((SrsAacObjectTypeAacHE - 1) << 6) | ((3 & 0x0F) << 2) | ((1 & 0x04) >> 2));
    EXPECT_EQ(expected_byte2, (unsigned char)buf[2]);
}

VOID TEST(AppTest2, AacRawAppendAdtsHeaderLargeSample)
{
    srs_error_t err;

    // Create a mock AAC packet
    SrsUniquePtr<SrsMediaPacket> audio_packet(new SrsMediaPacket());
    audio_packet->message_type_ = SrsFrameTypeAudio;

    // Create format with AAC
    SrsUniquePtr<SrsFormat> format(new SrsFormat());
    HELPER_EXPECT_SUCCESS(format->initialize());

    // Set up AAC codec info
    format->acodec_ = new SrsAudioCodecConfig();
    format->acodec_->id_ = SrsAudioCodecIdAAC;
    format->acodec_->aac_object_ = SrsAacObjectTypeAacLC;
    format->acodec_->aac_sample_rate_ = 4; // 44100 Hz index
    format->acodec_->aac_channels_ = 2;

    // Create audio frame info with single large sample
    format->audio_ = new SrsParsedAudioPacket();
    format->audio_->dts_ = 1000;
    format->audio_->cts_ = 0;
    format->audio_->nb_samples_ = 1;

    // Create large sample data (1KB)
    const int large_size = 1024;
    SrsUniquePtr<char> large_sample(new char[large_size]);
    for (int i = 0; i < large_size; i++) {
        large_sample.get()[i] = (char)(i % 256);
    }

    format->audio_->samples_[0].bytes_ = large_sample.get();
    format->audio_->samples_[0].size_ = large_size;

    char *buf = NULL;
    int nn_buf = 0;

    // Test: large sample should work correctly
    HELPER_EXPECT_SUCCESS(aac_raw_append_adts_header(audio_packet.get(), format.get(), &buf, &nn_buf));

    // Verify buffer was created with correct size
    EXPECT_TRUE(buf != NULL);
    EXPECT_EQ(7 + large_size, nn_buf);

    if (!buf) {
        return;
    }
    SrsUniquePtr<char> buf_ptr(buf);

    // Verify ADTS header
    EXPECT_EQ((unsigned char)0xFF, (unsigned char)buf[0]);
    EXPECT_EQ((unsigned char)0xF9, (unsigned char)buf[1]);

    // Verify frame length encoding for large frame
    int expected_frame_len = 7 + large_size;
    unsigned char byte4 = (expected_frame_len >> 3) & 0xFF;
    EXPECT_EQ(byte4, (unsigned char)buf[4]);

    // Verify sample data is copied correctly (check first and last bytes)
    EXPECT_EQ(large_sample.get()[0], buf[7]);
    EXPECT_EQ(large_sample.get()[large_size - 1], buf[7 + large_size - 1]);
}

VOID TEST(AppTest2, RtcConsumerUpdateSourceId)
{
    MockRtcSourceForConsumer mock_source;
    SrsRtcConsumer consumer(&mock_source);

    // Initially should_update_source_id_ should be false
    consumer.update_source_id();

    // After calling update_source_id, the flag should be set
    // We can verify this by calling dump_packet which checks and resets the flag
    SrsRtpPacket *pkt = NULL;
    srs_error_t err;
    HELPER_EXPECT_SUCCESS(consumer.dump_packet(&pkt));
    EXPECT_TRUE(pkt == NULL); // No packets in queue
}

VOID TEST(AppTest2, RtcConsumerEnqueueAndDumpSinglePacket)
{
    srs_error_t err;
    MockRtcSourceForConsumer mock_source;
    SrsRtcConsumer consumer(&mock_source);

    // Create a test RTP packet
    SrsRtpPacket *test_pkt = new SrsRtpPacket();
    test_pkt->header_.set_sequence(1234);
    test_pkt->header_.set_timestamp(5678);
    test_pkt->header_.set_ssrc(0x12345678);

    // Enqueue the packet (consumer takes ownership)
    HELPER_EXPECT_SUCCESS(consumer.enqueue(test_pkt));

    // Dump the packet
    SrsRtpPacket *dumped_pkt = NULL;
    HELPER_EXPECT_SUCCESS(consumer.dump_packet(&dumped_pkt));

    EXPECT_TRUE(dumped_pkt != NULL);
    EXPECT_EQ(1234, dumped_pkt->header_.get_sequence());
    EXPECT_EQ(5678, dumped_pkt->header_.get_timestamp());
    EXPECT_EQ(0x12345678, dumped_pkt->header_.get_ssrc());

    srs_freep(dumped_pkt);
}

VOID TEST(AppTest2, RtcConsumerEnqueueMultiplePackets)
{
    srs_error_t err;
    MockRtcSourceForConsumer mock_source;
    SrsRtcConsumer consumer(&mock_source);

    // Create and enqueue multiple test packets
    for (int i = 0; i < 5; i++) {
        SrsRtpPacket *test_pkt = new SrsRtpPacket();
        test_pkt->header_.set_sequence(1000 + i);
        test_pkt->header_.set_timestamp(2000 + i * 100);
        test_pkt->header_.set_ssrc(0x12345678);

        HELPER_EXPECT_SUCCESS(consumer.enqueue(test_pkt));
    }

    // Dump packets in FIFO order
    for (int i = 0; i < 5; i++) {
        SrsRtpPacket *dumped_pkt = NULL;
        HELPER_EXPECT_SUCCESS(consumer.dump_packet(&dumped_pkt));

        EXPECT_TRUE(dumped_pkt != NULL);
        EXPECT_EQ(1000 + i, dumped_pkt->header_.get_sequence());
        EXPECT_EQ(2000 + i * 100, dumped_pkt->header_.get_timestamp());

        srs_freep(dumped_pkt);
    }

    // Queue should be empty now
    SrsRtpPacket *empty_pkt = NULL;
    HELPER_EXPECT_SUCCESS(consumer.dump_packet(&empty_pkt));
    EXPECT_TRUE(empty_pkt == NULL);
}

VOID TEST(AppTest2, RtcConsumerDumpEmptyQueue)
{
    srs_error_t err;
    MockRtcSourceForConsumer mock_source;
    SrsRtcConsumer consumer(&mock_source);

    // Dump from empty queue should return NULL
    SrsRtpPacket *pkt = NULL;
    HELPER_EXPECT_SUCCESS(consumer.dump_packet(&pkt));
    EXPECT_TRUE(pkt == NULL);
}

VOID TEST(AppTest2, RtcConsumerStreamChangeCallback)
{
    MockRtcSourceForConsumer mock_source;
    MockRtcSourceChangeCallback mock_callback;
    SrsRtcConsumer consumer(&mock_source);

    // Set the callback handler
    consumer.set_handler(&mock_callback);

    // Create a mock stream description
    SrsUniquePtr<SrsRtcSourceDescription> stream_desc(new SrsRtcSourceDescription());
    stream_desc->id_ = "test-stream";

    // Trigger stream change
    consumer.on_stream_change(stream_desc.get());

    // Verify callback was called
    EXPECT_EQ(1, mock_callback.stream_change_count_);
    EXPECT_EQ(stream_desc.get(), mock_callback.last_stream_desc_);
}

VOID TEST(AppTest2, RtcConsumerStreamChangeCallbackNoHandler)
{
    MockRtcSourceForConsumer mock_source;
    SrsRtcConsumer consumer(&mock_source);

    // No handler set - should not crash
    SrsUniquePtr<SrsRtcSourceDescription> stream_desc(new SrsRtcSourceDescription());
    stream_desc->id_ = "test-stream";

    // This should not crash even without handler
    consumer.on_stream_change(stream_desc.get());
}

VOID TEST(AppTest2, RtcConsumerQueueSizeCheck)
{
    srs_error_t err;
    MockRtcSourceForConsumer mock_source;
    SrsRtcConsumer consumer(&mock_source);

    // Enqueue packets and verify queue behavior
    for (int i = 0; i < 5; i++) {
        SrsRtpPacket *test_pkt = new SrsRtpPacket();
        test_pkt->header_.set_sequence(1000 + i);
        HELPER_EXPECT_SUCCESS(consumer.enqueue(test_pkt));
    }

    // Verify we can dump packets in correct order
    SrsRtpPacket *pkt = NULL;
    HELPER_EXPECT_SUCCESS(consumer.dump_packet(&pkt));
    EXPECT_TRUE(pkt != NULL);
    EXPECT_EQ(1000, pkt->header_.get_sequence());
    srs_freep(pkt);
}

VOID TEST(AppTest2, RtcConsumerEnqueueBehavior)
{
    srs_error_t err;
    MockRtcSourceForConsumer mock_source;
    SrsRtcConsumer consumer(&mock_source);

    // Enqueue multiple packets to test queue behavior
    SrsRtpPacket *test_pkt1 = new SrsRtpPacket();
    test_pkt1->header_.set_sequence(1001);
    HELPER_EXPECT_SUCCESS(consumer.enqueue(test_pkt1));

    SrsRtpPacket *test_pkt2 = new SrsRtpPacket();
    test_pkt2->header_.set_sequence(1002);
    HELPER_EXPECT_SUCCESS(consumer.enqueue(test_pkt2));

    SrsRtpPacket *test_pkt3 = new SrsRtpPacket();
    test_pkt3->header_.set_sequence(1003);
    HELPER_EXPECT_SUCCESS(consumer.enqueue(test_pkt3));

    // Verify packets are in queue in FIFO order
    SrsRtpPacket *pkt = NULL;
    HELPER_EXPECT_SUCCESS(consumer.dump_packet(&pkt));
    EXPECT_TRUE(pkt != NULL);
    EXPECT_EQ(1001, pkt->header_.get_sequence());
    srs_freep(pkt);

    HELPER_EXPECT_SUCCESS(consumer.dump_packet(&pkt));
    EXPECT_TRUE(pkt != NULL);
    EXPECT_EQ(1002, pkt->header_.get_sequence());
    srs_freep(pkt);
}

VOID TEST(AppTest2, RtcConsumerSourceIdUpdate)
{
    MockRtcSourceForConsumer mock_source;
    SrsRtcConsumer consumer(&mock_source);

    // Set different source IDs
    mock_source.source_id_.set_value("new-source-id");
    mock_source.pre_source_id_.set_value("old-source-id");

    // Trigger source ID update
    consumer.update_source_id();

    // Dump packet should log the source ID change
    SrsRtpPacket *pkt = NULL;
    srs_error_t err;
    HELPER_EXPECT_SUCCESS(consumer.dump_packet(&pkt));
    EXPECT_TRUE(pkt == NULL); // No packets in queue

    // After dump_packet, the update flag should be reset
    // Calling dump_packet again should not trigger another log
    HELPER_EXPECT_SUCCESS(consumer.dump_packet(&pkt));
    EXPECT_TRUE(pkt == NULL);
}

VOID TEST(AppTest2, RtcConsumerPacketMemoryManagement)
{
    srs_error_t err;
    MockRtcSourceForConsumer mock_source;
    SrsRtcConsumer consumer(&mock_source);

    // Create test packets and enqueue copies (simulating real usage)
    std::vector<SrsRtpPacket *> original_packets;
    for (int i = 0; i < 3; i++) {
        SrsRtpPacket *original_pkt = new SrsRtpPacket();
        original_pkt->header_.set_sequence(2000 + i);
        original_pkt->header_.set_timestamp(3000 + i * 100);
        original_packets.push_back(original_pkt);

        // Enqueue a copy (this is how it's used in real code)
        HELPER_EXPECT_SUCCESS(consumer.enqueue(original_pkt->copy()));
    }

    // Dump all packets and verify they have correct data
    for (int i = 0; i < 3; i++) {
        SrsRtpPacket *dumped_pkt = NULL;
        HELPER_EXPECT_SUCCESS(consumer.dump_packet(&dumped_pkt));

        EXPECT_TRUE(dumped_pkt != NULL);
        EXPECT_TRUE(dumped_pkt != original_packets[i]); // Should be different instances
        EXPECT_EQ(2000 + i, dumped_pkt->header_.get_sequence());
        EXPECT_EQ(3000 + i * 100, dumped_pkt->header_.get_timestamp());

        srs_freep(dumped_pkt);
    }

    // Clean up original packets
    for (size_t i = 0; i < original_packets.size(); i++) {
        srs_freep(original_packets[i]);
    }
}

VOID TEST(AppTest2, RtcConsumerCallbackHandlerLifecycle)
{
    MockRtcSourceForConsumer mock_source;
    MockRtcSourceChangeCallback callback1;
    MockRtcSourceChangeCallback callback2;
    SrsRtcConsumer consumer(&mock_source);

    // Initially no handler
    SrsUniquePtr<SrsRtcSourceDescription> stream_desc(new SrsRtcSourceDescription());
    stream_desc->id_ = "test-stream-1";
    consumer.on_stream_change(stream_desc.get());
    EXPECT_EQ(0, callback1.stream_change_count_);
    EXPECT_EQ(0, callback2.stream_change_count_);

    // Set first handler
    consumer.set_handler(&callback1);
    consumer.on_stream_change(stream_desc.get());
    EXPECT_EQ(1, callback1.stream_change_count_);
    EXPECT_EQ(0, callback2.stream_change_count_);

    // Change handler
    consumer.set_handler(&callback2);
    stream_desc->id_ = "test-stream-2";
    consumer.on_stream_change(stream_desc.get());
    EXPECT_EQ(1, callback1.stream_change_count_); // Should not increase
    EXPECT_EQ(1, callback2.stream_change_count_); // Should increase

    // Remove handler
    consumer.set_handler(NULL);
    consumer.on_stream_change(stream_desc.get());
    EXPECT_EQ(1, callback1.stream_change_count_); // Should not increase
    EXPECT_EQ(1, callback2.stream_change_count_); // Should not increase
}

VOID TEST(AppTest2, RtcConsumerLargePacketQueue)
{
    srs_error_t err;
    MockRtcSourceForConsumer mock_source;
    SrsRtcConsumer consumer(&mock_source);

    const int large_count = 100;

    // Enqueue many packets
    for (int i = 0; i < large_count; i++) {
        SrsRtpPacket *test_pkt = new SrsRtpPacket();
        test_pkt->header_.set_sequence(4000 + i);
        test_pkt->header_.set_timestamp(5000 + i * 10);
        test_pkt->header_.set_ssrc(0xABCDEF00 + i);

        HELPER_EXPECT_SUCCESS(consumer.enqueue(test_pkt));
    }

    // Dump all packets and verify order
    for (int i = 0; i < large_count; i++) {
        SrsRtpPacket *dumped_pkt = NULL;
        HELPER_EXPECT_SUCCESS(consumer.dump_packet(&dumped_pkt));

        EXPECT_TRUE(dumped_pkt != NULL);
        EXPECT_EQ(4000 + i, dumped_pkt->header_.get_sequence());
        EXPECT_EQ(5000 + i * 10, dumped_pkt->header_.get_timestamp());
        EXPECT_EQ(0xABCDEF00 + i, dumped_pkt->header_.get_ssrc());

        srs_freep(dumped_pkt);
    }

    // Queue should be empty
    SrsRtpPacket *empty_pkt = NULL;
    HELPER_EXPECT_SUCCESS(consumer.dump_packet(&empty_pkt));
    EXPECT_TRUE(empty_pkt == NULL);
}
