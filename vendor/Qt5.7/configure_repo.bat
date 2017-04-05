call configure.bat -static -static-runtime -debug-and-release -mp ^
    -opensource -nomake examples -nomake tests ^
    -opengl desktop -prefix %_TMPOUTPATH%