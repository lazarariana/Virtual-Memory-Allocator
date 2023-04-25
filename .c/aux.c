#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "list.h"
#include "vma.h"
#include "aux.h"

void purge_miniblock(dll_node_t *miniblock_node)
{
	/* dezaloc structura miniblock-ului, eliminarea nodului corespunzator din
		lista de miniblock-uri nefiind suficienta */

	if (miniblock_node) {
		if (miniblock_node->data) {
			miniblock_t *miniblock = (miniblock_t *)miniblock_node->data;
			free(miniblock->rw_buffer);
			miniblock->rw_buffer = NULL;
			free(miniblock);
			miniblock = NULL;
		}
	}

	free(miniblock_node);
	miniblock_node = NULL;
}

void purge_block(dll_node_t *block_node)
{
	/* dezaloc structura block-ului, eliminarea nodului corespunzator din
		lista de block-uri nefiind suficienta */

	block_t *block = (block_t *)block_node->data;

	free(block->miniblock_list);
	block->miniblock_list = NULL;
	free(block);
	free(block_node);
}

block_t *init_block(const uint64_t address, const uint64_t size)
{
	block_t *block;

	block = malloc(sizeof(block_t));
	DIE(!block, "Failed malloc\n");
	block->start_address = address;
	block->size = size;
	block->miniblock_list = dll_create(sizeof(miniblock_t));

	return block;
}

miniblock_t *init_miniblock(const uint64_t address, const uint64_t size)
{
	miniblock_t *miniblock;

	miniblock = malloc(sizeof(miniblock_t));
	DIE(!miniblock, "Failed malloc\n");
	/* default RW- */
	miniblock->perm = (int8_t *)'6';
	miniblock->start_address = address;
	miniblock->size = size;
	miniblock->rw_buffer = calloc(miniblock->size, sizeof(int8_t));
	DIE(!miniblock->rw_buffer, "Failed calloc\n");
	return miniblock;
}

int find_addr_block(arena_t *arena, const uint64_t address)
{
	dll_node_t *current = arena->alloc_list->head;
	int cnt = 0;

	for (; current; current = current->next) {
		block_t *block = (block_t *)current->data;
		if (block->start_address > address) {
			break;
		} else if (block->start_address == address) {
			cnt++;
			break;
		}
		cnt++;
	}

	return cnt;
}

int find_addr_miniblock(block_t *block, const uint64_t address)
{
	/* stabilesc in ce miniblock se afla adresa data, luand in considerare
		cazul in care 2 miniblock-uri au frontiera comuna
	*/

	int cnt = 0;
	dll_node_t *current = block->miniblock_list->head;

	if (current) {
		if (current->data) {
			miniblock_t *miniblock = (miniblock_t *)current->data;
			for (; current; current = current->next) {
				miniblock = (miniblock_t *)current->data;
				if (miniblock->start_address > address) {
					cnt--;
					break;
				}
				cnt++;
			}
		}
	}

	return cnt;
}

bool zone_intersect(arena_t *arena, const uint64_t address,
					const uint64_t size)
{
	/*
	verificam daca zona target poate fi alocata de la adresa corespunzatoare si
	size-ul dat. Daca nu exista nicio structura alocata anterior in buffer,
	putem aloca zona.
	*/

	dll_node_t *current;
	block_t *block;
	unsigned int cnt = find_addr_block(arena, address);

	if (cnt == arena->alloc_list->size) {
		current = dll_get_nth_node(arena->alloc_list, cnt - 1);
		if (current) {
			block = (block_t *)current->data;
			if (address < block->start_address + block->size) {
				printf("This zone was already allocated.\n");
				return true;
			}
		}
	} else if (cnt == 0) {
		current = dll_get_nth_node(arena->alloc_list, 0);
		if (current) {
			block = (block_t *)current->data;
			if (address + size > block->start_address) {
				printf("This zone was already allocated.\n");
				return true;
			}
		}
	} else {
		current = dll_get_nth_node(arena->alloc_list, cnt);
		if (current) {
			block = (block_t *)current->data;
			if (address + size > block->start_address) {
				printf("This zone was already allocated.\n");
				return true;
			}

			current = current->prev;
			if (current) {
				block = (block_t *)current->data;
				if (address < block->start_address + block->size) {
					printf("This zone was already allocated.\n");
					return true;
				}
			}
		}
	}

	return false;
}

void merge_blocks(arena_t *arena, unsigned int kept, unsigned int del)
{
	/*
	plasez intr-un nou block o lista de miniblock-uri formata din block-urile
	ce au o granita comuna
	*/

	dll_node_t *kept_node = dll_get_nth_node(arena->alloc_list, kept);
	block_t *resized_block = (block_t *)kept_node->data;
	dll_node_t *del_node = dll_get_nth_node(arena->alloc_list, del);
	block_t *del_block = (block_t *)del_node->data;

	resized_block->size += del_block->size;

	/* Concatenez cele 2 liste pentru a nu avea 2 zone adiacente de memorie */

	resized_block->miniblock_list->tail->next = del_block->miniblock_list->head;
	del_block->miniblock_list->head->prev = resized_block->miniblock_list->tail;
	resized_block->miniblock_list->tail = del_block->miniblock_list->tail;

	resized_block->miniblock_list->size += del_block->miniblock_list->size;
	del_block->miniblock_list->head = NULL;

	del_node = dll_remove_nth_node(arena->alloc_list, del);
	purge_block(del_node);
}

void block_to_miniblocks(arena_t *arena, block_t *block, miniblock_t *miniblock,
						 dll_node_t *miniblock_node, unsigned int miniblock_cnt,
						 int block_cnt)
{
	/*
	transformam fiecare block intr-un miniblock si il adaugam in lista de
	miniblock-uri a noului block creat. Efectuam parcurgerile astfel incat
	zonele de memorie sa ramana consecutive in lista si dezalocam memoria
	corespunzator.
	*/

	uint64_t mini_total = 0;
	if (miniblock)
		mini_total = miniblock->start_address + miniblock->size;
	uint64_t total = block->start_address + block->size;

	if (!miniblock_cnt || miniblock_cnt == block->miniblock_list->size - 1) {
		if (block->miniblock_list->size != 1) {
			if (miniblock)
				block->size -= miniblock->size;
			miniblock_node =
				dll_remove_nth_node(block->miniblock_list, miniblock_cnt);
			purge_miniblock(miniblock_node);
			if (miniblock)
				block->start_address =
					((miniblock_t *)block->miniblock_list->head->data)
						->start_address;
		} else {
			purge_miniblock(dll_remove_nth_node(block->miniblock_list, 0));
			if (block_cnt)
				purge_block(dll_remove_nth_node(arena->alloc_list,
												block_cnt - 1));
			else
				purge_block(dll_remove_nth_node(arena->alloc_list, block_cnt));
		}
	} else {
		if (block->miniblock_list->size != 1) {
			block_t *first =
				init_block(block->start_address,
						   miniblock->start_address - block->start_address);
			block_t *second = init_block(mini_total, total - mini_total);

			purge_miniblock(dll_remove_nth_node(block->miniblock_list,
												miniblock_cnt));
			miniblock_t *removed = NULL;
			for (unsigned int i = 0; i < miniblock_cnt; i++) {
				removed = (miniblock_t *)block->miniblock_list->head->data;
				dll_add_nth_node(first->miniblock_list, i,
								 removed);
				free(dll_remove_nth_node(block->miniblock_list, 0));
			}

			unsigned int list_size = block->miniblock_list->size;
			for (unsigned int i = 0; i < list_size; i++) {
				removed = (miniblock_t *)block->miniblock_list->head->data;
				dll_add_nth_node(second->miniblock_list, i,
								 removed);
				free(dll_remove_nth_node(block->miniblock_list, 0));
			}

			purge_block(dll_remove_nth_node(arena->alloc_list, block_cnt));
			dll_add_nth_node(arena->alloc_list, block_cnt, first);
			dll_add_nth_node(arena->alloc_list, block_cnt + 1, second);
		} else {
			purge_miniblock(dll_remove_nth_node(block->miniblock_list, 0));
			if (block_cnt)
				purge_block(dll_remove_nth_node(arena->alloc_list,
												block_cnt - 1));
			else
				purge_block(dll_remove_nth_node(arena->alloc_list, block_cnt));
		}
	}
}

int verif_rperm(block_t *block, miniblock_t *miniblock, dll_node_t
			   *miniblock_node, uint64_t address, uint64_t size)
{
	/*
	daca cel putin una dintre zonele de memorie targetate nu are permisiuni de
	citire, operatia nu poate fi aplicata
	*/

	uint64_t perm_size = size;
	if (address + size > block->size + block->start_address)
		perm_size = block->size + block->start_address - address;

	if (miniblock->size + miniblock->start_address - address < perm_size)
		perm_size -= (miniblock->size + miniblock->start_address - address);
	else
		perm_size = 0;

	while (perm_size) {
		miniblock_node = miniblock_node->next;
		if (miniblock_node->data) {
			miniblock = (miniblock_t *)miniblock_node->data;
			if (miniblock->perm != (int8_t *)'4' &&
				miniblock->perm != (int8_t *)'5'  &&
				miniblock->perm != (int8_t *)'6' &&
				miniblock->perm != (int8_t *)'7') {
				printf("Invalid permissions for read.\n");
				return 0;
			}
			if (miniblock->size < perm_size) {
				perm_size -= miniblock->size;
			} else {
				if (miniblock->perm != (int8_t *)'4' &&
					miniblock->perm != (int8_t *)'5'  &&
					miniblock->perm != (int8_t *)'6' &&
					miniblock->perm != (int8_t *)'7') {
					printf("Invalid permissions for read.\n");
					return 0;
				}
				perm_size = 0;
			}
		}
	}
	return 1;
}

int verif_wperm(block_t *block, miniblock_t *miniblock, dll_node_t
				*miniblock_node, uint64_t address, uint64_t size)
{
	/*
	daca cel putin una dintre zonele de memorie targetate nu are permisiuni de
	scriere, operatia nu poate fi aplicata
	*/

	uint64_t perm_size = size;
	if (address + size > block->size + block->start_address)
		perm_size = block->size + block->start_address - address;

	if (miniblock->size + miniblock->start_address - address < size)
		perm_size -= (miniblock->size + miniblock->start_address - address);
	else
		perm_size = 0;

	while (perm_size && miniblock_node) {
		miniblock_node = miniblock_node->next;
		if (miniblock_node->data) {
			miniblock = (miniblock_t *)miniblock_node->data;
			if (miniblock->size < perm_size) {
				if (miniblock->perm != (int8_t *)'2' &&
					miniblock->perm != (int8_t *)'3' &&
					miniblock->perm != (int8_t *)'6' &&
					miniblock->perm != (int8_t *)'7') {
					printf("Invalid permissions for write.\n");
					return 0;
				}
				perm_size -= miniblock->size;
			} else {
				if (miniblock->perm != (int8_t *)'2' &&
					miniblock->perm != (int8_t *)'3' &&
					miniblock->perm != (int8_t *)'6' &&
					miniblock->perm != (int8_t *)'7') {
					printf("Invalid permissions for write.\n");
					return 0;
				}
				perm_size = 0;
			}
		}
	}
	return 1;
}
