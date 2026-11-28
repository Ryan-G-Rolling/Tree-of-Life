#ifndef CODEX_H
#define CODEX_H

/* Presentation data only: IDs follow the existing rule tables; never saved. */
typedef enum { CODEX_SPECIES = 0, CODEX_MUTATION = 1 } CodexKind;
typedef struct { const char *name; const char *tagline; } CodexEntry;
int codex_count(CodexKind kind);
const CodexEntry *codex_entry(CodexKind kind, int id);
/* v25：图鉴详情页的科普正文（约 100 字）。未收录时返回空串。 */
const char *codex_science(CodexKind kind, int id);

#endif
