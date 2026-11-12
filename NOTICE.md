# Upstream components

The compiled applications link Maxim Volkov's libbip, Copyright 2019–2021, licensed under Apache 2.0. See [the libbip license](docs/LICENSE.libbip) and `dependencies.lock` for the exact source revision. The app kit also includes notices for the linked compiler runtime and C library.

The BipOS screen lifecycle and build conventions draw on Maxim Volkov's Template and navigation demo and ngxson's Docker app template. The installer downloads the matching upstream BipOS base resources, relocation data, and Maxim Volkov's Flashlight app at assembly time. The app kit contains the Bip Outdoor application ELFs.
