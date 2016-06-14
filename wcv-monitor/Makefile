all: bin/player bin/listener

bin/player: src/*.c include/*.h
	make player -C src
	cp src/player bin/player

bin/listener: src/*.c include/*.h
	make listener -C src
	cp src/listener bin/listener

clean:
	-make clean -C src
	-rm bin/player
	-rm bin/listener
