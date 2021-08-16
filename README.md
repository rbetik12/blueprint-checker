# Blueprint checker

Unreal Engine-based utility, that parses uassets with local user engine infrastructure and outputs data stored in it.

## How it works

User can launch program in 3 modes:

- Single file mode (process single uasset file)
- Batch mode (parses list of uasset paths and process them)
- StdIn mode (works as a daemon that waits path to uassets from stdin)

### Launch options
```
OPTION       REQUIRED   DESCRIPTION
===========================================
-h, --help      no      Help
-t, --test      no      Run all tests
-m=, --mode=    no      Utility launch mode [Single|Batch|StdIn]
-f=, --filepath=        no      Path to a file.
In single mode - blueprint path.
In batch mode - filenames file path
In StdIn mode - Launch in daemon-like mode
```

Currently utility outputs content of export map in JSON format to standard output.
All information is gathered from Unreal Engine class `FLinkerLoad` it contains all needed information about uasset object.

### JSON output format

```
{
        "blueprintClasses": [
                {
                        "index": <int>,
                        "objectName": <string>,
                        "className": <string>,
                        "superClassName": <string>
                }
        ],
        "k2VariableSets": [
                {
                        "index": <int>,
                        "objectKind": <string>,
                        "memberName": <string>
                }
        ],
        "otherClasses": [
                {
                        "index": <int>,
                        "className": <string>
                }
        ]
}

```

### Useful links
- `FLinkerLoad` [official 4.26 documentation](https://docs.unrealengine.com/4.26/en-US/API/Runtime/CoreUObject/UObject/FLinkerLoad/)
- [Custom docs about blueprint internal format](https://gist.github.com/rbetik12/21201e3c40201e8f8aed16c4bcf0e75e)

## Tested on
- Windows 10 (MSVC version 19.28.29336)
- Unreal Engine built from sources of 4.26.2-release branch

## How to build
1. Clone the repo like this `<Unreal Engine root directory>/Engine/Programs/Source`
2. Run `GenerateProgramFiles.bat` to generate project file
3. Open your favorite IDE and add BlueprintChecker project from `<Unreal Engine root directory>/Engine/Intermediate/ProjectFiles`
4. Build and run

Program requires **~16 GB** of free memory on the disk, because it uses it own compilation environment and compiles as editor.

## Contacts

All the problems can be addressed in issues, and also to `vitaliy.prikota@jetbrains.com`
