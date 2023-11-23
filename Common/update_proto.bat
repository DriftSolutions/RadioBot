protoc.exe --cpp_out=. remote_protobuf.proto
protoc.exe --cpp_out=. autodj.proto
cd ..\Client5
..\Common\protoc.exe --cpp_out=. client5_savefile.proto
cd ..\v5\Plugins\Mumble
..\..\..\Common\protoc.exe --cpp_out=proto_win32 mumble.proto

pause