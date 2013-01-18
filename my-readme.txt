Working on this in VS 2012 and executing on a WinXP SP3 target requires some configuration.

Setting up as a new VS project:
Right click on the project -> Properties -> Configuration Properties -> General -> Platform Toolset -> Visual Studio 2012 - Windows XP (v110_xp)
Configuration Properties -> General -> Character Set -> Use Unicode Character Set
Configuration Properties -> C/C++ -> All Options -> Preprocessor Definitions -> WIN32;NDEBUG;_CONSOLE;%(PreprocessorDefinitions)
Configuration Properties -> Linker -> All Options -> SubSystem -> Console (/SUBSYSTEM:CONSOLE)
Configuration Properties -> Linker -> All Options -> References -> YES (/OPT:REF)
Configuration Properties -> Linker -> All Options -> Enable COMDAT Folding -> Yes (/OPT:ICF)
Configuration Properties -> Linker -> All Options -> Enable Incremental Linking -> No (/INCREMENTAL:NO)
Configuration Properties -> Linker -> All Options -> Additional Dependencies -> ws2_32.lib;%(AdditionalDependencies)

On the target machine, install the Visual Studio 2012 Runtime. Found at http://www.microsoft.com/en-us/download/details.aspx?id=30679

If when you run, it complains about not being able to find MSVCR110D.dll, it is trying to find the debug variant of the dll.
Try rebuilding it with the release target. This assume the target has installed the MS Visual C++ 2012 Redistributable.