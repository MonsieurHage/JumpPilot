#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
if ! command -v xcodebuild >/dev/null 2>&1; then
    echo "Xcode est requis sur un Mac pour compiler l’application iOS."
    exit 1
fi
xcodebuild -project JumpPilot.xcodeproj -scheme JumpPilot \
    -configuration Debug -sdk iphonesimulator \
    -destination 'generic/platform=iOS Simulator' \
    -derivedDataPath build/DerivedData CODE_SIGNING_ALLOWED=NO build
