CXX = g++
CXXFLAGS = -Wall -Wextra -pedantic -std=c++11 -g -O3
#-fsanitize=thread
#-fsanitize=address -fsanitize=leak 
SRC = test.cc
OBJ = $(SRC:.cc=.o)
EXEC = test
LBLIBS = -latomic -lpthread

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ) $(LBLIBS)

clean:
	rm -rf $(OBJ) $(EXEC)
