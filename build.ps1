# STOP ON BUILD ERROR
$ErrorActionPreference = 'Stop'

# PREPARE BUILD DIRECTORY
if (Test-Path build) { Remove-Item -Recurse -Path build }
New-Item -ItemType Directory -Path build

# COMPILE EXTERNALS
CL.EXE /std:c++latest /EHsc /Zi /Od /c /Iextern `
    /D_UNICODE=1 /DUNICODE=1 /source-charset:utf-8 /execution-charset:utf-8 `
    extern/m4x1m1l14n/Registry.cpp /Fobuild/ /Fdbuild/

# COMPILE INTERNALS
CL.EXE /std:c++latest /EHsc /Zi /Od /c /Iinclude /Iextern `
    /D_UNICODE=1 /DUNICODE=1 /source-charset:utf-8 /execution-charset:utf-8 `
    src/subprocess.cpp src/network.cpp src/info.cpp src/renderer.cpp src/main.cpp /Fobuild/ /Fdbuild/

# LINK EXECUTABLE
LINK.EXE /DEBUG:FULL build/Registry.obj `
    build/subprocess.obj build/network.obj build/info.obj build/renderer.obj build/main.obj `
    user32.lib Advapi32.lib `
    /OUT:build/holofetch.exe
