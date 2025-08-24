#!/bin/sh

set -eu

echo '>>>>>*' ; pigz -c /data > /dev/null ; echo '<<<<<*'
