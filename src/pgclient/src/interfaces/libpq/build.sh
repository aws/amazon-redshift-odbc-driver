#!/bin/bash
# Build libpq 9.1.2 with PA changes

cp ../../include/pg_config.h.linux64 ../../include/pg_config.h
cp ../../include/pg_config_os.h.linux64 ../../include/pg_config_os.h
make

