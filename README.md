# AoikProgramLauncher
AoikProgramLauncher is a Windows executable that launches another program according to the command specified in its config file.

The intended use case is when you have a program file that is not directly executable, e.g. a Python file `main.py`, instead of typing command `python main.py` in console, you can put the command to AoikProgramLauncher's config file and then double click AoikProgramLauncher to launch the program.

## Usage
Build the project in Visual Studio 2015 to get `AoikProgramLauncher.exe`.

When double click `AoikProgramLauncher.exe` to run it, it will look for config 
file `AoikProgramLauncher_config.json` in the same directory. If you rename
`AoikProgramLauncher.exe` to `my_program_launcher.exe`, it will look for config
file `my_program_launcher_config.json` instead.

The config file's format is:
```
{
    "CMD": [
        "D:/Software/Python/python.exe",
        "D:/Software/SomePythonProgram/main.py",
    ],
    "SHOW_CONSOLE": 0
}
```
- `CMD` specifies the file path and arguments of the program to launch.
- `SHOW_CONSOLE` specifies whether a console window is shown for the launched program.
