#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<EOT
Usage: $0 [-n NUM_SHARDS] [-o OUTPUT_DIR] [-d {wget|curl}] [--insecure]
  -n NUM_SHARDS         Number of shards to download (default: 1)
  -o OUTPUT_DIR         Directory for downloaded/extracted files (default: ./openwebtext2)
  -d {wget|curl}        Downloader to use; auto-detect if omitted
EOT
  echo "  --insecure            Allow TLS without certificate verification" >&2
}

NUM_SHARDS=1
OUTPUT_DIR="openwebtext2"
BASE_URL="https://the-eye.eu/public/AI/pile_preliminary_components/openwebtext2"
DOWNLOADER=""
INSECURE=0

detect_downloader() {
  if [ -n "$DOWNLOADER" ]; then
    return
  fi
  if command -v wget >/dev/null 2>&1; then
    DOWNLOADER="wget"
  elif command -v curl >/dev/null 2>&1; then
    DOWNLOADER="curl"
  else
    echo "Error: neither wget nor curl is available. Install one and retry." >&2
    exit 1
  fi
}

download_file() {
  local url="$1"
  local output="$2"
  detect_downloader
  if [ "$DOWNLOADER" = "wget" ]; then
    local wget_opts=("--https-only" "--tries=3" "-O" "$output")
    if [ "$INSECURE" -eq 1 ]; then
      wget_opts+=("--no-check-certificate")
    fi
    wget "${wget_opts[@]}" "$url"
  else
    local curl_opts=(-fL --retry 3 --retry-delay 2 -o "$output")
    if [ "$INSECURE" -eq 1 ]; then
      curl_opts+=(-k)
    fi
    curl "${curl_opts[@]}" "$url"
  fi
}

while getopts "hn:o:d:-:" opt; do
  case "$opt" in
    h)
      usage
      exit 0
      ;;
    n)
      NUM_SHARDS="$OPTARG"
      ;;
    o)
      OUTPUT_DIR="$OPTARG"
      ;;
    d)
      case "$OPTARG" in
        wget|curl)
          DOWNLOADER="$OPTARG"
          ;;
        *)
          echo "Invalid downloader: $OPTARG" >&2
          usage
          exit 1
          ;;
      esac
      ;;
    -)
      case "$OPTARG" in
        insecure)
          INSECURE=1
          ;;
        help)
          usage
          exit 0
          ;;
        *)
          echo "Unknown flag --$OPTARG" >&2
          usage
          exit 1
          ;;
      esac
      ;;
    *)
      usage
      exit 1
      ;;
  esac
 done

if ! [[ "$NUM_SHARDS" =~ ^[0-9]+$ ]] || [ "$NUM_SHARDS" -le 0 ]; then
  echo "Invalid number of shards: $NUM_SHARDS" >&2
  exit 1
fi

mkdir -p "$OUTPUT_DIR"
cd "$OUTPUT_DIR"

for (( i=0; i<NUM_SHARDS; i++ )); do
  shard_name=$(printf "openwebtext2_%05d.tar.xz" "$i")
  echo "Downloading $shard_name..."
  if [ ! -f "$shard_name" ]; then
    if ! download_file "$BASE_URL/$shard_name" "$shard_name"; then
      echo "Failed to download $shard_name" >&2
      exit 1
    fi
  else
    echo "Shard $shard_name already exists, skipping download"
  fi
  echo "Extracting $shard_name..."
  tar -xf "$shard_name"
 done

echo "Concatenating extracted text files into ../openwebtext2.txt..."
find . -type f -name '*.txt' -print0 | sort -z | xargs -0 cat > ../openwebtext2.txt
echo "Done. Combined corpus available at $(cd .. && pwd)/openwebtext2.txt"
