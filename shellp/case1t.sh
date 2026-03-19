#!/bin/bash 

while [ true ] 
do 
echo Enter 1 for ls, 2 for ls -1, 3 for ls -l and 4 to exit
read option

case $option in 

"1") echo you selected ls
   ls
   ;;

"2") echo you selected ls -1
   ls -1 
   ;;

"3") echo you selected ls -l
   ls -l 
   ;;

"4") break
   ;;

esac

done 


