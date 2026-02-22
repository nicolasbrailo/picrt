CC = clang
CFLAGS = -Wall -Wextra -O2 -target aarch64-linux-gnu --sysroot ./rpiz-xcompile/mnt
LDFLAGS = -lm -ljpeg -lcurl -lpthread

SYSROOT = ./rpiz-xcompile/mnt

SRCS = picrt.c img_client/img_client.c img_client/downloader.c img_client/prefetcher.c
HDRS = screen.h img_client/img_client.h img_client/downloader.h img_client/prefetcher.h

picrt: $(SRCS) screen_fb.c $(HDRS) $(SYSROOT)
	$(CC) $(CFLAGS) -o $@ $(SRCS) screen_fb.c $(LDFLAGS)

$(SYSROOT):
	cd rpiz-xcompile && ./mount_rpy_root.sh

picrt-sdl: $(SRCS) screen_sdl.c $(HDRS)
	cc -Wall -Wextra -O2 -o $@ $(SRCS) screen_sdl.c $(shell sdl2-config --cflags --libs) $(LDFLAGS)

clean:
	rm -f picrt picrt-sdl

deploy: picrt check_sdtv.sh setup_env.sh
	rsync -az $< batman@10.0.0.114:/home/batman/

run_setup:
	ssh batman@10.0.0.114 sudo /home/batman/disable_fbcon.sh
run:
	ssh batman@10.0.0.114 /home/batman/picrt
run-l:
	ssh batman@10.0.0.114 /home/batman/picrt -l
run-photo:
	ssh batman@10.0.0.114 /home/batman/picrt -f /home/batman/test.jpg

install_sysroot_deps:
	./rpiz-xcompile/add_sysroot_pkg.sh ./rpiz-xcompile http://deb.debian.org/debian/pool/main/libj/libjpeg-turbo/libjpeg62-turbo_2.1.5-2_arm64.deb
	./rpiz-xcompile/add_sysroot_pkg.sh ./rpiz-xcompile http://deb.debian.org/debian/pool/main/libj/libjpeg-turbo/libjpeg62-turbo-dev_2.1.5-2_arm64.deb
	./rpiz-xcompile/add_sysroot_pkg.sh ./rpiz-xcompile http://deb.debian.org/debian/pool/main/c/curl/libcurl4_7.88.1-10+deb12u14_arm64.deb
	./rpiz-xcompile/add_sysroot_pkg.sh ./rpiz-xcompile http://deb.debian.org/debian/pool/main/c/curl/libcurl4-openssl-dev_7.88.1-10+deb12u14_arm64.deb

.PHONY: clean deploy run
