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

struct LISTITEM_T;
struct LISTITEM_T{
	char	*string;
	struct LISTITEM_T *next;
	struct LISTITEM_T *prev;
};

struct SIMPLELISTITEM_T;
struct SIMPLELISTITEM_T{
	void	*object;
	struct SIMPLELISTITEM_T *next;
};

struct LISTITEM_T *createListItem(char *text);
void freeListItem(struct LISTITEM_T *item);
void freeList(struct LISTITEM_T *item);
void addItemToList(struct LISTITEM_T *parent, struct LISTITEM_T *child);
struct LISTITEM_T *convertStrToList(char *text, char *splitStr);
char *convertListToString(struct LISTITEM_T *parent, char *splitStr);
struct LISTITEM_T *getItemAtListIndex(struct LISTITEM_T *parent, int index);
char *getStringAtListIndex(struct LISTITEM_T *parent, int index);
int listCount(struct LISTITEM_T *parent);

struct SIMPLELISTITEM_T *createSimpleListItem(void *object);
void freeSimpleListItem(struct SIMPLELISTITEM_T *item);
void freeSimpleList(struct SIMPLELISTITEM_T *item);
void addObjectToSimpleList(struct SIMPLELISTITEM_T *parent, void *object);
void addItemToSimpleList(struct SIMPLELISTITEM_T *parent, struct SIMPLELISTITEM_T *child);
struct SIMPLELISTITEM_T *getItemAtSimpleListIndex(struct SIMPLELISTITEM_T *parent, int index);
int simpleListCount(struct SIMPLELISTITEM_T *parent);
struct SIMPLELISTITEM_T *deleteFromSimpleList(struct SIMPLELISTITEM_T **parent, int index);

