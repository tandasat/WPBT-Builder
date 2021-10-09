[Defines]
  DSC_SPECIFICATION              = 1.28
  PLATFORM_GUID                  = 2b3855ca-f81a-4235-8919-7c113de3d466
  PLATFORM_VERSION               = 1.00
  PLATFORM_NAME                  = WpbtTestPkg
  SKUID_IDENTIFIER               = DEFAULT
  SUPPORTED_ARCHITECTURES        = X64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT

[Components]
  WpbtTestPkg/WpbtBuilder/WpbtBuilder.inf

[LibraryClasses]
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  CacheMaintenanceLib|MdePkg/Library/BaseCacheMaintenanceLib/BaseCacheMaintenanceLib.inf
  DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  DxeServicesLib|MdePkg/Library/DxeServicesLib/DxeServicesLib.inf
  FileHandleLib|MdePkg/Library/UefiFileHandleLib/UefiFileHandleLib.inf
  HobLib|MdePkg/Library/DxeHobLib/DxeHobLib.inf
  HiiLib|MdeModulePkg/Library/UefiHiiLib/UefiHiiLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  RegisterFilterLib|MdePkg/Library/RegisterFilterLibNull/RegisterFilterLibNull.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiCpuLib|UefiCpuPkg/Library/BaseUefiCpuLib/BaseUefiCpuLib.inf
  UefiHiiServicesLib|MdeModulePkg/Library/UefiHiiServicesLib/UefiHiiServicesLib.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  UefiRuntimeLib|MdePkg/Library/UefiRuntimeLib/UefiRuntimeLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  IoLib|MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsicSev.inf
  LocalApicLib|UefiCpuPkg/Library/BaseXApicX2ApicLib/BaseXApicX2ApicLib.inf
  TimerLib|MdePkg/Library/BaseTimerLibNullTemplate/BaseTimerLibNullTemplate.inf
  ShellCEntryLib|ShellPkg/Library/UefiShellCEntryLib/UefiShellCEntryLib.inf
  ShellLib|ShellPkg/Library/UefiShellLib/UefiShellLib.inf
  SortLib|MdeModulePkg/Library/UefiSortLib/UefiSortLib.inf
  SynchronizationLib|MdePkg/Library/BaseSynchronizationLib/BaseSynchronizationLib.inf
  # Use an empty debug library for RELEASE buid. Otherwise, if DEBUG_ON_SERIAL_PORT
  # is defined, use serial port-based implementaion, and if not. ConOut-based
  # implementation.
  !if $(TARGET) == RELEASE
    DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  !else
    !ifdef $(DEBUG_ON_SERIAL_PORT)
      SerialPortLib|PcAtChipsetPkg/Library/SerialIoLib/SerialIoLib.inf
      DebugLib|MdePkg/Library/BaseDebugLibSerialPort/BaseDebugLibSerialPort.inf
    !else
      DebugLib|MdePkg/Library/UefiDebugLibConOut/UefiDebugLibConOut.inf
    !endif
  !endif

[LibraryClasses.common.DXE_RUNTIME_DRIVER, LibraryClasses.common.DXE_DRIVER]
  UefiDriverEntryPoint|MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf

[LibraryClasses.common.UEFI_APPLICATION]
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf

[PcdsFixedAtBuild]
  # Enable EDK2 debug features based on the TARGET configuration.
  # https://github.com/tianocore/tianocore.github.io/wiki/EDK-II-Debugging
  !if $(TARGET) == RELEASE
    # No debug code such as DEBUG() / ASSERT(). They will be removed.
    gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x0
    gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x0
  !else
    # Define DEBUG_ERROR | DEBUG_VERBOSE | DEBUG_INFO | DEBUG_WARN to enable
    # logging at those levels. Also, define DEBUG_PROPERTY_ASSERT_DEADLOOP_ENABLED
    # and such. Assertion failure will call CpuDeadLoop.
    gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x80400042
    gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x2f
  !endif
