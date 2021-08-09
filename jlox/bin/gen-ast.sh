#!/bin/bash
javac -d . src/main/java/dev/britannio/tool/GenerateAst.java && java dev.britannio.tool.GenerateAst src/main/java/dev/britannio/lox/
rm dev/britannio/tool/GenerateAst.class
rm -r dev/