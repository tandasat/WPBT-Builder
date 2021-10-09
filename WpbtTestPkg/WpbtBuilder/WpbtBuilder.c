/**
 * @file WpbtBuilder.c
 * @author Satoshi Tanda (tanda.sat@gmail.com)
 * @brief The UEFI shell application to install the Windows Platform Binary Table (WPBT).
 * @date 2021-10-09
 *
 * @copyright Copyright (c) 2021, Satoshi Tanda. All rights reserved.
 *
 */
#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>
#include <Library/ShellLib.h>
#include <Library/ShellCEntryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/AcpiTable.h>

//
// See: Windows Platform Binary Table (WPBT) format
// https://download.microsoft.com/download/8/A/2/8A2FB72D-9B96-4E2D-A559-4A27CF905A80/windows-platform-binary-table.docx
//
#pragma pack(1)
typedef struct _EFI_ACPI_5_0_PLATFORM_BINARY_TABLE
{
    EFI_ACPI_DESCRIPTION_HEADER Header;
    UINT32 HandoffMemorySize;
    EFI_PHYSICAL_ADDRESS HandoffMemoryLocation;
    UINT8 ContentLayout;
    UINT8 ContentType;
    UINT16 CommandLineArgumentLength;
    UINT8 CommandLineArgument[0];
} EFI_ACPI_5_0_PLATFORM_BINARY_TABLE;
#pragma pack()
STATIC_ASSERT(sizeof(EFI_ACPI_5_0_PLATFORM_BINARY_TABLE) == 52,  // 0x34
              "As per the specification");

#define EFI_ACPI_5_0_WPBT_REVISION                          1
#define EFI_ACPI_5_0_WPBT_CONTENT_LAYOUT_SINGLE_PE          1
#define EFI_ACPI_5_0_WPBT_CONTENT_TYPE_NATIVE_APPLICATION   1

/**
 * @brief Initializes the contents of WPBT.
 *
 * @param Wpbt - The WPBT to be initialized.
 * @param PlatformBinary - The address of the ACPI memory that contains the
 *      contents of the platform binary to be registered into WPBT.
 * @param PlatformBinarySize - The size of the platform binary buffer in bytes.
 * @param CommandLineArgs - The null-terminated command line arguments. NULL if
 *      no command line argument is specified.
 * @param CommandLineArgsSizeInBytes - The size of command line arguments in
 *      bytes including null-terminator. 0 if no command line argument is
 *      specified.
 */
STATIC
VOID
InitializeWpbt (
    OUT EFI_ACPI_5_0_PLATFORM_BINARY_TABLE* Wpbt,
    IN CONST VOID* PlatformBinary,
    IN UINT32 PlatformBinarySize,
    IN CONST CHAR16* CommandLineArgs,
    IN UINT16 CommandLineArgsSizeInBytes
    )
{
    CONST EFI_ACPI_DESCRIPTION_HEADER header =
    {
        EFI_ACPI_5_0_PLATFORM_BINARY_TABLE_SIGNATURE,
        sizeof(EFI_ACPI_5_0_PLATFORM_BINARY_TABLE) + CommandLineArgsSizeInBytes,
        EFI_ACPI_5_0_WPBT_REVISION,                     // Revision
        0,                                              // Checksum
        { 'P','U','R','R','R','R' },                    // OemId (any value)
        SIGNATURE_64('M','E','O','W','P','U','R','R'),  // OemTableId (any value)
        1,                                              // OemRevision (must be 1)
        SIGNATURE_32('M','E','O','W'),                  // CreatorId (any value)
        0,                                              // CreatorRevision (any value)
    };

    Wpbt->Header = header;
    Wpbt->HandoffMemorySize = PlatformBinarySize;
    Wpbt->HandoffMemoryLocation = (EFI_PHYSICAL_ADDRESS)PlatformBinary;
    Wpbt->ContentLayout = EFI_ACPI_5_0_WPBT_CONTENT_LAYOUT_SINGLE_PE;
    Wpbt->ContentType = EFI_ACPI_5_0_WPBT_CONTENT_TYPE_NATIVE_APPLICATION;
    Wpbt->CommandLineArgumentLength = CommandLineArgsSizeInBytes;
    if (CommandLineArgsSizeInBytes != 0)
    {
        CopyMem(Wpbt->CommandLineArgument, CommandLineArgs, CommandLineArgsSizeInBytes);
    }
    Wpbt->Header.Checksum = CalculateCheckSum8((UINT8*)Wpbt, Wpbt->Header.Length);
}

/**
 * @brief Install WPBT for the given platform binary and command line arguments.
 *
 * @param PlatformBinary - The address of the ACPI memory that contains the
 *      contents of the platform binary to be registered into WPBT.
 * @param PlatformBinarySize - The size of the platform binary buffer in bytes.
 * @param CommandLineArgs - The null-terminated command line arguments. NULL if
 *      no command line argument is specified.
 * @return EFI_SUCCESS on success.
 */
STATIC
EFI_STATUS
InstallWpbt (
    IN CONST VOID* PlatformBinary,
    IN UINT32 PlatformBinarySize,
    IN OPTIONAL CONST CHAR16* CommandLineArgs
    )
{
    EFI_STATUS status;
    UINT64 commandLineArgsSizeInBytes;
    EFI_ACPI_TABLE_PROTOCOL* acpiTableProtocol;
    EFI_ACPI_5_0_PLATFORM_BINARY_TABLE* tempWpbt;
    UINTN wpbtSize;
    UINTN tableKey;
    EFI_ACPI_5_0_PLATFORM_BINARY_TABLE* installedWpbt;

    tempWpbt = NULL;

    //
    // Compute the length of the command line arguments in bytes if specified.
    //
    if (CommandLineArgs == NULL)
    {
        commandLineArgsSizeInBytes = 0;
    }
    else
    {
        commandLineArgsSizeInBytes = (StrLen(CommandLineArgs) + 1) * sizeof(CommandLineArgs[0]);
        if (commandLineArgsSizeInBytes > MAX_UINT16)
        {
            ErrorPrint(L"Command line arguments are too long: %llu bytes\n",
                       commandLineArgsSizeInBytes);
            goto Exit;
        }
    }

    //
    // Allocate and initialize a temp WPBT to be installed. This is cloned by
    // the platform code and should be freed once installation completes.
    //
    wpbtSize = sizeof(EFI_ACPI_5_0_PLATFORM_BINARY_TABLE) + commandLineArgsSizeInBytes;
    status = gBS->AllocatePool(EfiBootServicesData, wpbtSize, (VOID**)&tempWpbt);
    if (EFI_ERROR(status))
    {
        ErrorPrint(L"AllocatePool failed: %llu\n", wpbtSize);
        goto Exit;
    }

    InitializeWpbt(tempWpbt,
                   PlatformBinary,
                   PlatformBinarySize,
                   CommandLineArgs,
                   (UINT16)commandLineArgsSizeInBytes);

    //
    // Install the temp WPBT to the platform using the ACPI protocol. Note that
    // VMware does not support this protocol. One could workaround it by modifying
    // other exiting ACPI table if desperately needed.
    //
    status = gBS->LocateProtocol(&gEfiAcpiTableProtocolGuid, NULL, (VOID**)&acpiTableProtocol);
    if (EFI_ERROR(status))
    {
        ErrorPrint(L"LocateProtocol(EFI_ACPI_TABLE_PROTOCOL) failed: %r\n", status);
        ErrorPrint(
            L"This error may be seen on a virtualization software that does\n"
            L"not implement necessary UEFI protocol(s) for this program.\n"
            L"Try on a physical machine.\n\n"
        );
        goto Exit;
    }
    status = acpiTableProtocol->InstallAcpiTable(acpiTableProtocol,
                                                 tempWpbt,
                                                 wpbtSize,
                                                 &tableKey);
    if (EFI_ERROR(status))
    {
        ErrorPrint(L"InstallAcpiTable failed: %r\n", status);
        goto Exit;
    }

    //
    // Installation was successful. Finally, Patch the installed (cloned) WPBT
    // when the OEM Revision field is not 1. The platform code may update the
    // field (along with other fields) during the table installation and Windows
    // requires the field to be 1. See nt!ExpGetSystemPlatformBinary.
    //
    installedWpbt = (EFI_ACPI_5_0_PLATFORM_BINARY_TABLE*)EfiLocateFirstAcpiTable(
                                    EFI_ACPI_5_0_PLATFORM_BINARY_TABLE_SIGNATURE);
    if (installedWpbt->Header.OemRevision != 1)
    {
        installedWpbt->Header.OemRevision = 1;
        installedWpbt->Header.Checksum = 0;
        installedWpbt->Header.Checksum = CalculateCheckSum8(
                                                (UINT8*)installedWpbt,
                                                installedWpbt->Header.Length);
    }

    Print(L"Successfully installed WPBT at: 0x%p\n", installedWpbt);
    Print(L"  Binary location at: 0x%p\n", installedWpbt->HandoffMemoryLocation);
    Print(L"  Binary size: 0x%x\n", installedWpbt->HandoffMemorySize);
    Print(L"  Command line size: 0x%x\n", installedWpbt->CommandLineArgumentLength);

Exit:
    if (tempWpbt != NULL)
    {
        gBS->FreePool(tempWpbt);
    }
    return status;
}

/**
 * @brief Allocates ACPI memory and copies the contents of the specified file.
 *
 * @param FilePath - The path to the file to read its contents.
 * @param PlatformBinaryAddress - The pointer to receive the address of allocated
 *      buffer that contains the contents of the specified file. The caller must
 *      free this with FreePool() when necessary.
 * @param PlatformBinarySizeInBytes - The pointer to receive the size of the
 *      allocated buffer in bytes.
 * @return EFI_SUCCESS on success.
 */
STATIC
EFI_STATUS
PreparePlatformBinaryOnMemory (
    IN CONST CHAR16* FilePath,
    OUT VOID** PlatformBinaryAddress,
    OUT UINT32* PlatformBinarySizeInBytes
    )
{
    EFI_STATUS status;
    EFI_LOADED_IMAGE_PROTOCOL* loadedImageInfo;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* simpleFileSystem;
    EFI_FILE_PROTOCOL* rootDir;
    EFI_FILE_PROTOCOL* file;
    EFI_FILE_INFO* fileInfo;
    UINT8 fileInfoBuffer[SIZE_OF_EFI_FILE_INFO + 100];
    UINTN bufferSize;
    VOID* fileContents;

    fileContents = NULL;

    //
    // Get EFI_SIMPLE_FILE_SYSTEM_PROTOCOL.
    //
    status = gBS->OpenProtocol(gImageHandle,
                               &gEfiLoadedImageProtocolGuid,
                               (VOID**)&loadedImageInfo,
                               gImageHandle,
                               NULL,
                               EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(status))
    {
        ErrorPrint(L"OpenProtocol(EFI_LOADED_IMAGE_PROTOCOL) failed: %r\n",
                   status);
        goto Exit;
    }

    status = gBS->OpenProtocol(loadedImageInfo->DeviceHandle,
                               &gEfiSimpleFileSystemProtocolGuid,
                               (VOID**)&simpleFileSystem,
                               loadedImageInfo->DeviceHandle,
                               NULL,
                               EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(status))
    {
        ErrorPrint(L"OpenProtocol(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL) failed: %r\n",
                   status);
        goto Exit;
    }

    //
    // Open the given file.
    //
    status = simpleFileSystem->OpenVolume(simpleFileSystem, &rootDir);
    if (EFI_ERROR(status))
    {
        ErrorPrint(L"OpenVolume failed: %r\n", status);
        goto Exit;
    }

    status = rootDir->Open(rootDir, &file, (CHAR16*)FilePath, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status))
    {
        ErrorPrint(L"Open failed: %r\n", status);
        goto Exit;
    }

    status = rootDir->Close(rootDir);
    ASSERT_EFI_ERROR(status);

    //
    // Get the size of the file, allocate buffer and read contents onto it.
    //
    bufferSize = sizeof(fileInfoBuffer);
    status = file->GetInfo(file, &gEfiFileInfoGuid, &bufferSize, fileInfoBuffer);
    if (EFI_ERROR(status))
    {
        ErrorPrint(L"GetInfo failed: %r\n", status);
        goto Exit;
    }

    fileInfo = (EFI_FILE_INFO*)fileInfoBuffer;
    if (fileInfo->FileSize > MAX_UINT32)
    {
        ErrorPrint(L"File size too large: %llu bytes\n", fileInfo->FileSize);
        goto Exit;
    }

    status = gBS->AllocatePool(EfiACPIReclaimMemory, fileInfo->FileSize, &fileContents);
    if (EFI_ERROR(status))
    {
        ErrorPrint(L"Memory allocation failed: %llu bytes\n", fileInfo->FileSize);
        goto Exit;
    }

    bufferSize = fileInfo->FileSize;
    status = file->Read(file, &bufferSize, fileContents);
    if (EFI_ERROR(status))
    {
        ErrorPrint(L"Read failed: %r\n", status);
        goto Exit;
    }

    status = file->Close(file);
    ASSERT_EFI_ERROR(status);

    //
    // All good.
    //
    *PlatformBinaryAddress = fileContents;
    *PlatformBinarySizeInBytes = (UINT32)fileInfo->FileSize;

    fileContents = NULL;

Exit:
    if (fileContents != NULL)
    {
        gBS->FreePool(fileContents);
    }
    return status;
}

/**
 * @brief The module entry point.
 *
 * @param[in] Argc - The number of elements in Argv.
 * @param[in] Argv - An array of command line strings.
 * @return 0 on success. 1 on error.
 */
INTN
EFIAPI
ShellAppMain (
    IN UINTN Argc,
    IN CHAR16** Argv
    )
{
    EFI_STATUS status;
    CONST CHAR16* filePath;
    CONST CHAR16* commandLineArgs;
    VOID* platformBinaryAddress;
    UINT32 platformBinarySizeInBytes;

    platformBinaryAddress = NULL;

    if (Argc == 1)
    {
        Print(L"> WpbtBuilder.efi <PlatformBinary> [Args]\n");
        status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    //
    // Bail out if WPBT already exists. Most platforms allow installation of multiple
    // instances of WPBT but Windows only consumes one. Do not mess up with the
    // existing one. Note that you may modify existing one, instead of adding one
    // if you like.
    //
    if (EfiLocateFirstAcpiTable(EFI_ACPI_5_0_PLATFORM_BINARY_TABLE_SIGNATURE) != NULL)
    {
        ErrorPrint(L"WPBT already exists. Exiting the program.\n");
        status = EFI_ACCESS_DENIED;
        goto Exit;
    }

    filePath = Argv[1];
    commandLineArgs = (Argc >= 3) ? Argv[2] : NULL;

    //
    // Read the specified file and place it in ACPI memory.
    //
    status = PreparePlatformBinaryOnMemory(filePath,
                                           &platformBinaryAddress,
                                           &platformBinarySizeInBytes);
    if (EFI_ERROR(status))
    {
        ErrorPrint(L"PreparePlatformBinaryOnMemory failed: %r\n", status);
        goto Exit;
    }

    //
    // Install WPBT for the allocated memory.
    //
    status = InstallWpbt(platformBinaryAddress, platformBinarySizeInBytes, commandLineArgs);
    if (EFI_ERROR(status))
    {
        ErrorPrint(L"InstallWpbt failed: %r\n", status);
        goto Exit;
    }

    //
    // All good. Ownership of the memory was handed over to the platform.
    //
    platformBinaryAddress = NULL;

Exit:
    if (platformBinaryAddress != NULL)
    {
        gBS->FreePool(platformBinaryAddress);
    }

    return EFI_ERROR(status);
}
