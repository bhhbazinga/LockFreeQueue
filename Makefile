CXX = g++
CXXFLAGS = -Wall -Wextra -pedantic -std=c++2a -g -o3
#-fsanitize=thread
#-fsanitize=address -fsanitize=leak 

EXEC = test
LBLIBS = -lpthread

all : $(EXEC)

$(EXEC): test.cc lockfree_queue.h HazardPointer/reclaimer.h
	$(CXX) $(CXXFLAGS) -o $(EXEC) test.cc $(LBLIBS)

HazardPointer/reclaimer.h:
	git submodule update --init --recursive --remote

.Phony: clean

clean:
	rm -rf $(EXEC)
