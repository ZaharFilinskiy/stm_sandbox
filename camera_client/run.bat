@echo off
setlocal
where py >nul 2>nul
if not errorlevel 1 (
    py -3 "%~dp0run.py" %*
    exit /b
)
where python >nul 2>nul
if not errorlevel 1 (
    python "%~dp0run.py" %*
    exit /b
)
echo Python 3.10+ is required. Install Python with tkinter and enable Add Python to PATH.
exit /b 1
