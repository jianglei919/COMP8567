#!/bin/bash
#ex4b.sh
echo -n "Enter file name/directory name> "
read fsd
echo The name of the file/directory is $fsd


if [ -d $fsd ]; then
echo "This is a directory"
elif [ -e $fsd ]; then
echo "This is a file"
else 
echo "File or directory does not exist" 
fi
