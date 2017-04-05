REM Copy those build scripts to a different folder and build Qt there.
REM I recommend you to build QT on a HDD, not your main SSD.

set _TMPOUTPATH=D:/qt_compile/

if exist _user.bat (
    call _user.bat
)

mkdir "%_TMPOUTPATH%"