#ifndef CODEX_H
#define CODEX_H

/* Presentation data only: IDs follow the existing rule tables; never saved. */
typedef enum { CODEX_SPECIES = 0, CODEX_MUTATION = 1 } CodexKind;
typedef struct { const char *name; const char *tagline; } CodexEntry;
int codex_count(CodexKind kind);
const CodexEntry *codex_entry(CodexKind kind, int id);

#endif
