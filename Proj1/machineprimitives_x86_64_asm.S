.globl minithread_switch, minithread_root, atomic_test_and_set

minithread_switch:
    mov  %rdi,%rcx
    mov  %rsi,%rdx
    mov  %rdx,%rax
    push %r15
    push %r14
    push %r13
    push %r12
    push %r11
    push %r10
    push %r9
    push %r8
    push %rbp
    push %rsi
    push %rdi
    push %rbx
    mov %rsp,(%rcx)
    mov (%rax),%rsp
    pop %rbx
    pop %rdi
    pop %rsi
    pop %rbp
    pop %r8
    pop %r9
    pop %r10
    pop %r11
    pop %r12
    pop %r13
    pop %r14
    pop %r15
    ret 

minithread_root: 
    sub $0x38,%rsp
    push %rsi
    callq %rbx    # call main proc

    pop %rsi      # get clean up location back
    mov %rbp,%rdi
    callq %rsi    # call the clean-up
    ret

atomic_test_and_set:

    mov %rdi,%rdx # Get pointer to l

    # operate on rax here to also zero the upper half
    mov $0x1,%rcx                  # load 1 into the cmpxchg source
    mov $0x0,%rax                  # load 0 into the accumulator

                                # _if l == 0 then
    # we operate on e*x which should be the lower half of r*x
    lock cmpxchg %ecx,(%rdx)    # l = 1 (and eax = 0)
                                # _else
                                # (l = 1 and) eax = 1
    ret

