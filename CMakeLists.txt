CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
SET(CMAKE_FIND_LIBRARY_PREFIXES "")
SET(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
FIND_LIBRARY(PROTOBUF_LIBRARY libprotobuf /usr/local/lib)
FIND_LIBRARY(MICROHTTPD_LIBRARY libmicrohttpd /usr/local/lib)
SET(CMAKE_CXX_FLAGS "-std=c++11 -Wno-deprecated-declarations")
SET(CMAKE_EXE_LINKER_FLAGS "-framework CoreFoundation -framework Security")
LINK_DIRECTORIES(/usr/local/lib)
PROJECT(c8tsender)
ADD_EXECUTABLE(c8tsender main.cpp chromecast.cpp playlist.cpp webserver.cpp jsoncpp/dist/jsoncpp.cpp cast_channel.pb.cc)
TARGET_LINK_LIBRARIES(c8tsender ${PROTOBUF_LIBRARY} ${MICROHTTPD_LIBRARY})
INCLUDE_DIRECTORIES(/usr/local/include jsoncpp/dist)
