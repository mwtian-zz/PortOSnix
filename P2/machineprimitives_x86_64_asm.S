.globl minithread_switch, minithread_root, atomic_test_and_set, swap, minithread_trampoline
.extern interrupt_level


minithread_switch:
    pushq %rax
    pushq %rcx
    pushq %rdx
    movq  %rdi,%rcx
    movq  %rsi,%rdx
    movq  %rdx,%rax
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %rbx
    movq %rsp,(%rcx)
    movq (%rax),%rsp
    movq $1,interrupt_level #Enable interrupts after context switch
    popq %rbx
    popq %rdi
    popq %rsi
    popq %rbp
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    popq %r12
    popq %r13
    popq %r14
    popq %r15
    popq %rdx
    popq %rcx
    popq %rax
    retq

minithread_root: 
    sub $0x78,%rsp
    pushq %rsi
    callq *%rbx    # call main proc

    popq %rsi      # get clean up location back
    sub $0x8,%rsp
    movq %rbp,%rdi
    callq *%rsi    # call the clean-up
    ret

atomic_test_and_set:
    movq %rdi,%rdx # Get pointer to l

    # operate on rax here to also zero the upper half
    movq $0x1,%rcx                  # load 1 into the cmpxchg source
    movq $0x0,%rax                  # load 0 into the accumulator

                                # _if l == 0 then
    # we operate on e*x which should be the lower half of r*x
    lock cmpxchgl %ecx,(%rdx)    # l = 1 (and eax = 0)
                                # _else
                                # (l = 1 and) eax = 1
    ret

swap:
    movq %rsi,%rax
    movq %rdi,%rdx

    lock xchgl %eax,(%rdx)

    ret

minithread_trampoline:
    popq %rax #fxrstor address
    cmpq $0,%rax
    je integer_regs #no fp state
    fxrstorq (%rax)
  integer_regs:
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    popq %r12
    popq %r13
    popq %r14
    popq %r15
    popq %rdi
    popq %rsi
    popq %rbp
    popq %rbx
    popq %rdx
    popq %rax
    popq %rcx 
    add $0x10,%rsp #jump over rip and rsp
    popfq 
    mov 0x70(%rsp),%rsp #move to end of sigcontext struct
#MUST BE VERY CAREFUL: add $0x70,%rsp changes the carry flag!!!
    retq  #return address is here, directly below old SP

