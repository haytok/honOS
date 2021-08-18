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
