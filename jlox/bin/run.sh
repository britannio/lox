#!/bin/bash

cd target/classes/
if [ "$1" ];
then
    java dev.britannio.lox.Lox "../../../scripts/$1.lox"
else 
    java dev.britannio.lox.Lox 
fi
