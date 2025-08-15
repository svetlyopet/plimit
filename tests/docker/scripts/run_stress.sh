#!/bin/bash

LOG_PREFIX_INFO="setup: info:"

STRESS_PID="/tmp/stress.pid"
STRESS_CPU=1
STRESS_VM=2
STRESS_IO=2
STRESS_TIMEOUT=120

set -e

echo "$LOG_PREFIX_INFO starting stress process (timeout $STRESS_TIMEOUT)"
stress --cpu $STRESS_CPU --vm $STRESS_VM --io $STRESS_IO --timeout $STRESS_TIMEOUT &
PID=$!

echo "$LOG_PREFIX_INFO writing PID $PID of stress process to $STRESS_PID"
echo $PID > $STRESS_PID

echo "$LOG_PREFIX_INFO waiting for stress timeout"
wait $PID

exit 0