#include "list.h"
#include "vma.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

doubly_linked_list_t *dll_create(unsigned int data_size)
{
	doubly_linked_list_t *list;
	list = malloc(sizeof(*list));
	DIE(!list, " Failed malloc\n");
	list->head = NULL;
	list->tail = NULL;
	list->size = 0;
	list->data_size = data_size;
	return list;
}

dll_node_t *dll_get_nth_node(doubly_linked_list_t *list, int n)
{
	if ((unsigned int)n >= list->size)
		n = list->size - 1;
	if (!list || !list->size)
		return NULL;

	dll_node_t *current;

	current = list->head;
	for (int i = 0; i < n; i++)
		current = current->next;

	return current;
}

void dll_add_nth_node(doubly_linked_list_t *list, unsigned int n,
					  void *new_data)
{
	if (n > list->size)
		n = list->size;

	if (!list)
		return;

	dll_node_t *new = malloc(sizeof(dll_node_t));
	DIE(!new, " Failed malloc\n");

	new->data = malloc(list->data_size);
	memcpy(new->data, new_data, list->data_size);
	free(new_data);

	if (!list->size) {
		new->next = NULL;
		new->prev = NULL;
		list->head = new;
		list->tail = new;
		list->size++;
	} else if (n == 0) {
		new->next = list->head;
		new->prev = NULL;
		list->head->prev = new;
		list->head = new;
		list->size++;
	} else {
		dll_node_t *prev;
		prev = dll_get_nth_node(list, n);
		if (n == list->size) {
			new->prev = list->tail;
			new->next = NULL;
			if (list->tail) {
				list->tail->next = new;
				list->tail = new;
			} else {
				list->head = new;
				list->tail = new;
			}
		} else {
			new->next = prev;
			new->prev = prev->prev;
			prev->prev->next = new;
			prev->prev = new;
		}
		list->size++;
	}
}

dll_node_t *dll_remove_nth_node(doubly_linked_list_t *list, unsigned int n)
{
	if (!list || !list->size)
		return NULL;

	if (n > list->size - 1)
		n = list->size - 1;

	dll_node_t *current = NULL;

	if (n == 0) {
		current = list->head;
		list->head = list->head->next;

		if (current == list->tail)
			list->tail = NULL;

		if (list->head)
			list->head->prev = NULL;

	} else if (n == list->size - 1) {
		current = list->tail;
		list->tail = list->tail->prev;
		if (list->tail)
			list->tail->next = NULL;

		if (current == list->head)
			list->head = NULL;
	} else {
		current = dll_get_nth_node(list, n);
		current->next->prev = current->prev;
		current->prev->next = current->next;
		current->prev = NULL;
		current->next = NULL;
	}

	list->size--;
	if (!list->size) {
		list->head = NULL;
		list->tail = NULL;
	}
	return current;
}

unsigned int dll_get_size(doubly_linked_list_t *list)
{
	return list->size;
}

void dll_free(doubly_linked_list_t **pp_list)
{
	dll_node_t *current;

	if (!pp_list || !*pp_list)
		return;

	while ((*pp_list)->size) {
		current = dll_remove_nth_node(*pp_list, 0);

		free(current->data);
		free(current);
	}

	free(*pp_list);
	*pp_list = NULL;
}

void dll_print_int_list(doubly_linked_list_t *list)
{
	if (!list)
		return;

	dll_node_t *current;

	unsigned int i = 0;
	current = list->head;
	while (i < list->size) {
		printf("%d ", *(int *)current->data);
		current = current->next;
		i++;
	}

	printf("\n");
}

void dll_print_string_list(doubly_linked_list_t *list)
{
	if (!list)
		return;

	dll_node_t *current;
	current = list->head->prev;
	unsigned int i = 0;
	while (i < list->size) {
		printf("%s ", (char *)current->data);
		current = current->prev;
		i++;
	}

	printf("\n");
}
