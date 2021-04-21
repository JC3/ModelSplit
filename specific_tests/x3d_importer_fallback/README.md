# x3d_importer_fallback

## Usage

    x3d_importer_fallback extension [ extensions ... ]

For each of the supplied extensions, this does two things:

1. Query and print support info for the extension.
2. Create a zero-length dummy file with the extension and attempt to import it, printing info about the results.

## Output

On Windows 10 64-bit, assimp DLL 64-bit, running with "3ds assjson qwerty" produces:

```
assimp 5.0.1 (master @ cd42b995)
Extension: "3ds"
  IsExtensionSupported:       true
  GetImporterIndex:           3
  GetImporter:                plugin=0x0000028831C618D0
  ->GetExtensionList:         3ds prj
  ->GetInfo->mName:           Discreet 3DS Importer
  ->GetInfo->mFileExtensions: 3ds prj
[created zero-length dummy file: dummy.3ds]
  ReadFile (pFlags=0):
    ReadFile:                 error: StreamReader: File is empty or EOF is already reached
    importerIndex:            3
    mName:                    Discreet 3DS Importer
[deleting dummy file]
---------------------------------------------------------------
Extension: "assjson"
  IsExtensionSupported:       false
  GetImporterIndex:           -1
  GetImporter:                plugin=0x0000000000000000
[created zero-length dummy file: dummy.assjson]
  ReadFile (pFlags=0):
    ReadFile:                 success
    importerIndex:            46
    mName:                    Extensible 3D(X3D) Importer
[deleting dummy file]
---------------------------------------------------------------
Extension: "qwerty"
  IsExtensionSupported:       false
  GetImporterIndex:           -1
  GetImporter:                plugin=0x0000000000000000
[created zero-length dummy file: dummy.qwerty]
  ReadFile (pFlags=0):
    ReadFile:                 success
    importerIndex:            46
    mName:                    Extensible 3D(X3D) Importer
[deleting dummy file]
---------------------------------------------------------------
```
