all:
	gcc -Wall -g segmenter.c -o segmenter -lavformat -lavcodec -lavutil -lva

clean:
	rm segmenter

install: segmenter
	cp segmenter /usr/local/bin/

uninstall:
	rm /usr/local/bin/segmenter
