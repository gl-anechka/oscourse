// itask
/* Check syscall correctness */

#include <inc/lib.h>
#include <inc/memlayout.h>
#include <inc/error.h>
#include <inc/mmu.h>

static void
tassert(bool cond, const char *msg) {
    if (!cond) panic("TEST FAIL: %s", msg);
}

static uint32_t
checksum32(const void *p, size_t n) {
    const uint8_t *b = (const uint8_t*)p;
    uint32_t s = 0;
    for (size_t i = 0; i < n; i++) s = (s * 131) + b[i];
    return s;
}

static void
test_basic_invariants(void) {
    envid_t me = sys_getenvid();
    tassert(me != 0, "getenvid returned 0");

    envid_t me2 = sys_getenvid();
    tassert(me == me2, "getenvid not stable");

    int m = sys_get_syscall_mechanism();
    tassert(m == JOS_SYSCALL_MECH_INT || m == JOS_SYSCALL_MECH_SYSCALL, "bad mechanism value");

    int64_t t0 = sys_gettime();
    for (volatile int i = 0; i < 100000; i++) ;
    int64_t t1 = sys_gettime();
    tassert(t1 >= t0, "gettime not monotonic");
}

static void
test_error_paths(void) {
    int r = sys_env_destroy((envid_t)0xDEADBEEF);
    tassert(r == -E_BAD_ENV, "env_destroy(bad) should be -E_BAD_ENV");

    r = sys_env_set_status((envid_t)0xDEADBEEF, ENV_RUNNABLE);
    tassert(r == -E_BAD_ENV, "env_set_status(bad) should be -E_BAD_ENV");

    r = sys_alloc_region(0, (void*)UTEMP, PAGE_SIZE, 0);
    tassert(r == -E_INVAL, "alloc_region(perm=0) should be -E_INVAL");
}

static void
test_alloc_unmap_rw(void) {
    void *va = (void*)UTEMP;
    int r = sys_alloc_region(0, va, PAGE_SIZE, PROT_R | PROT_W);
    tassert(r == 0, "alloc_region failed");

    *(volatile uint32_t*)va = 0xCAFEBABE;
    tassert(*(volatile uint32_t*)va == 0xCAFEBABE, "write/read after alloc failed");

    r = sys_unmap_region(0, va, PAGE_SIZE);
    tassert(r == 0, "unmap_region failed");
}

static void
test_map_region_6args(void) {
    void *src = (void*)(UTEMP + PAGE_SIZE);
    void *dst = (void*)(UTEMP + 2*PAGE_SIZE);

    int r = sys_alloc_region(0, src, PAGE_SIZE, PROT_R | PROT_W);
    tassert(r == 0, "alloc src failed");
    memset(src, 0x5A, PAGE_SIZE);
    uint32_t want = checksum32(src, PAGE_SIZE);

    envid_t child = sys_exofork();
    tassert(child > 0, "exofork failed");

    if (child == 0) {
        envid_t from = 0;
        (void)ipc_recv(&from, NULL, NULL, NULL);

        uint32_t got = checksum32(dst, PAGE_SIZE);

        ipc_send(thisenv->env_parent_id, got, NULL, 0, 0);
        return;
    }

    r = sys_map_region(0, src, child, dst, PAGE_SIZE, PROT_R);
    tassert(r == 0, "map_region failed (arg mapping/reg layout bug?)");

    r = sys_env_set_status(child, ENV_RUNNABLE);
    tassert(r == 0, "env_set_status(child) failed");

    ipc_send(child, 0x1234, NULL, 0, 0);

    uint32_t got = (uint32_t)ipc_recv(NULL, NULL, NULL, NULL);
    tassert(got == want, "child saw wrong data at dst (map_region broken)");

    sys_env_destroy(child);
    sys_unmap_region(0, src, PAGE_SIZE);
}

static void
run_suite_for_mech(int mech) {
    sys_set_syscall_mechanism(mech);
    tassert(sys_get_syscall_mechanism() == mech, "mechanism switch not applied");

    test_basic_invariants();
    test_error_paths();
    test_alloc_unmap_rw();
    test_map_region_6args();
}

void
umain(int argc, char **argv) {
    (void)argc; (void)argv;
    cprintf("=== itask: syscall correctness ===\n");

    cprintf("[1/2] INT mechanism...\n");
    run_suite_for_mech(JOS_SYSCALL_MECH_INT);

    cprintf("[2/2] SYSCALL/SYSRET mechanism...\n");
    run_suite_for_mech(JOS_SYSCALL_MECH_SYSCALL);

    cprintf("ALL TESTS PASSED\n");
}