#!/bin/bash

cd target/classes/
if [ "$1" ]; # $1 is the first argument to the script
then
    java dev.britannio.lox.Lox "../../../scripts/$1.lox"
else 
    java dev.britannio.lox.Lox 
fi
