@echo off
setlocal

REM Always run VPC to generate the solution
devtools\bin\vpc.exe /fp +game /mksln game_fp.sln

REM Check if the user file exists
if exist "game\client\client_fp.vcxproj.user" (
    echo "Debug setup already exists. Skipping setup."
	pause
    exit /b
)

echo "Installing VDF for Python. Make sure you have Python installed."
echo "Install it using winget"
echo "winget install -e --id Python.Python.3.13"

pip install vdf

echo "Creating auto debug setup for the solution"
py setup_debug.py

pause