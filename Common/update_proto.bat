protoc.exe --cpp_out=. remote_protobuf.proto
protoc.exe --cpp_out=. autodj.proto
cd ..\Client5
..\Common\protoc.exe --cpp_out=. client5_savefile.proto

pause