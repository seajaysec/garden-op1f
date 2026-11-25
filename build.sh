#!/bin/sh
set -e
ROOT="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
[ -f "$ROOT/local.env" ] && . "$ROOT/local.env"
OF="${OPEN_FIELD:?set OPEN_FIELD (local.env)}"
XY="${OPEN_XY:?set OPEN_XY (local.env)}"
TC="${TOOLCHAIN:?set TOOLCHAIN (local.env)}"
WORK="$ROOT/.build"
for need in "$OF/tools" "$XY/tooling" "$TC/bfin-elf-g++"; do
  [ -e "$need" ] || { echo "missing: $need" >&2; exit 1; }
done

mkdir -p "$WORK/open-field/open-xy/local/toolchain"
rsync -a --chmod=u+w --delete --exclude build --exclude out --exclude out-set --exclude open-xy --exclude .git "$OF/" "$WORK/open-field/"
mkdir -p "$WORK/open-field/open-xy/local/toolchain"
rsync -a --delete --exclude __pycache__ "$XY/tooling/" "$WORK/open-field/open-xy/tooling/"
ln -sfn "$TC" "$WORK/open-field/open-xy/local/toolchain/bin"
ln -sfn "$ROOT" "$WORK/garden"
# Blackfin anomaly 05000074: scheduled pairs the assembler refuses
python3 -c "p='$WORK/open-field/tools/module_build.py'; t=open(p).read(); f='-fno-schedule-insns2';
open(p,'w').write(t if f in t else t.replace('\"-O2\"', '\"-O2\", \"%s\"' % f))"

python3 "$ROOT/art/make_picture.py"
cd "$WORK/open-field"
if [ -n "$FIELD_DISPLAY" ] && [ -d "$FIELD_DISPLAY" ]; then
  python3 -I tools/picture_make.py check --instructions ../garden/garden.svg.make --display "$FIELD_DISPLAY"
else
  echo "FIELD_DISPLAY not set: picture not checked" >&2
fi
python3 -I tools/pack_build.py card --recipe ../garden/recipe.json --out ../garden/garden_card.s
python3 -I tools/pack_build.py pictures --recipe ../garden/recipe.json --out ../garden/garden_pictures.s
mkdir -p "$ROOT/out"
rm -f "$ROOT/out/garden.pack"
python3 -I tools/pack_build.py build3 \
  --recipe ../garden/recipe.json \
  --profile profiles/op1f-1.7.3.json \
  --out ../garden/out/garden.pack
python3 -I tools/pack_build.py check --pack ../garden/out/garden.pack | tee "$WORK/check.txt"
if grep '^wants:' "$WORK/check.txt" | grep -q '___'; then
  echo "the pack wants a library routine (a division?): the loader would refuse it" >&2; rm -f "$ROOT/out/garden.pack"; exit 1
fi
cp -f "$ROOT/out/garden.pack" "$ROOT/garden.pack"
echo "built $ROOT/out/garden.pack"
