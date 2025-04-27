@echo off

@REM Specify additional directories where the Python should look for modules and
@REM packages.
set PYTHONPATH=%~dp0.gnfiles

set TGN_PATH=%~dp0

@REM Doesn't generate .pyc files.
set PYTHONDONTWRITEBYTECODE=False

python %~dp0build.py %*
