#!/bin/zsh
echo "▶ running in container: $(date)"
exec docker run --rm \
  --platform=linux/arm64 \
  --cap-add=SYS_PTRACE \
  --security-opt seccomp=unconfined \
  -v "$PWD":/workspace -w /workspace \
  webserv-dev:arm64 sh -c 'echo "  [container] $(uname -sr) | $(hostname)"; exec make "$@"' -- "$@"