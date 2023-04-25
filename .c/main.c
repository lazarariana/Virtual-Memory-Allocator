#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "list.h"
#include "aux.h"
#include "vma.h"

#define LENGTH 100 

/*
stabilesc ce tipuri de permisiuni sunt citite pentru a afla reprezentarea in
binar a reuniunii acestora
*/

void read_mprotect(arena_t *arena, uint64_t address)
{
	int prot_read = 0, prot_write = 0, prot_exec = 0;
	char permissions[400], *p;
	fgets(permissions, sizeof(permissions), stdin);
	permissions[strlen(permissions) - 1] = '\0';
	if (!strcmp(permissions, "PROT_NONE")) {
		mprotect(arena, address, (int8_t *)'0');
	} else {
		if (!strcmp(permissions, "PROT_READ"))
			prot_read = 1;
		else if (!strcmp(permissions, "PROT_WRITE"))
			prot_write = 1;
		else
			prot_exec = 1;

		p = strtok(permissions, " | ");
		while (p) {
			if (!strcmp(p, "PROT_READ"))
				prot_read = 1;
			else if (!strcmp(p, "PROT_WRITE"))
				prot_write = 1;
			else
				prot_exec = 1;
			p = strtok(NULL, " | ");
		}

		switch (prot_exec) {
		case 0:
			switch (prot_write) {
			case 0:
				switch (prot_read) {
				case 0:
					mprotect(arena, address, (int8_t *)'0');
					break;
				case 1:
					mprotect(arena, address, (int8_t *)'4');
					break;
				}
				break;
			case 1:
				switch (prot_read) {
				case 0:
					mprotect(arena, address, (int8_t *)'2');
					break;
				case 1:
					mprotect(arena, address, (int8_t *)'6');
					break;
				}
				break;
			}
			break;
		case 1:
			switch (prot_write) {
			case 0:
				switch (prot_read) {
				case 0:
					mprotect(arena, address, (int8_t *)'1');
					break;
				case 1:
					mprotect(arena, address, (int8_t *)'5');
					break;
				}
				break;
			case 1:
				switch (prot_read) {
				case 0:
					mprotect(arena, address, (int8_t *)'3');
					break;
				case 1:
					mprotect(arena, address, (int8_t *)'7');
					break;
				}
				break;
			}
			break;
		default:
			mprotect(arena, address, (int8_t *)'6');
			break;
		}
	}
}

int main(void)
{
	arena_t *arena = NULL;
	int8_t *data = NULL;
	uint64_t address, size;
	char command[LENGTH], sep;

	while (scanf("%s", command) && strcmp(command, "DEALLOC_ARENA")) {
		if (!strcmp(command, "ALLOC_ARENA")) {
			scanf("%ld", &size);
			arena = alloc_arena(size);
		}  else if (!strcmp(command, "ALLOC_BLOCK")) {
			scanf("%ld %ld", &address, &size);
			alloc_block(arena, address, size);
		} else if (!strcmp(command, "FREE_BLOCK")) {
			scanf("%ld", &address);
			free_block(arena, address);
		} else if (!strcmp(command, "READ")) {
			scanf("%ld %ld", &address, &size);
			read(arena, address, size);
		} else if (!strcmp(command, "WRITE")) {
			scanf("%ld %ld", &address, &size);
			scanf("%c", &sep);
			data = malloc((size + 1) * sizeof(char));
			DIE(!data, " Failed malloc\n");
			fread(data, size, sizeof(char), stdin);
			write(arena, address, size, data);
			free(data);
		} else if (!strcmp(command, "PMAP")) {
			pmap(arena);
		} else if (!strcmp(command, "MPROTECT")) {
			scanf("%ld", &address);
			scanf("%c", &sep);
			read_mprotect(arena, address);
		} else {
			printf("Invalid command. Please try again.\n");
		}
	}
	dealloc_arena(arena);
	return 0;
}
