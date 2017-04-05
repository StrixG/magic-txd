set QTOUTNAME="qt58_static_x86_vs2017"

REM Search for compilation script.
set SEARCHNAME="vcvars32.bat"
call search_vs2017_file.bat

REM Set up the compiler environment.
call "%VC_FILEFIND%"

REM Set Qt build target (IMPORTANT).
set QTPLAT_PUSH="win32-msvc2017"
set QTPLAT_MSC_VER=1909

call bldscript.bat