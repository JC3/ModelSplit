The subdirectories here are:

- **modelsplit**: Main application.
- **generate_test_models**: A tool for generating some test model files.
- **3rdparty**: 3rd-party stuff (like libassimp).

# ModelSplit

Splits 3D model files into connected components. Current implementation is intended to be used as a shell context menu extension for 3D model files in Windows.

This takes, as input, a 3D model file (in any format supported by *libassimp*), splits isolated components and produces, as output, a new file for each component. 

See *modelsplit/README.md* for more info.

---

*Model Split, Copyright (c) 2021, Jason Cipriani, All rights reserved.*
*Open Asset Import Library (assimp), Copyright (c) 2006-2016, assimp team, All rights reserved.*
*Poly2Tri Copyright (c) 2009-2010, Poly2Tri Contributors, http://code.google.com/p/poly2tri/, All rights reserved.*
