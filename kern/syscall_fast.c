// itask

#include <inc/assert.h>
#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/mmu.h>
#include <inc/x86.h>

#include <kern/env.h>
#include <kern/sched.h>
#include <kern/syscall.h>
#include <kern/syscall_fast.h>

extern void syscall_entry(void);

static inline int
is_user_canonical(uint64_t va) {
    if (va >= MAX_USER_ADDRESS) return 0;
    return (va >> 48) == 0;
}

static inline int
syscall_needs_trapframe(uint64_t sysno) {
    return (sysno == SYS_yield) || (sysno == SYS_ipc_recv) || (sysno == SYS_exofork);
}

static inline void
fastframe_to_trapframe(const struct SyscallFastFrame *f, struct Trapframe *tf) {
    tf->tf_trapno = T_SYSCALL;
    tf->tf_err = 0;
    tf->tf_rip = (uintptr_t)f->user_rip;
    tf->tf_rsp = (uintptr_t)f->user_rsp;
    tf->tf_rflags = f->user_rflags;

    tf->tf_cs = GD_UT_SYSRET | 3;
    tf->tf_ss = GD_UD_SYSRET | 3;
    tf->tf_ds = GD_UD_SYSRET | 3;
    tf->tf_es = GD_UD_SYSRET | 3;

    tf->tf_regs.reg_rax = f->rax;
    tf->tf_regs.reg_rbx = f->rbx;
    tf->tf_regs.reg_rcx = 0;
    tf->tf_regs.reg_rdx = f->rdx;
    tf->tf_regs.reg_rbp = f->rbp;
    tf->tf_regs.reg_rdi = f->rdi;
    tf->tf_regs.reg_rsi = f->rsi;
    tf->tf_regs.reg_r8  = f->r8;
    tf->tf_regs.reg_r9  = 0;
    tf->tf_regs.reg_r10 = f->r10;
    tf->tf_regs.reg_r11 = 0;
    tf->tf_regs.reg_r12 = f->r12;
    tf->tf_regs.reg_r13 = f->r13;
    tf->tf_regs.reg_r14 = f->r14;
    tf->tf_regs.reg_r15 = f->r15;
}

void
syscall_fast_init(void) {
    uint64_t efer = rdmsr(EFER_MSR);
    if (!(efer & EFER_SCE))
        wrmsr(EFER_MSR, efer | EFER_SCE);

    const uint64_t star = ((uint64_t)GD_SYSRET_BASE << 48) | ((uint64_t)GD_KT << 32);
    wrmsr(MSR_STAR, star);
    wrmsr(MSR_LSTAR, (uint64_t)(uintptr_t)syscall_entry);
    wrmsr(MSR_FMASK, (uint64_t)(FL_IF | FL_DF | FL_TF));
}

uint64_t
syscall_fast_handler(struct SyscallFastFrame *f) {
    if (!is_user_canonical(f->user_rip) || !is_user_canonical(f->user_rsp)) {
        cprintf("[syscall] bad return state rip=%016lx rsp=%016lx\n",
                (unsigned long)f->user_rip, (unsigned long)f->user_rsp);
        env_destroy(curenv);
        sched_yield();
    }

    if (syscall_needs_trapframe(f->rax)) {
        fastframe_to_trapframe(f, &curenv->env_tf);
    }

    uint64_t ret = syscall(f->rax, f->rdx, f->r10, f->rbx, f->rdi, f->rsi, f->r8);
    return ret;
}