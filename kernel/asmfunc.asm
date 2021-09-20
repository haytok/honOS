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
LoadGDT;
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

;void CallApp(int argc, char** argv, uint16_t cs, uint16_t ss, uint64_t rip, uint64_t rsp)
global CallApp
CallApp:
    push rbp
    mov rbp, rsp
    push rcx ; SS
    push r9  ; RSP
    push rdx ; CS
    push r8  ; RIP
    o64 retf
