protoc -I=./ --cpp_out=./ stabilizedata.proto
protoc -I=./ --cpp_out=./ trackerdata.proto

mv *.cc ../
mv *.h ../../include/
