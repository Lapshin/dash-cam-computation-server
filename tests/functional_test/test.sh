#!/bin/bash

LD_PRELOAD=./libfunctional_test_lib.so ../../computation-server &
cs_pid=$!

sleep 1
OUTPUT=$(../dash_cam -s 80000000 -f 10000 | nc -v -q 2 localhost 5000 | ../dash_cam -r)
if [ $? != 0 ]; then
    echo "Error, killing server pid $cs_pid"
    kill -9 $cs_pid
    exit 1;
fi

kill $cs_pid
wait %1
cs_status=$?
if [ $cs_status != 0 ]; then
    echo "Server finished with error code $cs_status"
    exit 2
fi
echo $OUTPUT

if [ "$OUTPUT" == "80000000 80000000 80000000 " ]; then
    echo "All good"
    exit 0
fi

exit 3
