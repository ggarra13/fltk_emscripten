set -e

cmake --build bin
cp bin/index.* docs
cp bin/unittests.* docs
cp bin/simple.* docs
