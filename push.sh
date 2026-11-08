#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

git add -A

if git diff --cached --quiet; then
    echo "No changes to commit."
else
    message="${*:-Update latency-lab}"
    git commit -m "$message"
fi

git push
