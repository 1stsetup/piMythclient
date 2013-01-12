/* ***** BEGIN MIV LICENSE BLOCK *****
 * Version: MIV 1.0
 *
 * This file is part of the "MIV" license.
 *
 * Rules of this license:
 * - This code may be reused in other free software projects (free means the end user does not have to pay anything to use it).
 * - This code may be reused in other non free software projects. 
 *     !! For this rule to apply you will grant or provide the below mentioned author unlimited free access/license to the:
 *         - binary program of the non free software project which uses this code. By this we mean a full working version.
 *         - One piece of the hardware using this code. For free at no costs to the author. 
 *         - 1% of the netto world wide sales.
 * - When you use this code always leave this complete license block in the file.
 * - When you create binaries (executable or library) based on this source you 
 *     need to provide a copy of this source online publicaly accessable.
 * - When you make modifications to this source file you will keep this license block complete.
 * - When you make modifications to this source file you will send a copy of the new file to 
 *     the author mentioned in this license block. These rules will also apply to the new file.
 * - External packages used by this source might have a different license which you should comply with.
 *
 * Latest version of this license can be found at http://www.1st-setup.nl
 *
 * Author: Michel Verbraak (info@1st-setup.nl)
 * Website: http://www.1st-setup.nl
 * email: info@1st-setup.nl
 *
 *
 * ***** END MIV LICENSE BLOCK *****/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "lists.h"
#include "globalFunctions.h"

struct LISTITEM_T *createListItem(char *text)
{
#ifdef DEBUG
	logInfo( LOG_LISTS,"createListItem: text=%s.\n", text);
#endif
	struct LISTITEM_T *newItem = (struct LISTITEM_T *)malloc(sizeof(struct LISTITEM_T));
	if (newItem == NULL) {
		perror("create_list_item: malloc newItem");
		return NULL;
	}

	newItem->string = malloc(strlen(text)+1);
	if (newItem->string == NULL) {
		perror("create_list_item: malloc newItem->string");
		return NULL;
	}

	strcpy(newItem->string, text);

	newItem->next = NULL;
	newItem->prev = NULL;

	return newItem;
}

void freeListItem(struct LISTITEM_T *item)
{
	if (item != NULL) {
		if (item->string != NULL) {
			free(item->string);
		}
		free(item);
	}
}

void freeList(struct LISTITEM_T *item)
{
	if (item == NULL) return;

	if (item->prev != NULL) {
		// This item is not the start of the list.
		// Create a new end for the whole list.
#ifdef DEBUG
		logInfo( LOG_LISTS,"freeList: item->prev != NULL.\n");
#endif
		item->prev->next = NULL;
	}

	struct LISTITEM_T *tmpItem = item;
	struct LISTITEM_T *nextItem = item->next;
	while (tmpItem != NULL) {
		freeListItem(tmpItem);
		tmpItem = nextItem;
		if (nextItem != NULL) {
			nextItem = nextItem->next;
		}
	}
}

void addItemToList(struct LISTITEM_T *parent, struct LISTITEM_T *child)
{
	child->prev = parent;
	child->next = parent->next;
	parent-> next = child;
}

// Will delete th specified item from the list and return the existing or new parent.
struct LISTITEM_T *deleteItemFromList(struct LISTITEM_T *item)
{
	// First we locate the parent.
	struct LISTITEM_T *parent = item;
	while (parent->prev != NULL) {
		parent = parent->prev;
	}

	if (parent == item) {
#ifdef DEBUG
		logInfo( LOG_LISTS,"deleteItemFromList: Start of list is removed.\n");
#endif
		parent = item->next;
		parent->prev = NULL;
		freeListItem(item);
	}
	else {
#ifdef DEBUG
		logInfo( LOG_LISTS,"deleteItemFromList: Item to be removed is not first in list.\n");
#endif
		item->prev->next = item->next;
		item->next->prev = item->prev;
		freeListItem(item);
	}

	return parent;
}

void addStrToList(struct LISTITEM_T *parent, char *text)
{
	struct LISTITEM_T *newItem = createListItem(text);
	addItemToList(parent, newItem);
}

struct LISTITEM_T *convertStrToList(char *text, char *splitStr)
{
	char *textCopy = malloc(strlen(text)+1);
	strcpy(textCopy, text);

	char *tmpText = textCopy;

	struct LISTITEM_T *result = NULL;
	struct LISTITEM_T *listItem;
	struct LISTITEM_T *endOfList;

	int itemCount = 0;

	int tmpIndex = indexOf(tmpText, splitStr);
#ifdef DEBUG
	logInfo( LOG_LISTS,"convertStrToList: 1. tmpText=%s, tmpIndex=%d.\n", tmpText, tmpIndex);
#endif
	while (tmpIndex > -1) {
		tmpText[tmpIndex] = 0x00;
		listItem =  createListItem(tmpText);
		itemCount++;
		if (result == NULL) {
#ifdef DEBUG
			logInfo( LOG_LISTS,"convertStrToList: 1. Start of list will be created. index=%d, item->string=%s\n", itemCount, listItem->string);
#endif
			result = listItem;
			endOfList = result;
		}
		else {
#ifdef DEBUG
			logInfo( LOG_LISTS,"convertStrToList: 1. Start of list will be created. index=%d, item->string=%s\n", itemCount, listItem->string);
#endif
			addItemToList(endOfList, listItem);
			endOfList = listItem;
		}

		tmpText += tmpIndex + strlen(splitStr);
		tmpIndex = indexOf(tmpText, splitStr);
#ifdef DEBUG
		logInfo( LOG_LISTS,"convertStrToList: 2. tmpText=%s, tmpIndex=%d.\n", tmpText, tmpIndex);
#endif
	}

	listItem =  createListItem(tmpText);
	itemCount++;
	if (result == NULL) {
#ifdef DEBUG
		logInfo( LOG_LISTS,"convertStrToList: 2. Start of list will be created. index=%d, item->string=%s\n", itemCount, listItem->string);
#endif
		result = listItem;
		endOfList = result;
	}
	else {
#ifdef DEBUG
		logInfo( LOG_LISTS,"convertStrToList: 2. Start of list will be created. index=%d, item->string=%s\n", itemCount, listItem->string);
#endif
		addItemToList(endOfList, listItem);
		endOfList = listItem;
	}
	
	free(textCopy);
	return result;
}

char *convertListToString(struct LISTITEM_T *parent, char *splitStr)
{
	struct LISTITEM_T *tmpItem = parent;
	int items = listCount(parent);
	int newLen = items*strlen(splitStr);
	while (tmpItem != NULL) {
		newLen += strlen(tmpItem->string);
		tmpItem = tmpItem->next;
	}

	char *result = malloc(newLen+1);
	memset(result, 0, newLen+1);
	tmpItem = parent;
	char *tmpChar = result;
	while (tmpItem != NULL) {
		strncpy(tmpChar, tmpItem->string, strlen(tmpItem->string));
		tmpChar += strlen(tmpItem->string);
		if (tmpItem->next != NULL) {
			strncpy(tmpChar, splitStr, strlen(splitStr));
			tmpChar += strlen(splitStr);
		}
		tmpItem = tmpItem->next;
	}

	return result;
}

struct LISTITEM_T *getItemAtListIndex(struct LISTITEM_T *parent, int index)
{
	struct LISTITEM_T *result = parent;
	while ((index > 0) && (result != NULL)) {
		result = result->next;
		index--;
	}

	return result;
}

char *getStringAtListIndex(struct LISTITEM_T *parent, int index)
{
	struct LISTITEM_T *result = getItemAtListIndex(parent, index);

	if (result != NULL) {
		return result->string;
	}

	return NULL;
}

int listCount(struct LISTITEM_T *parent)
{
	int result = 0;
	struct LISTITEM_T *tmpItem = parent;
	while (tmpItem != NULL) {
		result++;
		tmpItem = tmpItem->next;
	}
	return result;
}

struct SIMPLELISTITEM_T *createSimpleListItem(void *object)
{
	struct SIMPLELISTITEM_T *newItem = (struct SIMPLELISTITEM_T *)malloc(sizeof(struct SIMPLELISTITEM_T));
	if (newItem == NULL) {
		perror("createSimpleListItem: malloc newItem");
		return NULL;
	}

	newItem->next = NULL;
	newItem->object = object;

	return newItem;
}

void freeSimpleListItem(struct SIMPLELISTITEM_T *item)
{
	if (item != NULL) {
		free(item);
	}
}

void freeSimpleList(struct SIMPLELISTITEM_T *item)
{
	struct SIMPLELISTITEM_T *tmpItem = item;
	struct SIMPLELISTITEM_T *tmpItem2;

	while (tmpItem) {
		tmpItem2 = tmpItem->next;
		free(tmpItem);
		tmpItem = tmpItem2;
	}
}

void addObjectToSimpleList(struct SIMPLELISTITEM_T *parent, void *object)
{
	struct SIMPLELISTITEM_T *tmpItem = createSimpleListItem(object);
	addItemToSimpleList(parent, tmpItem);
}

void addItemToSimpleList(struct SIMPLELISTITEM_T *parent, struct SIMPLELISTITEM_T *child)
{
	parent->next = child;
}

struct SIMPLELISTITEM_T *getItemAtSimpleListIndex(struct SIMPLELISTITEM_T *parent, int index)
{
	struct SIMPLELISTITEM_T *tmpItem = parent;
	while ((index > 0) && (tmpItem != NULL)) {
		tmpItem = tmpItem->next;
		index--;
	}

	return tmpItem;
}

int simpleListCount(struct SIMPLELISTITEM_T *parent)
{
	int result = 0;
	struct SIMPLELISTITEM_T *tmpItem = parent;

	while (tmpItem != NULL) {
		result++;
		tmpItem = tmpItem->next;
	}
	return result;
}

struct SIMPLELISTITEM_T *deleteFromSimpleList(struct SIMPLELISTITEM_T **parent, int index)
{
	struct SIMPLELISTITEM_T *tmpItem = parent[0];

	if (tmpItem == NULL) return NULL;

	if (index == 0) {
		parent[0] = tmpItem->next;
		return tmpItem;
	}

	while ((index > 1) && (tmpItem != NULL)) {
		index--;
		tmpItem = tmpItem->next;
	}

	if ((tmpItem != NULL) && (index == 1)) {
		struct SIMPLELISTITEM_T *tmpItem2 = tmpItem->next;
		if (tmpItem2 == NULL) {
			return NULL;
		}
		tmpItem->next = tmpItem2->next;
		return tmpItem2;
	}

	return NULL;
}

