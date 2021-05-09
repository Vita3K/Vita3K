#include <nids/functions.h>

#define VAR_NID(name, nid) extern const char name_##name[] = #name;
#define NID(name, nid) extern const char name_##name[] = #name;
#include <nids/nids.inc>
#undef NID
#undef VAR_NID

const char *import_name(uint32_t nid) {
    switch (nid) {
#define VAR_NID(name, nid) \
    case nid:              \
        return name_##name;
#define NID(name, nid) \
    case nid:          \
        return name_##name;
#include <nids/nids.inc>
#undef NID
#undef VAR_NID
    default:
        return "UNRECOGNISED";
    }
}
