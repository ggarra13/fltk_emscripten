#!/usr/bin/env bash

cp bin/index.* ../ggarra13.github.io/fltk_emscripten
cp bin/unittests.* ../ggarra13.github.io/fltk_emscripten
cp bin/simple.* ../ggarra13.github.io/fltk_emscripten

cd ../ggarra13.github.io/fltk_emscripten
git commit -a
git push
cd -

