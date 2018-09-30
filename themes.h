#ifndef THEMES_H
#define THEMES_H

#define KNVERSIONS    17

typedef struct _dirent{
	char name[ISFS_MAXPATH + 1];
	int type;
	u32 ownerID;
	u16 groupID;
	u8 attributes;
	u8 ownerperm;
	u8 groupperm;
	u8 otherperm;
} dirent_t;



typedef struct{
	u8 *name;
	u8 *region;
	u32 version;
	int type;
	u32 size;
}CurthemeStats;

typedef struct{
	u8 *name;
	u8 *region;
	u32 version;
	int type;
	u32 size;
}Installthemestats;

u32 Versionsys;
u32 ThemeCount;

void *allocate_memory(u32 size);
char *getappname2(u32 Versionsys);
char *getappname(u32 Versionsys);
void checknandapp();
s32 __FileCmp(const void *a, const void *b);
s32 read_file(char *filepath, char **buffer, u32 *filesize);
s32 getdir(char *path, dirent_t **ent, u32 *cnt);
char *Sversion2(u32 num);
int downloadApp();

#endif// THEMES_H
