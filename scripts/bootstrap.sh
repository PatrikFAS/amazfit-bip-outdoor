#!/usr/bin/env bash
# Downloads only development sources/tools. Never accesses a watch.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p vendor .tools
revision=df2e0122fa7fd3954df818c63ab9a43f29fa0b1b
if [[ ! -d vendor/libbip/.git ]]; then
    git clone https://github.com/MNVolkov/libbip.git vendor/libbip
    git -C vendor/libbip checkout --detach "$revision"
fi
[[ $(git -C vendor/libbip rev-parse HEAD) == "$revision" ]] || {
    echo "libbip revision differs from lock; preserve local edits and inspect it." >&2; exit 1;
}
archive=.tools/gcc-arm.tar.bz2
url=https://developer.arm.com/-/media/Files/downloads/gnu-rm/7-2018q2/gcc-arm-none-eabi-7-2018-q2-update-linux.tar.bz2
if [[ ! -f "$archive" ]] || ! sha256sum -c scripts/toolchain.sha256 --status; then
    curl -fL --retry 2 --connect-timeout 20 --max-time 900 "$url" -o "$archive.download"
    mv "$archive.download" "$archive"
fi
sha256sum -c scripts/toolchain.sha256
if [[ ! -x .tools/gcc-arm-none-eabi-7-2018-q2-update/bin/arm-none-eabi-gcc ]]; then
    tar -xjf "$archive" -C .tools
fi
.tools/gcc-arm-none-eabi-7-2018-q2-update/bin/arm-none-eabi-gcc --version | head -1
