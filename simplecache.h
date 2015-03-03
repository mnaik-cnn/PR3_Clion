#ifndef _SIMPLECACHE_H_
#define _SIMPLECACHE_H_

int simplecache_init(char *filename);

int simplecache_get(char *key);

void simplecache_destroy();

#endif