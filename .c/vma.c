#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "list.h"
#include "aux.h"
#include "vma.h"

arena_t *alloc_arena(const uint64_t size)
{
	arena_t *arena;
	doubly_linked_list_t *list;

	arena = malloc(sizeof(arena_t));
	DIE(!arena, " Failed malloc\n");
	arena->arena_size = size;
	list = dll_create(sizeof(doubly_linked_list_t));
	arena->alloc_list = list;

	return arena;
}

void dealloc_arena(arena_t *arena)
{
	/* dezaloc fiecare nod din listele imbricate pentru a putea elibera si
		structurile corespunzatoarer acestora
	*/

	while (arena->alloc_list->size) {
		dll_node_t *block_node = arena->alloc_list->head;
		block_t *block = block_node->data;
		while (block->miniblock_list->size)
			purge_miniblock(dll_remove_nth_node(block->miniblock_list, 0));

		purge_block(dll_remove_nth_node(arena->alloc_list, 0));
	}

	free(arena->alloc_list);
	free(arena);
}

void alloc_block(arena_t *arena, const uint64_t address, const uint64_t size)
{
	/*
	se aloca memorie pentru un nou miniblock. Daca adresa data nu se afla in
	arena, este necesara crearea unui block avand acelasi start_address si
	size. In cazul in care blocul exista, ne asiguram ca nu are granita comuna
	cu alte blockuri
	*/

	unsigned int cnt = find_addr_block(arena, address);

	if (arena->arena_size <= address) {
		printf("The allocated address is outside the size of arena\n");
		return;
	}

	if (arena->arena_size < address + size) {
		printf("The end address is past the size of the arena\n");
		return;
	}

	if (zone_intersect(arena, address, size))
		return;

	block_t *block;
	miniblock_t *miniblock = NULL;

	block = init_block(address, size);
	miniblock = init_miniblock(address, size);
	dll_add_nth_node(block->miniblock_list, block->miniblock_list->size,
					 miniblock);
	dll_add_nth_node(arena->alloc_list, cnt, block);

	dll_node_t *check = dll_get_nth_node(arena->alloc_list, cnt);
	if (check && cnt) {
		block_t *check_block = (block_t *)check->data;
		block_t *prev_block = (block_t *)check->prev->data;
		if (check_block->start_address ==
			prev_block->start_address + prev_block->size) {
			int block_cnt = find_addr_block(arena, address);
			dll_node_t *block_node = NULL;
			if (block_cnt)
				block_node = dll_get_nth_node(arena->alloc_list, block_cnt - 1);
			else
				block_node = dll_get_nth_node(arena->alloc_list, block_cnt);
			block_t *block = (block_t *)block_node->data;
			int miniblock_cnt = find_addr_miniblock(block, address);
			dll_node_t *miniblock_node = NULL;
			if (miniblock_cnt)
				miniblock_node =
					dll_get_nth_node(block->miniblock_list, miniblock_cnt - 1);
			else
				miniblock_node =
					dll_get_nth_node(block->miniblock_list, miniblock_cnt);
			if (miniblock_node->data)
				miniblock = (miniblock_t *)miniblock_node->data;
			merge_blocks(arena, cnt - 1, cnt);
			cnt--;
			check = dll_get_nth_node(arena->alloc_list, cnt);
			check_block = (block_t *)check->data;
		}
	}
	if (check && cnt < arena->alloc_list->size - 1) {
		block_t *check_block = (block_t *)check->data;
		block_t *next_block = (block_t *)check->next->data;
		if (check_block->start_address + check_block->size ==
			next_block->start_address)
			merge_blocks(arena, cnt, cnt + 1);
	}
}

void free_block(arena_t *arena, const uint64_t address)
{
	/*
	daca se dezaloca un miniblock din interiorul unei liste de miniblock-uri,
	alocam 2 noi block-uri ce stocheaza cele 2 parti ale listei.
	*/

	if (!arena->alloc_list->size) {
		printf("Invalid address for free.\n");
		return;
	}

	int block_cnt = find_addr_block(arena, address);

	dll_node_t *block_node = NULL;
	if (block_cnt)
		block_node = dll_get_nth_node(arena->alloc_list, block_cnt - 1);
	else
		block_node = dll_get_nth_node(arena->alloc_list, block_cnt);
	block_t *block = (block_t *)block_node->data;

	unsigned int miniblock_cnt = find_addr_miniblock(block, address);
	dll_node_t *miniblock_node = NULL;
	if (miniblock_cnt) {
		if (miniblock_cnt < block->miniblock_list->size)
			miniblock_node =
				dll_get_nth_node(block->miniblock_list, miniblock_cnt);
		else
			miniblock_node =
				dll_get_nth_node(block->miniblock_list, miniblock_cnt - 1);
	} else {
		miniblock_node = dll_get_nth_node(block->miniblock_list, miniblock_cnt);
	}

	miniblock_t *miniblock = NULL;
	if (miniblock_node)
		if (miniblock_node->data)
			miniblock = (miniblock_t *)miniblock_node->data;

	if (miniblock)
		if (miniblock->start_address != address &&
			block->start_address != address) {
			printf("Invalid address for free.\n");
			return;
		}

	block_to_miniblocks(arena, block, miniblock, miniblock_node, miniblock_cnt,
						block_cnt);
}

/*
	utilizez fwrite pentru a avea un mai bun control al datelor atunci cand
	parsez miniblock-urile. Este esential sa nu depasim intervalul cerut, de
	aceea nu este posibila afisarea completa a buffer-ului unui miniblock.
*/

void read(arena_t *arena, uint64_t address, uint64_t size)
{
	if (!arena->alloc_list->size) {
		printf("Invalid address for read.\n");
		return;
	}
	uint64_t resize = size;
	int sum = 0;
	dll_node_t *block_node = NULL;
	dll_node_t *miniblock_node = NULL;

	int block_cnt = find_addr_block(arena, address);
	if (block_cnt)
		block_node = dll_get_nth_node(arena->alloc_list, block_cnt - 1);
	else
		block_node = dll_get_nth_node(arena->alloc_list, block_cnt);

	block_t *block = (block_t *)block_node->data;
	int miniblock_cnt = find_addr_miniblock(block, address);
	miniblock_node = dll_get_nth_node(block->miniblock_list, miniblock_cnt);

	miniblock_t *miniblock = (miniblock_t *)miniblock_node->data;

	if (!miniblock->rw_buffer)
		miniblock->rw_buffer = calloc(miniblock->size, sizeof(int8_t));

	if (!(block->start_address <= address &&
		  block->start_address + block->size >= address)) {
		printf("Invalid address for read.\n");
		return;
	}

	if (miniblock->perm != (int8_t *)'4' && miniblock->perm != (int8_t *)'5'  &&
		miniblock->perm != (int8_t *)'6' && miniblock->perm != (int8_t *)'7') {
		printf("Invalid permissions for read.\n");
		return;
	}

	if (verif_rperm(block, miniblock, miniblock_node, address, size)) {
		miniblock_node = dll_get_nth_node(block->miniblock_list, miniblock_cnt);
		miniblock = (miniblock_t *)miniblock_node->data;

		if (address + size > block->size + block->start_address) {
			resize = block->size + block->start_address - address;
			printf("Warning: size was bigger than the block size. Reading %ld "
				"characters.\n", resize);
		}

		if (miniblock->size + miniblock->start_address - address < resize) {
			fwrite(miniblock->rw_buffer + (address -
				   miniblock->start_address), sizeof(int8_t),
				   miniblock->size + miniblock->start_address - address,
				   stdout);
			sum += miniblock->size + miniblock->start_address - address + 1;
			resize -= (miniblock->size + miniblock->start_address - address);
		} else {
			fwrite(miniblock->rw_buffer + (address - miniblock->start_address),
				   sizeof(int8_t), resize, stdout);
			resize = 0;
		}

		while (resize) {
			miniblock_node = miniblock_node->next;
			miniblock = (miniblock_t *)miniblock_node->data;
			if (!miniblock->rw_buffer)
				miniblock->rw_buffer = calloc(miniblock->size, sizeof(int8_t));
			if (miniblock->size < resize) {
				fwrite(miniblock->rw_buffer, sizeof(int8_t), miniblock->size,
					   stdout);
				sum += miniblock->size + 1;
				resize -= miniblock->size;
			} else {
				fwrite(miniblock->rw_buffer, sizeof(int8_t), resize, stdout);
				resize = 0;
			}
		}

		printf("\n");
	}
}

/*
	dupa ce ma asigur ca buffer-ul este alocat, copiez caracterele din data
	avand ca si contor al indexului curent variabila sum. Astfel nu sarim peste
	si nu printam succesiv acelasi caractere.
*/

void write(arena_t *arena, const uint64_t address, const uint64_t size,
		   int8_t *data)
{
	if (!arena->alloc_list->size) {
		printf("Invalid address for write.\n");
		return;
	}

	uint64_t resize = size;
	int sum = 0;
	dll_node_t *block_node = NULL;
	dll_node_t *miniblock_node = NULL;
	int block_cnt = find_addr_block(arena, address);
	if (block_cnt)
		block_node = dll_get_nth_node(arena->alloc_list, block_cnt - 1);
	else
		block_node = dll_get_nth_node(arena->alloc_list, block_cnt);

	block_t *block = (block_t *)block_node->data;
	int miniblock_cnt = find_addr_miniblock(block, address);
	miniblock_node = dll_get_nth_node(block->miniblock_list, miniblock_cnt);
	miniblock_t *miniblock = (miniblock_t *)miniblock_node->data;

	if (!(block->start_address <= address &&
		  block->start_address + block->size >= address)) {
		printf("Invalid address for write.\n");
		return;
	}

	if (miniblock->perm != (int8_t *)'2' && miniblock->perm != (int8_t *)'3' &&
		miniblock->perm != (int8_t *)'6' && miniblock->perm != (int8_t *)'7') {
		printf("Invalid permissions for write.\n");
		return;
	}

	if (verif_wperm(block, miniblock, miniblock_node, address, size)) {
		miniblock_node = dll_get_nth_node(block->miniblock_list, miniblock_cnt);
		miniblock = (miniblock_t *)miniblock_node->data;

		if (!miniblock->rw_buffer)
			miniblock->rw_buffer = calloc(miniblock->size, sizeof(int8_t));
		DIE(!miniblock->rw_buffer, " Failed calloc\n");

		if (address + size > block->size + block->start_address) {
			resize = block->size + block->start_address - address;
			printf("Warning: size was bigger than the block size. Writing %ld "
				"characters.\n", resize);
		}

		if (miniblock->size + miniblock->start_address - address < resize) {
			memcpy(miniblock->rw_buffer + (address - miniblock->start_address),
				   data + sum,
				   miniblock->size + miniblock->start_address - address);
			sum += miniblock->size + miniblock->start_address - address;
			resize -= (miniblock->size + miniblock->start_address - address);
		} else {
			memcpy(miniblock->rw_buffer + (address - miniblock->start_address),
				   data + sum, resize);
			resize = 0;
		}

		while (resize && miniblock_node) {
			miniblock_node = miniblock_node->next;
			miniblock = (miniblock_t *)miniblock_node->data;
			if (!miniblock->rw_buffer)
				miniblock->rw_buffer = calloc(miniblock->size, sizeof(int8_t));
			DIE(!miniblock->rw_buffer, " Failed calloc\n");
			if (miniblock->size < resize) {
				memcpy(miniblock->rw_buffer, data + sum, miniblock->size);
				sum += miniblock->size;
				resize -= miniblock->size;
			} else {
				memcpy(miniblock->rw_buffer, data + sum, resize);
				resize = 0;
			}
		}
	}
}

void pmap(const arena_t *arena)
{
	printf("Total memory: 0x%lX bytes\n", arena->arena_size);
	int64_t sum = 0;
	int64_t total_mini = 0;
	block_t *block;
	dll_node_t *current = arena->alloc_list->head;

	/*
	calculez zona ocupata din arena pentru a afla spatiul ramas liber
	*/

	if (!current) {
		sum = 0;
	} else {
		for (unsigned int i = 0; i < arena->alloc_list->size; i++) {
			block = (block_t *)current->data;
			sum += block->size;
			total_mini += block->miniblock_list->size;
			current = current->next;
		}
	}

	printf("Free memory: 0x%lX bytes\n", arena->arena_size - sum);

	printf("Number of allocated blocks: %d\n", arena->alloc_list->size);
	printf("Number of allocated miniblocks: %ld\n", total_mini);
	if (arena->alloc_list->size)
		printf("\n");
	current = arena->alloc_list->head;
	for (unsigned int i = 1; i <= arena->alloc_list->size; i++) {
		block = (block_t *)current->data;
		printf("Block %u begin\n", i);
		printf("Zone: 0x%lX - 0x%lX\n", block->start_address,
			   block->start_address + block->size);

		miniblock_t *miniblock;
		dll_node_t *mini_current = block->miniblock_list->head;
		int j = 1;
		for (; mini_current; j++, mini_current = mini_current->next) {
			miniblock = (miniblock_t *)mini_current->data;
			printf("Miniblock %d:\t\t0x%lX\t\t-\t\t0x%lX\t\t|", j,
				   miniblock->start_address,
				   miniblock->start_address + miniblock->size);

			/*
			permisiunile sunt stocate dupa codificarea in binar
			*/

			if (miniblock->perm == (int8_t *)'0')
				printf(" ---\n");
			else if (miniblock->perm == (int8_t *)'1')
				printf(" --X\n");
			else if (miniblock->perm == (int8_t *)'2')
				printf(" -W-\n");
			else if (miniblock->perm == (int8_t *)'3')
				printf(" -WX\n");
			else if (miniblock->perm == (int8_t *)'4')
				printf(" R--\n");
			else if (miniblock->perm == (int8_t *)'5')
				printf(" R-X\n");
			else if (miniblock->perm == (int8_t *)'6')
				printf(" RW-\n");
			else
				printf(" RWX\n");
		}
		if (i == arena->alloc_list->size) {
			printf("Block %u end\n", i);
		} else {
			printf("Block %u end\n", i);
			printf("\n");
		}

		current = current->next;
	}
}

void mprotect(arena_t *arena, uint64_t address, int8_t *permission)
{
	if (!arena->alloc_list->size) {
		printf("Invalid address for free.\n");
		return;
	}

	int block_cnt = find_addr_block(arena, address);
	dll_node_t *block_node = NULL;
	if (block_cnt)
		block_node = dll_get_nth_node(arena->alloc_list, block_cnt - 1);
	else
		block_node = dll_get_nth_node(arena->alloc_list, block_cnt);
	block_t *block = (block_t *)block_node->data;

	dll_node_t *miniblock_node = NULL;
	unsigned int miniblock_cnt = find_addr_miniblock(block, address);

	if (miniblock_cnt) {
		if (miniblock_cnt < block->miniblock_list->size)
			miniblock_node =
				dll_get_nth_node(block->miniblock_list, miniblock_cnt);
		else
			miniblock_node =
				dll_get_nth_node(block->miniblock_list, miniblock_cnt - 1);
	} else {
		miniblock_node = dll_get_nth_node(block->miniblock_list, miniblock_cnt);
	}

	miniblock_t *miniblock = NULL;
	if (miniblock_node)
		if (miniblock_node->data)
			miniblock = (miniblock_t *)miniblock_node->data;

	if (miniblock)
		if (miniblock->start_address != address) {
			printf("Invalid address for mprotect.\n");
			return;
		}

	/*
	daca adresa este valida, putem aplica noile permisiuni pe miniblock-ul
	target
	*/

	miniblock->perm = permission;
}
