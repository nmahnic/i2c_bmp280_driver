exec=pruebaI2C

all:
	mkdir -p obj
	gcc -c src/$(exec).c -o obj/$(exec).o
	gcc obj/$(exec).o -o $(exec)

exec:
	clear
	make all
	./$(exec)

clean:
	rm -R -f obj
	rm -f $(exec)