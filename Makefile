CXX = g++
CXXFLAGS = -Wall -Wextra -pedantic -std=c++17 -g -O2 
#-fsanitize=thread
#-fsanitize=address -fsanitize=leak 
SRC = test.cc
OBJ = $(SRC:.cc=.o)
EXEC = test
LBLIBS = -latomic -lpthread
DEFS = -DMAX_THREADS=4
CXXFLAGS += $(DEFS)

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ) $(LBLIBS)

clean:
	rm -rf $(OBJ) $(EXEC)
