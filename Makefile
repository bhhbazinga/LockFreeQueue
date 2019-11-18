CXX = g++
CXXFLAGS = -Wall -Wextra -pedantic -std=c++17 -g -fsanitize=address -fno-omit-frame-pointer -fsanitize=leak

SRC = test.cc
OBJ = $(SRC:.cc=.o)
EXEC = test
LBLIBS = -latomic -lpthread
DEFS = -DLOCKFREE_QUEUE_MAX_THREADS=5
CXXFLAGS += $(DEFS)

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ) $(LBLIBS)

clean:
	rm -rf $(OBJ) $(EXEC)