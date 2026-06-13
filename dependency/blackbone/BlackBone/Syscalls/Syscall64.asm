.code
     
syscall_stub proc
    mov eax, ecx

    ; validate number of arguments
    cmp edx, 1
    jl skip

    mov r10, r8
    cmp edx, 2
    jl skip

    xchg rdx, r9
    cmp r9d, 3
    jl skip

    mov r8, [rsp + 28h]
    cmp r9d, 4
    jl skip

    mov r9, [rsp + 30h]

skip:
    add rsp, 10h    ; skip first 2 args
    syscall
    sub rsp, 10h
    ret
syscall_stub endp

; DoIndirectSyscall(DWORD ssn, PVOID gadget, DWORD numArgs, ...)
DoIndirectSyscall proc
    mov [rsp + 8], ecx      ; Save ssn on the stack shadow space
    mov r11, rdx            ; Save gadget address in r11
    mov rcx, r8             ; Move numArgs to rcx
    
    ; Save arg1 (in r9) to r10
    mov r10, r9
    
    ; Load other registers
    mov rdx, [rsp + 28h]    ; arg2
    mov r8, [rsp + 30h]     ; arg3
    mov r9, [rsp + 38h]     ; arg4

    cmp rcx, 4
    jbe perform_jmp         ; If <= 4, no stack shifts

    ; Shift stack arguments safely using rax as a temp register
    cmp rcx, 5
    jb perform_jmp
    mov rax, [rsp + 40h]
    mov [rsp + 28h], rax

    cmp rcx, 6
    jb perform_jmp
    mov rax, [rsp + 48h]
    mov [rsp + 30h], rax

    cmp rcx, 7
    jb perform_jmp
    mov rax, [rsp + 50h]
    mov [rsp + 38h], rax

    cmp rcx, 8
    jb perform_jmp
    mov rax, [rsp + 58h]
    mov [rsp + 40h], rax

    cmp rcx, 9
    jb perform_jmp
    mov rax, [rsp + 60h]
    mov [rsp + 48h], rax

    cmp rcx, 10
    jb perform_jmp
    mov rax, [rsp + 68h]
    mov [rsp + 50h], rax

    cmp rcx, 11
    jb perform_jmp
    mov rax, [rsp + 70h]
    mov [rsp + 58h], rax

perform_jmp:
    mov eax, [rsp + 8]      ; Load ssn into eax
    jmp r11                 ; Jump to gadget
DoIndirectSyscall endp

end