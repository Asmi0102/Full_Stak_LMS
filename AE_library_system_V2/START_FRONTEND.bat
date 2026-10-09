@echo off
title AE Library — Frontend
echo Starting Frontend on http://localhost:3000 ...
echo.
echo  Landing Page : http://localhost:3000/index.html
echo  Staff Login  : http://localhost:3000/staff_login.html
echo  Admin Login  : http://localhost:3000/admin_login.html
echo.
cd /d "%~dp0frontend"
python -m http.server 3000
