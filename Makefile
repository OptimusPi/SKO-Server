CXX=g++
CXXFLAGS=-c -Wall -g -D_GLIBCXX_USE_NANOSLEEP -std=c++11 -O3
DMYSQL_CXXFLAGS=-stdlib=libc++
LDLIBS=-lmysqlclient -largon2 -lpthread
SOURCES:=$(wildcard main.cpp) $(wildcard SKO_Game/*.cpp) $(wildcard SKO_Network/*.cpp) $(wildcard SKO_Physics/*.cpp)  $(wildcard SKO_Repository/*.cpp) $(wildcard SKO_Utilities/*.cpp) 
OBJECTS:=$(patsubst %.cpp,%.o,$(SOURCES))
EXECUTABLE=skoserver-dev

all : $(EXECUTABLE)

$(EXECUTABLE) : $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDLIBS)

$(OBJECTS) : %.o : %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean :
	$(RM) $(OBJECTS) $(EXECUTABLE)

