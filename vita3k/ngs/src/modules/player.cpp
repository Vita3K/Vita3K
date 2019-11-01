#include <ngs/modules/player.h>

namespace emu::ngs::player {
    Module::Module() 
        : VoiceDefinition(emu::ngs::BUSS_NORMAL_PLAYER) {
    }

    void Module::process(Voice *voice) {
        
    }
};