CFLAGS := -Wall --std=gnu99 -g -Wno-format-security
CURL_LIBS := $(shell curl-config --libs)
CURL_CFLAGS := $(shell curl-config --cflags)

ARCH := $(shell uname)
ifneq ($(ARCH),Darwin)
  LDFLAGS += -lpthread -lrt
endif

all: webproxy simplecached

simplecached: simplecached.o simplecache.o steque.o shm_channel.o shm_channel.o
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)	

webproxy: webproxy.o steque.o gfserver.o handle_with_cache.o
	$(CC) -o $@ $(CFLAGS) $(CURL_CFLAGS) $^ $(LDFLAGS) $(CURL_LIBS)

webproxy_Par1: webproxy_Part1.o steque.o gfserver.o handle_with_curl.o
	$(CC) -o $@ $(CFLAGS) $(CURL_CFLAGS) $^ $(LDFLAGS) $(CURL_LIBS)

.PHONY: clean

clean:
	rm -fr *.o webproxy webproxy_Part1 simplecached