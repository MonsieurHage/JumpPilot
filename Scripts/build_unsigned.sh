#!/bin/bash
# Build local (sur Mac) d'une IPA non signée pour appareil.
# Usage : ./scripts/build_unsigned.sh
set -euo pipefail
cd "$(dirname "$0")/.."

if ! command -v xcodebuild >/dev/null 2>&1; then
  echo "Xcode est requis (Mac uniquement)."
  exit 1
fi

rm -rf build/DerivedData
xcodebuild \
  -project JumpPilot.xcodeproj \
  -scheme JumpPilot \
  -configuration Release \
  -sdk iphoneos \
  -destination 'generic/platform=iOS' \
  -derivedDataPath build/DerivedData \
  CODE_SIGNING_ALLOWED=NO \
  CODE_SIGNING_REQUIRED=NO \
  CODE_SIGN_IDENTITY="" \
  VALIDATE_PRODUCT=NO \
  build

APP="build/DerivedData/Build/Products/Release-iphoneos/JumpPilot.app"
test -d "$APP"
rm -rf Payload && mkdir Payload
cp -R "$APP" Payload/
rm -f JumpPilot-unsigned.ipa
zip -r JumpPilot-unsigned.ipa Payload
echo "IPA créée : JumpPilot-unsigned.ipa"
