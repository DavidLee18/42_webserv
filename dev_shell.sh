#!/bin/zsh
set -e

CONTAINER=webserv-run

if ! docker ps --format '{{.Names}}' | grep -q "^${CONTAINER}$"; then
  echo "✗ container '${CONTAINER}' is not running. start it with: ./dev-up.sh"
  exit 1
fi

# pass-through: with args, run them; without, open interactive shell
if [[ $# -eq 0 ]]; then
  exec docker exec -it "${CONTAINER}" bash
else
  exec docker exec -it "${CONTAINER}" "$@"
fi