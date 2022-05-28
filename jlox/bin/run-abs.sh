#!/bin/bash
DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR/../target/classes/" || exit
java dev.britannio.lox.Lox "$1"
