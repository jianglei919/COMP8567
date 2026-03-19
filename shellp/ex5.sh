#!/bin/bash
#ex5.sh various forms of if statements and nesting of if 
echo -n "Enter file name> "
read file

if [ ! -e $file ]; then   # File does not exist
echo "Sorry, $file does not exist."
elif [ ! -w $file ]; then   # File exists, but it has no write permission
	echo "You have no write permission on $file"
	if [ -O $file ]; then   #file exists, no write permission, but you are the owner 
	chmod u+w $file   #(grant write permission)
	echo "Write permission granted"
	else
	echo "Write permission cannot be granted"  
	echo "because you don't own this file"  #You are not the owner
	fi
else   # File exists, and it has the write permission, add contents of ls 
ls >> $file  
echo "More input has been appended"
fi

