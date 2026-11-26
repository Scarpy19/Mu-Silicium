# AGENTS.md — Windows 11 ARM64 on Xiaomi `garnet` (Redmi Note 13 Pro 5G, SM7435-AB)

Instructions for AI coding agents working with a human to reproduce the garnet Windows boot.
Human-oriented overview, status and credits: `Platforms/Xiaomi/garnetPkg/WINDOWS.md`.

## 1. Goal, scope, evidence

**Goal:** Windows 11 23H2 ARM64 (build 22631.2715) reaching the desktop from an external USB drive, using this UEFI image loaded into RAM with `fastboot boot`. One experimental proof of concept; touch, battery, Wi-Fi, audio, sensors and brightness do not work.

**Provenance:** this recipe was derived from the boot-tested image `063203606e82a2b398e3b3a1e592dcee0cbc1c3501d18dc9eb497d15cffa3bd2`, which also contained diagnostic code (not published). **The clean build from this branch has NOT been boot-tested.** Expect to debug; one variable per iteration.

Evidence: PROVEN = isolated boot; BUNDLED = only tested together with other changes; CARRIED = present in the tested image, never isolated. Each commit message repeats this.

| Item | Where (branch `garnet-windows`) | Evidence |
|---|---|---|
| Remove fake `WinBoot ImageBase 0x10000000` RAM + new reserved regions | Mu-Silicium `MemoryMapLib.c` | removal PROVEN; new regions CARRIED |
| KEP (KernelErrataPatcher) re-enabled + shellcode unprotect | Mu-Silicium `KernelErrataPatcherLib/*`, `ErrataPatchesLib.c` | BUNDLED / CARRIED |
| ACPI enabled; MADT/GTDT/FADT/PPTT | Mu-Silicium `garnet.dsc/.fdf`; Silicium-ACPI `Platforms/Xiaomi/garnet/` | BUNDLED (FACP CARRIED) |
| DSDT `USB0` PNP0D10, `_DSM` fn6=1, `_CCA=0` | Silicium-ACPI `DSDT.usb5.dsl/.aml` | PROVEN |
| USB host mode: UsbConfigDxe, XhciPciEmulationDxe (PciIo->Map identity) | Device-Binaries `garnet/PATCH_NOTES.md` | host PROVEN; Map PROVEN then CARRIED; EBS nop no effect |
| `uefiplat.cfg` minus `CPU Vectors` | Device-Binaries | UNKNOWN |
| Drop UFSDxe/SdccDxe | Mu-Silicium `DXE.inc`, `APRIORI.inc` | CARRIED/BUNDLED |
| DTB: host mode, EUD off | Mu-Silicium `Resources/DTBs/garnet.dts/.dtb` | CARRIED |
| `PcdImageLargeAddressLoad`, WARN print level | Mu-Silicium `garnet.dsc` | CARRIED (verbosity only) |
| Boot `\EFI\Microsoft\Boot\bootmgfw.efi`; SecurityStub stubs; NX check bypass | mu_basecore (3 commits) | CARRIED |
| AuthService runtime guard; timer freq fallback; `USB_BUS_TPL=TPL_CALLBACK` | mu_basecore | CARRIED (origin undocumented; TPL change is a candidate for isolated test) |
| MsBootPolicy `FilterOnlyUSB` returns TRUE, SetAltBoot failure ignored | mu_plus | CARRIED |

`git log` in each fork (`Scarpy19/{Mu-Silicium,Silicium-ACPI,Device-Binaries,mu_basecore,mu_plus}`, branch `garnet-windows`) lists the commits.

## 2. Hard safety rules (never violate)

- **NEVER write to the phone's internal storage.** No `fastboot flash`, `fastboot erase`, `fastboot format`, `fastboot flashing unlock/lock`, no EDL/Firehose/QFIL, no `rawprogram*.xml`, no NV or persistent UEFI variable changes, no recovery/bootloader changes.
- The only allowed way to run firmware is **`fastboot boot Mu-garnet.img`** (RAM-only, does not persist).
- **Do not modify the Windows USB drive** (no writes, repartitioning, BCD edits, driver injection) unless the human explicitly confirms. Do not touch other block devices.
- **Never reset, reboot or unplug a device in Crashdump Mode (USB `05c6:900e`) while a capture is in progress.** Do not hand-craft Sahara packets.
- **Crash dumps contain private memory.** Never upload or share them; store each in a new `0700` directory.
- Do not publish logs, serial numbers or personal paths.

## 3. Prerequisites and build

Host: Linux (Ubuntu used), `fastboot`, git, Python 3.11+ (3.14 used), LLVM/clang/lld (**21.1.8** used), `aarch64-linux-gnu-` binutils/gcc, nasm, mono, uuid-dev, build-essential.

```sh
./setup_env.sh -p apt   # installs packages + pip deps (pip-requirements.txt); or use a venv: python3 -m venv .venv && . .venv/bin/activate && pip install -r pip-requirements.txt
git clone --recursive -b garnet-windows https://github.com/Scarpy19/Mu-Silicium.git
cd Mu-Silicium
git submodule status
grep -A3 -E 'Binaries|Common/Mu"|Mu_Basecore|Silicium-ACPI' .gitmodules
```
Verify that Binaries, Common/Mu, Mu_Basecore and Silicium-ACPI point to `https://github.com/Scarpy19/*` with `branch = garnet-windows`, and that `Silicium-ACPI/Platforms/Xiaomi/garnet/` exists. Expected submodule commits: Silicium-ACPI `b411010`, Binaries (Device-Binaries) `2eb46eb`, Mu_Basecore `57799af`, Common/Mu `b5aab0b`. The full clone is several GB.

```sh
# BootShim (embedded in the image)
(cd BootShim && clang --target=aarch64-linux-gnu -c BootShim.S -o BootShim.elf \
  -DREQUIRES_KERNEL_HEADER=1 -DFD_BASE=0xa7100000 -DFD_SIZE=0x300000 \
  && llvm-objcopy -O binary BootShim.elf BootShim.bin)
export CLANGPDB_AARCH64_PREFIX=aarch64-linux-gnu-
python3 build_uefi.py -d garnet -r DEBUG -m 0     # do NOT use -c or -u (not used for this recipe)
sha256sum Mu-garnet.img
```
Output: `Mu-garnet.img` (~1.7 MB). Report its SHA-256 to the human. A different hash than a previous image means a different experiment; record it.

## 4. Windows USB drive and hardware

- Windows 11 23H2 **ARM64** ISO, build 22631.2715 (obtain from Microsoft; the original author used massgrave links). Do not switch versions.
- Create the drive on a Windows PC with Rufus: **"Windows To Go"**, partition scheme **GPT**, filesystem **NTFS**. Resulting layout seen: ESP 260 MiB, MSR 128 MiB, NTFS rest, BCD `{bootmgr}` → `\EFI\Microsoft\Boot\bootmgfw.efi`. Exact Rufus/BCD options were not recorded.
- **Powered USB-C dock/hub (own PSU + PD) is required.** UEFI does not supply VBUS; the phone's port is **USB 2.0 only** (no USB 3, no DisplayPort). Plug the flash drive into a dock USB port **before** booting. Mouse/keyboard (and Ethernet) also go through the dock. Touch is unsupported.
- Bootloader must be unlocked (human's responsibility; the agent must not run unlock commands).

## 5. Boot procedure

1. Phone in fastboot mode, dock + drive + mouse connected. `fastboot devices` shows the device.
2. `fastboot boot Mu-garnet.img` (nothing is written).
3. Expected screen lines (WARN level; clean build has no `DIAG:` lines or long pauses):
   - `[KEP] Memory Attribute Protocol available`
   - `[KEP] ReadyToBoot hooks installed`
   - `[KEP] Winload ACTLR_EL1 scan complete`, `[KEP] Patched ACTLR_EL1 Instruction at 0x…`
   - `[KEP] Detected Codebase Semester = Legacy`, `[KEP] Injected Shell Code at 0x…`, `[KEP] Patched Branch Instruction at 0x…` (at ExitBootServices; may flash by)
   - `ConvertPages: failed to find range 10000000 - 101F9FFF` — benign.
   - Any `[KEP] … failed` / `Status =` line is an error.
4. Windows logo → "Getting ready" with **several automatic reboots. After EACH reboot the phone returns to Android** (fastboot boot is not persistent): put it back in fastboot and re-run step 2 until OOBE.
5. OOBE: use the mouse and the on-screen keyboard (or a USB keyboard). `OOBE\BYPASSNRO` (Shift+F10) allows a local account.
6. Desktop: 8 cores, 8 GB RAM (Task Manager shows ~2 GB "hardware reserved", cause not investigated).

## 6. Troubleshooting

| Symptom | Known cause | Check / fix |
|---|---|---|
| Black screen hang after `ConvertPages … 10000000 - 101F9FFF` | The message itself is benign. A hang here with `WinBoot ImageBase 0x10000000` still in the memory map = fake RAM / alias regression (bootmgr page-table root lands in non-DRAM) | Ensure `MemoryMapLib.c` has no 0x10000000 region and `ArmMmuLib`/`MemoryInitPei` have no alias |
| Spinner, then white screen with blue line, device self-resets into Crashdump Mode (AOP `WDOGBITE`) | `usbxhci` does 64-bit MMIO accesses that abort the SoC | DSDT must be `DSDT.usb5` with `_DSM` fn6 = 1; check `AcpiTables.inf` wires it |
| BSOD `0x7B` INACCESSIBLE_BOOT_DEVICE | No `USB0` `PNP0D10` node, or `_CCA = 1`, or USB drive unreachable | Check DSDT node and `_CCA=0`; check dock has its own power and the drive is on the dock |
| Nothing enumerates / UEFI sees no USB disk | No VBUS from the phone | Powered dock with PD; replug before `fastboot boot` |
| Hang right after ExitBootServices | KEP disabled or hooks not installed | Look for the `[KEP] …` lines above |
| Hang / no spinner after capping published RAM below 4 GiB | Past experiment starved Windows (reverted; `RamManagerUsbFixLib` is not wired) | Do not cap RAM |
| Phone returns to Android mid-setup | Normal (RAM-only image) | Re-run `fastboot boot` |

## 7. Human–agent loop

1. Agent analyses the previous result, changes **one variable**, builds, and reports: image SHA-256, what changed, and **the exact screen lines to look for with expected values and what each outcome means** (keep the list short). Ask for transcribed lines before photos; also ask for the last visible screen line.
2. Human runs `fastboot boot`, reports the lines/behaviour (and whether the phone self-reset into Crashdump Mode).
3. Never mix evidence from different boots; record per boot: image hash, result, dump directory. An absent log line does not prove a path did not run.
4. After three boots without progress on a milestone, return to static analysis.

**Crash dump capture (only when it answers a specific question; the human must say the phone is in Crashdump Mode `05c6:900e`):**
- Use an audited `linux-msm/qdl` build with the reset call removed from `src/sahara.c`; never add a reset flag. Do not reboot/unplug during capture.
- `umask 077; mkdir -m 700 dump_<tag>; cd dump_<tag>; qdl ramdump -o . > capture.log 2>&1` (never reuse a directory). A full dump is ~8 GB; a filtered one (host-side segment filter, e.g. `'DDRCS0_0*,RST_STAT*,PMIC_PON*,MSGRAM*,SMEMINFO*,OCIMEM*,CODERAM*,DATARAM*'`) covers DDR below 4 GiB with less traffic. Use a full dump for higher DDR.
- Validate: dumped regions == requested patterns; every announced region is dumped or listed as `skipped per filter`; no `.partial`/empty files; `RST_STAT.BIN` present; device still `05c6:900e`. A trailing `SAHARA: failed to read request from device` is known and harmless. If Sahara waits for EDL, physically re-enumerate the cable (ask the human).
- Interpret: `RST_STAT=0x42` = forced dump (not necessarily a Windows bugcheck); `RST_STAT=0x3` = watchdog auto-reset. A `…ABORT → WATCHDOG → WDOGBITE` signature in `MSGRAM` indicates the AOP watchdog bite (see 64-bit MMIO above).
- Crashdump reads RAM: any in-RAM debug sensor must be cache-flushed (`dc civac`) by its writer. Residual Android logs may remain in RAM; do not treat them as evidence of the current boot without a validated clock.

## 8. Known limits and next ideas

- Not working: touch (no driver), battery/PEP, Wi-Fi, audio, sensors, brightness. USB 3 / DP impossible (USB 2.0 port).
- Open: the ~2 GB "hardware reserved" RAM; efficiency classes in MADT/PPTT (a topology-fix version exists but is untested; the boot-tested APIC/PPTT are published); which CARRIED items are actually required (test one at a time, e.g. `USB_BUS_TPL`, NX bypass, SecurityStub, LargeAddressLoad); peripherals need Windows drivers for SM7435.

## 9. Credits

See `Platforms/Xiaomi/garnetPkg/WINDOWS.md`.
