@echo off
rem // Runs CMake to configure htm.core for Visual Studio 2017 and 2019
rem // Execute this file to start up CMake and configure Visual Studio.
rem //
rem // There is no need to use Developer Command Prompt for running CMake with 
rem // Visual Studio generators, corresponding environment will be loaded automatically by CMake.
rem // 
rem // Prerequisites:
rem //
rem //     Microsoft Visual Studio 2017, 2019 or newer (any flavor)
rem //
rem //     CMake: 3.7+ Download CMake from https://cmake.org/download/
rem //            NOTE: CMake 3.14+ is required for MSCV 2019
rem //                  CMake 3.21+ is required for MSCV 2022
rem //
rem //     Python 2.7 or 3.x Download from https://www.python.org/downloads/windows/  (optional)
rem // 
rem //
rem //   This script will create a Vsual Studio solution file at build/scripts/htm_core.sln
rem //   Double click htm_core.sln to start up Visual Studio.  Then perform a full build.
rem //
rem //   Note: if you were originally using this repository with VS 2017 and you now
rem //         want to use VS 2019, Do the following:
rem //           1) delete the build folder in the repository.  This will remove the .sln and other VS files.
rem //           2) run startupMSVC.bat again.   It will detect that VS 2019 is installed and configure for it but if not it will start VS 2017.
rem //           3) after .sln is re-created in build/scripts/, right click, on the .sln file, select "open with", click "choose another app", select VS 2019
rem //         The same applies for VS 2022.
rem //
rem // Tricks for executing Visual Studio in Release or Build mode.
rem // https://stackoverflow.com/questions/24460486/cmake-build-type-not-being-used-in-cmakelists-txt

  
:CheckCMake
rem  // make sure CMake is installed.  (version 3.7 minimum)
cmake -version > cmake_version.txt 
if %errorlevel% neq 0 (
  @echo startup.bat;  CMake was not found. 
  @echo Download and install from  https://cmake.org/download/
  @echo Make sure its path is in the system PATH environment variable.
  pause
  del cmake_version.txt
  exit /B 1
)
set /p ver=<cmake_version.txt
set "ver=%ver:* =%"
set "ver=%ver:* =%"
set "ver=%ver:.= %"
for /f "tokens=1*" %%a in ("%ver%") do set pri=%%a & set ver=%%b
for /f "tokens=1*" %%a in ("%ver%") do set sec=%%a
set /a cmake_version = pri*100 + sec
del cmake_version.txt
rem echo "cmake version=%cmake_version%"

rem // position to full path of HTM_BASE (the repository)
pushd %~dp0
set HTM_BASE=%CD%
@echo HTM_BASE=%HTM_BASE%
if not exist "%HTM_BASE%\CMakeLists.txt" (
  @echo CMakeList.txt not found in base folder.
  popd
  pause
  exit /B 1
)


:Build
set BUILDDIR=build\scripts
if not exist "%BUILDDIR%" (
  mkdir "%BUILDDIR%"
)
cd "%BUILDDIR%"

rem // If htm_core.sln already exists, just startup Visual Studio
if exist "htm_core.sln" (
  htm_core.sln
  popd
  exit /B 0
)

if not exist "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" (
  @echo Is Visual Studio installed?
  popd
  pause
  exit /B 1
)

"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -legacy -prerelease -format json > vswhereTmp.jsn
find /c "VisualStudio.15" vswhereTmp.jsn > vswhereTmp.cnt && (set Has2017=1) || (set Has2017=0)
find /c "VisualStudio.16" vswhereTmp.jsn > vswhereTmp.cnt && (set Has2019=1) || (set Has2019=0)
find /c "VisualStudio.17" vswhereTmp.jsn > vswhereTmp.cnt && (set Has2022=1) || (set Has2022=0)
del vswhereTmp.*

rem // Run CMake using the Visual Studio generator.  The generator can be one of these.
rem //   cmake -G "Visual Studio 15 2017 Win64"
rem //   cmake -G "Visual Studio 16 2017 ARM"
rem //
rem //   cmake -G "Visual Studio 16 2019" -A x64
rem //   cmake -G "Visual Studio 16 2019" -A ARM64
rem //      NOTE: MSVC 2019 tool set generator requires CMake V3.14 or greater)
rem //
rem //   cmake -G "Visual Studio 17 2022" -A x64
rem //   
rem //  arguments:
rem //   -G "Visual Studio 16 2022" -A x64   Sets the generator toolset and platform (compilers/linkers) to use. 
rem //   -Thost=x64                  Tell CMake to tell VS to use 64bit tools for compiler and linker
rem //   --config "Release"          Start out in Release mode
rem //   -DCMAKE_CONFIGURATION_TYPES="Debug;Release"   Specify the build types allowed.
rem //   ../..                       set the source directory (top of repository)
rem //
rem //  NOTE: only 64bit compiles supported.

if %Has2022%==1 (
    if %cmake_version% GEQ 321 (
      cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_CONFIGURATION_TYPES="Release;Debug"  ../..
    ) else (
          @echo Visual Studio 2022 requires CMake 3.21 or higher.
          popd
          pause
          exit /B 1
    )
) else (
    if %Has2019%==1 (
        if %cmake_version% GEQ 314 (
            cmake -G "Visual Studio 16 2019" -A x64 -DCMAKE_CONFIGURATION_TYPES="Release;Debug"  ../..
        ) else (
              @echo Visual Studio 2019 requires CMake 3.14 or higher.
              popd
              pause
              exit /B 1
        )
    ) else (
        if %Has2017%==1 (
          if %cmake_version% GEQ 307 (
              cmake -G "Visual Studio 15 2017 Win64" -DCMAKE_CONFIGURATION_TYPES="Release;Debug"  ../..
          ) else (
                @echo Visual Studio 2017 requires CMake 3.7 or higher.
                popd
                pause
                exit /B 1
          )
        ) else (
          @echo Did not fond Visual studio 2017, 2019 or 2022.
          popd
          pause
          exit /B 1
        )
    )
)

  
if exist "htm_core.sln" (
    @echo " "
    @echo You can now start Visual Studio using solution file %HTM_BASE%\build\scripts\htm_core.sln
    @echo Dont forget to set your default Startup Project to unit_tests.
    @echo Press any key to start Visual Studio 
    pause >nul

    rem // Location is %HTM_BASE%\build\scripts\htm_core.sln
    htm_core.sln

    popd
    exit /B 0
) else (
    @echo An error occured. Correct problem. Delete %HTM_BASE%\build before trying again.
    popd
    pause
    exit /B 1
)
  



