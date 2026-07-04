
#ifndef EXEC_PAGE_VARY_H
#define EXEC_PAGE_VARY_H

typedef struct {
    bool decided;
    int bits;
    uint64_t mask;
} TargetPageBits;

#ifdef IN_PAGE_VARY
bool set_preferred_target_page_bits_common(int bits);
void finalize_target_page_bits_common(int min);
#endif

bool set_preferred_target_page_bits(int bits);

void finalize_target_page_bits(void);

#endif /* EXEC_PAGE_VARY_H */
