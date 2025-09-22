//
// Copyright (c) 2013-2025 The SRS Authors
//
// SPDX-License-Identifier: MIT
//

#ifndef SRS_UTEST_APP7_HPP
#define SRS_UTEST_APP7_HPP

/*
#include <srs_utest_app7.hpp>
*/
#include <srs_utest.hpp>

#include <srs_app_rtc_conn.hpp>
#include <srs_app_rtc_source.hpp>
#include <srs_utest_app6.hpp>

// Mock video recv track for testing check_send_nacks
class MockRtcVideoRecvTrackForNack : public SrsRtcVideoRecvTrack
{
public:
    srs_error_t check_send_nacks_error_;
    int check_send_nacks_count_;

public:
    MockRtcVideoRecvTrackForNack(ISrsRtcPacketReceiver *receiver, SrsRtcTrackDescription *track_desc);
    virtual ~MockRtcVideoRecvTrackForNack();

public:
    virtual srs_error_t check_send_nacks();
    void set_check_send_nacks_error(srs_error_t err);
    void reset();
};

// Mock audio recv track for testing check_send_nacks
class MockRtcAudioRecvTrackForNack : public SrsRtcAudioRecvTrack
{
public:
    srs_error_t check_send_nacks_error_;
    int check_send_nacks_count_;

public:
    MockRtcAudioRecvTrackForNack(ISrsRtcPacketReceiver *receiver, SrsRtcTrackDescription *track_desc);
    virtual ~MockRtcAudioRecvTrackForNack();

public:
    virtual srs_error_t check_send_nacks();
    void set_check_send_nacks_error(srs_error_t err);
    void reset();
};

#endif
