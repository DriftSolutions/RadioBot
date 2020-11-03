cd %1
echo src: %2
echo dest1: %3
IF NOT ()==(%4) echo dest2: %4
add_checksum.exe %2
copy /y %2 %3
IF %ERRORLEVEL% NEQ 0 EXIT %ERRORLEVEL%
IF NOT ()==(%4) copy /y %2 %4
IF NOT ()==(%4) IF %ERRORLEVEL% NEQ 0 EXIT %ERRORLEVEL%
