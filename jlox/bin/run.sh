#!/bin/bash

DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR/../target/classes/"
if [ "$1" ]; # $1 is the first argument to the script
then
    java dev.britannio.lox.Lox "../../../scripts/$1.lox"
else 
    java dev.britannio.lox.Lox 
fi
