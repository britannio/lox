#!/bin/bash
DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR/../target/classes/"
java dev.britannio.lox.Lox "$1"
