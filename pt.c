#include <stdio.h>
#include <stdlib.h>
#include "os.h"

#define BITS 64
#define LEVELS_AMOUNT  3
#define LOWER_OFFSET_BITS  12
#define UPPER_UNUSED_BITS 7

const unsigned int VPN_BITS_AMOUNT = BITS - LOWER_OFFSET_BITS - UPPER_UNUSED_BITS;
const unsigned int ENTRY_SIZE_BITS = VPN_BITS_AMOUNT / LEVELS_AMOUNT;

uint64_t getTableSize() {
    static int isInit = 0;
    static uint64_t tableSize;
    if (isInit == 0) {
        tableSize = 2u << ENTRY_SIZE_BITS;
        isInit = 1;
    }
    return tableSize;
}

long getBitMask() {
    static int isInit = 0;
    static uint64_t mask = 0;

    if (isInit == 0) {
        //mask = (uint64_t)((0x1 << ENTRY_SIZE_BITS)-1)<< (VPN_BITS_AMOUNT - ENTRY_SIZE_BITS);
        mask = (1u << ENTRY_SIZE_BITS) - 1;
        isInit = 1;
    }
    return mask;
}

void **ensureTable(void **srcCell, size_t size) {
    if (*srcCell == NULL) {
        *srcCell = calloc(getTableSize(), size);
    }
    return srcCell;
}

void **ensureNextFinalTable(void **srcCell) {
    return ensureTable(srcCell, sizeof(uint64_t));
}

void **ensureNextNonFinalTable(void **srcCell) {
    return ensureTable(srcCell, sizeof(void **));
}

uint64_t handleLevel(void **curPagingTable, uint64_t curLevelBits, unsigned int ensureFinalTable, unsigned int readOnly) {
    void **levelToLevelRoot = (void **) *curPagingTable + curLevelBits;
    if (readOnly == 1) {
        if (*levelToLevelRoot == NULL) return NO_MAPPING;
        else *curPagingTable = *levelToLevelRoot;
    } else {
        if (ensureFinalTable == 1) *curPagingTable = *ensureNextFinalTable(levelToLevelRoot);
        else *curPagingTable = *ensureNextNonFinalTable(levelToLevelRoot);
    }
    return 0;
}

uint64_t walkAndUpdate(void *levelsRoot, uint64_t vpn, uint64_t ppn, unsigned int readOnly) {
    void *curPagingTable;
    uint64_t curLevelBits, *finalRoot, bitMask = getBitMask();
    curPagingTable = levelsRoot;
    for (int curLevel = 1; curLevel <= LEVELS_AMOUNT; curLevel++) {
        curLevelBits = (vpn >> ((LEVELS_AMOUNT - curLevel) * ENTRY_SIZE_BITS)) & bitMask;
        if (curLevel == LEVELS_AMOUNT) {
            finalRoot = (uint64_t *) curPagingTable + curLevelBits;
            if (readOnly == 1) {
                return *finalRoot == 0 ? NO_MAPPING : *finalRoot;
            } else {
                *finalRoot = ppn == NO_MAPPING ? 0 : ppn;
            }
        } else if (curLevel < LEVELS_AMOUNT - 1) {
            if (handleLevel(&curPagingTable, curLevelBits, 0, readOnly) == NO_MAPPING) return NO_MAPPING;
        } else if (curLevel == LEVELS_AMOUNT - 1) {
            if (handleLevel(&curPagingTable, curLevelBits, 1, readOnly) == NO_MAPPING) return NO_MAPPING;
        }
    }
    return NO_MAPPING;
}

void page_table_update(uint64_t pt, uint64_t vpn, uint64_t ppn) {
    void **root;
    root = ensureNextNonFinalTable(phys_to_virt(pt));
    walkAndUpdate(*root, vpn, ppn, 0);
}

uint64_t page_table_query(uint64_t pt, uint64_t vpn) {
    uint64_t **root = (uint64_t **) phys_to_virt(pt);
    if (root == NULL) return NO_MAPPING;
    if (*root == NULL) return NO_MAPPING;
    return walkAndUpdate(*root, vpn, -1, 1);
}