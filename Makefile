CXX=g++
CXXFLAGS=-c -g -std=c++11 -O3
DMYSQL_CXXFLAGS=-stdlib=libc++
LDLIBS=-lmysqlclient -largon2 -lpthread -l:libcryptopp.a
SOURCES:=$(wildcard SKO_Hub/*.cpp) $(wildcard SKO_Game/*.cpp) $(wildcard SKO_Network/*.cpp) $(wildcard SKO_Physics/*.cpp)  $(wildcard SKO_Repository/*.cpp) $(wildcard SKO_Utilities/*.cpp) $(wildcard *.cpp) 
OBJECTS:=$(patsubst %.cpp,%.o,$(SOURCES))
EXECUTABLE=skoserver-dev

all : $(EXECUTABLE)

$(EXECUTABLE) : $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDLIBS)

$(OBJECTS) : %.o : %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean :
	$(RM) $(OBJECTS) $(EXECUTABLE)

