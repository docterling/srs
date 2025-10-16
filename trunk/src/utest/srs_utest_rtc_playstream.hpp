//
// Copyright (c) 2013-2025 The SRS Authors
//
// SPDX-License-Identifier: MIT
//

#ifndef SRS_UTEST_RTC_PLAYSTREAM_HPP
#define SRS_UTEST_RTC_PLAYSTREAM_HPP

#include <srs_utest.hpp>
#include <map>

class SrsRtcTrackDescription;

// Helper class to create mock track descriptions for testing
class MockRtcTrackDescriptionFactory
{
public:
    MockRtcTrackDescriptionFactory();
    virtual ~MockRtcTrackDescriptionFactory();

public:
    // Default SSRCs for audio and video tracks
    uint32_t audio_ssrc_;
    uint32_t video_ssrc_;

public:
    // Create a map of track descriptions with audio and video tracks
    std::map<uint32_t, SrsRtcTrackDescription *> create_audio_video_tracks();

    // Create a single audio track description
    SrsRtcTrackDescription *create_audio_track(uint32_t ssrc, std::string id);

    // Create a single video track description
    SrsRtcTrackDescription *create_video_track(uint32_t ssrc, std::string id);
};

#endif
