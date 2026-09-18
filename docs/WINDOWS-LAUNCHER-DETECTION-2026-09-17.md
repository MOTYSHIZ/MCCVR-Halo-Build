# Windows launcher warning investigation - September 17, 2026

## Request and current result

User prioritizes investigation of Windows warnings/blocking/quarantine and identifies
**HaloMCCVRLauncher.exe** as the affected file. Exact warning, detection name,
reporter's file hash, security product/version and time remain unavailable.
The four gameplay/refinement items remain explicitly ON HOLD.

Fresh official Alpha 0.4.2 download matches the recorded published ZIP hash.
A local Defender custom scan of that ZIP and its extracted contents reported
**found no threats**, exit 0. This does not reproduce or dismiss the report, prove
absence of malware, exercise behavior at launch, or establish SmartScreen/cloud
reputation on another machine. Nothing was executed, installed or published.
No runtime or packaging behavior was changed. CURRENT-STATE.md is unchanged.

## Exact artifacts and local evidence

Release: https://github.com/moistman42069/MCCVR-Halo-Build/releases/tag/MCC_VR_ALPHA_0.4.2

| File | SHA-256 |
|---|---|
| Halo-MCC-VR.zip | 8DC79C7A5A9E3FC565EB525994208D9D477DA6D57DF869301F6907ED0CB03565 |
| HaloMCCVRLauncher.exe | 095463D4F9CFC7FED181E852F71F0A87C644CC185DAEC66C73BE22F3F2A67C48 |
| HaloMCCVR.dll | 8B6FFB78884420588432DA19A5348F330689F201094660729FA8B9AB72C58F60 |

Launcher size 233,984 bytes. Authenticode: NotSigned for launcher AND DLL.
Defender platform 4.18.26080.3; engine 1.1.26080.3; intelligence 1.459.252.0
(last updated locally September 17, 01:07:24). Real-time protection was enabled.
Scan used MpCmdRun -Scan -ScanType 3 -File <review directory> -DisableRemediation.
This documented scan mode ignores file exclusions, scans archives, reports in
stdout and takes no remediation action; it does not disable real-time protection.

Evidence: out/antivirus-review-20260917/{files.json,defender-scan.txt,
scan-exit.txt,defender-status.json,pe-inspection.json}; downloaded ZIP and extracted
files are in the same directory. The launcher PE has standard MSVC sections,
no Authenticode blob, and an asInvoker/uiAccess=false manifest. Imported libraries
are ole32.dll, KERNEL32.dll, USER32.dll and COMCTL32.dll. Version/publisher description
fields are empty. These are observations, not proof of scanner causality or safety.
No code-signing certificate was present in the current user's personal certificate
store; this says nothing about other stores or a remote signing service.

Local Defender history contains detections for a development test executable and
an older downloaded archive, not this release launcher. Do not transfer their threat
names to the reporter's launcher or treat them as the current detection.

## Source findings and limits

src/launcher/launcher.cpp: InjectDll uses VirtualAllocEx with PAGE_READWRITE,
WriteProcessMemory for the DLL path, and CreateRemoteThread calling LoadLibraryW.
The loader starts the selected local MCC installation with anti-cheat disabled;
Steam uses CreateProcess and Store uses packaged activation. It does not request
administrator elevation in its manifest. Current launcher source is unchanged from
accepted runtime 1a9766c. CMake builds a normal MSVC executable; the inspected build
path has no signing step or packer. This was a targeted source/PE review, not a
complete independent security audit or proof of byte-reproducible compilation.

Inference: unsigned publisher identity and legitimate injection behavior are
plausible contributors to warnings. Without the actual warning/detection they are
NOT a confirmed root cause. Do not rewrite injection, obfuscate imports, change
filenames or vary builds just to make a scanner stop recognizing them.

## Resolution paths

1. Obtain the exact warning text/product and launcher SHA-256 from the reporter.
   A named Defender threat/quarantine is different from SmartScreen's unknown-app
   warning or Smart App Control blocking an unsigned executable.
2. For a suspected incorrect malware detection, submit this exact affected file
   through Microsoft Security Intelligence as a software developer with the threat
   name and context. A draft is prepared under out/antivirus-review-20260917;
   nothing has been submitted. A Microsoft determination is required before
   calling this a confirmed false positive.
3. For future releases, establish an actual trusted publisher signing identity and
   consistently Authenticode-sign the launcher and DLL with a timestamp. Signing
   identifies the publisher; it is not an antivirus exemption or guarantee of no
   reputation warning. Self-signing is not trusted publisher enrollment. Enrollment,
   certificate/service access and any fees have not been performed or authorized.
4. Once signing is available, sign before computing final manifest/archive hashes,
   preserve exact signed release bytes and scan those final artifacts. Do not sign
   over or replace the accepted release assets without publication authorization.
5. Add accurate product/version metadata in a future reviewed launcher build for
   identification; this alone is not a malware-detection fix. Keep both-edition
   launch behavior and the accepted gameplay runtime intact.

No security exclusions, protection changes, quarantine restores, signing purchases,
external submissions or new ZIPs were made. The unresolved next input is the exact
reporter warning, not another speculative gameplay or loader modification.

## Microsoft references checked during investigation

- Developer FAQ (detection disputes and separate SmartScreen handling):
  https://learn.microsoft.com/en-us/defender-xdr/developer-faq
- Scan mode and its limits:
  https://learn.microsoft.com/en-us/defender-endpoint/command-line-arguments-microsoft-defender-antivirus
- Trusted code signing for Smart App Control:
  https://learn.microsoft.com/en-us/windows/apps/develop/smart-app-control/code-signing-for-smart-app-control
- File analysis submission portal:
  https://www.microsoft.com/en-us/wdsi/filesubmission
