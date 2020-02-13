#!/bin/sh

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
for i in $(seq 5)
do
  cd ${SCRIPT_DIR}/testcases/${i}/
  make
done
