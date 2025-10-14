//
// Copyright (c) 2013-2025 The SRS Authors
//
// SPDX-License-Identifier: MIT
//

#ifndef SRS_APP_INGEST_HPP
#define SRS_APP_INGEST_HPP

#include <srs_core.hpp>

#include <vector>

#include <srs_app_reload.hpp>
#include <srs_app_st.hpp>

class SrsFFMPEG;
class ISrsFFMPEG;
class SrsConfDirective;
class SrsPithyPrint;
class ISrsPithyPrint;
class ISrsAppFactory;
class ISrsAppConfig;

// The ingest ffmpeg interface.
class ISrsIngesterFFMPEG
{
public:
    ISrsIngesterFFMPEG();
    virtual ~ISrsIngesterFFMPEG();

public:
    virtual srs_error_t initialize(ISrsFFMPEG *ff, std::string v, std::string i) = 0;
    // The ingest uri, [vhost]/[ingest id]
    virtual std::string uri() = 0;
    // The alive in srs_utime_t.
    virtual srs_utime_t alive() = 0;
    virtual bool equals(std::string v, std::string i) = 0;
    virtual bool equals(std::string v) = 0;

public:
    virtual srs_error_t start() = 0;
    virtual void stop() = 0;
    virtual srs_error_t cycle() = 0;
    virtual void fast_stop() = 0;
    virtual void fast_kill() = 0;
};

// Ingester ffmpeg object.
class SrsIngesterFFMPEG : public ISrsIngesterFFMPEG
{
SRS_DECLARE_PRIVATE:
    std::string vhost_;
    std::string id_;
    ISrsFFMPEG *ffmpeg_;
    srs_utime_t starttime_;

public:
    SrsIngesterFFMPEG();
    virtual ~SrsIngesterFFMPEG();

public:
    virtual srs_error_t initialize(ISrsFFMPEG *ff, std::string v, std::string i);
    // The ingest uri, [vhost]/[ingest id]
    virtual std::string uri();
    // The alive in srs_utime_t.
    virtual srs_utime_t alive();
    virtual bool equals(std::string v, std::string i);
    virtual bool equals(std::string v);

public:
    virtual srs_error_t start();
    virtual void stop();
    virtual srs_error_t cycle();
    virtual void fast_stop();
    virtual void fast_kill();
};

// The ingest interface.
class ISrsIngester
{
public:
    ISrsIngester();
    virtual ~ISrsIngester();

public:
};

// Ingest file/stream/device,
// encode with FFMPEG(optional),
// push to SRS(or any RTMP server) over RTMP.
class SrsIngester : public ISrsIngester, public ISrsCoroutineHandler
{
SRS_DECLARE_PRIVATE:
    ISrsAppFactory *app_factory_;
    ISrsAppConfig *config_;

SRS_DECLARE_PRIVATE:
    std::vector<ISrsIngesterFFMPEG *> ingesters_;

SRS_DECLARE_PRIVATE:
    ISrsCoroutine *trd_;
    ISrsPithyPrint *pprint_;
    // Whether the ingesters are expired, for example, the listen port changed,
    // all ingesters must be restart.
    bool expired_;
    // Whether already disposed.
    bool disposed_;

public:
    SrsIngester();
    virtual ~SrsIngester();

public:
    virtual void dispose();

public:
    virtual srs_error_t start();
    virtual void stop();

SRS_DECLARE_PRIVATE:
    // Notify FFMPEG to fast stop.
    virtual void fast_stop();
    // When SRS quit, directly kill FFMPEG after fast stop.
    virtual void fast_kill();
    // Interface ISrsReusableThreadHandler.
public:
    virtual srs_error_t cycle();

SRS_DECLARE_PRIVATE:
    virtual srs_error_t do_cycle();

SRS_DECLARE_PRIVATE:
    virtual void clear_engines();
    virtual srs_error_t parse();
    virtual srs_error_t parse_ingesters(SrsConfDirective *vhost);
    virtual srs_error_t parse_engines(SrsConfDirective *vhost, SrsConfDirective *ingest);
    virtual srs_error_t initialize_ffmpeg(ISrsFFMPEG *ffmpeg, SrsConfDirective *vhost, SrsConfDirective *ingest, SrsConfDirective *engine);
    virtual void show_ingest_log_message();
};

#endif
