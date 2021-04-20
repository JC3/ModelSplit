# test_exporter_crashes

This does not actually require Qt to build. I've only tested it on Windows, built with MSVC 2019 64-bit.

## Usage:

```
test_exporter_crashes -r [ <modelfile> ]
test_exporter_crashes -t <format_id> [ <modelfile> ]

  -r         Run test for every exporter.
  -t         Run test for exporter <format_id>.
  modelfile  Use as test model, otherwise generates tetrahedron.
             Make sure model file name doesn't contain double quotes!
```

There are some test models in the models subdirectory. Reports from my own tests are in the reports subdirectory. Here are some example commands:

To reproduce https://github.com/assimp/assimp/issues/3778:

    test_exporter_crashes -r

To test with one of the test models:

    test_exporter_crashes -r models/terrain.ply
    test_exporter_crashes -r models/tetra.obj
    test_exporter_crashes -r models/torus.dae

If the debugger pops up assertion failure dialogs, just ignore them -- individual tests are done in separate processes.

## Test Models:

- terrain.ply: A large model with color information and normals. Tests will be slow.
- tetra.obj: A hand-created obj file that is theoretically the same as the one the test program generates in `buildObject()`.
- torus.dae: A low-res torus with color information and normals.
