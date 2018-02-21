#pragma once

#include <memory>

struct ReportingState;

typedef std::shared_ptr<ReportingState> ReportingStatePtr;

ReportingStatePtr init_reporting();
