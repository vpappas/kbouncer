@echo off

set WDK_ROOT=C:\WinDDK\7600.16385.1
set WDK_SETENV=%WDK_ROOT%\bin\setenv.bat %WDK_ROOT% fre x64 WIN7

set SDK_ROOT=C:\Program Files\Microsoft SDKs\Windows\v7.1
set SDK_SETENV=%SDK_ROOT%\bin\setenv.cmd

set KB_ROOT=C:\kbouncer

set OUT_DIR=.\bin


echo will put binary files in '%OUT_DIR%'

:: if exist %OUT_DIR%\nul (
:: 	echo %OUT_DIR% already exists!
:: 	exit /b
:: )

mkdir %OUT_DIR%

echo compiling the driver

cmd /c "call %WDK_SETENV% & cd %KB_ROOT%\windows\drv & build"

echo compiling the detour

cmd /c "call "%SDK_SETENV%" /x86 & cd %KB_ROOT%\windows\detour & nmake"

echo copying files

copy %KB_ROOT%\windows\drv\sys\objfre_win7_amd64\amd64\kb.sys %OUT_DIR%
copy %KB_ROOT%\windows\drv\exe\objfre_win7_amd64\amd64\test.exe %OUT_DIR%
copy %KB_ROOT%\windows\drv\run.bat %OUT_DIR%

copy %KB_ROOT%\windows\detour\root\bin.x86\kb.dll %OUT_DIR%
copy %KB_ROOT%\windows\detour\root\bin.x86\withdll.exe %OUT_DIR%

copy %KB_ROOT%\windows\exploits\reader\*.pdf %OUT_DIR%
copy %KB_ROOT%\windows\exploits\mplayer\*.m3u %OUT_DIR%


