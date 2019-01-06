CXX=g++
CXXFLAGS=-c -std=c++11 -O0
DMYSQL_CXXFLAGS=-stdlib=libc++
LDLIBS=-lpthread -lmysqlclient -lSDL -largon2
SOURCES:=$(wildcard *.cpp)
OBJECTS:=$(patsubst %.cpp,%.o,$(SOURCES))
EXECUTABLE=skoserver-dev

all : $(EXECUTABLE)

$(EXECUTABLE) : $(OBJECTS)
	$(CXX) $(OBJECTS) -g -o $@ $(LDLIBS)

$(OBJECTS) : %.o : %.cpp
	$(CXX) $(CXXFLAGS) $< -g -o $@

clean :
	$(RM) $(OBJECTS) $(EXECUTABLE)

