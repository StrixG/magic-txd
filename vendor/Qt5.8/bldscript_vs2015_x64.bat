set QTOUTNAME="qt58_static_x64_vs2015"

REM Set up the compiler environment.
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\amd64\vcvars64.bat"

REM Set Qt build target (IMPORTANT).
set QTPLAT_PUSH="win32-msvc2015"
set QTPLAT_MSC_VER=1899

call bldscript.bat