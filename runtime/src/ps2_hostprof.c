#include "ps2_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <pthread.h>

#define HP_BITS 16
#define HP_N (1u << HP_BITS)
static struct { u64 rip, n; } hp_tab[HP_N];
static u64 hp_total, hp_dropped;
static HANDLE hp_target;
static volatile int hp_running;
static pthread_t hp_thread;
static char hp_who[32];
static u64 hp_first_field, hp_last_field = ~(u64)0;
extern u64 ps2_kernel_vblank_count(void);


/* Export names are only a nearest-symbol hint, never a full stack trace. */
static void hp_external_name(u64 rip, char *out, size_t size) {
    HMODULE module=NULL;
    char path[MAX_PATH];
    snprintf(out,size,"unknown");
    if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCSTR)(uintptr_t)rip,&module))return;
    if(!GetModuleFileNameA(module,path,sizeof path))return;
    const char *name=strrchr(path,'\\'); name=name ? name+1 : path;
    snprintf(out,size,"%s",name);
    const u8 *base=(const u8 *)module;
    const IMAGE_DOS_HEADER *dos=(const IMAGE_DOS_HEADER *)base;
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE)return;
    const IMAGE_NT_HEADERS *nt=(const IMAGE_NT_HEADERS *)(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE)return;
    IMAGE_DATA_DIRECTORY dir=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if(!dir.VirtualAddress || !dir.Size)return;
    const IMAGE_EXPORT_DIRECTORY *ex=(const IMAGE_EXPORT_DIRECTORY *)(base+dir.VirtualAddress);
    const DWORD *functions=(const DWORD *)(base+ex->AddressOfFunctions);
    const DWORD *names=(const DWORD *)(base+ex->AddressOfNames);
    const WORD *ordinals=(const WORD *)(base+ex->AddressOfNameOrdinals);
    u64 rva=rip-(u64)(uintptr_t)base;
    DWORD best=0,best_index=0;
    for(DWORD i=0;i<ex->NumberOfFunctions;i++) {
        DWORD a=functions[i];
        if(a>=dir.VirtualAddress && a<dir.VirtualAddress+dir.Size)continue;
        if(a && a<=rva && a>=best){best=a;best_index=i;}
    }
    if(!best)return;
    for(DWORD i=0;i<ex->NumberOfNames;i++)if(ordinals[i]==best_index) {
        snprintf(out,size,"%s!%s+0x%llx",name,(const char *)(base+names[i]),
            (unsigned long long)(rva-best));break;
    }
}

static void hp_record(u64 rip) {
    u32 i = (u32)((rip * 0x9E3779B97F4A7C15ull) >> (64 - HP_BITS));
    for (u32 n = 0; n < HP_N; n++, i = (i + 1u) & (HP_N - 1u)) {
        if (hp_tab[i].rip == rip) { hp_tab[i].n++; hp_total++; return; }
        if (!hp_tab[i].rip) { hp_tab[i].rip = rip; hp_tab[i].n = 1; hp_total++; return; }
    }
    hp_dropped++;
}

static void *hp_main(void *unused) {
    HANDLE t = CreateWaitableTimerExW(NULL, NULL, 0x00000002 ,
                                      TIMER_ALL_ACCESS);
    (void)unused;
    while (hp_running) {
        CONTEXT c;
        if (t) {
            LARGE_INTEGER due;
            due.QuadPart = -10000;
            SetWaitableTimer(t, &due, 0, NULL, NULL, FALSE);
            WaitForSingleObject(t, INFINITE);
        } else {
            Sleep(1);
        }
        u64 field=ps2_kernel_vblank_count();
        if(field<hp_first_field || field>hp_last_field)continue;
        if (SuspendThread(hp_target) == (DWORD)-1) continue;
        memset(&c, 0, sizeof c);
        c.ContextFlags = CONTEXT_CONTROL;
        if (GetThreadContext(hp_target, &c)) hp_record((u64)c.Rip);
        ResumeThread(hp_target);
    }
    if (t) CloseHandle(t);
    return NULL;
}

void ps2_host_prof_attach(const char *who) {
    const char *e = getenv("PS2_PROFILE_HOST");
    if (!e || !*e || *e == '0' || hp_running) return;
    if ((strcmp(e, "ee") == 0) != (strcmp(who, "ee") == 0)) return;
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(),
                         &hp_target, THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT
                         | THREAD_QUERY_INFORMATION, FALSE, 0)) {
        ps2_log("host profile: cannot open the %s thread", who);
        return;
    }
    const char *range=getenv("PS2_PROFILE_HOST_RANGE");
    if(range) {
        unsigned long long first,last;
        if(sscanf(range,"%llu:%llu",&first,&last)==2 && first<=last) {
            hp_first_field=(u64)first;hp_last_field=(u64)last;
            ps2_log("host profile: field range %llu..%llu",first,last);
        }
    }
    snprintf(hp_who, sizeof hp_who, "%s", who);
    hp_running = 1;
    if (pthread_create(&hp_thread, NULL, hp_main, NULL) != 0) {
        hp_running = 0;
        CloseHandle(hp_target);
        return;
    }
    ps2_log("host profile: sampling the %s thread", who);
}

void ps2_host_prof_report(void) {
    const char *path = getenv("PS2_PROFILE_HOST_OUT");
    u64 base = (u64)(uintptr_t)GetModuleHandleW(NULL);
    FILE *f;
    if (!hp_running) return;
    hp_running = 0;
    pthread_join(hp_thread, NULL);
    CloseHandle(hp_target);
    if (!path || !*path) path = "out/host_profile.txt";
    f = fopen(path, "w");
    if (!f) { ps2_log("host profile: cannot write %s", path); return; }
    fprintf(f, "thread %s\nsamples %llu\ndropped %llu\n", hp_who,
            (unsigned long long)hp_total, (unsigned long long)hp_dropped);
    for (u32 i = 0; i < HP_N; i++)
        if (hp_tab[i].rip)
            fprintf(f, "%lld %llu\n", (long long)(hp_tab[i].rip - base),
                    (unsigned long long)hp_tab[i].n);
    fclose(f);
    char module_path[1024];
    snprintf(module_path,sizeof module_path,"%s.external",path);
    f=fopen(module_path,"w");
    if(f) {
        for(u32 i=0;i<HP_N;i++)if(hp_tab[i].rip) {
            HMODULE m=NULL;
            if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                (LPCSTR)(uintptr_t)hp_tab[i].rip,&m) && (u64)(uintptr_t)m==base)continue;
            char name[512];hp_external_name(hp_tab[i].rip,name,sizeof name);
            fprintf(f,"%llu %s\n",(unsigned long long)hp_tab[i].n,name);
        }
        fclose(f);
    }
    ps2_log("host profile: %llu samples of the %s thread written to %s",
            (unsigned long long)hp_total, hp_who, path);
}

#else

void ps2_host_prof_attach(const char *who) { (void)who; }
void ps2_host_prof_report(void) {}

#endif
