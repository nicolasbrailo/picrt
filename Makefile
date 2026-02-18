CC = aarch64-linux-gnu-gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lm

picrt: picrt.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f picrt

deploy: picrt
	rsync -az picrt check_sdtv.sh batman@10.0.0.114:/home/batman/

run:
	ssh batman@10.0.0.114 /home/batman/picrt

.PHONY: clean deploy run
