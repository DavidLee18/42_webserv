#!/bin/zsh
set -e

CONTAINER=webserv-run

if ! docker ps -a --format '{{.Names}}' | grep -q "^${CONTAINER}$"; then
  echo "▶ container '${CONTAINER}' not present"
  exit 0
fi

echo "▶ stopping '${CONTAINER}'"
docker stop "${CONTAINER}" >/dev/null 2>&1 || true
docker rm "${CONTAINER}" >/dev/null
echo "▶ removed"