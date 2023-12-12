disk_image: disk_image.c
	gcc -o disk_image disk_image.c

SUBMIT_FILENAME=nts7bcj.mck7py.cmv5xbf.smd8xna.inodes.tar.gz

archive:
	tar -zcf $(SUBMIT_FILENAME) $(wildcard *.cc *.h *.hh *.H *.cpp *.C *.c *.txt *.md *.pdf) Makefile 
	@echo "Created $(SUBMIT_FILENAME); please upload and submit this file."

submit: archive

clean:
	rm -f *.o