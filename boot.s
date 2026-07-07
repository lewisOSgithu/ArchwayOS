BITS 32

section .note.GNU-stack note

section .multiboot
align 8
multiboot_start:
    dd 0xe85250d6
    dd 0
    dd multiboot_end - multiboot_start
    dd -(0xe85250d6 + 0 + (multiboot_end - multiboot_start))

align 8
    dw 5
    dw 0
    dd 20
    dd 1920
    dd 1080
    dd 32

align 8
    dw 0
    dw 0
    dd 8
multiboot_end:

global _start
global stack_top
global boot_magic
global boot_info_addr
extern long_mode_start

section .text
_start:
    cli

    mov [boot_magic], eax
    mov [boot_info_addr], ebx
   
    mov esp, stack_top

    lgdt [gdt_descriptor]

    ; paging setup
    call setup_paging

    ; enable PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; enable long mode
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; enable paging
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    jmp CODE_SEG:long_mode_start

.hang:
    hlt
    jmp .hang


setup_paging:
    mov edi, pml4
    mov ecx, 4096*6/4
    xor eax, eax
    rep stosd

    mov eax, pdpt
    or eax, 0b11
    mov [pml4], eax

    mov eax, pd0
    or eax, 0b11
    mov [pdpt], eax

    mov eax, pd1
    or eax, 0b11
    mov [pdpt + 8], eax

    mov eax, pd2
    or eax, 0b11
    mov [pdpt + 16], eax

    mov eax, pd3
    or eax, 0b11
    mov [pdpt + 24], eax

    mov ecx, 0

.map_loop:
    mov eax, ecx
    shl eax, 21
    or eax, 0b10000011
    mov [pd0 + ecx*8], eax

    inc ecx
    cmp ecx, 2048
    jne .map_loop

    mov eax, pml4
    mov cr3, eax

    ret


section .rodata

gdt_start:
    dq 0x0

gdt_code:
    dw 0xffff          
    dw 0x0000          
    db 0x00            
    db 10011010b       
    db 00100000b       
    db 0x00            

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start


section .bss
align 4096

pml4:
    resb 4096

pdpt:
    resb 4096

pd0:
    resb 4096

pd1:
    resb 4096

pd2:
    resb 4096

pd3:
    resb 4096

stack_bottom:
    resb 4096 * 4

stack_top:

boot_magic:
    resd 1

boot_info_addr:
    resd 1
