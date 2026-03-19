#!/bin/bash
if [[ $1 > 0 && $(($2 % 10)) != 0 ]]; then
echo Operands are valid
let a = "$2 % 10"
let r = "$(($1 * $2)) / $a"
echo "expression value is $r"
else
echo "Operand problem"
fi

