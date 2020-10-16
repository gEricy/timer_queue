def:
	gcc  -Wall -g  -c -o  timerqueue.o timerqueue.c -lpthread -lrt
	gcc  -Wall -g  -c -o  main.o main.c -lpthread -lrt

	gcc  -Wall -g  -o  main main.o timerqueue.o -lpthread -lrt
check:
	valgrind --leak-check=full -v ./main

clean: 
	rm -rf *.o
	rm -rf main

.PHONY: def clean ut test

