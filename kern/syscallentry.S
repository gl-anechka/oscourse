# itask

#include <inc/memlayout.h>

.text
.globl syscall_entry
.type syscall_entry, @function

/*
 * User ABI:
 *   rax = sysno
 *   rdx = a1
 *   r10 = a2 (RCX is clobbered by SYSCALL)
 *   rbx = a3
 *   rdi = a4
 *   rsi = a5
 *   r8  = a6
 */
syscall_entry:
    movq %rsp, %r9
    movabs $KERN_STACK_TOP, %rsp
    subq $8, %rsp

    pushq %r9
    pushq %r11
    pushq %rcx

    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r10
    pushq %r8
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %rdx
    pushq %rbx
    pushq %rax

    movq %rsp, %rdi
    call syscall_fast_handler

    addq $8, %rsp
    popq %rbx
    popq %rdx
    popq %rbp
    popq %rdi
    popq %rsi
    popq %r8
    popq %r10
    popq %r12
    popq %r13
    popq %r14
    popq %r15

    popq %rcx
    popq %r11
    popq %rsp

    sysretq