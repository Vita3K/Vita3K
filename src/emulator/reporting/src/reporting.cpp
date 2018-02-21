#include <reporting/functions.h>

#include "state.h"

ReportingStatePtr init_reporting() {
    return std::make_shared<ReportingState>();
}
