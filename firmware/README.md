# Intel DMC firmware

Unmodified binaries from linux-firmware commit
6569484e6b57bc1c688b383ba6719bf2e25574c4:
https://kernel.googlesource.com/pub/scm/linux/kernel/git/firmware/linux-firmware/+/6569484e6b57bc1c688b383ba6719bf2e25574c4/

The upstream WHENCE identifies these i915 DMC files under LICENSE.i915.
The complete license is included unchanged and shipped in the IPS license action.
These are binary firmware, not MIT source; do not include them in source licence
whitelists or modify them. No GuC/HuC blobs are supplied.

SHA256:
- i915/skl_dmc_ver1_27.bin: d3b6dc1a39bb2aeb37a1179f2b4e8145c24986da78c533571a5712dc91ec3f61
- i915/kbl_dmc_ver1_04.bin: 2cde41c3e5ad181423bcc3e98ff9c49f743c88f18646af4d0b3c3a9664b831a1
- LICENSE.i915: 8542aeabf2761935122d693561e16766ce1bcc2b0d003204f9040b7d6d929f2e

SKL requests the first file; KBL/CFL/WHL/CML request the second. The driver
retains its INIT power reference if firmware cannot be loaded. Kernel runtime
PM remains constrained by the illumos shim; firmware presence is not evidence
of suspend or runtime power-saving support.

# AMD GPU firmware (amdgpu, Tahiti)

Unmodified binaries of Debian's firmware-amd-graphics 20250410-2 (source
package firmware-nonfree, a linux-firmware snapshot of 2025-04-10), taken
from an installed copy (/lib/firmware/amdgpu, symlinks resolved:
tahiti_ce.bin and tahiti_me.bin are hainan_ce.bin and hainan_me.bin,
tahiti_pfp.bin is pitcairn_pfp.bin, as linux-firmware links them).
LICENSE.amdgpu is the licence text Debian gives for amdgpu/*.bin. These are
binary firmware, not MIT source; do not modify them.

amdgpu requests these for Tahiti: gfx_v6_0 (pfp, me, ce, rlc), gmc_v6_0
(mc), si_dpm (smc; Tahiti has no _k_smc variant), UVD 3.1 (uvd). This
snapshot has no tahiti_vce.bin: VCE 1.0 is not available until a newer
linux-firmware.

SHA256:
- amdgpu/tahiti_ce.bin: 04027c3a7c875aaf581ca438ae5ff93ec90002e0e057dc107e7b47048331ee3c
- amdgpu/tahiti_mc.bin: d33b357cce7af8eabea0fa22197131e7a337fd4201a8cff96ad288cd882af533
- amdgpu/tahiti_me.bin: 87617358bb876187786c422c3456fef1d072e6532382ecace0847259045131cb
- amdgpu/tahiti_pfp.bin: aabdb871219fe135cb125c4e38adababdd27a02519324f5534abc00f46b24b5b
- amdgpu/tahiti_rlc.bin: 46da3468097bb48a24ba47d0cc883695752d8e4f1d333c8a8bdb6e339894bac3
- amdgpu/tahiti_smc.bin: 6fa1ebd7df9eabfe5764e3f4f1791bfb87a0c9ba970698680bf276b71e5cfa51
- amdgpu/tahiti_uvd.bin: d10f2a580ad40f44aa5c76c41e9cc3639d2d96f940a57c45e2af1bd7a67c1e7f
- LICENSE.amdgpu: f3c5002a0ab06334ab4b44dc410bc6aafbe3c2aeddcc6669420c3db1c42dd1c9
