#!/bin/bash
# Download the CC0 Poly Haven assets the planet uses: rock/boulder models (recoloured alien rocks)
# and the two tiling ground textures (planet surface).
# Usage: fetch_polyhaven.sh <dest_dir>
set -euo pipefail
DEST="$1"
UA='ue-setup-script'
RES=2k
MODELS=(rock_moss_set_01 rock_moss_set_02 boulder_01)
GROUNDS=(rocky_terrain_02 cracked_red_ground)

mkdir -p "$DEST"
for a in "${MODELS[@]}"; do
    d="$DEST/$a"; mkdir -p "$d"
    [ -f "$d/$a.gltf" ] && { echo "have $a"; continue; }
    meta=$(curl -sf -A "$UA" "https://api.polyhaven.com/files/$a")
    main=$(jq -r --arg r $RES '.gltf[$r].gltf.url' <<<"$meta")
    curl -sfL -A "$UA" -o "$d/$a.gltf" "$main"
    # every file the .gltf references (bin + textures), keeping relative paths
    jq -r --arg r $RES '.gltf[$r].gltf.include | to_entries[] | "\(.key)\t\(.value.url)"' <<<"$meta" |
    while IFS=$'\t' read -r rel url; do
        mkdir -p "$d/$(dirname "$rel")"
        curl -sfL -A "$UA" -o "$d/$rel" "$url"
    done
    echo "ok $a ($(du -sh "$d" | cut -f1))"
done

for GROUND in "${GROUNDS[@]}"; do
    d="$DEST/$GROUND"; mkdir -p "$d"
    meta=$(curl -sf -A "$UA" "https://api.polyhaven.com/files/$GROUND")
    for map in Diffuse nor_gl arm; do
        url=$(jq -r --arg m $map --arg r $RES '.[$m][$r].jpg.url' <<<"$meta")
        [ -f "$d/${GROUND}_${map}_${RES}.jpg" ] || curl -sfL -A "$UA" -o "$d/${GROUND}_${map}_${RES}.jpg" "$url"
    done
    echo "ok $GROUND ($(du -sh "$d" | cut -f1))"
done

cat > "$DEST/LICENSE.txt" <<'EOF'
All assets in this folder are from Poly Haven (https://polyhaven.com) and are licensed CC0 (public domain).
EOF
echo DONE
