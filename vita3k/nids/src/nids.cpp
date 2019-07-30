#include <nids/functions.h>

#define NID(name, nid) extern const char name_##name[] = #name;
#include <nids/nids.h>
#undef NID

const char *import_name(uint32_t nid) {
    switch (nid) {
#define NID(name, nid) \
    case nid:          \
        return name_##name;
#include <nids/nids.h>
#undef NID
    default:
        return "UNRECOGNISED";
    }
}
