#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
revision=a7c6631ad86bbfae4d8cc6342a9da4fba450939d
mkdir -p vendor
if [[ ! -d vendor/res-tools/.git ]]; then
    git clone https://github.com/ngxson/amazfit-bip-tools-ts.git vendor/res-tools
    git -C vendor/res-tools checkout --detach "$revision"
fi
[[ $(git -C vendor/res-tools rev-parse HEAD) == "$revision" ]] || {
    echo 'RES tools revision differs from lock; inspect local checkout.' >&2; exit 1;
}
npm --prefix vendor/res-tools ci --ignore-scripts --no-audit --no-fund
