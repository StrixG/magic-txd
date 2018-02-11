set QTOUTNAME="qt510_static_x86_vs2015"

REM Set up the compiler environment.
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\vcvars32.bat"

REM Set Qt build target (IMPORTANT).
set QTPLAT_PUSH="win32-msvc2015"
set QTPLAT_MSC_VER=1899

call bldscript.bat