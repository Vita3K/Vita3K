from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[3]
source=(ROOT/'vita3k/modules/SceAvPlayer/SceAvPlayer.cpp').read_text()
def export(name):
    start=source.index('EXPORT(',source.index('EXPORT('))
    for part in source.split('\nEXPORT(')[1:]:
        if ', '+name+',' in part.split('{',1)[0]:
            return 'EXPORT('+part.split('\nEXPORT(',1)[0].rstrip()+'\n'
    raise ValueError(name)
structs=source[source.index('struct SceAvPlayerAudio {'):source.index('enum SceAvPlayerErrorCode')]
program=r"""#include <codec/state.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
using SceUID=int; using SceSize=uint32_t;
template<class T> struct Ptr {T *pointer=nullptr; T *get(auto &) const {return pointer;} explicit operator bool() const {return pointer!=nullptr;} };
"""+structs+r"""
struct PlayerInfoState {PlayerState player; std::mutex mutex; bool do_loop=false,paused=false; uint64_t last_frame_time=0;};
using PlayerPtr=std::shared_ptr<PlayerInfoState>;
struct AvPlayerState {std::mutex mutex;std::unordered_map<int,PlayerPtr> players;};
struct Memory {std::vector<uint8_t> bytes; unsigned allocations=0;};
struct Store {std::shared_ptr<AvPlayerState> state=std::make_shared<AvPlayerState>(); template<class T> std::shared_ptr<T> get(){return state;}};
struct Kernel {Store obj_store;};
struct Env {Kernel kernel;Memory mem;} emuenv;
static PlayerPtr lock_and_find(int id,auto &players,std::mutex &mutex) {std::lock_guard lock(mutex);auto it=players.find(id);return it==players.end()?nullptr:it->second;}
static Ptr<uint8_t> get_buffer(const PlayerPtr &,MediaType,Memory &memory,uint32_t size,bool=true){memory.bytes.resize(size);++memory.allocations;return {memory.bytes.data()};}
static std::atomic<uint64_t> ticks{0};
static uint64_t current_time(){return ticks;}
constexpr bool REJECT_DATA_ON_PAUSE=true,CATCHUP_VIDEO_PLAYBACK=true;
constexpr int SCE_AVPLAYER_ERROR_INVALID_ARGUMENT=-2;
#define RET_ERROR(x) (x)
#define STUBBED(...) 0
#define EXPORT(ret,name,...) ret name(__VA_ARGS__)
"""+export('sceAvPlayerGetVideoData')+export('sceAvPlayerGetAudioData')+export('sceAvPlayerSetLooping')+r"""
int main(int argc,char **argv){
    assert(argc==2);
    auto p=std::make_shared<PlayerInfoState>();
    emuenv.kernel.obj_store.state->players[1]=p;
    p->player.queue(argv[1]);
    assert(sceAvPlayerSetLooping(1,true)==0);
    assert(p->player.looping);
    assert(sceAvPlayerSetLooping(1,false)==0);
    assert(!p->player.looping);
    SceAvPlayerFrameInfo frame,before;
    std::memset(&frame,0xcd,sizeof(frame));before=frame;
    p->last_frame_time=1000000;ticks=1001000;
    assert(!sceAvPlayerGetVideoData(1,&frame));
    assert(std::memcmp(&frame,&before,sizeof(frame))==0);
    assert(emuenv.mem.allocations==0);
    ticks=1100000;
    assert(sceAvPlayerGetVideoData(1,&frame));
    assert(frame.timestamp==0 && frame.stream_details.video.width==64 && frame.stream_details.video.height==48);
    ticks=1141667;
    assert(sceAvPlayerGetVideoData(1,&frame));assert(frame.timestamp==42);
    assert(sceAvPlayerGetAudioData(1,&frame));assert(frame.timestamp==0);
    assert(sceAvPlayerGetAudioData(1,&frame));assert(frame.timestamp==21);
    assert(!sceAvPlayerGetAudioData(1,nullptr));
    assert(!sceAvPlayerGetVideoData(1,nullptr));
    assert(!sceAvPlayerGetVideoData(9,&frame));
    assert(!sceAvPlayerGetAudioData(9,&frame));
    assert(sceAvPlayerSetLooping(9,true)==SCE_AVPLAYER_ERROR_INVALID_ARGUMENT);
    p->paused=true;assert(!sceAvPlayerGetVideoData(1,&frame));assert(!sceAvPlayerGetAudioData(1,&frame));
    p->paused=false;p->player.free_video();
    before=frame;const auto allocations=emuenv.mem.allocations;ticks+=100000;
    assert(!sceAvPlayerGetVideoData(1,&frame));assert(!sceAvPlayerGetAudioData(1,&frame));
    assert(allocations==emuenv.mem.allocations);assert(std::memcmp(&frame,&before,sizeof(frame))==0);
    p->player.queue(argv[1]);
    std::atomic<unsigned> video_count{0},audio_count{0};
    auto active=[&] {std::lock_guard lock(p->mutex);return !p->player.video_playing.empty();};
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    std::thread video_thread([&] {
        SceAvPlayerFrameInfo info{};
        while(active() && std::chrono::steady_clock::now()<deadline) {
            ticks+=41667;
            if(sceAvPlayerGetVideoData(1,&info)) ++video_count;
            std::this_thread::yield();
        }
    });
    std::thread audio_thread([&] {
        SceAvPlayerFrameInfo info{};
        while(active() && std::chrono::steady_clock::now()<deadline) {
            if(sceAvPlayerGetAudioData(1,&info)) ++audio_count;
            std::this_thread::yield();
        }
    });
    video_thread.join();audio_thread.join();
    assert(!active());assert(video_count==24);assert(audio_count==47);
    std::printf("Concurrent HLE polling: video=%u audio=%u, fully drained\n",video_count.load(),audio_count.load());
    std::puts("PASS: actual AvPlayer exports: looping, pacing, per-stream timestamps, EOF, paused/null/invalid-handle guards");
}
"""
Path(sys.argv[1]).write_text(program)
