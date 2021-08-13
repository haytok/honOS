typedef unsigned short CHAR16;
typedef unsigned long long EFI_STATUS;
typedef void *EFI_HANDLE;

struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef EFI_STATUS (*EFI_TEXT_STRING)(struct _EFI_SIMPLE_OUTPU_PROTOCOL *This,
                                      CHAR16 *String);

typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    void *dummy;
    EFI_TEXT_STRING OutputString;
} _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef struct {
    char dummy[52];
    EFI_HANDLE ConsoleOutHandle;
    _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
} EIF_SYSTEM_TABLE;

EFI_STATUS EfiMain(EFI_HANDLE ImageHandle, EIF_SYSTEM_TABLE *SystemTable) {
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Hello, World!\n");
    while (1);
    return 0;
}
