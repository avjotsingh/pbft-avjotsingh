#!/bin/bash

# Ports to kill in range 60060-60069
for port in {60060..60069}; do
    pid=$(lsof -t -i:$port)
    if [ -n "$pid" ]; then
        kill -9 "$pid" && echo "Killed process on port $port"
    fi
done

# Ports to kill in range 50051-50057
for port in {50051..50057}; do
    pid=$(lsof -t -i:$port)
    if [ -n "$pid" ]; then
        kill -9 "$pid" && echo "Killed process on port $port"
    fi
done
