@echo off
cd /d "%~dp0\.."

REM Add runtime DLLs to PATH so main.exe can find them
set PATH=%cd%\ten_packages\system\ten_runtime_go\lib;%cd%\ten_packages\system\ten_runtime\lib;%PATH%

REM Find libpython using find_libpython.py and set TEN_PYTHON_LIB_PATH
if exist "ten_packages\system\ten_runtime_python\tools\find_libpython.py" (
    for /f "delims=" %%i in ('python3 ten_packages\system\ten_runtime_python\tools\find_libpython.py') do set TEN_PYTHON_LIB_PATH=%%i
)

set PYTHONPATH=%cd%;%cd%\ten_packages\system\ten_ai_base\interface
set NODE_PATH=%cd%\ten_packages\system\ten_runtime_nodejs\lib;%NODE_PATH%

bin\main.exe
