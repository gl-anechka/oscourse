// itask

#ifndef JOS_KERN_SYSCALL_FAST_H
#define JOS_KERN_SYSCALL_FAST_H

#ifndef JOS_KERNEL
#endif

#include <inc/types.h>

/* Layout of the stack frame built by kern/syscallentry.S. */
struct SyscallFastFrame {
    uint64_t rax;
    uint64_t rbx;
    uint64_t rdx;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t r8;
    uint64_t r10;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t user_rip;
    uint64_t user_rflags;
    uint64_t user_rsp;
} __attribute__((packed));

void syscall_fast_init(void);
uint64_t syscall_fast_handler(struct SyscallFastFrame *frame);

#endif