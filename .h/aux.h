#pragma once
#include "list.h"
#include "vma.h"

void purge_miniblock(dll_node_t *miniblock_node);
void purge_block(dll_node_t *block_node);
block_t *init_block(const uint64_t address, const uint64_t size);
miniblock_t *init_miniblock(const uint64_t address, const uint64_t size);
int find_addr_block(arena_t *arena, const uint64_t address);
int find_addr_miniblock(block_t *block, const uint64_t address);
bool zone_intersect(arena_t *arena, const uint64_t address,
					const uint64_t size);
void merge_blocks(arena_t *arena, unsigned int kept, unsigned int del);
void block_to_miniblocks(arena_t *arena, block_t *block, miniblock_t *miniblock,
						 dll_node_t *miniblock_node, unsigned int miniblock_cnt,
						 int block_cnt);
int verif_rperm(block_t *block, miniblock_t *miniblock, dll_node_t
			   *miniblock_node, uint64_t address, uint64_t size);
int verif_wperm(block_t *block, miniblock_t *miniblock, dll_node_t
				*miniblock_node, uint64_t address, uint64_t size);
