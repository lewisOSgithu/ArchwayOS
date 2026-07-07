BITS 64

section .note.GNU-stack note

global long_mode_start
extern Kernel_main
extern stack_top
extern boot_magic
extern boot_info_addr

section .text
long_mode_start:
    mov rsp, stack_top

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    
    cld

    mov edi, [boot_magic]
    mov esi, [boot_info_addr]
    call Kernel_main

.hang:
    hlt
    jmp .hang
