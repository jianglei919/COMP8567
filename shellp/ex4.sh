#!/bin/bash

echo "Enter file name:"
read file
echo The name of the file is $file

# Use elif in bash for the else if.
# >> in the example is output redirection(append).
# The ls output will be appended to the file.

if [ -w $file ]; then
ls >> $file
echo "More input has been appended"
elif [ -e $file ]; then
echo "The file exists, but you have no write permission on $file"
else
echo "$file does not exist"
fi

