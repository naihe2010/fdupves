#ifndef FDUPVES_FINGERPRINT_H
#define FDUPVES_FINGERPRINT_H

#ifdef __cplusplus
extern "C" {
#endif
typedef int (*fingerprint_callback)(const char *, int, void *);
typedef void *fingerprint_arg;

void fingerprint(float *data, int data_size, float fs, int amp_min, fingerprint_callback cb, fingerprint_arg arg);

int test_fingerprint(const char *);

#ifdef __cplusplus
}
#endif

#endif