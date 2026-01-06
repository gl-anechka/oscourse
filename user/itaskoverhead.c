// itask
/* Measuring syscall overhead */

#include <inc/lib.h>
#include <inc/memlayout.h>
#include <inc/error.h>

static inline uint64_t
rdtsc_serial(void) {
    uint32_t lo, hi;
    asm volatile("lfence; rdtsc"
                 : "=a"(lo), "=d"(hi)
                 :
                 : "memory");
    return ((uint64_t)hi << 32) | lo;
}

static uint64_t
bench_empty(int iters) {
    uint64_t t0 = rdtsc_serial();
    for (int i = 0; i < iters; i++) {
        asm volatile("" ::: "memory");
    }
    uint64_t t1 = rdtsc_serial();
    return (t1 - t0) / (uint64_t)iters;
}

static uint64_t
bench_nop(int mech, int iters) {
    sys_set_syscall_mechanism(mech);
    volatile int sink = 0;
    uint64_t t0 = rdtsc_serial();
    for (int i = 0; i < iters; i++) sink += sys_nop();
    uint64_t t1 = rdtsc_serial();
    return (t1 - t0) / (uint64_t)iters;
}

static uint64_t
bench_getenvid(int mech, int iters) {
    sys_set_syscall_mechanism(mech);
    volatile uint64_t sink = 0;
    uint64_t t0 = rdtsc_serial();
    for (int i = 0; i < iters; i++) sink += (uint64_t)sys_getenvid();
    uint64_t t1 = rdtsc_serial();
    return (t1 - t0) / (uint64_t)iters;
}

static uint64_t
bench_gettime(int mech, int iters) {
    sys_set_syscall_mechanism(mech);
    volatile uint64_t sink = 0;
    uint64_t t0 = rdtsc_serial();
    for (int i = 0; i < iters; i++) sink += (uint64_t)sys_gettime();
    uint64_t t1 = rdtsc_serial();
    return (t1 - t0) / (uint64_t)iters;
}

static uint64_t
bench_cputs0(int mech, int iters) {
    sys_set_syscall_mechanism(mech);
    volatile int sink = 0;
    uint64_t t0 = rdtsc_serial();
    for (int i = 0; i < iters; i++) {
        sys_cputs("", 0);
        sink++;
    }
    uint64_t t1 = rdtsc_serial();
    return (t1 - t0) / (uint64_t)iters;
}

static uint64_t
bench_env_destroy_bad(int mech, int iters) {
    sys_set_syscall_mechanism(mech);
    volatile int sink = 0;
    uint64_t t0 = rdtsc_serial();
    for (int i = 0; i < iters; i++) sink += sys_env_destroy((envid_t)0xDEADBEEF);
    uint64_t t1 = rdtsc_serial();
    return (t1 - t0) / (uint64_t)iters;
}

static uint64_t
bench_alloc_unmap_1pg(int mech, int iters) {
    sys_set_syscall_mechanism(mech);
    void *va = (void*)UTEMP;
    volatile int sink = 0;

    uint64_t t0 = rdtsc_serial();
    for (int i = 0; i < iters; i++) {
        int r = sys_alloc_region(0, va, PAGE_SIZE, PROT_R | PROT_W);
        if (r < 0) { sink += r; continue; }
        *(volatile uint32_t*)va = 0xAABBCCDD;
        sink += sys_unmap_region(0, va, PAGE_SIZE);
    }
    uint64_t t1 = rdtsc_serial();

    return (t1 - t0) / (uint64_t)iters;
}

static uint64_t
bench_map_unmap_1pg(int mech, int iters, envid_t child, void *src, void *dst) {
    sys_set_syscall_mechanism(mech);
    volatile int sink = 0;

    uint64_t t0 = rdtsc_serial();
    for (int i = 0; i < iters; i++) {
        int r = sys_map_region(0, src, child, dst, PAGE_SIZE, PROT_R);
        if (r < 0) { sink += r; continue; }
        sink += sys_unmap_region(child, dst, PAGE_SIZE);
    }
    uint64_t t1 = rdtsc_serial();

    return (t1 - t0) / (uint64_t)iters;
}

static void
print_row(const char *name, uint64_t c_int, uint64_t c_sys, int iters_note) {
    int diff = (int)(c_int - c_sys);
    cprintf("  %-22s INT:%6lu  SYSCALL:%6lu  diff:%d  (iters=%d)\n",
            name, c_int, c_sys, diff, iters_note);
}

void
umain(int argc, char **argv) {
    (void)argc; (void)argv;
    cprintf("=== itask test: syscall overhead ===\n");

    const int it_fast = 200000;
    const int it_slow = 5000;

    uint64_t empty_fast = bench_empty(it_fast);
    uint64_t empty_slow = bench_empty(it_slow);

    sys_set_syscall_mechanism(JOS_SYSCALL_MECH_INT);
    for (int i = 0; i < 2000; i++) sys_nop();
    sys_set_syscall_mechanism(JOS_SYSCALL_MECH_SYSCALL);
    for (int i = 0; i < 2000; i++) sys_nop();

    void *src = (void*)(UTEMP + PAGE_SIZE);
    void *dst = (void*)(UTEMP + 2*PAGE_SIZE);

    int r = sys_alloc_region(0, src, PAGE_SIZE, PROT_R | PROT_W);
    if (r < 0) panic("alloc src failed: %d", r);
    memset(src, 0x5A, PAGE_SIZE);

    envid_t child = sys_exofork();
    if (child < 0) panic("exofork failed: %d", (int)child);

    uint64_t nop_i = bench_nop(JOS_SYSCALL_MECH_INT, it_fast);
    uint64_t nop_s = bench_nop(JOS_SYSCALL_MECH_SYSCALL, it_fast);

    uint64_t ge_i  = bench_getenvid(JOS_SYSCALL_MECH_INT, it_fast);
    uint64_t ge_s  = bench_getenvid(JOS_SYSCALL_MECH_SYSCALL, it_fast);

    uint64_t gt_i  = bench_gettime(JOS_SYSCALL_MECH_INT, it_fast);
    uint64_t gt_s  = bench_gettime(JOS_SYSCALL_MECH_SYSCALL, it_fast);

    uint64_t cp_i  = bench_cputs0(JOS_SYSCALL_MECH_INT, it_fast);
    uint64_t cp_s  = bench_cputs0(JOS_SYSCALL_MECH_SYSCALL, it_fast);

    uint64_t ed_i  = bench_env_destroy_bad(JOS_SYSCALL_MECH_INT, it_fast);
    uint64_t ed_s  = bench_env_destroy_bad(JOS_SYSCALL_MECH_SYSCALL, it_fast);

    uint64_t au_i  = bench_alloc_unmap_1pg(JOS_SYSCALL_MECH_INT, it_slow);
    uint64_t au_s  = bench_alloc_unmap_1pg(JOS_SYSCALL_MECH_SYSCALL, it_slow);

    uint64_t mu_i  = bench_map_unmap_1pg(JOS_SYSCALL_MECH_INT, it_slow, child, src, dst);
    uint64_t mu_s  = bench_map_unmap_1pg(JOS_SYSCALL_MECH_SYSCALL, it_slow, child, src, dst);

    nop_i = nop_i - empty_fast; nop_s = nop_s - empty_fast;
    ge_i  = ge_i - empty_fast; ge_s  = ge_s - empty_fast;
    gt_i  = gt_i - empty_fast; gt_s  = gt_s - empty_fast;
    cp_i  = cp_i - empty_fast; cp_s  = cp_s - empty_fast;
    ed_i  = ed_i - empty_fast; ed_s  = ed_s - empty_fast;
    au_i  = au_i - empty_slow; au_s  = au_s - empty_slow;
    mu_i  = mu_i - empty_slow; mu_s  = mu_s - empty_slow;

    cprintf("\ncycles/op (avg):\n");
    print_row("SYS_nop",              nop_i, nop_s, it_fast);
    print_row("SYS_getenvid",         ge_i,  ge_s,  it_fast);
    print_row("SYS_gettime",          gt_i,  gt_s,  it_fast);
    print_row("SYS_cputs(len=0)",     cp_i,  cp_s,  it_fast);
    print_row("SYS_env_destroy(bad)", ed_i,  ed_s,  it_fast);
    print_row("alloc+unmap (2 sys)",  au_i,  au_s,  it_slow);
    print_row("map+unmap (2 sys)",    mu_i,  mu_s,  it_slow);

    sys_env_destroy(child);
    sys_unmap_region(0, src, PAGE_SIZE);

    cprintf("\nDone.\n");
}