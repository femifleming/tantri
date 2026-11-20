#!/bin/zsh
# render each mockup html → png (2x)
cd "$(dirname "$0")"
for f in ${@:-*.html}; do
  "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --headless=new --disable-gpu --hide-scrollbars \
    --force-device-scale-factor=2 --window-size=1200,780 --virtual-time-budget=1500 \
    --screenshot="$PWD/${f%.html}.png" "file://$PWD/$f" 2>/dev/null
done
