## Description

```
Usage: suspicious_3mf_materials.exe [ -v | -vv ] <filename>

  Imports <filename>, exports to 3MF, imports that, re-exports, and
  one more time for good luck.

  Produces output-[123].3mf, input.assxml, and output-[123].assxml.

  -v    Slightly more verbose output.
  -vv   Slightly more verbose output + print assimp logs.
```

There are some test models in the models/ subdirectory.

## Example Output

When run on box.3mf. Output illustrates one manifestation of https://github.com/assimp/assimp/issues/3830 (note difference in materials with each pass):

```
assimp version 5.0.1 (master @ cd42b995)

[IMPORT] models/box.3mf
Info,  T13880: Load models/box.3mf
Debug, T13880: Assimp 5.0.3443702165 amd64 msvc debug shared singlethreadedsingle :
Info,  T13880: Found a matching importer for this file format: 3mf Importer.
Info,  T13880: Import root directory is 'models\'
Warn,  T13880: Ignored file of unknown type: 3D/3dmodel.model
Warn,  T13880: Ignored file of unsupported type CONTENT_TYPES_ARCHIVES[Content_Types].xml
Debug, T13880: 3D/3dmodel.model
Debug, T13880: UpdateImporterScale scale set: %f1
Debug, T13880: ScenePreprocessor: Adding default material 'DefaultMaterial'
  <SCENE> '' meshes=1 materials=1 textures=0
    <MATERIAL 0> 'DefaultMaterial'
      <TEXTURES> none present
      <PROPERTIES> count=2 capacity=5
        <PROPERTY> '$clr.diffuse' type=1 len=12 data={ 9A 99 19 3F 9A 99 19 3F ... }
        <PROPERTY> '?mat.name' type=3 len=20 data={ 0F 00 00 00 44 65 66 61 ... }
    <MESH 0> '1' verts=8 faces=12
      <MATERIAL> #0 (DefaultMaterial)
      <VCOLORS> none present
      <TEXCOORDS> none present
    <NODE> '3MF' meshes=0
      <NODE> 'Object_1' meshes=1
        <MESH> #0 (1)
  [EXPORT] input.assxml
  [EXPORT] output-1.3mf

[IMPORT] output-1.3mf
Info,  T13880: Load output-1.3mf
Debug, T13880: Assimp 5.0.3443702165 amd64 msvc debug shared singlethreadedsingle :
Info,  T13880: Found a matching importer for this file format: 3mf Importer.
Info,  T13880: Import root directory is '.\'
Warn,  T13880: Ignored file of unknown type: 3D/3DModel.model
Warn,  T13880: Ignored file of unsupported type CONTENT_TYPES_ARCHIVES[Content_Types].xml
Debug, T13880: 3D/3DModel.model
Debug, T13880: UpdateImporterScale scale set: %f1
  <SCENE> '' meshes=1 materials=1 textures=0
    <MATERIAL 0> 'id1_DefaultMaterial'
      <TEXTURES> none present
      <PROPERTIES> count=1 capacity=5
        <PROPERTY> '?mat.name' type=3 len=24 data={ 13 00 00 00 69 64 31 5F ... }
    <MESH 0> '2' verts=8 faces=12
      <MATERIAL> #0 (id1_DefaultMaterial)
      <VCOLORS> none present
      <TEXCOORDS> none present
    <NODE> '3MF' meshes=0
      <NODE> 'Object_2' meshes=1
        <MESH> #0 (2)
  [EXPORT] output-1.assxml
  [EXPORT] output-2.3mf

[IMPORT] output-2.3mf
Info,  T13880: Load output-2.3mf
Debug, T13880: Assimp 5.0.3443702165 amd64 msvc debug shared singlethreadedsingle :
Info,  T13880: Found a matching importer for this file format: 3mf Importer.
Info,  T13880: Import root directory is '.\'
Warn,  T13880: Ignored file of unknown type: 3D/3DModel.model
Warn,  T13880: Ignored file of unsupported type CONTENT_TYPES_ARCHIVES[Content_Types].xml
Debug, T13880: 3D/3DModel.model
Debug, T13880: UpdateImporterScale scale set: %f1
  <SCENE> '' meshes=1 materials=1 textures=0
    <MATERIAL 0> 'id1_id1_DefaultMaterial'
      <TEXTURES> none present
      <PROPERTIES> count=2 capacity=5
        <PROPERTY> '?mat.name' type=3 len=28 data={ 17 00 00 00 69 64 31 5F ... }
        <PROPERTY> '$clr.diffuse' type=1 len=16 data={ 00 00 80 3F 00 00 80 3F ... }
    <MESH 0> '2' verts=8 faces=12
      <MATERIAL> #0 (id1_id1_DefaultMaterial)
      <VCOLORS> none present
      <TEXCOORDS> none present
    <NODE> '3MF' meshes=0
      <NODE> 'Object_2' meshes=1
        <MESH> #0 (2)
  [EXPORT] output-2.assxml
  [EXPORT] output-3.3mf

[IMPORT] output-3.3mf
Info,  T13880: Load output-3.3mf
Debug, T13880: Assimp 5.0.3443702165 amd64 msvc debug shared singlethreadedsingle :
Info,  T13880: Found a matching importer for this file format: 3mf Importer.
Info,  T13880: Import root directory is '.\'
Warn,  T13880: Ignored file of unknown type: 3D/3DModel.model
Warn,  T13880: Ignored file of unsupported type CONTENT_TYPES_ARCHIVES[Content_Types].xml
Debug, T13880: 3D/3DModel.model
Debug, T13880: UpdateImporterScale scale set: %f1
  <SCENE> '' meshes=1 materials=1 textures=0
    <MATERIAL 0> 'id1_id1_id1_DefaultMaterial'
      <TEXTURES> none present
      <PROPERTIES> count=2 capacity=5
        <PROPERTY> '?mat.name' type=3 len=32 data={ 1B 00 00 00 69 64 31 5F ... }
        <PROPERTY> '$clr.diffuse' type=1 len=16 data={ 00 00 80 3F 00 00 80 3F ... }
    <MESH 0> '2' verts=8 faces=12
      <MATERIAL> #0 (id1_id1_id1_DefaultMaterial)
      <VCOLORS> none present
      <TEXCOORDS> none present
    <NODE> '3MF' meshes=0
      <NODE> 'Object_2' meshes=1
        <MESH> #0 (2)
  [EXPORT] output-3.assxml

[FINISHED]
```