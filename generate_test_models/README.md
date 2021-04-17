# generate_test_models

Generates some test models in various formats for testing modelsplit.

## current issues

- [stl] must hand-fix corrupt file (fixed in assimp dev branch, will be in next release)
- [stp] fails to import: a node of the scenegraph is null
- [gltf2] [glb2] [gltf] [glb] needs guessInputFormat disambiguation (all detected as "glb")
- [assxml] no context menu (.assxml)
- [fbx] [fbxa] needs guessInputFormat disambiguation (all detected as "fbx")
- [3mf] fails to import: validation failed: mNumMeshes == 0
- [assjson] no context menu (.json)

## import formats without matching exporters

