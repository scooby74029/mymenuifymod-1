#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>
#include <fat.h>
#include <sys/dir.h>
#include <ogc/isfs.h>
#include <malloc.h>
#include <wiiuse/wpad.h>
#include <network.h>
#include <gccore.h>
#include <sys/errno.h>
#include <smb.h>
#include <sdcard/wiisd_io.h>
#include <dirent.h>
#include <wiiuse/wpad.h>

#include "smb.h"
#include "id.h"
#include "sys.h"
#include "video.h"
#include "install.h"
#include "wpad.h"
#include "fat2.h"
#include "usbstorage.h"
#include "global.h"
#include "themes.h"
#include "http.h"
#include "rijndael.h"
#include "libpng/pngu/pngu.h"
#include "grfx/menumod.h"


#define DEVMAX          3

/* filelist constants */
#define FILES_PER_PAGE		5
#define FILE_DIRECTORY		"themes"

/* console constants */
#define CONSOLE_XCOORD		10
#define CONSOLE_YCOORD		100

/* Constants */
#define MAX_FILELIST_LEN	65536
#define MAX_FILEPATH_LEN	256
#define MAX_Uneek_filelist  25

/* Variables */
dirent_t *ent = NULL;
static s32 nb_files=0, start=0, selected=0;
bool firsttime;
int i,j = 1,ret;
int device = 1;
u32 buttons, GCbuttons; 
int reload = 0,reloadios = 0,defaultios = 249,defaultios2 = 36, ios = 0;
extern const unsigned char menumod[];

u8 commonkey[16] = { 0xeb, 0xe4, 0x2a, 0x22, 0x5e, 0x85, 0x93, 0xe4, 0x48,
     0xd9, 0xc5, 0x45, 0x73, 0x81, 0xaa, 0xf7
  };

dirent_t *neeklist;
dirent_t *nandfilelist2;
u32 neekcount,cntr;
IMGCTX ctx;
void pngu_free_info(IMGCTX ctx);
void manage(u32,bool,bool);
const char *dev_getname(int);
void sleep(int);
void usleep(int);
CurthemeStats curthemestats;
Installthemestats installthemestats;
UserSelect_Device();
ios_selectionmenu(default_ios);
void __exception_setreload(int t);

void restart(void)
{
    printf("\n[+] Restarting Wii...\n");
    fflush(stdout);

    sys_loadmenu();
}

void restart_wait(void)
{
    printf("\n[+] Press Any Button to Restart HBC...");
    fflush(stdout);

    /* Wait for button */
    wpad_waitbuttons();

    printf("restarting... HBC...\n");
    fflush(stdout);

    /* Load HBC | system menu */
    sysHBC();
}

void show_banner(void)
{
    PNGUPROP imgProp;
    s32 ret;

    /* Select PNG data */
    ctx = PNGU_SelectImageFromBuffer(menumod);
    if (!ctx)
        return;

    /* Get image properties */
    ret = PNGU_GetImageProperties(ctx, &imgProp);
    if (ret != PNGU_OK)
        return;

    /* Draw image */
    video_drawpng(ctx, imgProp, 0, 0);

    /* Free image context */
    PNGU_ReleaseImageContext(ctx);
}

void freeBanner()
{
	pngu_free_info(ctx);
}

char *getregion(u32 num)
{
    switch(num)
    {
    case 289:
    case 417:
    case 449:
    case 481:
    case 513:
        return "Usa";
        break;
    case 290:
    case 418:
    case 450:
    case 482:
    case 514:
        return "Pal";
        break;
    case 288:
    case 416:
    case 448:
    case 480:
    case 512:
        return "Jpn";
        break;
    case 486:
    case 454:
    case 518:
        return "Kor";
        break;
    default:
        if(curthemestats.region == (u8*)69)
			return "Pal";
		else if(curthemestats.region == (u8*)74)
			return "Jpn";
		else if(curthemestats.region == (u8*)75)
			return "Kor";
		else if(curthemestats.region == (u8*)85)
			return "Usa";
		else return "Unk";
    break;
    }
}
char *getsysvernum(u32 num)
{
    switch(num)
    {
    case 288:
    case 289:
    case 290:
        return "3.2";
        break;
    case 416:
    case 417:
    case 418:
        return "4.0";
        break;
    case 448:
    case 449:
    case 450:
	case 454:
        return "4.1";
        break;
    case 480:
    case 481:
    case 482:
	case 486:
        return "4.2";
        break;
    case 512:
    case 513:
    case 514:
	case 518:
        return "4.3";
        break;
    default:
         switch(curthemestats.version)
		{
			case 32: return "3.2";
			break;
			case 40: return "4.0";
			break;
			case 41: return "4.1";
			break;
			case 42: return "4.2";
			break;
			case 43: return "4.3";
			break;
			default: return "unk";
			break;
		}
        break;
    }
}


u32 GetSysMenuVersion()
{
    //Get sysversion from TMD
    u64 TitleID = 0x0000000100000002LL;
    u32 tmd_size;
    s32 r = ES_GetTMDViewSize(TitleID, &tmd_size);
    if(r<0)
    {
        gprintf("error getting TMD views Size. error %d\n",r);
        return 0;
    }

    tmd_view *rTMD = (tmd_view*)memalign( 32, (tmd_size+31)&(~31) );
    if( rTMD == NULL )
    {
        gprintf("error making memory for tmd views\n");
        return 0;
    }
    memset(rTMD,0, (tmd_size+31)&(~31) );
    r = ES_GetTMDView(TitleID, (u8*)rTMD, tmd_size);
    if(r<0)
    {
        gprintf("error getting TMD views. error %d\n",r);
        free( rTMD );
        return 0;
    }
    u32 version = rTMD->title_version;
    if(rTMD)
    {
        free(rTMD);
    }
    return version;
}

void checkfile(FILE *fp)
{
    long lSize;
    char * buffer;
    size_t result;
    int i;

    fseek (fp , 0 , SEEK_END);
    lSize = ftell (fp);
    rewind (fp);

    buffer = (char*) malloc (sizeof(char)*lSize);
    if (buffer == NULL)
    {
        printf("Could not allocate memory. \n");
        return;
    }

    result = fread (buffer,1,lSize,fp);
    if (result != lSize)
    {
        printf("Error reading file. \n");
        return;
    }
    gprintf("for buffer \n");
    char *s1;
    char *s2 = "c";

    for(i= 0; i < lSize; i++)
    {
        s1 = &buffer[i];
        gprintf("s1 (%s)  (%s)s2 i(%d) \n",s1,s2,i);
        if(strcmp(s1 ,s2)==0)
            gprintf("EQUAL  !!! \n");

    }
}

int getcurrentregion()
{
	int ret = 0;
	
	if(curthemestats.region == (u8*)69)
		ret = 1;
	else if(curthemestats.region == (u8*)74)
		ret = 2;
	else if(curthemestats.region == (u8*)75)
		ret = 3;
	else if(curthemestats.region == (u8*)85)
		ret = 4;
		
	return ret;
}
int getinstallregion()
{
	int ret = 0;
	
	if(installthemestats.region == (u8*)69)
		ret = 1;
	else if(installthemestats.region == (u8*)74)
		ret = 2;
	else if(installthemestats.region == (u8*)75)
		ret = 3;
	else if(installthemestats.region == (u8*)85)
		ret = 4;
		
	return ret;
}
void manage(u32 index,bool foundSD,bool foundUSB)
{
    char *filepath = (char*)memalign(32,256);

    FILE *fp = NULL;
    u32 length,i;
    u8 *data;
    //u8 *buffer;
    //u32 buttons;

    /* Move to the specified entry */
	gprintf("index = %d  [%i] [%i] \n",index, foundSD, foundUSB);

    // Generate filepath
    

        sprintf(filepath, "Fat:/"FILE_DIRECTORY"/%s", ent[index].name);
        fflush(stdout);
        gprintf("Manage:filepath:%s\n",filepath);

        fp = fopen(filepath, "rb");
        if (!fp)
        {
            gprintf("[+] File Open Error not on SD!\n");
        }
        length = filesize(fp);
        data = allocate_memory(length);
        memset(data,0,length);
        fread(data,1,length,fp);

        if(length <= 0)
        {
            printf("[-] Unable to read file !! \n");
            return;
        }
        else
        {
            for(i = 0; i < length;)
            {
                if(data[i] == 83)
                {
                    if(data[i+6] == 52)  // 4
                    {
                        if(data[i+8] == 48)  // 0
                        {
                            if(data[i+28] == 85)  // usa
                            {
                                installthemestats.version = 40;
                                installthemestats.region = (u8*)85;
                                break;

                            }
                            else if(data[i+28] == 74)  //jap
                            {
                                installthemestats.version = 40;
                                installthemestats.region = (u8*)74;
                                break;
                            }
                            else if(data[i+28] == 69)  // pal
                            {
                                installthemestats.version = 40;
                                installthemestats.region = (u8*)69;
                                break;
                            }
                        }
                        else
                        {
                            if(data[i+8] == 49)  // 4.1
                            {
                                if(data[i+31] == 85)  // usa
                                {
                                    installthemestats.version = 41;
                                    installthemestats.region = (u8*)85;
                                    break;

                                }
                                else if(data[i+31] == 74)  //jap
                                {
                                    installthemestats.version = 41;
                                    installthemestats.region = (u8*)74;
                                    break;
                                }
                                else if(data[i+31] == 69)  // pal
                                {
                                    installthemestats.version = 41;
                                    installthemestats.region = (u8*)69;
                                    break;
                                }
                                else if(data[i+31] == 75)  // kor
                                {
                                    installthemestats.version = 41;
                                    installthemestats.region = (u8*)75;
                                    break;
                                }

                            }
                            else
                            {
                                if(data[i+8] == 50)  // 4.2
                                {
                                    if(data[i+28] == 85)  // usa
                                    {
                                        installthemestats.version = 42;
                                        installthemestats.region = (u8*)85;
                                        break;
                                    }
                                    else if(data[i+28] == 74)  // jap
                                    {
                                        installthemestats.version = 42;
                                        installthemestats.region = (u8*)74;
                                        break;
                                    }
                                    else if(data[i+28] == 69)  // pal
                                    {
                                        installthemestats.version = 42;
                                        installthemestats.region = (u8*)69;
                                        break;
                                    }
                                    else if(data[i+28] == 75)  // kor
                                    {
                                        installthemestats.version = 42;
                                        installthemestats.region = (u8*)75;
                                        break;
                                    }
                                }
                                else
                                {
                                    if(data[i+8] == 51) // 4.3
                                    {
                                        if(data[i+28] == 85)  // usa
                                        {
                                            installthemestats.version = 43;
                                            installthemestats.region = (u8*)85;
                                            break;
                                        }
                                        else if(data[i+28] == 74)  //jap
                                        {
                                            installthemestats.version = 42;
                                            installthemestats.region = (u8*)74;
                                            break;
                                        }
                                        else if(data[i+28] == 69)  // pal
                                        {
                                            installthemestats.version = 43;
                                            installthemestats.region = (u8*)69;
                                            break;
                                        }
                                        else if(data[i+28] == 75)  // kor
                                        {
                                            installthemestats.version = 43;
                                            installthemestats.region = (u8*)75;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        if(data[i+6] == 51)  // 3
                        {
                            if(data[i+8] == 50)  // 2
                            {
                                if(data[i+28] == 85)  // usa
                                {
                                    installthemestats.version = 32;
                                    installthemestats.region = (u8*)85;
                                    break;
                                }
                                else
                                {
                                    if(data[i+28] == 69)  // pal
                                    {
                                        installthemestats.version = 32;
                                        installthemestats.region = (u8*)69;
                                        break;
                                    }
                                    else
                                    {
                                        if(data[i+28] == 74)  // jap
                                        {
                                            installthemestats.version = 32;
                                            installthemestats.region = (u8*)74;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                i++;
            }

        }


        int current;
		current = getcurrentregion();
		int install;
		install = getinstallregion();
		
		gprintf("current(%d) install(%d) \n",current,install);
        gprintf("install theme .version(%d) .region(%c) \n",installthemestats.version,installthemestats.region);
        gprintf("cur theme .version(%d) .region(%c) \n",curthemestats.version,curthemestats.region);
		
        if(curthemestats.version != installthemestats.version)
        {
            con_clear();
            printf("\n\n\n\n\nInstall can not continue ! The install theme version %d is not a match \n\nfor the system menu version %d  \
			\n\nPlease press any button to Exit to HBC ! \n",installthemestats.version,curthemestats.version);
            free(data);
            fclose(fp);
            buttons = wpad_waitbuttons();
            sysHBC();
        }
        else if(current != install)
        {
            con_clear();
            printf("\n\n\n\n\nInstall can not continue ! The install theme region is not a match \n\nfor the system menu region.  \
			\n\nPlease press any button to Exit to HBC ! \n");
            free(data);
            fclose(fp);
            buttons = wpad_waitbuttons();
            sysHBC();
        }
        free(data);
        fclose(fp);
        fp = fopen(filepath, "rb");
        if (!fp)
        {
            gprintf("[+] File Open Error not on SD!\n");
        }
    /*
    if(foundUSB)
    {
        sprintf(filepath, "Usb:/"FILE_DIRECTORY"/%s",ent[index].name);
        gprintf("Manage:filepath:%s\n",filepath);
        fflush(stdout);
        fp = fopen(filepath, "rb");
		if (!fp)
		{
			printf("[+] File Open Error not on USB!\n");
			return;
		}

        length = filesize(fp);
        data = allocate_memory(length);
        memset(data,0,length);
        fread(data,1,length,fp);

        if(length <= 0)
        {
            printf("[-] Unable to read file !! \n");
            return;
        }
        else
        {
            for(i = 0; i < length;)
            {
                if(data[i] == 83)
                {
                    if(data[i+6] == 52)  // 4
                    {
                        if(data[i+8] == 48)  // 0
                        {
                            if(data[i+28] == 85)  // usa
                            {
                                installthemestats.version = 40;
                                installthemestats.region = (u8*)85;
                                break;

                            }
                            else if(data[i+28] == 74)  //jap
                            {
                                installthemestats.version = 40;
                                installthemestats.region = (u8*)74;
                                break;
                            }
                            else if(data[i+28] == 69)  // pal
                            {
                                installthemestats.version = 40;
                                installthemestats.region = (u8*)69;
                                break;
                            }
                        }
                        else
                        {
                            gprintf("\n\ndata [%s] [%s] \n\n", data[i+8], data[i+31]);
					   if(data[i+8] == 49)  // 4.1
                            {
                                if(data[i+31] == 85)  // usa
                                {
                                    installthemestats.version = 41;
                                    installthemestats.region = (u8*)85;
                                    break;

                                }
                                else if(data[i+31] == 74)  //jap
                                {
                                    installthemestats.version = 41;
                                    installthemestats.region = (u8*)74;
                                    break;
                                }
                                else if(data[i+31] == 69)  // pal
                                {
                                    installthemestats.version = 41;
                                    installthemestats.region = (u8*)69;
                                    break;
                                }
                                else if(data[i+31] == 75)  // kor
                                {
                                    installthemestats.version = 41;
                                    installthemestats.region = (u8*)75;
                                    break;
                                }
                                else
                                {
                                    if(data[i+8] == 50)  // 4.2
                                    {
                                        if(data[i+28] == 85)  // usa
                                        {
                                            installthemestats.version = 42;
                                            installthemestats.region = (u8*)85;
                                            break;
                                        }
                                        else if(data[i+28] == 74)  // jap
                                        {
                                            installthemestats.version = 42;
                                            installthemestats.region = (u8*)74;
                                            break;
                                        }
                                        else if(data[i+28] == 69)  // pal
                                        {
                                            installthemestats.version = 42;
                                            installthemestats.region = (u8*)69;
                                            break;
                                        }
                                        else if(data[i+28] == 75)  // kor
                                        {
                                            installthemestats.version = 42;
                                            installthemestats.region = (u8*)75;
                                            break;
                                        }
                                    }
                                    else
                                    {
                                        if(data[i+8] == 51) // 4.3
                                        {
                                            if(data[i+28] == 85)  // usa
                                            {
                                                installthemestats.version = 43;
                                                installthemestats.region = (u8*)85;
                                                break;
                                            }
                                            else if(data[i+28] == 74)  //jap
                                            {
                                                installthemestats.version = 42;
                                                installthemestats.region = (u8*)74;
                                                break;
                                            }
                                            else if(data[i+28] == 69)  // pal
                                            {
                                                installthemestats.version = 43;
                                                installthemestats.region = (u8*)69;
                                                break;
                                            }
                                            else if(data[i+28] == 75)  // kor
                                            {
                                                installthemestats.version = 43;
                                                installthemestats.region = (u8*)75;
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        if(data[i+6] == 51)  // 3
                        {
                            if(data[i+8] == 50)  // 2
                            {
                                if(data[i+28] == 85)  // usa
                                {
                                    installthemestats.version = 32;
                                    installthemestats.region = (u8*)85;
                                    break;
                                }
                                else
                                {
                                    if(data[i+28] == 69)  // pal
                                    {
                                        installthemestats.version = 32;
                                        installthemestats.region = (u8*)69;
                                        break;
                                    }
                                    else
                                    {
                                        if(data[i+28] == 74)  // jap
                                        {
                                            installthemestats.version = 32;
                                            installthemestats.region = (u8*)74;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                i++;
            }

        }
		int current;
		current = getcurrentregion();
		int install;
		install = getinstallregion();
		
		gprintf("current(%d) install(%d) \n",current,install);
        gprintf("install theme .version(%d) .region(%c) \n",installthemestats.version,installthemestats.region);
        gprintf("cur theme .version(%d) .region(%c) \n",curthemestats.version,curthemestats.region);
		
		
		
        if(curthemestats.version != installthemestats.version)
        {
            con_clear();
            printf("\n\n\n\n\nInstall can not continue ! The install theme version[%u] is not a match \n\nfor the system menu version[%u]  \
			\n\nPlease press any button to Exit to HBC ! \n",installthemestats.version,curthemestats.version);
            free(data);
            fclose(fp);
            wpad_waitbuttons();
            sysHBC();
        }
        else if(current != install)
        {
            con_clear();
            printf("\n\n\n\n\nInstall can not continue ! The install theme region is not a match \n\nfor the system menu region.  \
			\n\nPlease press any button to Exit to HBC ! \n");
            free(data);
            fclose(fp);
            wpad_waitbuttons();
            sysHBC();
        }
        
        free(data);
        fclose(fp);
        fp = fopen(filepath, "rb");
        if (!fp)
        {
            gprintf("[+] File Open Error not on USB!\n");
        }

    }


    */
    /* Install */
    InstallFile(fp);

    /* Close file */
    if (fp)
        fclose(fp);
}

void filelist_scroll(s8 delta)
{
    s32 index;

    /* No files */
    if (!nb_files)
        return;

    /* Select next entry */
    selected += delta;


    /* Out of the list? */
    if (selected <= -1)
        selected = nb_files - 1;
    if (selected >= nb_files)
        selected = 0;

    /* List scrolling */
    index = (selected - start);

    if (index >= FILES_PER_PAGE)
        start += index - (FILES_PER_PAGE - 1);
    if (index <= -1)
        start += index;
    //gprintf("selected(%d) delta(%d) index(%d)\n",selected,delta,index);
}

void filelist_print(int i)
{
        u32 cnt;
        /* No files */
        if(!nb_files)
        {
            printf("\n	[-] No Files Found on %s\n",dev_getname(i));
            return;
        }

        /* Show entries */
        for (cnt = start; cnt < nb_files; cnt++)
        {
            /* Files per page limit */
            if ((cnt - start) >= FILES_PER_PAGE)
                break;

            /* Selected file */
            (cnt == selected) ? printf("-> ") : printf("   ");
            fflush(stdout);

            /* Print filename */
            printf("%s\n", ent[cnt].name);
        }
}



s32 getfilesuneek(char *path)
{
    s32 rtn;
    gprintf("path(%s) \n",path);
    rtn = getdir(path,&neeklist,&neekcount);
    if(rtn < 0)
    {
        gprintf("gitdir failed (%d) \n",rtn);
        return -1;
    }
    for(i = 0; i < neekcount;)
    {

        gprintf("path(%s) neeklist[i](%s)  neekcount(%d) \n",path,neeklist[i].name,neekcount);
        i++;
    }
    if(neekcount > 0)
    {
        foundUSBuneek = true;
        return 1;
    }
    else
    {
        foundUSBuneek = false;
        return 0;
    }

}

s32 filelist_retrieve(int i)
{
    char *dirpath = (char*)memalign(32,256);
    //char dirpath2[ISFS_MAXPATH + 1];
    //s32 rtn;

    /* Generate dirpath */
     sprintf(dirpath, "%s:/"FILE_DIRECTORY, DEV_MOUNT);
     gprintf("Path: %s\n",dirpath);
    
    /* Reset file counter */
    nb_files = 0;

	DIR *mydir;
	mydir = opendir(dirpath);
	u32 x = 0;
    struct dirent *entry = NULL;
	
    while((entry = readdir(mydir))) // If we get EOF, the expression is 0 and
                                     // the loop stops. 
    {
		if(strncmp(entry->d_name, ".", 1) != 0 && strncmp(entry->d_name, "..", 2) != 0)
		x+=1;
    }
	nb_files = x;
	gprintf("x = %d nb_files = %d \n",x,nb_files);
    closedir(mydir);
	ent = allocate_memory(sizeof(dirent_t) * x);
	x = 0;
	mydir = opendir(dirpath);
	while((entry = readdir(mydir)))
	{
		gprintf("start next while \n");
		if(strncmp(entry->d_name, ".", 1) != 0 && strncmp(entry->d_name, "..", 2) != 0)
		{	
			strcpy(ent[x].name, entry->d_name);
			gprintf("ent[x].name %s \n",ent[x].name);
			x+=1;
		}
	}
	gprintf("leaving retreive \n");
    return 0;
}

void waitforbuttonpress(u32 *out, u32 *outGC)
{
    u32 pressed = 0;
    u32 pressedGC = 0;

    while (true)
    {
        WPAD_ScanPads();
        pressed = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);

        PAD_ScanPads();
        pressedGC = PAD_ButtonsDown(0) | PAD_ButtonsDown(1) | PAD_ButtonsDown(2) | PAD_ButtonsDown(3);

        if(pressed || pressedGC)
        {
            if (pressedGC)
            {
                // Without waiting you can't select anything
                usleep (20000);
            }
            if (out) *out = pressed;
            if (outGC) *outGC = pressedGC;
            return;
        }
    }
}



char *getsavename(u32 idx)
{
    switch(idx)
    {
    case 289:
        return "42";// usa
        break;
    case 417:
        return "72";
        break;
    case 449:
        return "7b";
        break;
    case 481:
        return "87";
        break;
    case 513:
        return "97";// usa
        break;
    case 290:
        return "45";// pal
        break;
    case 418:
        return "75";
        break;
    case 450:
        return "7e";
        break;
    case 482:
        return "8a";
        break;
    case 514:
        return "9a";// pal
        break;
    case 288:
        return "40";// jpn
        break;
    case 416:
        return "70";
        break;
    case 448:
        return "78";
        break;
    case 480:
        return "84";
        break;
    case 512:
        return "94";// jpn
        break;
    case 486:
        return "8d";// kor
        break;
    case 454:
        return "81";
        break;
    case 518:
        return "9d";// kor
        break;
    default:
        return "UNK";
        break;
    }
}
void get_title_key(signed_blob *s_tik, u8 *key)
{
    static u8 iv[16] ATTRIBUTE_ALIGN(0x20);
    static u8 keyin[16] ATTRIBUTE_ALIGN(0x20);
    static u8 keyout[16] ATTRIBUTE_ALIGN(0x20);

    const tik *p_tik;
    p_tik = (tik*) SIGNATURE_PAYLOAD(s_tik);
    u8 *enc_key = (u8 *) &p_tik->cipher_title_key;
    memcpy(keyin, enc_key, sizeof keyin);
    memset(keyout, 0, sizeof keyout);
    memset(iv, 0, sizeof iv);
    memcpy(iv, &p_tik->titleid, sizeof p_tik->titleid);

    aes_set_key(commonkey);
    aes_decrypt(iv, keyin, keyout, sizeof keyin);

    memcpy(key, keyout, sizeof keyout);
}
static void decrypt_buffer(u16 index, u8 *source, u8 *dest, u32 len)
{
    static u8 iv[16];
    memset(iv, 0, 16);
    memcpy(iv, &index, 2);
    aes_decrypt(iv, source, dest, len);
}
const char *getPath(int ind)
{
    switch(ind)
    {
    case 0:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/cetk";
        break;
    case 1:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/tmd.";
        break;
    case 2:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/0000003f";
        break;
    case 3:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/00000042";
        break;
    case 4:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/00000045";
        break;
    case 5:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/0000006f";
        break;
    case 6:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/00000072";
        break;
    case 7:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/00000075";
        break;
    case 8:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/00000078";
        break;
    case 9:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/0000007b";
        break;
    case 10:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/0000007e";
        break;
    case 11:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/00000081";
        break;
    case 12:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/00000084";
        break;
    case 13:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/00000087";
        break;
    case 14:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/0000008a";
        break;
    case 15:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/0000008d";
        break;
    case 16:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/00000094";
        break;
    case 17:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/00000097";
        break;
    case 18:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/0000009a";
        break;
    case 19:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/0000009d";
        break;
    default:
        return "ERROR";
        break;
    }
}
int getslot(int num)
{
    switch(num)
    {
    case 288:
        return 2;
        break;
    case 289:
        return 3;
        break;
    case 290:
        return 4;
        break;
    case 416:
        return 5;
        break;
    case 417:
        return 6;
        break;
    case 418:
        return 7;
        break;
    case 448:
        return 8;
        break;
    case 449:
        return 9;
        break;
    case 450:
        return 10;
        break;
    case 454:
        return 11;
        break;
    case 480:
        return 12;
        break;
    case 481:
        return 13;
        break;
    case 482:
        return 14;
        break;
    case 486:
        return 15;
        break;
    case 512:
        return 16;
        break;
    case 513:
        return 17;
        break;
    case 514:
        return 18;
        break;
    case 518:
        return 19;
        break;
    default:
        return -1;
        break;
    }
}

u32 checkcustom(u32 m)
{
	s32 rtn;
	u32 nandfilesizetmp,j;
	
	char *CHECK;
			
	gprintf("versionsys(%d) \n",Versionsys);
	if(m > 518){
		rtn = getdir("/title/00000001/00000002/content",&nandfilelist2,&nandfilesizetmp);
		gprintf("rtn(%d) \n",rtn);
		int k;
		for(k = 0;k <nandfilesizetmp ;){
			gprintf("name = %s \n",nandfilelist2[k]);
			for(j = 0;j < KNVERSIONS;){
				CHECK = Sversion2(j);
				//gprintf("check =%s \n",CHECK);
				if(strcmp(nandfilelist2[k].name,CHECK) == 0){
					gprintf("equal j = %d \n",j);
					gprintf("check = %s \n",CHECK);
					switch(j){
						case 0: return 289;
						break;
						case 1: return 417;
						break;
						case 2: return 449;
						break;
						case 3: return 481;
						break;
						case 4: return 513;
						break;
						case 5: return 290;
						break;
						case 6: return 418;
						break;
						case 7: return 450;
						break;
						case 8: return 482;
						break;
						case 9: return 514;
						break;
						case 10: return 288;
						break;
						case 11: return 416;
						break;
						case 12: return 448;
						break;
						case 13: return 480;
						break;
						case 14: return 512;
						break;
						case 15: return 454;
						break;
						case 16: return 486;
						break;
						case 17: return 518;
						break;
						
					}
				
				}
				gprintf("no match \n");
				j++;
			}
			k++;
		}
	}
	return 0;
}

int downloadApp()  
{
	s32 rtn;
    u32 dvers;
    int ret;
    int counter;
    char *savepath = (char*)memalign(32,256);
    char *oldapp = (char*)memalign(32,256);

    dvers = GetSysMenuVersion();
    gprintf("dvers =%d \n",dvers);
    if(dvers > 518){
        
		dvers = checkcustom(dvers);
		
	}
    con_clear();
    printf("\nDownloading %s for System Menu v%u \n",getappname(dvers),dvers);

    Fat_MakeDir("Fat:/tmp");

    signed_blob * s_tik = NULL;
    signed_blob * s_tmd = NULL;
	
    

    printf("Initializing  Network ....");
    for(;;)
    {
        ret=net_init();
        if(ret<0 && ret!=-EAGAIN){
            printf("Failed !!\n");
        }
		if(ret==0){
            printf("Complete !! \n\n");
            break;
        }
    }
		
	for(counter = 0; counter < 3; counter++)
    {	
        char *path = (char*)memalign(32,256);
        int aa = getslot(dvers);


        if(counter == 0)
        {

            sprintf(path,"%s",getPath(counter));
            printf("Dowloading %s ....",path);
        }
        if(counter == 1)
        {

            sprintf(path,"%s%u",getPath(counter),dvers);
            printf("Dowloading %s ....",path);
        }
        if(counter == 2)
        {

            sprintf(path,"%s",getPath(aa));
            printf("Dowloading %s ....",path);
        }
        u32 outlen=0;
        u32 http_status=0;
        ret = http_request(path, 1<<31);
        if(ret == 0 )
        {
            free(path);
            printf("Failed !! ret(%d)\n",ret);
            sleep(3);
            return -1;
        }
        else
            printf("Complete !! \n\n");
        free(path);
        u8* outbuf = (u8*)malloc(outlen);
        if(counter == 0)
        {
            ret = http_get_result(&http_status, (u8 **)&s_tik, &outlen);
        }
        if(counter == 1)
        {
            ret = http_get_result(&http_status, (u8 **)&s_tmd, &outlen);
        }
        if(counter == 2)
        {
            ret = http_get_result(&http_status, &outbuf, &outlen);
        }
        printf("Decrypting files ....");
        //set aes key
        u8 key[16];
        u16 index;
        get_title_key(s_tik, key);
        aes_set_key(key);
        u8* outbuf2 = (u8*)malloc(outlen);
        if(counter == 2)
        {
            if(outlen>0) //suficientes bytes
            {
                index = 01;
                //then decrypt buffer
                decrypt_buffer(index,outbuf,outbuf2,outlen);
                sprintf(savepath,"Fat:/tmp/000000%s.app",getsavename(dvers));
                ret = Fat_SaveFile(savepath, (void *)&outbuf2,outlen);
            }
        }
        printf("Complete !! \n\n");
        if(outbuf!=NULL)
            free(outbuf);
    }
	net_deinit();
	sleep(2);
    sprintf(oldapp,"/title/00000001/00000002/%s.app",getappname(dvers));
	if(!Fat_CheckFile(savepath))
		return -99;
	else
		ISFS_Delete(oldapp);
    FILE *f = NULL;
    f = fopen(savepath,"rb");
    if(!f)
    {
        return -1;
    }
    printf("\nInstalling %s.app ....",getappname(dvers));
	sleep(2);
    /* Install */
    rtn = InstallFile(f);
	if(rtn < 0){
		if(f)
			fclose(f);
		return rtn;
	}
    /* Close file */
    if(f){
        fclose(f);
	}
	int error;
	error = remove(savepath);	
	printf("[+] Done!\n\n	Press any button to return to the SysMenu\n");
	wpad_waitbuttons();
	restart();
    return 0;
}

int ThemePreview(int PreviewNum) {
	gprintf("\n\nin themepreview ...... \n\n");
	
	video_clear(COLOR_BLACK);
	con_clear();
	
	char *filepath = (char*)memalign(32,256);
	//FILE *sd = NULL;
	PNGUPROP imgProp;
	s32 ret;
	//int x = 0;
	sprintf(filepath, "Fat:/themes/images/test.png"); //, ent[PreviewNum].name
	
	gprintf("\n\nfilepath [%s] \n\n", filepath);
	
    /* Select PNG data */
    ctx = PNGU_SelectImageFromDevice(filepath);
    if (!ctx)
        return -314;

    /* Get image properties */
    ret = PNGU_GetImageProperties(ctx, &imgProp);
    if (ret != PNGU_OK)
        return ret;

    /* Draw image */
    video_drawpng(ctx, imgProp, 0, 0);

    /* Free image context */
    PNGU_ReleaseImageContext(ctx);
    
	return 0;
}
int controls()
{	
	int newcios, newdevice;
	u32 buttons; 
	u32 buttonsGC;
	waitforbuttonpress(&buttons,&buttonsGC);
		/* UP button */
	if (buttons == 2048 || buttonsGC == PAD_BUTTON_UP || buttons == WPAD_CLASSIC_BUTTON_UP)
		filelist_scroll(-1);
	/* DOWN button */
	else if (buttons == 1024 || buttonsGC == PAD_BUTTON_DOWN || buttons == WPAD_CLASSIC_BUTTON_DOWN)
		filelist_scroll(1);
	// B button
	else if (buttons == 4 || buttonsGC == PAD_BUTTON_B || buttons == WPAD_CLASSIC_BUTTON_B)
		sysHBC();
	/* A button */
	else if (buttons == 8 || buttonsGC == PAD_BUTTON_A || buttons == WPAD_CLASSIC_BUTTON_A)
	{
		Versionsys = GetSysMenuVersion();
		checknandapp();
		manage(selected,foundSD,foundUSB);
		printf("[+] Done!\n\n	Press any button to return to the SysMenu\n");
		wpad_waitbuttons();
		restart();
	}
	/* HOME button */
	else if(buttons == 128 || buttonsGC == PAD_BUTTON_START || buttons == WPAD_BUTTON_HOME)
		restart();
// 1 button
	else if(buttons == 2 || buttonsGC == PAD_BUTTON_Y || buttons == WPAD_CLASSIC_BUTTON_FULL_L)
	{
		con_clear();
		newcios = ios_selectionmenu(defaultios);
		if (newcios != 0)
		{
			printf("\nReloading to IOS:%d  ...  ", newcios);
			Wpad_Disconnect();
			sleep(2);
			ret = IOS_ReloadIOS(newcios);
			if(ret == 0)
			printf(" Complete ! \n");
			/* Initialize Wiimote */
			PAD_Init();
			wpad_init();
			sleep(3);
		}
	}
	else if(buttons == 1 || buttonsGC == PAD_BUTTON_X || buttons == WPAD_CLASSIC_BUTTON_FULL_R) // 2
	{
		con_clear();
		Fat_Unmount(device);
		device = UserSelect_Device();
		filelist_retrieve(device);
	}
	else if(buttons == 4096 || buttonsGC == PAD_TRIGGER_R || buttons == WPAD_CLASSIC_BUTTON_PLUS) // plus
	{
		int result;
		result = downloadApp();
	}
	else if(buttons == 16 || buttons == WPAD_CLASSIC_BUTTON_MINUS || buttonsGC == PAD_TRIGGER_L) // minus
	{
		//video_clear(COLOR_BLACK);
		//int x = ThemePreview(selected);
		//gprintf("\n\nx [%i] \n\n", x);
		//waitforbuttonpress(&buttons,&buttonsGC);
		//if(buttons == 4 || buttonsGC == PAD_BUTTON_B || buttons == WPAD_CLASSIC_BUTTON_B)
		//{
		//	waitforbuttonpress(&buttons,&buttonsGC);
		//	video_clear(COLOR_BLACK);
		//	con_clear();
		//	show_banner();
		//}
	}
	return 0;
}


int UserSelect_Device()
{
	for(;;){
		ret = -1;
		con_clear();
		show_banner();
		printf("\nPlease choose device to load theme list from ..... \n\n");
		printf("\nMount :  %s  \n\n",dev_getname(j));
		waitforbuttonpress(&buttons, &GCbuttons);
		if(buttons == 256){
			
			if(j == 1)
				j = 2;
			else
				j = 1;
		}
		else if(buttons == 512){
			
			if(j == 2)
				j = 1;
			else
				j = 2;
		}
		else if(buttons == 8){
		gprintf("\n [%s] Device Selected [%i] ! \n",dev_getname(j), j);
			ret = Fat_Mount(j);
			if(ret == 0)
				break;
			if(ret < 0)
			{
				printf("\nNo %s Device Detected ! \n",dev_getname(j));
				sleep(2);
			}
				
		}
		else if(buttons == 128)
			sysHBC();
	}	
	return j;
}
const char *dev_getname(int type)
{
    switch(type)
    {
    case 1:
        return "SD";
        break;
    case 2:
        return "USB";
        break;
    default:
        return "UNK";
        break;
    }
}

void print_welcome()
{
    printf("[+] Welcome to MyMenuifyMod!\n\nName your theme files anything,\n\nworks on all System menu versions!\n\n");
}
void print_menu_start(int i)
{
	printf("\n\n");
	printf("Current IOS: %d r%d\n",IOS_GetVersion(),IOS_GetRevision());
	printf("Device: %s\n",dev_getname(i));
	u32 SysVersion=GetSysMenuVersion();
	printf("System Menu: %s %s\n\n",getsysvernum(SysVersion),getregion(SysVersion));
	printf("[+] Available Custom Menus : \n\n");
}
void print_menu_end(int i)
{
    printf("\n\n");
    printf("[Home]:Button:<Return to Sys_Menu>          [B]:Button:<Return to HBC>\n\n");
    printf("[+]Plus:Button:<Install original .app> \n\n");
    printf("[1]:One:Button:<Reload IOS>          [2]:Two:Button:<Load Storage Device> \n\n");
}

void set_highlight(bool highlight)
{
    if (highlight)
    {
        printf("\x1b[%u;%um", 47, false);
        printf("\x1b[%u;%um", 30, false);
    }
    else
    {
        printf("\x1b[%u;%um", 37, false);
        printf("\x1b[%u;%um", 40, false);
    }
}
s32 __u8Cmp(const void *a, const void *b)
{
    return *(u8 *)a-*(u8 *)b;
}
u8 *get_ioslist(u32 *cnt)
{
    u64 *buf = 0;
    s32 i, res;
    u32 tcnt = 0, icnt;
    u8 *ioses = NULL;

    //Get stored IOS versions.
    res = ES_GetNumTitles(&tcnt);
    if(res < 0)
    {
        printf("\nES_GetNumTitles: Error! (result = %d)\n", res);
        return 0;
    }
    buf = memalign(32, sizeof(u64) * tcnt);
    res = ES_GetTitles(buf, tcnt);
    if(res < 0)
    {
        printf("\nES_GetTitles: Error! (result = %d)\n", res);
        if (buf) free(buf);
        return 0;
    }

    icnt = 0;
    for(i = 0; i < tcnt; i++)
    {
        if(*((u32 *)(&(buf[i]))) == 1 && (u32)buf[i] > 2 && (u32)buf[i] < 0x100)
        {
            icnt++;
            ioses = (u8 *)realloc(ioses, sizeof(u8) * icnt);
            ioses[icnt - 1] = (u8)buf[i];
        }
    }

    ioses = (u8 *)malloc(sizeof(u8) * icnt);
    icnt = 0;

    for(i = 0; i < tcnt; i++)
    {
        if(*((u32 *)(&(buf[i]))) == 1 && (u32)buf[i] > 2 && (u32)buf[i] < 0x100)
        {
            icnt++;
            ioses[icnt - 1] = (u8)buf[i];
        }
    }
    free(buf);
    qsort(ioses, icnt, 1, __u8Cmp);

    *cnt = icnt;
    return ioses;
}

int ios_selectionmenu(int default_ios)
{
    u32 buttons;
    int selection = 0;
    u32 ioscount;
    u8 *list = get_ioslist(&ioscount);

    int i;
    for (i=0; i<ioscount; i++)
    {
        // Default to default_ios if found, else the loaded IOS
        if (list[i] == default_ios)
        {
            selection = i;
            break;
        }
        if (list[i] == IOS_GetVersion())
        {
            selection = i;
        }
    }
    while (true)
    {
        printf("\x1B[%d;%dH",3,0);	// move console cursor to y/x
        printf("Select the IOS you want to load:       \b\b\b\b\b\b");
        set_highlight(true);
        printf("IOS%u\n", list[selection]);
        set_highlight(false);
        printf("\nIt is recommended to choose an IOS with NAND permissions\n");
        printf("patched, like IOS249(d2x56). If you don't have it, install it with\n");
        printf("ModMii.\n");
        buttons = wpad_waitbuttons();
        if (buttons == 256 )
        {
            if (selection > 0)
            {
                selection--;
            }
            else
            {
                selection = ioscount - 1;
            }
        }
        if (buttons == 512 )
        {
            if (selection < ioscount - 1	)
            {
                selection++;
            }
            else
            {
                selection = 0;
            }
        }
        if (buttons == 8 )
		break;
    }
    return list[selection];
}
int main(int argc, char **argv)
{
	int cios, ret = -1; 
	__exception_setreload(5);
	/* Initialize subsystems */
	sys_init();
	/* Set video mode */
	video_setmode();
	/* Initialize console */
	con_init(CONSOLE_XCOORD, CONSOLE_YCOORD);
	/* Initialize Wiimote */
	wpad_init();
	PAD_Init();
	/* Clear screen */
	con_clear();
	printf("\n\n");
	/* Draw banner */
	show_banner();
	print_welcome();
	sleep(5);
	con_clear();
	cios = ios_selectionmenu(defaultios);
	if (cios != 0)
	{
		printf("\nReloading to IOS:%d  ...  ", cios);
		Wpad_Disconnect();
		sleep(1);
		ret = IOS_ReloadIOS(cios);
		if(ret == 0)
		printf("  Complete ! \n");
		/* Initialize Wiimote */
		PAD_Init();
		wpad_init();
		sleep(2);
	}
	printf("\n\n");
	device = UserSelect_Device();
	filelist_retrieve(device);
	con_clear();
	show_banner();
	for(;;)
	{	
		con_clear();
		printf("\n\n");
		print_menu_start(device);
		/* Print filelist */
		filelist_print(device);
		print_menu_end(device);
		/* Controls */
		controls();
	}
	return 0;
}
