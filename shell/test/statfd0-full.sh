#!/bin/bash
exec /usr/bin/stat -L -c %i/%d /proc/self/fd/0
