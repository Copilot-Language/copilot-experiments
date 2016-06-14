bin/player: player/*.c include/*.h
	make -C player
	mv player/player bin/player

bin/listener: listener/*.c include/*.h
	make -C listener
	mv listener/listener bin/listener

clean:
	-make clean -C player
	-rm bin/player
	-make clean -C listener
	-rm bin/listener
