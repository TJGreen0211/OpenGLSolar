@echo off
setlocal
cd /d "%~dp0"

set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    echo [build] Visual Studio 2022 Build Tools not found.
    exit /b 1
)
call "%VCVARS%" >nul || (echo [build] vcvars64 failed & exit /b 1)

set "GLFW_ROOT=third_party\glfw\glfw-3.4.bin.WIN64"
set "GLAD_ROOT=third_party\glad\gladgen"
if not exist "%GLFW_ROOT%\include\GLFW\glfw3.h" (
    echo [build] Missing %GLFW_ROOT% - run setup_third_party first.
    exit /b 1
)
if not exist "%GLAD_ROOT%\include\glad\gl.h" (
    echo [build] Missing %GLAD_ROOT% - run setup_third_party first.
    exit /b 1
)

if not exist build\obj mkdir build\obj

cl /nologo /W3 /O2 /MD /DWIN32 /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS ^
  /Isrc /Isrc\render /Isrc\ui /Isrc\geometry /I"third_party\soil\src" /I"%GLFW_ROOT%\include" /I"%GLAD_ROOT%\include" ^
  /Fobuild\obj\ /Fesolarsystem.exe ^
  src\Main.c src\render\Common.c src\render\LoadShader.c src\ui\GUI.c src\geometry\LinearAlg.c src\ui\Camera.c src\geometry\Sphere.c src\geometry\Geometry.c src\geometry\Obj.c ^
  "%GLAD_ROOT%\src\gl.c" "third_party\soil\src\SOIL.c" "third_party\soil\src\stb_image_aug.c" "third_party\soil\src\image_helper.c" "third_party\soil\src\image_DXT.c" ^
  /link /LIBPATH:"%GLFW_ROOT%\lib-vc2022" glfw3dll.lib opengl32.lib gdi32.lib user32.lib shell32.lib winmm.lib
if errorlevel 1 (
    echo [build] FAILED
    exit /b 1
)

copy /y "%GLFW_ROOT%\lib-vc2022\glfw3.dll" . >nul
echo [build] OK: solarsystem.exe
echo [build] Run: run.bat  (or double-click solarsystem.exe - it self-locates assets)
