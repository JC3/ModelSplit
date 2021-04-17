The subdirectories here are:

- **modelsplit**: Main application.
- **generate_test_models**: A tool for generating some test model files.
- **3rdparty**: 3rd-party stuff (like libassimp).

# ModelSplit

Splits 3D model files into connected components. Current implementation is intended to be used as a shell context menu extension for 3D model files in Windows.

This takes, as input, a 3D model file (in any format supported by *libassimp*), splits isolated components and produces, as output, a new file for each component. 

## Command Line Parameters

```
Usage: modelsplit.exe [options] input

Options:
  -?, -h, --help  Displays help on commandline options.
  --help-all      Displays help including Qt specific options.
  -v, --version   Displays version information.
  --no-unify      Do not unify duplicate vertices.
  -y              Always overwrite, don't prompt.
  -n              Never overwrite, don't prompt (overrides -y).
  -f <format>     Output format (default is same as input, or OBJ if
                  unsupported).
  -F              Pick output format from dialog (overrides -f).
  --register      Register shell context menus for model file types.
  --unregister    Deregister shell context menus created by --register.
  --elevate       Try to elevate to admin if [un]register fails.
  --quieter       Don't show message box on success (for --[un]register).

Arguments:
  input           Input file name.
```

Simplest example:

    modelsplit lots_of_parts.stl
    
That will take *lots_of_parts.stl* and produce *lots_of_parts-NNNN.stl* for each distinct object in the input file.

## Shell Extension Setup

To install context menus:

    modelsplit --register --elevate
    
To uninstall context menus:

    modelsplit --unregister --elevate
    
When installed, files with extensions supported by *libassimp* will have "Split 3D Model..." added to their context menus in Explorer.

If `--elevate` doesn't work, just leave that parameter off and run modelsplit from some administrator context somewhere instead (like an administrative command prompt).

Bug: It doesn't work for some extensions because the Windows registry is confusing. This will be fixed eventually.
