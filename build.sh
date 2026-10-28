set -e

export EM_CONFIG=~/.emscripten

cmake --build bin 2>&1 | tee compile.log
cp bin/index.* docs
cp bin/unittests.* docs
cp bin/simple.* docs
