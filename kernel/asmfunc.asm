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
