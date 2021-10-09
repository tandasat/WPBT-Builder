#! /usr/bin/env python3
"""
This script is invoked by VSCode with Ctrl + Shift + B to build the project with
the EDK2 command and copy output to, say a USB drive and an ISO file.
On native Windows, install Python x64 3.6+ then, copy python.exe as python3.exe
"""
import os
import subprocess
import platform
import shutil
import getpass
import time
import ntpath

# pylint: disable=subprocess-run-check,line-too-long

is_wsl = (
    lambda: platform.system() == "Linux" and "Microsoft" in platform.uname().release
)

# User configurations begin

# PACKAGE: the name of the package to compile
PACKAGE = "WpbtTestPkg"

# FILES_TO_COPY: the list of build output files to be copied to COPY_DEST
FILES_TO_COPY = [
    "WpbtBuilder.efi",
    "startup.nsh",
]

# STARTUP_NSH: the contents of startup.nsh file
STARTUP_NSH = """
fs0:
WpbtBuilder.efi NativeHello.exe
"""

# BUILD_TARGET: the build target, such as NOOPT, DEBUG and RELEASE
BUILD_TARGET = "NOOPT"

# ARCHITECTURE: the build architecture such as X64
ARCHITECTURE = "X64"

# COPY_DEST: the USB drive path to copy the compiled files.
if is_wsl() or platform.system() == "Windows":
    COPY_DEST = "D:\\"
elif platform.system() == "Linux":
    COPY_DEST = f"/media/{getpass.getuser()}/FAT32"
elif platform.system() == "Darwin":
    COPY_DEST = "/Volumes/FAT32/"
else:
    COPY_DEST = ""

# User configurations end


def main():
    """Builds the package, copies the output files into external media, and
    starts a VM as configured
    """
    edk_dir = os.path.abspath(os.pardir)
    base_dir = os.path.abspath(os.path.join(edk_dir, os.pardir))

    # Build variables based on the user configurations
    if platform.system() == "Linux":
        edk_setup = "source edksetup.sh"
        compiler = "GCC5"  # can be "GCC5" or "CLANG38"
        executable = "/bin/bash"
        deploy_methods = [copy_to_usb]
    elif platform.system() == "Darwin":
        edk_setup = "source ./edksetup.sh"
        compiler = "CLANGPDB"
        executable = None
        deploy_methods = []
    elif platform.system() == "Windows":
        edk_setup = "edksetup.bat"
        compiler = "VS2019"  # can be "VS2019" or "CLANGPDB"
        executable = None
        deploy_methods = [copy_to_usb]
    else:
        raise NotImplementedError

    output_dir = os.path.join(
        edk_dir,
        "Build",
        PACKAGE,
        BUILD_TARGET + "_" + compiler,
        ARCHITECTURE,
    )

    # On Windows, use the prepared dependencies if the system is not configured yet.
    if platform.system() == "Windows":
        if not os.environ.get("NASM_PREFIX", ""):
            os.environ["NASM_PREFIX"] = (
                os.path.join(base_dir, "Build", "Windows", "nasm") + os.sep
            )
        if not os.environ.get("IASL_PREFIX", ""):
            os.environ["IASL_PREFIX"] = os.path.join(
                base_dir, "Build", "Windows", "asl"
            )

    os.chdir(edk_dir)

    # Make the BaseTool if it has not. It is required only once.
    if platform.system() == "Windows":
        command = [edk_setup, "Rebuild"]
    else:
        command = ["make", "-C", os.path.join(edk_dir, "BaseTools", "Source", "C")]
    print(f"🕒 Running: {' '.join(command)}")
    retcode = subprocess.run(command).returncode
    if retcode != 0:
        print(
            f"❌ Failed to build BaseTool with error {retcode}."
            f" EDK2 prerequisit may not be installed."
        )
        return

    # Delete old build artifacts of the target package. This is to prevent the
    # build command inadvertently skipping rebuilding new ones despite that source
    # files are changed.
    if os.path.exists(os.path.join(output_dir, PACKAGE)):
        shutil.rmtree(os.path.join(output_dir, PACKAGE))

    # Kick off the build command.
    command = (
        f"{edk_setup} && "
        f"build"
        f" -a {ARCHITECTURE}"
        f" -t {compiler}"
        f" -b {BUILD_TARGET}"
        f" -p {PACKAGE}/{PACKAGE}.dsc -D DEBUG_ON_SERIAL_PORT"
    )
    print(f"🕒 Running: {command}")
    retcode = subprocess.run(command, shell=True, executable=executable).returncode
    if retcode != 0:
        print(f"❌ Failed to build the project with error {retcode}.")
        return
    print(f"✅ Successfully built onto {output_dir}")

    # Drop startup.nsh onto the output direcotry
    with open(os.path.join(output_dir, "startup.nsh"), "w") as nsh_file:
        nsh_file.write(STARTUP_NSH)

    # Copy the output files into ISO and/or USB
    for method in deploy_methods:
        method(output_dir)


def copy_to_usb(output_dir):
    """Copies build output into a USB drive"""
    if not COPY_DEST or (not is_wsl() and not os.path.exists(COPY_DEST)):
        print(
            "❗ Did not copy output to USB."
            " The copy destination does not exist or is not set up."
        )
        return

    # Copy the build output files into the destination directory.
    for file in FILES_TO_COPY:
        try:
            if is_wsl():
                # On WSL, copy with Windows' path, using xcopy. No shutil is used
                # because we need to mount the USB onto WSL first, which needs
                # root and often flaky.
                win_path = get_native_windows_path(os.path.join(output_dir, file))
                command = f"/mnt/c/Windows/System32/xcopy.exe {win_path} {COPY_DEST} /Y"
                retcode = subprocess.run(
                    command.split(" "),
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                ).returncode
                if retcode != 0:
                    print(f"❗ Failed to copy {file} with error {retcode}")
                else:
                    print(f"✅ Copied as: {ntpath.join(COPY_DEST, file)}")
            else:
                shutil.copyfile(
                    os.path.join(output_dir, file), os.path.join(COPY_DEST, file)
                )
                print(f"✅ Copied as: {os.path.join(COPY_DEST, file)}")
        except FileNotFoundError as exp:
            print(f"❗ Failed to copy {file}: {str(exp)}")


def get_native_windows_path(wsl_path):
    """Returns the native Windows path given the WSL path"""
    return str(
        subprocess.run(
            ["wslpath", "-a", "-w", wsl_path], stdout=subprocess.PIPE
        ).stdout,
        "utf-8",
    ).strip()


if __name__ == "__main__":
    main()
