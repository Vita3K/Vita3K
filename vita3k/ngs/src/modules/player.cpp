#include <ngs/modules/player.h>

namespace emu::ngs::player {
    Module::Module() 
        : VoiceDefinition(emu::ngs::BUSS_NORMAL_PLAYER) {
    }

    void Module::process(const MemState &mem, Voice *voice) {
        Parameters *params = voice->get_parameters<Parameters>(mem);
        int a = 5;
    }
};