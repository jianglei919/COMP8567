#!/bin/bash
# trap.sh
# notice you cannot make Ctrl-C work in this shell becuase it has been trapped


trap "date" SIGINT

echo "The script is going to run until you hit Ctrl+Z"
echo "Try CTRL+C if you want to"


while [ true ]       
do
sleep 1     
done


