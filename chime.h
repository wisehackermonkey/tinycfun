#ifndef CHIME_H
#define CHIME_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Chime Chime;

Chime *chime_load(const char *model_path);
void   chime_free(Chime *m);
char **chime_suggest(Chime *m, const char *text, int k, size_t *n_out);
void   chime_free_suggestions(char **sug, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* CHIME_H */