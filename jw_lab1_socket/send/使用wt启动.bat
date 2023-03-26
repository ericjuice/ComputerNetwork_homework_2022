@REM #启动1个server,3个client
start "server" wt .\chat_server.exe
start "client1" wt .\chat_client.exe
start "client2" wt .\chat_client.exe
start "client3" wt .\chat_client.exe