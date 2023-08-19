
reader.o:
	gcc debuggingReader.c -g -lpthread -lwiringPi -lrt -lcurl -o reader.o

clean: 
	rm *.o
