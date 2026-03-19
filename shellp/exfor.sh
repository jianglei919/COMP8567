
#!/bin/bash
# exfor.sh

echo For loop with an explicit list 

for i in sun 4 12 mon 15
do
echo $i
done

echo For Loop with a range and default increment of 1

for i in {1..10}
do
echo $i
done


echo For loop with increments of 2 

for i in {1..10..2}
do
echo $i
done

echo For loop similar to C

for ((i=0;i<10;i++)) 
do
echo $i
done


