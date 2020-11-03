#include <stdio.h>
#include <stdlib.h>
#include "os.c"

#define BITS 64
#define LEVELS_AMOUNT  5
#define LOWER_OFFSET_BITS  12
#define UPPER_UNUSED_BITS 7

const unsigned int VPN_BITS_AMOUNT = BITS - LOWER_OFFSET_BITS - UPPER_UNUSED_BITS;
const unsigned int ENTRY_SIZE_BITS = VPN_BITS_AMOUNT / LEVELS_AMOUNT;

uint64_t getTableSize() {
    static int isInit = 0;
    static uint64_t tableSize;
    if (isInit == 0) {
        tableSize = 1u << ENTRY_SIZE_BITS;
        isInit = 1;
    }
    return tableSize;
}

long getBitMask() {
    static int isInit = 0;
    static uint64_t mask = 0;
    if (isInit == 0) {
        mask = (1u << ENTRY_SIZE_BITS) - 1;
        isInit = 1;
    }
    return mask;
}

uint64_t *getOrCreateLevel(uint64_t **root) {
    if (*root == NULL) {
        *root = calloc(getTableSize(), sizeof(uint64_t));
    }
    return *root;
}

uint64_t *getOrCreateNextLevel(uint64_t *srcCell) {
    if (*srcCell == 0) {
        *srcCell = alloc_page_frame() << 12u;
    }
    uint64_t **ptToTable = (uint64_t **) phys_to_virt(*srcCell);
    getOrCreateLevel(ptToTable);
    return *ptToTable;
}

uint64_t
handleLevel(uint64_t **curPagingTable, uint64_t curLevelBits, unsigned int readOnly) {
    uint64_t *curRoot = *curPagingTable + curLevelBits;
    if (readOnly == 1) {
        if (*curRoot == 0) return NO_MAPPING;
        else *curPagingTable = *(uint64_t **) (phys_to_virt(*curRoot));
    } else {
        *curPagingTable = getOrCreateNextLevel(curRoot);
    }
    return 0;
}

uint64_t walkAndUpdate(uint64_t *levelsRoot, uint64_t vpn, uint64_t ppn, unsigned int readOnly) {
    uint64_t *curPagingTable;
    uint64_t curLevelBits, *finalRoot, bitMask = getBitMask();
    curPagingTable = levelsRoot;
    for (int curLevel = 1; curLevel <= LEVELS_AMOUNT; curLevel++) {
        curLevelBits = (vpn >> ((LEVELS_AMOUNT - curLevel) * ENTRY_SIZE_BITS)) & bitMask;
        if (curLevel == LEVELS_AMOUNT) {
            finalRoot = curPagingTable + curLevelBits;
            if (readOnly == 1) return *finalRoot == 0 ? NO_MAPPING : *finalRoot;
            else *finalRoot = ppn == NO_MAPPING ? 0 : ppn;
        } else {
            if (handleLevel(&curPagingTable, curLevelBits, readOnly) == NO_MAPPING) return NO_MAPPING;
        }
    }
    return NO_MAPPING;
}

void page_table_update(uint64_t pt, uint64_t vpn, uint64_t ppn) {
    uint64_t *root = getOrCreateLevel((uint64_t **) phys_to_virt(pt));
    walkAndUpdate(root, vpn, ppn, 0);
}

uint64_t page_table_query(uint64_t pt, uint64_t vpn) {
    uint64_t **root = (uint64_t **) phys_to_virt(pt);
    if (root == NULL) return NO_MAPPING;
    if (*root == NULL) return NO_MAPPING;
    return walkAndUpdate(*root, vpn, -1, 1);
}