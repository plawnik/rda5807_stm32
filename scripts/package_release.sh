#!/usr/bin/env bash
set -euo pipefail

version="${1:-dev}"
output_dir="${2:-release}"
firmware_dir="build/firmware"
safe_version="${version//[^A-Za-z0-9._-]/_}"
archive_name="rda5807_stm32-${safe_version}.zip"

for extension in elf hex bin map; do
  test -s "${firmware_dir}/rda5807_stm32.${extension}" || {
    echo "Missing firmware file: ${firmware_dir}/rda5807_stm32.${extension}" >&2
    exit 1
  }
done

mkdir -p "${output_dir}"
output_abs="$(cd "${output_dir}" && pwd -P)"
stage_dir="$(mktemp -d)"
trap 'rm -rf "${stage_dir}"' EXIT

cp "${firmware_dir}/rda5807_stm32.elf" "${stage_dir}/"
cp "${firmware_dir}/rda5807_stm32.hex" "${stage_dir}/"
cp "${firmware_dir}/rda5807_stm32.bin" "${stage_dir}/"
cp "${firmware_dir}/rda5807_stm32.map" "${stage_dir}/"
cp docs/FLASHING.md "${stage_dir}/FLASHING.md"

{
  echo "RDA5807M STM32 firmware"
  echo "Version: ${version}"
  echo "Commit: ${GITHUB_SHA:-$(git rev-parse HEAD 2>/dev/null || echo unknown)}"
  echo "Workflow run: ${GITHUB_RUN_NUMBER:-local}"
  echo "Built (UTC): $(date -u +'%Y-%m-%dT%H:%M:%SZ')"
} > "${stage_dir}/BUILD_INFO.txt"

(
  cd "${stage_dir}"
  sha256sum BUILD_INFO.txt FLASHING.md rda5807_stm32.bin \
    rda5807_stm32.elf rda5807_stm32.hex rda5807_stm32.map > SHA256SUMS.txt
  zip -9 -q "${output_abs}/${archive_name}" ./*
)

cp "${stage_dir}/SHA256SUMS.txt" "${output_abs}/SHA256SUMS.txt"
echo "${output_abs}/${archive_name}"
