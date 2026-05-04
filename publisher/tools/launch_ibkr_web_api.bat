@echo OFF
set dir=%cd%
cd "C:\Program Files\IBKR\clientportal.gw"
START bin\run.bat root\conf.yaml
echo Please Open https://localhost:5000 and authenticate the client
pause
cd "%dir%"
python ibkr_web_api_auth_status.py