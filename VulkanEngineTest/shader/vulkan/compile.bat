@echo off
chcp 65001 >nul

echo ============================================
echo           正在编译着色器
echo.

REM 获取脚本所在目录
set "SCRIPT_DIR=%~dp0"
echo bat directory: %SCRIPT_DIR%
cd /d "%SCRIPT_DIR%"

REM 检查 test.exe 是否存在
if exist "glslc.exe" (
    echo.
    
    REM 开始编译
    if no exist ".\SPV\FirstTriangle.vert.spv"(
        glslc.exe .\GLSL\FirstTriangle.vert -o .\SPV\FirstTriangle.vert.spv
    )
    if no exist ".\SPV\FirstTriangle.frag.spv"(
        glslc.exe .\GLSL\FirstTriangle.frag -o .\SPV\FirstTriangle.frag.spv
    )
    echo shader FirstTriangle done...
    
    REM 检查执行结果
    if errorlevel 1 (
        echo.
        echo glslc.exe error ,code: %errorlevel%
    ) else (
        echo.
        echo done.
    )
) else (
    echo error: not found glslc.exe!
    echo.
    echo List of files in the current directory:
    dir /b
)
