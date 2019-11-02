#include <memlayout.h>

# vectors.S sends all traps here.
.text
.globl __alltraps
__alltraps:
    # push registers to build a trap frame
    # therefore make the stack look like a struct trapframe
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    pushal

    # load GD_KDATA into %ds and %es to set up data segments for kernel
    movl $GD_KDATA, %eax
    movw %ax, %ds
    movw %ax, %es

    # push %esp to pass a pointer to the trapframe as an argument to trap()
    # esp <- esp - 4
    # M[ss:esp] <- SRC
    pushl %esp

    # call trap(tf), where tf=%esp
    call trap

    # pop the pushed stack pointer
    popl %esp

    # return falls through to trapret...
.globl __trapret
__trapret:
    # restore registers from stack
    popal

    # restore %ds, %es, %fs and %gs
    popl %gs
    popl %fs
    popl %es
    popl %ds

    # get rid of the trap number and error code
    addl $0x8, %esp
    # the stack is supposed to be
    # Low address -> High address
    # 32: eip
    # 32: cs (and padding)
    # 32: elflags
    # (Optional) BELOW ONLY WHEN CROSSING RINGS:
    # 32: esp 
    # 32: ss (and padding)
    iret

# call graph: switch_to --> forkret(by modify stack and ret)
#     --> forkrets --> jump to __trapret
.globl forkrets
forkrets:
    # set stack to this new process's trapframe
    # call to here, 4(%esp) = first parameter = current->tf
    movl 4(%esp), %esp
    jmp __trapret
