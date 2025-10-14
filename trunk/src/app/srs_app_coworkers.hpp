//
// Copyright (c) 2013-2025 The SRS Authors
//
// SPDX-License-Identifier: MIT
//

#ifndef SRS_APP_COWORKERS_HPP
#define SRS_APP_COWORKERS_HPP

#include <srs_core.hpp>

#include <map>
#include <string>

class SrsJsonAny;
class ISrsRequest;
class SrsLiveSource;

// For origin cluster.
class SrsCoWorkers
{
SRS_DECLARE_PRIVATE:
    static SrsCoWorkers *instance_;

SRS_DECLARE_PRIVATE:
    std::map<std::string, ISrsRequest *> streams_;

SRS_DECLARE_PRIVATE:
    SrsCoWorkers();
    virtual ~SrsCoWorkers();

public:
    static SrsCoWorkers *instance();

public:
    virtual SrsJsonAny *dumps(std::string vhost, std::string coworker, std::string app, std::string stream);

SRS_DECLARE_PRIVATE:
    virtual ISrsRequest *find_stream_info(std::string vhost, std::string app, std::string stream);

public:
    virtual srs_error_t on_publish(ISrsRequest *r);
    virtual void on_unpublish(ISrsRequest *r);
};

#endif
