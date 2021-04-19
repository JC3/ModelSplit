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

There are some test models in the models subdirectory. Reports from my own tests are in the reports subdirectory.
