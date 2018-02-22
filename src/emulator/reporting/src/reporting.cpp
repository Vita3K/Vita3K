#include <reporting/functions.h>

#include "state.h"

ReportingStatePtr init_reporting() {
    return std::make_shared<ReportingState>();
}

void report_missing_shader(ReportingState &state, const char *hash, const char *glsl) {
}
