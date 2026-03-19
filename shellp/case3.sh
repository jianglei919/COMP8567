#!/bin/bash 

while [ true ]

do

echo Select a day: MON WED or FRI 
read option

case $option in

 
"MON") echo you selected MON;;

"WED") echo you selected WED;;

"FRI") echo you selected FRI;;
*) echo sorry,your input was incorrect
break;;

esac

done 



