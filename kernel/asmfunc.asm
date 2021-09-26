bits 64
section .text

; 引数で渡された IO ポートアドレス addr に対して 32 bit を出力する。
; void IoOut32(uint16_t addr, uint32_t data)
global IoOut32
IoOut32:
    mov dx, di
    mov eax, esi
    out dx, eax
    ret

; 引数で渡された IO ポートアドレス addr から 32 bit 整数を入力して返す。
; dx に格納された IO ポートアドレスから 32 bit 整数を入力し、eax に設定する。
; IO ポートから入力された値が関数の呼び出し元へ返される。
; uint32_t IoIn32(uint16_t addr)
global IoIn32
IoIn32:
    mov dx, di
    in eax, dx
    ret

; uint16_t GetCS()
global GetCS
GetCS:
    xor eax, eax
    mov ax, cs
    ret

; void LoadIDT(uint16_t limit, uint64_t offset)
global LoadIDT
LoadIDT:
    push rbp
    mov rbp, rsp
    sub rsp, 10; limit と offset の領域分の 10 Byte のメモリ領域を確保
    mov [rsp], di
    mov [rsp + 2], rsi
    lidt [rsp]; CPU に IDT を登録する
    mov rsp, rbp
    pop rbp
    ret

; void LoadGDT(sizeof(gdt) - 1, reinterpret_cast<uintptr_t>(&gdt[0]))
; void LoadGDT(uint16_t limit, uint64_t offset)
global LoadGDT
LoadGDT:
    push rbp
    mov rbp, rsp
    sub rsp, 10
    mov [rsp], di
    mov [rsp + 2], rsi
    lgdt [rsp] ; LGDT レジスタに設定する
    mov rsp, rbp
    pop rbp
    ret

; void SetCSSS(uint16_t cs, uint16_t ss)
global SetCSSS
SetCSSS:
    push rbp
    mov rbp, rsp
    mov ss, si
    mov rax, .next
    push rdi
    push rax
    o64 retf
.next:
    mov rsp, rbp
    pop rbp
    ret

; void SetDSAll(uint16_t value)
global SetDSAll
SetDSAll:
    mov ds, di
    mov es, di
    mov fs, di
    mov gs, di
    ret

; void SetCR3(uint64_t value)
; CR3 を書き換えると、CPU は新しい階層ページング構造を使ってアドレス変換を行う
global SetCR3
SetCR3:
    mov cr3, rdi
    ret

extern kernel_main_stack
extern KernelMainNewStack

; UEFI から受け渡すデータ構造を移行するために、エントリポイント名を変更する
global KernelMain
KernelMain:
    mov rsp, kernel_main_stack + 1024 * 1024
    call KernelMainNewStack
.fin:
    hlt
    jmp .fin

;void GetCR3();
global GetCR3
GetCR3:
    mov rax, cr3
    ret

;void SwitchContext(void* next_ctx, void* current_ctx);
; 現在のレジスタ (ex. rax) の値を current_ctx にコピーする。
; next_ctx の値をレジスタ (ex. rax) にコピーする。
; rdi, rsi
global SwitchContext
SwitchContext:
    ; レジスタの値を current_ctx にコピーする
    mov [rsi + 0x40], rax
    mov [rsi + 0x48], rbx
    mov [rsi + 0x50], rcx
    mov [rsi + 0x58], rdx
    mov [rsi + 0x60], rdi
    mov [rsi + 0x68], rsi

    lea rax, [rsp + 8]
    mov [rsi + 0x70], rax ; RSP
    mov [rsi + 0x78], rbp

    mov [rsi + 0x80], r8
    mov [rsi + 0x88], r9
    mov [rsi + 0x90], r10
    mov [rsi + 0x98], r11
    mov [rsi + 0xa0], r12
    mov [rsi + 0xa8], r13
    mov [rsi + 0xb0], r14
    mov [rsi + 0xb8], r15

    mov rax, cr3
    mov [rsi + 0x00], rax
    mov rax, [rsp]
    mov [rsi + 0x08], rax ; RIP
    pushfq
    pop qword [rsi + 0x10] ; RFLAGS

    mov ax, cs
    mov [rsi + 0x20], rax
    mov bx, ss
    mov [rsi + 0x28], rbx
    mov cx, fs
    mov [rsi + 0x30], rcx
    mov dx, gs
    mov [rsi + 0x38], rdx

    fxsave [rsi + 0xc0]
    ; fall through to RestoreContext

global RestoreContext
RestoreContext:
    ; next_ctx の値をレジスタに書き込む
    ; iret 用のスタックフレーム
    push qword [rdi + 0x28] ; SS
    push qword [rdi + 0x70] ; RSP
    push qword [rdi + 0x10] ; RFLAGS
    push qword [rdi + 0x20] ; CS
    push qword [rdi + 0x08] ; RIP

    ; コンテキストの復帰
    fxrstor [rdi + 0xc0]

    mov rax, [rdi + 0x00]
    mov cr3, rax
    mov rax, [rdi + 0x30]
    mov fs, ax
    mov rax, [rdi + 0x38]
    mov gs, ax

    mov rax, [rdi + 0x40]
    mov rbx, [rdi + 0x48]
    mov rcx, [rdi + 0x50]
    mov rdx, [rdi + 0x58]
    mov rsi, [rdi + 0x68]
    mov rbp, [rdi + 0x78]
    mov r8,  [rdi + 0x80]
    mov r9,  [rdi + 0x88]
    mov r10, [rdi + 0x90]
    mov r11, [rdi + 0x98]
    mov r12, [rdi + 0xa0]
    mov r13, [rdi + 0xa8]
    mov r14, [rdi + 0xb0]
    mov r15, [rdi + 0xb8]

    mov rdi, [rdi + 0x60]

    o64 iret

; rdi, rsi, rdx, r10, r8, r9
;void CallApp(int argc, char** argv, uint16_t ss,
;             uint64_t rip, uint64_t rsp, uint64_t* os_stack_ptr)
global CallApp
CallApp:
    ; OS スタックの情報を保持する
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    mov [r9], rsp

    push rdx ; SS
    push r8  ; RSP
    add rdx, 8
    push rdx ; CS
    push rcx ; RIP
    o64 retf

;void LAPICTimerOnInterrupt(const TaskContext& ctx_task)
extern LAPICTimerOnInterrupt

global IntHandlerLAPICTimer
IntHandlerLAPICTimer:
    push rbp
    mov rbp, rsp

    ; レジスタに入っている値を用いてスタック上に TaskContext の構造体を構築する。
    sub rsp, 512
    fxsave [rsp] ; よくわからん命令
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push qword [rbp] ; RBP
    push qword [rbp + 0x20] ; RSP
    push rsi
    push rdi
    push rdx
    push rcx
    push rbx
    push rax

    mov ax, fs
    mov bx, gs
    mov rcx, cr3

    push rbx                ; GS
    push rax                ; FS
    push qword [rbp + 0x28] ; SS
    push qword [rbp + 0x10] ; CS
    push rbp                ; reserved1
    push qword [rbp + 0x18] ; RFLAGS
    push qword [rbp + 0x08] ; RIP
    push rcx                ; CR3

    mov rdi, rsp
    call LAPICTimerOnInterrupt

    add rsp, 8*8 ; CR3 から GS までを無視
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rdi
    pop rsi
    add rsp, 16 ; RSP と RBP を無視
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    fxrstor [rsp]

    mov rsp, rbp
    pop rbp
    iretq

;void LoadTR(uint16_t sel)
global LoadTR
LoadTR:
    ltr di
    ret

;void WriteMSR(uint32_t msr, uint64_t value) rdi, rsi
global WriteMSR
WriteMSR:
    mov rdx, rsi
    shr rdx, 32
    mov eax, esi
    mov ecx, edi ; ECX レジスタに MSR レジスタ番号を設定する。
    wrmsr
    ret

extern syscall_table
; syscall が実行されると、SyscallEntry が起動する。
; void SyscallEntry(void);
global SyscallEntry
SyscallEntry:
    push rbp
    push rcx
    push r11

    push rax ; システムコール番号を保存

    mov rcx, r10
    and eax, 0x7fffffff
    mov rbp, rsp
    and rsp, 0xfffffffffffffff0

    call [syscall_table + 8 * eax]

    mov rsp, rbp

    pop rsi ; システムコール番号を保存
    cmp esi, 0x80000002
    je .exit

    pop r11
    pop rcx
    pop rbp
    o64 sysret

.exit:
    ; rax には SyscallExit を実行した結果の返り値が入っている。
    ; rax に task.OSStackPointer() が rdx に static_cast<int>(arg1) が引き渡される。
    ; アプリを終了させて、OS に戻るための処理を行う。
    mov rsp, rax
    mov eax, edx

    ; CallApp で詰んだ OS スタックの情報を保持する
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ret
