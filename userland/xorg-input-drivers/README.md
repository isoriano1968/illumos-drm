# xf86-input-keyboard 1.9.0, xf86-input-mouse 1.9.5

No source changes: Xorg 21.1 has a new input driver ABI (24), so both are
rebuilt against it (keyboard revision 5); oi-userland branch
`xorg-server-21`. Building them before the new server is installed needs
the 21.1 SDK headers first in the include path. `xf86-input-hotkey` must
be rebuilt the same way (the 1.19 build crashes Xorg 21.1 in
EnableDevice).
