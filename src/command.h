#ifndef __COMMAND_H__
#define __COMMAND_H__


#include "data.h"


struct treeNode_t;
typedef struct treeNode_t {
    char hotkey;
    char *str;
    callback func;
    struct treeNode_t *children;
    int nChildren;
    struct treeNode_t *parent;
} TreeNode;


extern TreeNode commands;

extern callback delayedCmd;
extern char delayedExtra;
TreeNode *getDelayTree(TreeNode *parent);

void initializeCommands();
bool updateCommands(AtcsoData *data);


#endif
