#!/bin/zsh
set -e

CONTAINER=webserv-run
IMAGE=webserv-dev:arm64
PORTS=(8080 8081)  # adjust to your config

if docker ps -a --format '{{.Names}}' | grep -q "^${CONTAINER}$"; then
  if docker ps --format '{{.Names}}' | grep -q "^${CONTAINER}$"; then
    echo "▶ container '${CONTAINER}' already running"
    exit 0
  fi
  echo "▶ removing stopped container '${CONTAINER}'"
  docker rm "${CONTAINER}" >/dev/null
fi

PORT_ARGS=()
for p in "${PORTS[@]}"; do
  PORT_ARGS+=(-p "${p}:${p}")
done

echo "▶ starting '${CONTAINER}' from ${IMAGE}"
docker run -d --name "${CONTAINER}" \
  --platform=linux/arm64 \
  --cap-add=SYS_PTRACE \
  --security-opt seccomp=unconfined \
  -v "$PWD":/workspace -w /workspace \
  "${PORT_ARGS[@]}" \
  "${IMAGE}" sleep infinity >/dev/null

echo "▶ ready. attach with: ./dev-shell.sh"