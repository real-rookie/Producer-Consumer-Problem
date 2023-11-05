CC = g++
C_FLAGS = -c -std=c++11 -g -Wall -O0 -pthread
main = prodcon
tands = tands

$(main): $(main).o $(tands).o
	$(CC) -o $@ $^
$(main).o: $(main).cpp
	$(CC) $(C_FLAGS) $<
$(tands).o: $(tands).c
	$(CC) $(C_FLAGS) $<

run:
	./$(main)
memcheck:
	valgrind --tool=memcheck --leak-check=full ./$(main)
clean:
	rm -f $(main) *.o 