#!/usr/bin/env bash
# Fetch pinned offline source inputs only. No device access.
set -euo pipefail
cd "$(dirname "$0")/.."
revision=74ee2ba142f88d1a7d08babe61e8ff372b431661
if [[ ! -d vendor/bip-web/.git ]]; then
    git clone https://github.com/ngxson/amazfit-bip-web.git vendor/bip-web
    git -C vendor/bip-web checkout --detach "$revision"
fi
[[ $(git -C vendor/bip-web rev-parse HEAD) == "$revision" ]] || { echo 'Unexpected bundle source revision.' >&2; exit 1; }
mkdir -p vendor/font-review
font=vendor/font-review/latin-v72.download
if [[ ! -f "$font" ]]; then
    # Notify's Amazfit Bip Latin v72 / old FW 1.1.5.x link resolves here.
    curl -fL --connect-timeout 20 --max-time 120 \
      'https://drive.google.com/uc?id=1akWRuJmxsjk7nLWOZFIRyzMMD5IVcnvp&export=download' \
      -o "$font.partial"
    mv "$font.partial" "$font"
fi
sha256sum -c scripts/candidate-inputs.sha256
