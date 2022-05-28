#!/bin/bash

cd "$(dirname "$0")" || exit
dart "test_harness/bin/test.dart" clox --interpreter jlox/bin/run-test.sh
