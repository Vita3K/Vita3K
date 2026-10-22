from pathlib import Path
import subprocess
import argparse
import tempfile
parser = argparse.ArgumentParser()
parser.add_argument('--cxx', default='c++')
parser.add_argument('--sanitize', action='store_true')
args = parser.parse_args()
SANITIZERS = ['-fsanitize=address,undefined'] if args.sanitize else []
ROOT=Path(__file__).resolve().parents[3]
s=(ROOT/'vita3k/modules/SceAppMgr/SceAppMgr.cpp').read_text();start=s.index('EXPORT(SceInt32, _sceAppMgrAppParamGetString,');end=s.index('\nEXPORT(',start+1);body=s[start:end]
body=body.replace('EXPORT(SceInt32, _sceAppMgrAppParamGetString, int pid, int param, char *string, sceAppMgrAppParamGetStringOptParam *optParam)', 'int app_param(int pid, int param, char *string, sceAppMgrAppParamGetStringOptParam *optParam)')
prelude=r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#define TRACY_FUNC(...) do {} while(0)
#define STUBBED(...) do {} while(0)
#define RET_ERROR(x) (x)
constexpr int SCE_APPMGR_ERROR_INVALID_PARAMETER=-1;
constexpr int SCE_APPMGR_ERROR_INVALID_PARAMETER2=-2;
constexpr int SCE_APPMGR_ERROR_INVALID=-3;
struct sceAppMgrAppParamGetStringOptParam { uint32_t size; int32_t a,b,c; };
struct Environment { int sfo_handle; } emuenv;
static std::string returned;
static bool found=true;
static int selected;
namespace sfo { bool get_data_by_id(std::string &out,int,int param) { selected=param;out=returned;return found; } }
'''
tests=r'''
int main() {
    unsigned cases=0;
    for(const std::string value : {std::string(),std::string("WAW000001"),std::string(399,'A'),std::string(400,'B')}) {
        for(uint32_t size : {0u,1u,9u,10u,12u,256u,400u,401u}) {
            returned=value;found=true;std::array<unsigned char,512> bytes;bytes.fill(0xa5);
            sceAppMgrAppParamGetStringOptParam opt{size,0,0,0};
            const int result=app_param(0,12,reinterpret_cast<char *>(bytes.data()+16),&opt);
            if(value.size()<std::min(size,400u)) {
                assert(result==0);
                assert(!std::memcmp(bytes.data()+16,value.c_str(),value.size()+1));
                assert(std::all_of(bytes.begin()+16+value.size()+1,bytes.end(),[](unsigned char x){return x==0xa5;}));
            } else {
                assert(result==SCE_APPMGR_ERROR_INVALID_PARAMETER2);
                assert(std::all_of(bytes.begin(),bytes.end(),[](unsigned char x){return x==0xa5;}));
            }
            assert(std::all_of(bytes.begin(),bytes.begin()+16,[](unsigned char x){return x==0xa5;}));++cases;
        }
    }
    char out[32];sceAppMgrAppParamGetStringOptParam opt{sizeof(out),0,0,0};returned="WAW000001";
    assert(app_param(0,12,nullptr,&opt)==SCE_APPMGR_ERROR_INVALID_PARAMETER);++cases;
    assert(app_param(0,5,out,&opt)==SCE_APPMGR_ERROR_INVALID_PARAMETER2);++cases;
    assert(app_param(0,15,out,&opt)==SCE_APPMGR_ERROR_INVALID_PARAMETER2);++cases;
    found=false;assert(app_param(0,12,out,&opt)==SCE_APPMGR_ERROR_INVALID);++cases;found=true;
    assert(app_param(0,100,out,&opt)==0 && selected==6);++cases;
    assert(app_param(0,0x65,out,&opt)==0 && selected==9);++cases;
    std::printf("%u actual HLE string cases passed: NUL, limits, guards, missing data and aliases\n",cases);
}
'''
_temporary=tempfile.TemporaryDirectory(prefix='vita3k-app-param-');out=Path(_temporary.name);(out/'app-param-string-test.cpp').write_text(prelude+body+tests)
subprocess.run([args.cxx,'-std=c++23','-O1','-g',*SANITIZERS,str(out/'app-param-string-test.cpp'),'-o',str(out/'app-param-string-test')],check=True,timeout=120)
subprocess.run([str(out/'app-param-string-test')],check=True,timeout=10)
