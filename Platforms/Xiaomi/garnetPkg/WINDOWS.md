# Windows 11 23H2 ARM64 on Xiaomi Redmi Note 13 Pro 5G (`garnet`, SM7435-AB)

Windows 11 23H2 ARM64 (build 22631.2715) booted to the **desktop** from an external USB drive,
using a Mu-Silicium v4.1 UEFI image loaded into RAM with `fastboot boot`. Nothing is flashed.
**Experimental proof of concept, not a daily driver.** Based on Project-Silicium/Mu-Silicium.

## Status
Works: desktop; 8 cores; 8 GB RAM (Task Manager shows 2 GB "hardware reserved", cause not investigated);
USB 2.0 through a powered dock; USB mouse and keyboard; Ethernet on the dock.

Does not work: touch (no driver), battery/PEP, Wi-Fi, audio, sensors, display brightness.
Impossible by hardware: USB 3 and DisplayPort over USB-C (the phone port is USB 2.0 only, no Alt Mode).
Not investigated / not proven: the 2 GB reserved, anything needing drivers that are not installed.

## Hardware needed
- garnet with **unlocked bootloader** (fastboot access).
- USB flash drive (used: SanDisk Ultra Flair 3.2, ~57 GiB).
- **Powered USB-C dock/hub with its own PSU + PD** (used: SUTOUG ST-HB029). UEFI does not supply VBUS,
  so without external power nothing enumerates. Plug the Windows drive into the dock before booting.
- USB mouse (touch is dead; use the on-screen keyboard in OOBE, or a USB keyboard).
- Linux PC with fastboot, git, LLVM/clang 21, `aarch64-linux-gnu-` binutils, Python 3 (build);
  a Windows PC with Rufus (to create the drive).

## Create the USB drive
1. Get the Windows 11 23H2 ARM64 ISO (build 22631.2715 used) from Microsoft; the author obtained links via massgrave.
2. Rufus -> "Windows To Go", partition scheme **GPT**, filesystem **NTFS**. (Exact Rufus/BCD options were not recorded; the
   resulting layout was: ESP 260 MiB, MSR 128 MiB, NTFS ~57 GiB, BCD `{bootmgr}` -> `\EFI\Microsoft\Boot\bootmgfw.efi`.)
3. Do not modify the drive afterwards.

## Build and boot
```sh
git clone -b garnet-windows --recurse-submodules https://github.com/Scarpy19/Mu-Silicium.git
cd Mu-Silicium
# BootShim
(cd BootShim && clang --target=aarch64-linux-gnu -c BootShim.S -o BootShim.elf \
  -DREQUIRES_KERNEL_HEADER=1 -DFD_BASE=0xa7100000 -DFD_SIZE=0x300000 && llvm-objcopy -O binary BootShim.elf BootShim.bin)
export CLANGPDB_AARCH64_PREFIX=aarch64-linux-gnu-
python3 build_uefi.py -d garnet -r DEBUG -m 0     # do not use -c / -u
fastboot boot Mu-garnet.img
```
`fastboot boot` is RAM-only and **does not persist**: Windows setup reboots several times ("Getting ready"); after each
reboot the phone returns to Android, so re-run `fastboot boot Mu-garnet.img` until OOBE. `OOBE\BYPASSNRO` allows a local account.
The submodules (Silicium-ACPI, Device-Binaries, mu_basecore, mu_plus) point to `Scarpy19/*` forks, branch `garnet-windows`; the `Silicium-ACPI` `Platforms/Xiaomi/garnet/` directory is new in this work.

## Key fixes (one-line root causes)
- **Fake RAM + MMU alias:** a bogus `WinBoot ImageBase 0x10000000-0x107FFFFF` "Conv" region plus an alias `0x10000000 -> 0xF3C00000` had no real DRAM behind it; bootmgfw page-table walks aborted. Removed both.
- **KEP (KernelErrataPatcher):** re-enabled so `mrs actlr_el1` in winload/kernel is patched; shellcode page is unprotected/re-protected.
- **MADT/GTDT for SM7435:** tables inherited from another SoC; set GICD `0x17200000`, GICR `0x17260000`, 8 GICC, timer GSIV 29/30/27/28.
- **DSDT `USB0` generic xHCI:** without a `PNP0D10` node there is no `usbxhci` and the kernel BSODs 0x7B.
- **`_DSM` fn6 = 1 (32-bit MMIO):** `usbxhci` does 64-bit MMIO accesses that abort the SoC (AOP watchdog bite) unless fn6 requests 32-bit.
- **`_CCA = 0`:** `_CCA=1` declares DMA coherent, which is not true on this DWC3 path; the disk never enumerated (BSOD 0x7B).
- **USB host mode:** `UsbConfigDxe.efi` binary-patched to force host (DFP); `XhciPciEmulationDxe.efi` patched so `PciIo->Map` is identity.
  (A `nop` of `UsbConfigCoresDeinit` before EBS is also applied; it had no observed effect.)
- Also carried (not isolated): boot `bootmgfw.efi` path, SecurityStub bypass, NX-check bypass, USB-only boot filter, NOUFS (no UFS/SD drivers).

## Safety
Never write the internal UFS or the USB from the firmware/tools: no flashing, EDL/Firehose, `erase`, NV or persistent UEFI variable
changes. Firmware runs from RAM only. Crash dumps (if you capture them) contain private memory: do not publish them. Use at your own risk.

## Provenance / test status
This recipe was derived from the **boot-tested image** `063203606e82a2b398e3b3a1e592dcee0cbc1c3501d18dc9eb497d15cffa3bd2`,
which also contained diagnostic code (logging rings, pauses, probes) that is **not** published here.
**The clean build produced from this branch has not been boot-tested yet.** Known deltas from the tested image:
no diagnostics; `RamManagerUsbFixLib` and cosmetic boot-menu changes excluded; ACPI APIC/PPTT are the boot-tested versions.
Each commit message states its evidence: PROVEN (isolated boot), BUNDLED (tested only together with other changes) or CARRIED (present in the tested image, never isolated).
The patched Qualcomm binaries (Device-Binaries `garnet/PATCH_NOTES.md`) are redistributed as-is; check their licensing before reuse.

## Credits
- **Project-Silicium / Mu-Silicium** (Robotix22 et al.): base firmware, build system, garnetPkg; **Silicium-ACPI**, **Device-Binaries**, **Guides**: ACPI framework, vendor binaries, porting guides.
- **Microsoft Project Mu** and **TianoCore EDK II**: UEFI core that was modified.
- **Samuel Tulach (rainbow)** and **DuoWoA authors**: origin of KernelErrataPatcherLib (per its header).
- **WOA-Project (mu_andromeda_platforms)** and **Project Aloha (mu_aloha_platforms)**: base/reference named in the Mu-Silicium README.
- **nishant713/woa-pipa** and **HacksYT23-2/WoA-On-Moto-Edge-2022-UW**: Windows-on-Qualcomm references read for USB-boot precedents.
- **JosephMontfort/device_xiaomi_garnet**, **garnet-linux/Mu-Silicium**, **garnet-stuff/vendor_xiaomi_garnet-firmware**: garnet device/firmware sources consulted.
- **linux-msm/qdl**: used (reset patched out) to capture crash dumps while debugging.
- **massgrave** (ISO source) and **Rufus** (Windows To Go drive): used to prepare the USB drive.
