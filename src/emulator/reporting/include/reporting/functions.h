#pragma once

#include <memory>

struct ReportingState;

typedef std::shared_ptr<ReportingState> ReportingStatePtr;

ReportingStatePtr init_reporting();
void report_missing_shader(ReportingState &state, const char *hash, const char *glsl);
