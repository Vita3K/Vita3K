#include <ngs/ngs.h>
#include <mem/mem.h>

namespace emu::ngs {
    System::System(const Ptr<void> memspace, const std::uint32_t memspace_size)
        : max_voices(0)
        , granularity(0)
        , sample_rate(0)
        , memspace(memspace)
        , alloc(memspace_size) {

    }
    
    bool init(const MemState &mem, SystemInitParameters *parameters, Ptr<void> memspace, const std::uint32_t memspace_size) {
        // Reserve first memory allocation for our System struct
        System *sys = memspace.cast<System>().get(mem);
        sys = new (sys) System(memspace, memspace_size);

        sys->racks.resize(parameters->max_racks);

        sys->max_voices = parameters->max_voices;
        sys->granularity = parameters->granularity;
        sys->sample_rate = parameters->sample_rate;

        // Alloc first block for System struct
        sys->alloc.alloc(sizeof(System));

        return true;
    }
}