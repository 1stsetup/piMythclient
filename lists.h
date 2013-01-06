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

