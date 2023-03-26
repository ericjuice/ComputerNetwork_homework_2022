@REM #启动1个server,3个client
start "server" cmd /k .\chat_server.exe
start "client1" cmd /k .\chat_client.exe
start "client2" cmd /k .\chat_client.exe
start "client3" cmd /k .\chat_client.exe