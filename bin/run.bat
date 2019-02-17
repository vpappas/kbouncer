@echo OFF

set drv=%~dp0\kb.sys

if not exist %drv% set drv=%~dp0\sys\objfre_win7_amd64\amd64\kb.sys

echo using driver %drv%

echo creating the kb service
sc.exe create kb type= kernel binpath= %drv%
if %errorlevel% neq 0 exit /b %errorlevel%

echo starting the kb service
sc.exe start kb
if %errorlevel% neq 0 exit /b %errorlevel%

echo press enter to stop
pause

echo stopping the kb service
sc.exe stop kb
if %errorlevel% neq 0 exit /b %errorlevel%

echo deleting the kb service
sc.exe delete kb
if %errorlevel% neq 0 exit /b %errorlevel%
