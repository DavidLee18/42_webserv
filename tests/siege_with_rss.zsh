#!/usr/bin/env zsh
# siege_with_rss.zsh — run siege via Docker while sampling webserv's RSS.
#
# Usage:  ./siege_with_rss.zsh [duration]   # default 60s

emulate -L zsh
set -u

DURATION=${1:-60s}
TARGET=${TARGET:-http://127.0.0.1:8080/}
# Use host.docker.internal on OrbStack/Docker Desktop, 127.0.0.1 on Linux host.
NET_FLAG=(--network=host)
CONTAINER_URL=$TARGET
SIEGE_BIN=${SIEGE_BIN:-$HOME/siege-local/bin/siege}

PID=$(pgrep -x webserv | head -1)
if [[ -z $PID ]]; then
  print -- "webserv not running (pgrep -x webserv returned nothing)"; exit 1
fi

RSS_LOG=$(mktemp -t siege_rss.XXXXXX)
print -- "monitoring pid=$PID; samples → $RSS_LOG"

# Background sampler.
(
  while kill -0 $PID 2>/dev/null; do
    ps -o rss= -p $PID 2>/dev/null | awk -v ts="$(date +%H:%M:%S)" \
      '{printf "%s %d\n", ts, $1}' >> "$RSS_LOG"
    sleep 1
  done
) &
SAMPLER=$!

# Foreground siege.
print -- "── siege ${DURATION} @ ${CONTAINER_URL} ──"
"$SIEGE_BIN" -b -c 25 -t ${DURATION} ${TARGET}
kill $SAMPLER 2>/dev/null; wait $SAMPLER 2>/dev/null

# Summary.
print -- ""
print -- "── RSS trajectory ──"
awk 'NR==1{first=$2; min=$2; max=$2}
     {if($2<min)min=$2; if($2>max)max=$2; last=$2; n++}
     END{
       printf "samples:  %d\n", n
       printf "first:    %d KiB\n", first
       printf "last:     %d KiB\n", last
       printf "min/max:  %d / %d KiB\n", min, max
       printf "Δ:        %+d KiB (last - first)\n", last-first
       if (last-first > 5000) print "WARNING: RSS grew by >5 MiB over the run"
       else                   print "OK: no significant RSS growth"
     }' "$RSS_LOG"
