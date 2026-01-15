#!/bin/bash

git ls-files '*.h' '*.cpp' | xargs clang-format-14 -style=file -i
