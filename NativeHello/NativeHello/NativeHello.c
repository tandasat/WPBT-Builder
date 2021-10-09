/**
 * @file NativeHello.c
 * @author Satoshi Tanda (tanda.sat@gmail.com)
 * @brief A sample native NT program.
 * @date 2021-10-09
 *
 * @copyright Copyright (c) 2021, Satoshi Tanda. All rights reserved.
 *
 */
#if defined(_M_X64) || defined(_M_AMD64)
#define _AMD64_
#elif defined(_M_IX86)
#define _X86_
#endif

#include <intrin.h>
#include <ntddk.h>

NTSYSCALLAPI
NTSTATUS
NTAPI
NtDisplayString (
    _In_ PUNICODE_STRING DisplayString
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtDelayExecution (
    _In_ BOOLEAN Alertable,
    _In_ LARGE_INTEGER* Interval
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtTerminateProcess (
    _In_ HANDLE ProcessHandle,
    _In_ NTSTATUS ExitStatus
    );

typedef struct _RTL_USER_PROCESS_PARAMETERS
{
    UINT8 Reserved1[16];
    PVOID Reserved2[10];
    UNICODE_STRING ImagePathName;
    UNICODE_STRING CommandLine;
} RTL_USER_PROCESS_PARAMETERS, *PRTL_USER_PROCESS_PARAMETERS;

typedef struct _PEB
{
    UINT8 Reserved1[4];
    PVOID Reserved3[2];
    PVOID Ldr;
    PRTL_USER_PROCESS_PARAMETERS ProcessParameters;
    // ...
} PEB, *PPEB;

VOID
NTAPI
NtProcessStartup (
    _In_ PPEB Peb
    )
{
    UNICODE_STRING displayString;
    LARGE_INTEGER delay;

    UNREFERENCED_PARAMETER(Peb);

    //
    // http://www.ascii-art.de/
    //
    CONST WCHAR* iDontLikeTurkeyTbh =
    L"\n"
    L"\n"
    L"\n"
    L"\n"
    L"\n"
    L"\n"
    L"\n"
    L"\n"
    L"\n"
    L"\n"
    L"\n"
    L"\n"
    L"            .-\"\"\"\"\"\"\"-.       \n"
    L"           {      _____}---. .-.     \n"
    L"          {      /          (  o\\   \n"
    L"         {      /            \\ \\V  \n"
    L"         {     |             _\\ \\. \n"
    L"          {    |            / '-' \\     HAPPY TURKEY DAY !!\n"
    L"           {___\\   /\\______/    __/     ~~~~~~~~~~~~~~~~~~~\n"
    L"                ~~/   /    /____//   \n"
    L"                  '--'\\___/ \\___/                    ||| _/T\\_ ||| \n"
    L"                          '\\_ \\_                     ||| \\\\|// |||\n"
    L"                           /\\ /\\                     ||| '-|-' |||  \n"
    L"\n"
    L"\n"
    L"Displayed by: ";

    RtlInitUnicodeString(&displayString, iDontLikeTurkeyTbh);
    NtDisplayString(&displayString);
    NtDisplayString(&Peb->ProcessParameters->CommandLine);

    delay.QuadPart = -50000000ll; // 5000 milliseconds
    NtDelayExecution(FALSE, &delay);

    NtTerminateProcess((HANDLE)-1, STATUS_SUCCESS);
}
