#!/usr/bin/env bash
# Regenere components/contrat/contrat.pb.{c,h} depuis ../ContratCM5RPI/contrat.proto
# Prerequis : pip install nanopb grpcio-tools
set -euo pipefail
cd "$(dirname "$0")/.."
SRC=../ContratCM5RPI/contrat.proto
OUT=components/contrat
cp "$SRC" "$OUT/contrat.proto.tmp"
mv "$OUT/contrat.proto.tmp" "$OUT/contrat.proto"
( cd "$OUT" && python3 -m nanopb.generator.nanopb_generator -I. -f contrat.options contrat.proto )
mv "$OUT/contrat.pb.h" "$OUT/include/contrat.pb.h"
rm "$OUT/contrat.proto"
echo "OK : contrat.pb.c / contrat.pb.h regeneres"
