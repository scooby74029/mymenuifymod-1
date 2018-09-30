#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>
#include <fat.h>
#include <sys/dir.h>
#include <ogc/isfs.h>
#include <malloc.h>
#include <unistd.h>
#include <network.h>
#include <ogc/lwp_watchdog.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <fcntl.h>

#include "id.h"
#include "sys.h"
#include "video.h"
#include "install.h"
#include "wpad.h"
#include "fat2.h"
#include "usbstorage.h"
#include "global.h"
#include "themes.h"
#include "manager.h"
#include "http.h"

#define MAX_THEMES     500


extern CurthemeStats curthemestats;
extern Installthemestats installthemestats;
const char *Theme = NULL;
u32 Wbuttons;
bool cancel;
dirent_t *nandfilelist;

//void sleep(int);

void *allocate_memory(u32 size){
	return memalign(32, (size+31)&(~31) );
}
s32 __FileCmp(const void *a, const void *b){
	dirent_t *hdr1 = (dirent_t *)a;
	dirent_t *hdr2 = (dirent_t *)b;
	
	if (hdr1->type == hdr2->type){
		return strcmp(hdr1->name, hdr2->name);
	}else{
		return 0;
	}
}
s32 getdir(char *path, dirent_t **ent, u32 *cnt){
	s32 res;
	u32 num = 0;

	int i, j, k;
	
	res = ISFS_ReadDir(path, NULL, &num);
	if(res != ISFS_OK){
		gprintf("Error: could not get dir entry count! (result: %d)\n", res);
		return -1;
	}

	char ebuf[ISFS_MAXPATH + 1];

	char *nbuf = (char *)allocate_memory((ISFS_MAXPATH + 1) * num);
	if(nbuf == NULL){
		gprintf("ERROR: could not allocate buffer for name list!\n");
		return -2;
	}

	res = ISFS_ReadDir(path, nbuf, &num);
	DCFlushRange(nbuf,13*num); //quick fix for cache problems?
	if(res != ISFS_OK){
		gprintf("ERROR: could not get name list! (result: %d)\n", res);
		free(nbuf);
		return -3;
	}
	
	*cnt = num;
	
	*ent = allocate_memory(sizeof(dirent_t) * num);
	if(*ent==NULL){
		gprintf("Error: could not allocate buffer\n");
		free(nbuf);
		return -4;
	}

	for(i = 0, k = 0; i < num; i++){	    
		for(j = 0; nbuf[k] != 0; j++, k++)
			ebuf[j] = nbuf[k];
		ebuf[j] = 0;
		k++;

		strcpy((*ent)[i].name, ebuf);
		//gprintf("Name of file (%s)\n",(*ent)[i].name);
	}
	
	qsort(*ent, *cnt, sizeof(dirent_t), __FileCmp);
	
	free(nbuf);
	return 0;
}
s32 read_file(char *filepath, char **buffer, u32 *filesize){
	s32 Fd;
	int ret;
	
	
	if(buffer == NULL){
		gprintf("NULL Pointer\n");
		return -1;
	}

	Fd = ISFS_Open(filepath, ISFS_OPEN_READ);
	if (Fd < 0){
		gprintf("\n   * ISFS_Open %s failed %d", filepath, Fd);
		//Pad_WaitButtons();
		return Fd;
	}

	fstats *status;
	status = allocate_memory(sizeof(fstats));
	if (status == NULL){
		gprintf("Out of memory for status\n");
		return -1;
	}
	
	ret = ISFS_GetFileStats(Fd, status);
	if (ret < 0){
		gprintf("ISFS_GetFileStats failed %d\n", ret);
		ISFS_Close(Fd);
		free(status);
		return -1;
	}
	
	*buffer = allocate_memory(status->file_length);
	if (*buffer == NULL){
		gprintf("Out of memory for buffer\n");
		ISFS_Close(Fd);
		free(status);
		return -1;
	}
		
	ret = ISFS_Read(Fd, *buffer, status->file_length);
	if (ret < 0){
		printf("ISFS_Read failed %d\n", ret);
		ISFS_Close(Fd);
		free(status);
		free(*buffer);
		return ret;
	}
	ISFS_Close(Fd);

	*filesize = status->file_length;
	free(status);

	return 0;
}
char *Sversion2(u32 num){
	switch(num){
		case 0:{
			curthemestats.version = 32;
			curthemestats.region = (u8*)85;
			return "00000042.app";// usa
		}
		break;
		case 1:{
			curthemestats.version = 40;
			curthemestats.region = (u8*)85;
			return "00000072.app";
		}
		break;
		case 2:{
			curthemestats.version = 41;
			curthemestats.region = (u8*)85;
			return "0000007b.app";
		}
		break;
		case 3:{
			curthemestats.version = 42;
			curthemestats.region = (u8*)85;
			return "00000087.app";
		}
		break;
		case 4:{
			curthemestats.version = 43;
			curthemestats.region = (u8*)85;
			return "00000097.app";// usa
		}
		break;
		case 5:{
			curthemestats.version = 32;
			curthemestats.region = (u8*)69;
			return "00000045.app";// pal
		}
		break;
		case 6:{
			curthemestats.version = 40;
			curthemestats.region = (u8*)69;
			return "00000075.app";
		}
		break;
		case 7:{
			curthemestats.version = 41;
			curthemestats.region = (u8*)69;
			return "0000007e.app";
		}
		break;
		case 8:{
			curthemestats.version = 42;
			curthemestats.region = (u8*)69;
			return "0000008a.app";
		}
		break;
		case 9:{
			curthemestats.version = 43;
			curthemestats.region = (u8*)69;
			return "0000009a.app";// pal
		}
		break;
		case 10:{
			curthemestats.version = 32;
			curthemestats.region = (u8*)74;
			return "00000040.app";// jpn
		}
		break;
		case 11:{
			curthemestats.version = 40;
			curthemestats.region = (u8*)74;
			return "00000070.app";
		}
		break;
		case 12:{
			curthemestats.version = 41;
			curthemestats.region = (u8*)74;
			return "00000078.app";
		}
		break;
		case 13:{
			curthemestats.version = 42;
			curthemestats.region = (u8*)74;
			return "00000084.app";
		}
		break;
		case 14:{
			curthemestats.version = 43;
			curthemestats.region = (u8*)74;
			return "00000094.app";// jpn
		}
		break;
		case 15:{
			curthemestats.version = 41;
			curthemestats.region = (u8*)75;
			return "0000008d.app";// kor
		}
		break;
		case 16:{
			curthemestats.version = 42;
			curthemestats.region = (u8*)75;
			return "00000081.app";
		}
		break;
		case 17:{
			curthemestats.version = 43;
			curthemestats.region = (u8*)75;
			return "0000009d.app";// kor
		}
		break;
		default: return "UNK";
		break;
	}
}



char *Sversion(u32 num){
	switch(num){
		case 0: return "00000042";// usa
		break;
		case 1: return "00000072";
		break;
		case 2: return "0000007b";
		break;
		case 3: return "00000087";
		break;
		case 4: return "00000097";// usa
		break;
		case 5: return "00000045";// pal
		break;
		case 6: return "00000075";
		break;
		case 7: return "0000007e";
		break;
		case 8: return "0000008a";
		break;
		case 9: return "0000009a";// pal
		break;
		case 10: return "00000040";// jpn
		break;
		case 11: return "00000070";
		break;
		case 12: return "00000078";
		break;
		case 13: return "00000084";
		break;
		case 14: return "00000094";// jpn
		break;
		case 15: return "0000008d";// kor
		break;
		case 16: return "00000081";
		break;
		case 17: return "0000009d";// kor
		break;
		default: return "UNK";
		break;
	}
}


char *getappname(u32 Versionsys){
	switch(Versionsys){
		case 289: return "00000042";// usa
		break;
		case 417: return "00000072";
		break;
		case 449: return "0000007b";
		break;
		case 481: return "00000087";
		break;
		case 513: return "00000097";// usa
		break;
		case 290: return "00000045";// pal
		break;
		case 418: return "00000075";
		break;
		case 450: return "0000007e";
		break;
		case 482: return "0000008a";
		break;
		case 514: return "0000009a";// pal
		break;
		case 288: return "00000040";// jpn
		break;
		case 416: return "00000070";
		break;
		case 448: return "00000078";
		break;
		case 480: return "00000084";
		break;
		case 512: return "00000094";// jpn
		break;
		case 486: return "0000008d";// kor
		break;
		case 454: return "00000081";
		break;
		case 518: return "0000009d";// kor
		break;
		default: return "UNK";
		break;
	}
}

void checknandapp(){
	gprintf("check nandapp(%d):\n",Versionsys);
	switch(Versionsys)
	{
		case 288:{
			curthemestats.version = 32;
			curthemestats.region = (u8*)74;
		}
		break;
		case 289:{
			curthemestats.version = 32;
			curthemestats.region = (u8*)85;
		}
		break;
		case 290:{
			curthemestats.version = 32;
			curthemestats.region = (u8*)69;
		}
		break;
		case 416:{
			curthemestats.version = 40;
			curthemestats.region = (u8*)74;
		}
		break;
		case 417:{
			curthemestats.version = 40;
			curthemestats.region = (u8*)85;
		}
		break;
		case 418:{
			curthemestats.version = 40;
			curthemestats.region = (u8*)69;

		}
		break;
		case 448:{
			curthemestats.version = 41;
			curthemestats.region = (u8*)74;
		}
		break;
		case 449:{
			curthemestats.version = 41;
			curthemestats.region = (u8*)85;
		}
		break;
		case 450:{
			curthemestats.version = 41;
			curthemestats.region = (u8*)69;
		}
		break;
		case 454:{
			curthemestats.version = 41;
			curthemestats.region = (u8*)75;
		}
		break;
		case 480:{
			curthemestats.version = 42;
			curthemestats.region = (u8*)74;
		}
		break;
		case 481:{
			curthemestats.version = 42;
			curthemestats.region = (u8*)85;
		}
		break;
		case 482:{
			curthemestats.version = 42;
			curthemestats.region = (u8*)69;
		}
		break;
		case 486:{
			curthemestats.version = 42;
			curthemestats.region = (u8*)75;
		}
		break;
		case 512:{
			curthemestats.version = 43;
			curthemestats.region = (u8*)74;
		}
		break;
		case 513:{
			curthemestats.version = 43;
			curthemestats.region = (u8*)85;
		}
		break;
		case 514:{
			curthemestats.version = 43;
			curthemestats.region = (u8*)69;
		}
		break;
		case 518:{
			curthemestats.version = 43;
			curthemestats.region = (u8*)75;
		}
		break;
		default:{
			s32 rtn;
			u32 nandfilesizetmp,j;
			char *CHECK;
			
			gprintf("versionsys(%d) \n",Versionsys);
			if(Versionsys > 518){
				rtn = getdir("/title/00000001/00000002/content",&nandfilelist,&nandfilesizetmp);
				gprintf("rtn(%d) \n",rtn);
				int k,done;
				for(k = 0;k <nandfilesizetmp ;){
					gprintf("name = %s \n",nandfilelist[k]);
					for(j = 0;j < KNVERSIONS;){
						done = 0;
						CHECK = Sversion2(j);
						//gprintf("check =%s \n",CHECK);
						if(strcmp(nandfilelist[k].name,CHECK) == 0){
							gprintf("equal \n");
							done = 1;
							break;
						}
						gprintf("no match \n");
						j++;
					}
					if(done == 1)
						break;
					k++;
				}
			}
			
			//gprintf("cur .version(%d) .region(%c) \n",curthemestats.version,curthemestats.region);
			//free(buffer);
		}
		break;
	}

	
	return;
}
