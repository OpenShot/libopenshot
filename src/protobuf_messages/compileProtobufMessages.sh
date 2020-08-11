protoc -I=./ --cpp_out=./ stabilizedata.proto
protoc -I=./ --cpp_out=./ trackerdata.proto
protoc -I=./ --cpp_out=./ objdetectdata.proto

mv *.cc ../
mv *.h ../../include/
