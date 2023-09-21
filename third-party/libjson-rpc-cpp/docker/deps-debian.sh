#!/bin/bash
set -evu
apt-get update && apt-get update && apt-get install -y wget build-essential cmake libjsoncpp-dev libargtable2-dev libcurl4-openssl-dev libmicrohttpd-dev git libhiredis-dev redis-server lcov curl