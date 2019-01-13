#pragma once

#include <cpu/common.h>
#include <memory>

class CPUInterface;
struct CPUState;

CPUInterfacePtr new_cpu(CPUBackend backend, Address pc, Address sp, bool log_code, CPUState *state);