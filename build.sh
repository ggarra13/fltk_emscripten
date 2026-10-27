set -e

export EM_CONFIG=~/.emscripten

cmake --build bin
cp bin/index.* docs
cp bin/unittests.* docs
cp bin/simple.* docs
