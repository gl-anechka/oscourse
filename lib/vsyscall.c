#include <inc/vsyscall.h>
#include <inc/lib.h>

static inline uint64_t
vsyscall(int num) {
    // LAB 12: Your code here
    (void)num;
    if (num < NSYSCALLS) {
        return vsys[num];
    } else {
        return -E_INVAL;
    }
   return 0;
}

int
vsys_gettime(void) {
    return vsyscall(VSYS_gettime);
}
