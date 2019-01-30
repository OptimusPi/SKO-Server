CXX=g++
CXXFLAGS=-c -g -std=c++11 -O3
DMYSQL_CXXFLAGS=-stdlib=libc++
LDLIBS=-lmysqlclient -lSDL -largon2 -lpthread
SOURCES:=$(wildcard *.cpp)
OBJECTS:=$(patsubst %.cpp,%.o,$(SOURCES))
EXECUTABLE=skoserver-dev

all : $(EXECUTABLE)

$(EXECUTABLE) : $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDLIBS)

$(OBJECTS) : %.o : %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean :
	$(RM) $(OBJECTS) $(EXECUTABLE)

