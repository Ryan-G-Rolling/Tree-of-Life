/* Complete presentation data, stable IDs and safe invalid lookups. */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "codex.h"
#include "game_types.h"

int main(void)
{
    assert(codex_count(CODEX_SPECIES) == SPECIES_COUNT);
    assert(codex_count(CODEX_MUTATION) == MUTATION_COUNT);
    for (int kind = CODEX_SPECIES; kind <= CODEX_MUTATION; ++kind) {
        for (int id = 0; id < codex_count((CodexKind)kind); ++id) {
            const CodexEntry *entry = codex_entry((CodexKind)kind, id);
            assert(entry && entry->name[0] && entry->tagline[0]);
            assert(strlen(entry->name) < 80 && strlen(entry->tagline) < 160);
            /* Every approved quote is short enough for the three-line card body. */
            int characters = 0;
            for (const unsigned char *p = (const unsigned char *)entry->tagline; *p; ++p)
                if ((*p & 0xC0u) != 0x80u) ++characters;
            assert(characters <= 30);
            for (int previous = 0; previous < id; ++previous)
                assert(strcmp(entry->name, codex_entry((CodexKind)kind, previous)->name) != 0);
        }
        assert(!codex_entry((CodexKind)kind, -1));
        assert(!codex_entry((CodexKind)kind, codex_count((CodexKind)kind)));
        assert(!codex_entry((CodexKind)kind, INT_MAX));
    }
    assert(codex_count((CodexKind)-1) == 0 && codex_count((CodexKind)2) == 0);
    assert(!codex_entry((CodexKind)2, 0));
    assert(strcmp(codex_entry(CODEX_SPECIES, SPECIES_PROTOCELL)->name, "原始细胞") == 0);
    assert(strcmp(codex_entry(CODEX_MUTATION, MUTATION_NUCLEUS)->tagline, "说明书请妥善保管。") == 0);
    assert(strcmp(codex_entry(CODEX_SPECIES, SPECIES_COUNT - 1)->name, "绝对心智") == 0);
    assert(strcmp(codex_entry(CODEX_MUTATION, MUTATION_COUNT - 1)->name, "最终整合") == 0);
    puts("Codex: all 186 species and 204 mutation names/quotes, IDs and bounds passed");
    return 0;
}
