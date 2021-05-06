#!/bin/bash

find assimp -name "*.h" -exec unix2dos {} \;
find assimp -name "*.hh" -exec unix2dos {} \;
find assimp -name "*.hpp" -exec unix2dos {} \;
find assimp -name "*.inl" -exec unix2dos {} \;
unix2dos assimp/LICENSE assimp/CREDITS

find zip -name "*.c" -exec unix2dos {} \;
find zip -name "*.h" -exec unix2dos {} \;
unix2dos zip/UNLICENSE

find assimp -name "*.pdb" -exec bzip2 -kf9v {} \;
