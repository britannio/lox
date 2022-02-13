#!/bin/bash

DIR="$(cd "$(dirname "$0")" && pwd)"
dart "$DIR/test_harness/bin/test.dart" jlox --interpreter jlox/bin/run-test.sh