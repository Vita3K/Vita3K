// Vita3K emulator project; GPL-2.0-or-later.
#include <mem/functions.h>
#include <mem/state.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <iostream>
#include <array>
#include <atomic>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <sys/mman.h>
#ifdef BEFORE_FIX
#define add_cache_protect add_protect
static constexpr bool allocation_check = false;
#else
static constexpr bool allocation_check = true;
#endif
static thread_local bool forbid_allocations = false;
void *operator new(std::size_t size) {
    if (forbid_allocations) throw std::bad_alloc();
    if (auto pointer = std::malloc(size ? size : 1)) return pointer;
    throw std::bad_alloc();
}
void *operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void *pointer) noexcept {std::free(pointer);}
void operator delete[](void *pointer) noexcept {std::free(pointer);}
void operator delete(void *pointer,std::size_t) noexcept {std::free(pointer);}
void operator delete[](void *pointer,std::size_t) noexcept {std::free(pointer);}
static void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
static vm_prot_t permission(const void *pointer) {
    mach_vm_address_t address = reinterpret_cast<mach_vm_address_t>(pointer);
    mach_vm_size_t size = 0;
    vm_region_basic_info_data_64_t info{};
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object = MACH_PORT_NULL;
    require(mach_vm_region(mach_task_self(), &address, &size, VM_REGION_BASIC_INFO_64,
        reinterpret_cast<vm_region_info_t>(&info), &count, &object) == KERN_SUCCESS, "query permission");
    if (object != MACH_PORT_NULL) mach_port_deallocate(mach_task_self(), object);
    return info.protection;
}
struct Fixture {
    MemState mem;
    static constexpr Address base=0x60d70000;
    explicit Fixture(bool mapping=false, uint32_t pages=16) { require(init(mem,mapping), "init"); require(try_alloc_at(mem,base,pages*mem.host_page_size,"cache protection fixture")==base,"allocate"); }
    ~Fixture() {deinit_mem(mem);}
    void write(Address address, uint8_t value=0x71) { *reinterpret_cast<volatile uint8_t *>(mem.memory.get()+address)=value; }
    uint8_t read(Address address) {return *reinterpret_cast<volatile uint8_t *>(mem.memory.get()+address);}
    void cache(Address address,uint32_t size,MemPerm perm,std::atomic<unsigned> &count) {
        require(add_cache_protect(mem,address,size,perm,[&count](Address,bool){++count;return true;}),"register cache");
    }
};
int main() try {
    unsigned passed=0;
    // Ordinary direct access is meaningful on both 4KiB and 16KiB hosts.
    {
        Fixture f; std::atomic<unsigned> count{0};
        f.cache(f.base,4096,MemPerm::ReadOnly,count);f.write(f.base);
        require(count==1&&!is_protecting(f.mem,f.base),"direct cache write");++passed;
    }
    Fixture size_probe;const auto page=size_probe.mem.host_page_size;
    if(page<16384) {std::cout<<"cache_protection_cases="<<passed<<" shared_page_cases=SKIPPED_NEED_16K\n";return 0;}
    for(int direction=0;direction<2;++direction) {
        Fixture f;std::atomic<unsigned> count{0};
        const auto protected_address=f.base+(direction?12288:4096);
        f.cache(protected_address,4096,MemPerm::ReadOnly,count);
        require(permission(f.mem.memory.get()+f.base)==VM_PROT_READ,"initial RO");
        forbid_allocations=allocation_check;
        f.write(f.base+0x2800);
        forbid_allocations=false;
        require(count==1,"neighbor write did not invalidate cache");
        require(!is_protecting(f.mem,protected_address),"invalidated cache registration retained");
        require(permission(f.mem.memory.get()+f.base)==(VM_PROT_READ|VM_PROT_WRITE),"neighbor page permission");
        f.write(protected_address);require(count==1,"retired callback repeated");++passed;
    }
    // Both sides of a shared page are notified even for an ordinary exact fault.
    {
        Fixture f;std::atomic<unsigned> ro{0},none{0};
        f.cache(f.base+4096,4096,MemPerm::ReadOnly,ro);
        f.cache(f.base+8192,4096,MemPerm::None,none);
        (void)f.read(f.base+8192);
        require(ro==1&&none==1&&f.mem.protect_tree.empty(),"mixed neighboring cache callbacks lost");
        f.write(f.base+4096);require(ro==1,"mixed retired RO callback repeated");++passed;
    }
    // Retiring a long cache restores the watch on a page not being opened.
    {
        Fixture f;std::atomic<unsigned> count{0};
        f.cache(f.base+4096,page+4096,MemPerm::ReadOnly,count);
        const auto destination=f.base+page+0x2800;
        const auto token=capture_mem_allocation(f.mem,destination,16);
        auto watch=begin_gpu_readback_watch(f.mem,token,destination,16,1,16);
        require(bool(watch),"long cache watch");f.write(f.base+0x800);
        require(count==1&&!watch->invalidated&&permission(f.mem.memory.get()+destination)==VM_PROT_NONE,"distant page watch not preserved");
        f.write(destination,0x5a);require(watch->invalidated,"distant CPU store did not cancel watch");const uint8_t pixels[16]={1};
        require(!write_gpu_readback_rows(f.mem,destination,pixels,16,1,16,&token,watch.get())&&f.read(destination)==0x5a,"stale expanded-page copy overwrote CPU");
        finish_gpu_readback_watch(f.mem,watch);++passed;
    }
    // Distant registrations overlapping a retired long cache stay protected.
    {
        Fixture f;std::atomic<unsigned> a{0},b{0},c{0};
        f.cache(f.base,2*page,MemPerm::ReadOnly,a);
        f.cache(f.base+page,3*page,MemPerm::ReadOnly,b);
        f.cache(f.base+3*page,2*page,MemPerm::ReadOnly,c);
        unprotect_inner(f.mem,f.base+3*page+0x100,1);
        require(a==0&&b==1&&c==1&&f.mem.protect_tree.size()==1,"distant cache was unnecessarily retired");
        require(permission(f.mem.memory.get()+f.base)==VM_PROT_READ&&permission(f.mem.memory.get()+f.base+page)==VM_PROT_READ&&permission(f.mem.memory.get()+f.base+4*page)==(VM_PROT_READ|VM_PROT_WRITE),"remaining cache permissions not restored");
        f.write(f.base+page);require(a==1,"remaining cache write callback lost");++passed;
    }
    // Collateral invalidation must not synthesize a guest abort at another address.
    {
        Fixture f;std::atomic<unsigned> cache{0},guest{0};
        require(add_protect(f.mem,f.base+12288,4096,MemPerm::ReadOnly,[&](Address,bool){++guest;return true;}),"guest protection");
        f.cache(f.base+4096,4096,MemPerm::ReadOnly,cache);
        f.write(f.base+0x2800);
        require(cache==1&&guest==0&&is_protecting(f.mem,f.base+12288),"collateral cache invalidation fired guest callback");++passed;
        // Sub-host-page guest permissions retain their pre-existing coarse-page
        // limitation; this test does not claim precise 4KiB guest enforcement.
    }
    // Mixed merged segments retain the guest range and permission metadata.
    {
        Fixture f;std::atomic<unsigned> cache{0},guest{0};
        require(add_protect(f.mem,f.base+8192,8192,MemPerm::ReadOnly,[&](Address,bool){++guest;return true;}),"merged guest");
        f.cache(f.base+4096,8192,MemPerm::None,cache);
        forbid_allocations=allocation_check;
        unprotect_inner(f.mem,f.base+0x100,1);
        forbid_allocations=false;
        require(cache==1&&guest==0&&f.mem.protect_tree.size()==1,"mixed segment retirement");
        require(!is_protecting(f.mem,f.base+4096)&&is_protecting(f.mem,f.base+8192),"mixed segment bounds not restored");
        require(f.mem.protect_tree.begin()->first==f.base+8192&&f.mem.protect_tree.begin()->second.size==8192&&f.mem.protect_tree.begin()->second.perm==MemPerm::ReadOnly,"mixed segment metadata");++passed;
    }
    // Genuine guest callbacks keep the actual fault address and access kind.
    {
        Fixture f;std::atomic<unsigned> guest{0};std::atomic<Address> observed{0};std::atomic<bool> write{false};
        require(add_protect(f.mem,f.base+4096,4096,MemPerm::ReadOnly,[&](Address a,bool w){++guest;observed=a;write=w;return true;}),"direct guest");
        f.write(f.base+0x1100);
        require(guest==1&&observed==f.base+0x1100&&write,"guest direct fault changed");++passed;
    }
    // A watch on the faulting host page must cancel before its cache retires.
    {
        Fixture f;std::atomic<unsigned> cache{0};
        f.cache(f.base+4096,4096,MemPerm::ReadOnly,cache);
        const auto destination=f.base+0x2800;
        const auto token=capture_mem_allocation(f.mem,destination,16);
        auto watch=begin_gpu_readback_watch(f.mem,token,destination,16,1,16);
        require(bool(watch),"same-page watch");f.write(destination,0x5a);
        const uint8_t pixels[16]={1};
        require(cache==1&&watch->invalidated&&!write_gpu_readback_rows(f.mem,destination,pixels,16,1,16,&token,watch.get())&&f.read(destination)==0x5a,"same-page watch/callback/CPU store");
        finish_gpu_readback_watch(f.mem,watch);++passed;
    }
    // Independent pages keep their protections, callback and pending GPU copy.
    {
        Fixture f;std::atomic<unsigned> cache{0},guest{0};
        f.cache(f.base+4096,4096,MemPerm::ReadOnly,cache);
        const auto destination=f.base+2*page;
        require(add_protect(f.mem,destination,4096,MemPerm::None,[&](Address,bool){++guest;return true;}),"unrelated None");
        const auto token=capture_mem_allocation(f.mem,destination,16);
        auto watch=begin_gpu_readback_watch(f.mem,token,destination,16,1,16);
        require(bool(watch),"unrelated watch");f.write(f.base+0x2800);
        require(cache==1&&guest==0&&!watch->invalidated&&permission(f.mem.memory.get()+destination)==VM_PROT_NONE,"unrelated page weakened");
        const uint8_t pixels[16]={0x5a};
        require(write_gpu_readback_rows(f.mem,destination,pixels,16,1,16,&token,watch.get()),"unrelated GPU copy rejected");
        finish_gpu_readback_watch(f.mem,watch);
        require(permission(f.mem.memory.get()+destination)==VM_PROT_NONE&&guest==0,"unrelated protection not restored");
        require(f.read(destination)==0x5a&&guest==1,"unrelated callback lost");++passed;
    }
    // Opening a cache-only range leaves the external-mapping cleanup tree empty.
    {
        Fixture f;std::atomic<unsigned> cache{0};
        f.cache(f.base+4096,4096,MemPerm::ReadOnly,cache);
        remove_external_mapping(f.mem,f.mem.memory.get()+f.base,page);
        require(cache==1&&f.mem.protect_tree.empty(),"empty-tree mapping cleanup");++passed;
    }
    // Page-table mode retains its original direct-fault behavior. External
    // noncontiguous backing is intentionally outside collateral retirement.
    {
        Fixture f(true);std::atomic<unsigned> cache{0};
        f.cache(f.base,4096,MemPerm::ReadOnly,cache);f.write(f.base);
        require(cache==1&&f.mem.protect_tree.empty(),"mapped direct cache behavior changed");++passed;
    }
    // Read cancellation alone keeps an existing RO cache armed until a write.
    {
        Fixture f;std::atomic<unsigned> cache{0};
        f.cache(f.base+4096,4096,MemPerm::ReadOnly,cache);
        const auto destination=f.base+0x2800;
        const auto token=capture_mem_allocation(f.mem,destination,16);
        auto watch=begin_gpu_readback_watch(f.mem,token,destination,16,1,16);
        require(bool(watch),"read cancellation watch");(void)f.read(destination);
        require(watch->invalidated&&cache==0&&is_protecting(f.mem,f.base+4096)&&permission(f.mem.memory.get()+destination)==VM_PROT_READ,"read cancellation weakened RO cache");
        f.write(f.base+0x1100);require(cache==1,"RO cache write callback lost after read cancellation");
        finish_gpu_readback_watch(f.mem,watch);++passed;
    }
    // Many unrelated registrations must not be notified or opened.
    {
        Fixture f(false,520);std::atomic<unsigned> target{0};std::array<std::atomic<unsigned>,256> unrelated{};
        f.cache(f.base+4096,4096,MemPerm::ReadOnly,target);
        for(unsigned i=0;i<unrelated.size();++i)f.cache(f.base+(i+1)*2*page+4096,4096,MemPerm::ReadOnly,unrelated[i]);
        forbid_allocations=allocation_check;f.write(f.base+0x2800);forbid_allocations=false;
        require(target==1&&f.mem.protect_tree.size()==unrelated.size(),"unrelated registrations retired");
        for(unsigned i=0;i<unrelated.size();++i)require(unrelated[i]==0&&permission(f.mem.memory.get()+f.base+(i+1)*2*page)==VM_PROT_READ,"unrelated cache callback or permission changed");++passed;
    }
    // A 4KiB mapping inside a 16KiB guest page must select its external
    // backing before rounding physical protection addresses.
    {
        Fixture f(true);
        auto *external=static_cast<uint8_t *>(mmap(nullptr,page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0));
        require(external!=MAP_FAILED,"external mmap");
        const auto mapped=f.base+4096;
        add_external_mapping(f.mem,mapped,4096,external);
        protect_inner(f.mem,mapped,4096,MemPerm::ReadOnly);
        require(permission(external)==VM_PROT_READ&&permission(f.mem.memory.get()+mapped)==VM_PROT_NONE,"external setup");
        unprotect_inner(f.mem,mapped,4096);
        require(permission(external)==(VM_PROT_READ|VM_PROT_WRITE)&&permission(f.mem.memory.get()+mapped)==VM_PROT_NONE,"rounded guest address selected wrong backing");
        std::atomic<unsigned> cache{0};std::atomic<Address> observed{0};
        require(add_cache_protect(f.mem,mapped,4096,MemPerm::ReadOnly,[&](Address address,bool write){++cache;observed=address;return write;}),"mapped external cache");
        *reinterpret_cast<volatile uint8_t *>(external)=0x5a;
        require(cache==1&&observed==mapped&&permission(external)==(VM_PROT_READ|VM_PROT_WRITE)&&permission(f.mem.memory.get()+mapped)==VM_PROT_NONE,"mapped fault selected wrong backing");
        remove_external_mapping(f.mem,external,4096);
        require(f.mem.external_mapping.empty()&&f.read(mapped)==0x5a,"external copyback/cleanup changed");
        munmap(external,page);++passed;
    }
    // A long invalidated cache must not release a distant guest guard.
    {
        Fixture f;std::atomic<unsigned> cache{0},guest{0};
        f.cache(f.base+4096,3*page,MemPerm::ReadOnly,cache);
        require(add_protect(f.mem,f.base+2*page,4096,MemPerm::None,[&](Address,bool){++guest;return true;}),"distant guest None");
        f.write(f.base+0x800);
        require(cache==1&&guest==0&&is_protecting(f.mem,f.base+2*page)&&permission(f.mem.memory.get()+f.base+2*page)==VM_PROT_NONE,"distant guest guard weakened");
        (void)f.read(f.base+2*page);require(guest==1,"distant guest callback lost");++passed;
    }
    std::cout<<"cache_protection_cases="<<passed<<" host_page="<<page<<" passed=true\n";
    return 0;
} catch(const std::exception &error) {std::cerr<<error.what()<<'\n';return 1;}
