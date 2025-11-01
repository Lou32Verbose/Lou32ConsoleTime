@echo off
setlocal

REM Try to find and initialize Visual Studio environment
set "VS_PATH="

REM Try Visual Studio 2022 (Community, Professional, Enterprise)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
    goto :found
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat"
    goto :found
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
    goto :found
)

REM Try Visual Studio 2019
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat"
    goto :found
)
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvarsall.bat"
    goto :found
)
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
    goto :found
)

REM Try Visual Studio 2017
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat"
    goto :found
)
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvarsall.bat"
    goto :found
)
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
    goto :found
)

REM If not found, try using vswhere (Visual Studio Installer)
where vswhere.exe >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    for /f "usebackq tokens=*" %%i in (`vswhere.exe -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        if exist "%%i\VC\Auxiliary\Build\vcvarsall.bat" (
            set "VS_PATH=%%i\VC\Auxiliary\Build\vcvarsall.bat"
            goto :found
        )
    )
)

echo Error: Could not find Visual Studio Developer Command Prompt.
echo Please ensure Visual Studio is installed or run this script from a Developer Command Prompt.
exit /b 1

:found
echo Initializing Visual Studio Developer Command Prompt...
call "%VS_PATH%" x64 >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    call "%VS_PATH%" x86 >nul 2>&1
    if %ERRORLEVEL% NEQ 0 (
        echo Error: Failed to initialize Visual Studio environment.
        exit /b 1
    )
)

echo Building ascii_time.exe...
cl.exe /W4 /O2 /utf-8 /Fe:ascii_time.exe ascii_time.c /link /SUBSYSTEM:CONSOLE

endlocal

