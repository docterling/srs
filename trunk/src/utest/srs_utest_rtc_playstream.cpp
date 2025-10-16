//
// Copyright (c) 2013-2025 The SRS Authors
//
// SPDX-License-Identifier: MIT
//

#include <srs_utest_rtc_playstream.hpp>

#include <srs_app_rtc_conn.hpp>
#include <srs_app_rtc_source.hpp>
#include <srs_kernel_error.hpp>
#include <srs_utest_app.hpp>
#include <srs_utest_app6.hpp>

// MockRtcTrackDescriptionFactory implementation
MockRtcTrackDescriptionFactory::MockRtcTrackDescriptionFactory()
{
    audio_ssrc_ = 12345;
    video_ssrc_ = 67890;
}

MockRtcTrackDescriptionFactory::~MockRtcTrackDescriptionFactory()
{
}

std::map<uint32_t, SrsRtcTrackDescription *> MockRtcTrackDescriptionFactory::create_audio_video_tracks()
{
    std::map<uint32_t, SrsRtcTrackDescription *> sub_relations;

    // Create audio track
    SrsRtcTrackDescription *audio_desc = create_audio_track(audio_ssrc_, "audio-track-1");
    sub_relations[audio_desc->ssrc_] = audio_desc;

    // Create video track
    SrsRtcTrackDescription *video_desc = create_video_track(video_ssrc_, "video-track-1");
    sub_relations[video_desc->ssrc_] = video_desc;

    return sub_relations;
}

SrsRtcTrackDescription *MockRtcTrackDescriptionFactory::create_audio_track(uint32_t ssrc, std::string id)
{
    SrsRtcTrackDescription *audio_desc = new SrsRtcTrackDescription();
    audio_desc->type_ = "audio";
    audio_desc->ssrc_ = ssrc;
    audio_desc->id_ = id;
    audio_desc->is_active_ = true;
    audio_desc->direction_ = "sendrecv";
    audio_desc->mid_ = "0";
    audio_desc->media_ = new SrsAudioPayload(111, "opus", 48000, 2);
    return audio_desc;
}

SrsRtcTrackDescription *MockRtcTrackDescriptionFactory::create_video_track(uint32_t ssrc, std::string id)
{
    SrsRtcTrackDescription *video_desc = new SrsRtcTrackDescription();
    video_desc->type_ = "video";
    video_desc->ssrc_ = ssrc;
    video_desc->id_ = id;
    video_desc->is_active_ = true;
    video_desc->direction_ = "sendrecv";
    video_desc->mid_ = "1";
    video_desc->media_ = new SrsVideoPayload(96, "H264", 90000);
    return video_desc;
}

// Test SrsRtcPlayStream::start() - Basic success scenario
VOID TEST(RtcPlayStreamTest, ManuallyVerifyBasicWorkflow)
{
    srs_error_t err;

    // Create mock objects for dependencies
    MockAppConfig mock_config;
    MockRtcSourceManager mock_rtc_sources;
    MockRtcStatistic mock_stat;
    MockRtcAsyncCallRequest mock_request("test.vhost", "live", "stream1");
    MockRtcAsyncTaskExecutor mock_exec;
    MockExpire mock_expire;
    MockRtcPacketSender mock_sender;
    MockRtcTrackDescriptionFactory track_factory;
    SrsContextId cid;
    cid.set_value("test-play-stream-start-cid");

    // Create RTC play stream - uses real app_factory_ and real pli_worker_
    SrsUniquePtr<SrsRtcPlayStream> play_stream(new SrsRtcPlayStream(&mock_exec, &mock_expire, &mock_sender, cid));

    // Mock the play stream object.
    if (true) {
        // Inject mock dependencies
        play_stream->config_ = &mock_config;
        play_stream->rtc_sources_ = &mock_rtc_sources;
        play_stream->stat_ = &mock_stat;

        // Set mw_msgs to 0 to make the consumer block until we push a packet
        mock_config.mw_msgs_ = 0;
    }

    // Create track descriptions using factory
    if (true) {
        std::map<uint32_t, SrsRtcTrackDescription *> sub_relations = track_factory.create_audio_video_tracks();

        // Initialize the play stream (it will take ownership of track descriptions)
        HELPER_EXPECT_SUCCESS(play_stream->initialize(&mock_request, sub_relations));

        // Test: First call to start() should succeed
        HELPER_EXPECT_SUCCESS(play_stream->start());

        // Verify is_started_ flag is set
        EXPECT_TRUE(play_stream->is_started_);

        // Wait for coroutine to start and create consumer. Normally it should be ready 
        // and stopped at wait for RTP packets from consumer.
        srs_usleep(1 * SRS_UTIME_MILLISECONDS);
    }

    // Push a video packet to the source to feed the consumer.
    if (true) {
        SrsUniquePtr<SrsRtpPacket> test_pkt(new SrsRtpPacket());
        test_pkt->frame_type_ = SrsFrameTypeVideo;
        test_pkt->header_.set_sequence(1000);
        test_pkt->header_.set_timestamp(5000);
        test_pkt->header_.set_ssrc(track_factory.video_ssrc_);

        // Push packet to source - this will feed all consumers including the play stream's consumer
        HELPER_EXPECT_SUCCESS(play_stream->source_->on_rtp(test_pkt.get()));
    }

    // Verify the video packet is sent out
    if (true) {
        // Wait for coroutine to process the packet
        srs_usleep(1 * SRS_UTIME_MILLISECONDS);

        // Check sender should have received the packet
        EXPECT_EQ(mock_sender.send_packet_count_, 1);
        // Check the tracks, should be a video track.
        EXPECT_EQ(play_stream->video_tracks_.size(), 1);
        // The packet should create a cached track for this ssrc.
        EXPECT_EQ(play_stream->cache_ssrc0_, track_factory.video_ssrc_);
        EXPECT_TRUE(play_stream->cache_track0_ != NULL);
        // The packet should be in the nack ring buffer.
        SrsRtpPacket *pkt = play_stream->cache_track0_->rtp_queue_->at(1000);
        EXPECT_TRUE(pkt != NULL);
        EXPECT_EQ(pkt->header_.get_ssrc(), track_factory.video_ssrc_);
    }

    // Push a audio packet to the source to feed the consumer.
    if (true) {
        SrsUniquePtr<SrsRtpPacket> test_pkt(new SrsRtpPacket());
        test_pkt->frame_type_ = SrsFrameTypeAudio;
        test_pkt->header_.set_sequence(1000);
        test_pkt->header_.set_timestamp(5000);
        test_pkt->header_.set_ssrc(track_factory.audio_ssrc_);

        // Push packet to source - this will feed all consumers including the play stream's consumer
        HELPER_EXPECT_SUCCESS(play_stream->source_->on_rtp(test_pkt.get()));
    }

    // Verify the audio packet is sent out
    if (true) {
        // Wait for coroutine to process the packet
        srs_usleep(1 * SRS_UTIME_MILLISECONDS);

        // Check sender should have received the packet
        EXPECT_EQ(mock_sender.send_packet_count_, 2);
        // Check the tracks, should be a video track.
        EXPECT_EQ(play_stream->audio_tracks_.size(), 1);
        // The packet should create a cached track for this ssrc.
        EXPECT_EQ(play_stream->cache_ssrc1_, track_factory.audio_ssrc_);
        EXPECT_TRUE(play_stream->cache_track1_ != NULL);
        // The packet should be in the nack ring buffer.
        SrsRtpPacket *pkt = play_stream->cache_track1_->rtp_queue_->at(1000);
        EXPECT_TRUE(pkt != NULL);
        EXPECT_EQ(pkt->header_.get_ssrc(), track_factory.audio_ssrc_);
    }

    // Stop the play stream
    play_stream->stop();

    // Clean up - set to NULL to avoid double-free
    play_stream->config_ = NULL;
    play_stream->rtc_sources_ = NULL;
    play_stream->stat_ = NULL;
}
