# PlayStation Vita Bare-metal Linux Loader

## Instructions

You will need a compiled Linux kernel image (build instructions [here](https://gist.github.com/xerpi/5c60ce951caf263fcafffb48562fe50f)), which has to be placed at `ux0:/linux/zImage`, and the corresponding DTB file, which has to be placed at `ux0:/linux/vita.dtb`.

## Debugging

This Linux loader will print debug info over UART0. Check [UART Console](https://wiki.henkaku.xyz/vita/UART_Console) for the location of the pins.

## Credits
Thanks to everybody who has helped me, specially the [Team Molecule](https://twitter.com/teammolecule) (formed by [Davee](https://twitter.com/DaveeFTW), Proxima, [xyz](https://twitter.com/pomfpomfpomf3), and [YifanLu](https://twitter.com/yifanlu)), [TheFloW](https://twitter.com/theflow0), [motoharu](https://github.com/motoharu-gosuto), and everybody at the [HENkaku](https://discord.gg/m7MwpKA) Discord channel.
