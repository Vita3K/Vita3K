#pragma once

#include <cstdint>

struct VarExport {
    uint32_t nid;
    const char* name;
    uint8_t* data;
    unsigned int size;
};

const char *import_name(uint32_t nid);

static VarExport hle_var_export[] = {
#define EXPORT_VAR(name, nid, value_type, value) \
    {  \
        nid, \
        #name, \
        reinterpret_cast<uint8_t*>(new value_type(value)), \
        sizeof(value_type) \
    },
#include <nids/nid_vars.h>
#undef EXPORT_VAR
};
