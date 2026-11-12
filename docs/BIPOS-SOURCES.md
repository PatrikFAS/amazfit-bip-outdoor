# BipOS sources, support and English

- Author's SDK: https://github.com/MNVolkov/libbip — app headers, imports and library; not established as complete BipOS firmware source.
- Firmware binary archive and web packer: https://github.com/ngxson/amazfit-bip-web — the pinned public/files directory supplies our English build 7 firmware/resources/font.
- Gadgetbridge installation/background and archived forum links: https://gadgetbridge.org/internals/specifics/amazfit-bip-os/
- Archived English MyAmazfit thread: https://web.archive.org/web/20201112031034/https://myamazfit.ru/threads/bip-mnvolkov-bipos-en.1087/
- Archived Russian thread: https://web.archive.org/web/20201111171822/https://myamazfit.ru/threads/bip-mnvolkov-bipos.1010/
- English package first-hand installation report: https://meinard.dev/blog/amazfit-bip-os
- Language families: https://gadgetbridge.org/internals/specifics/amazfit-bip/ — Latin stock English starts at 1.1.6.48; non-Latin has English by default.

English stock firmware is an alternative if custom apps are abandoned. Our app launcher requires BipOS. A separate open firmware experiment exists at https://github.com/dslul/Amazfitbip-FreeRTOS but its README explicitly says incomplete, not tested on a real device and requiring ST-Link wiring. It is not a practical replacement recommendation.

Recommendation: explicitly English non-Latin 1.1.2.05-based BipOS 0.5.5 build 7 with matching FW/font/RES. No newer matched English BipOS package has been verified by this project; this is not proof none exists. Changing only BipOS launcher language does not add English to the installed Spanish stock firmware base. See INSTALL-ENGLISH.md for the prepared replacement; it still needs separate approval before flashing.
