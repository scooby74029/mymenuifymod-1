/****************************************************************************
 * Samba Operations
 *
 ****************************************************************************/
#include <ctype.h>
#include <network.h>
#include <ogcsys.h>
#include <ogc/machine/processor.h>
#include <smb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "network.h"
#include "smb.h"
#include "gecko.h"
#include "fat2.h"


//#define SMB_USER     "admin"
//#define SMB_PWD      "admin"
//#define SMB_SHARE    "\\Home-PC_Network\\music\\wads"
//#define SMB_IP       "192.168.1.100"
char SMB_USER[20];
char SMB_PWD[40];
char SMB_SHARE[40]; 
char SMB_IP[20]; 
static int connected = 0;
static lwp_t spinnerThread = LWP_THREAD_NULL;
static bool spinnerRunning = false;

static void * spinner(void *args) {
    char *spinnerChars = (char*) "/-\\|";
    int spin = 0;
    int flash = 0;
    int cnt = 0;
    int ratio = 5;
    //int flashIntensity[24] = { 15, 31, 47, 63, 79, 95, 111, 127, 159, 191, 223,
    //    255, 223, 191, 159, 127, 111, 95, 79, 63, 47, 31, 15, 0 };
    while (1) {
        if (!spinnerRunning) break;
        printf("\b%c", spinnerChars[spin++]);
        cnt++;
        if (!spinnerChars[spin]) spin = 0;
        fflush(stdout);
        if (!spinnerRunning) break;
        if (cnt >= ratio) {
            cnt = 0;
           // WIILIGHT_SetLevel(flashIntensity[flash++]);
          //  WIILIGHT_TurnOn();
        }
        if (flash >= 24) {
            flash = 0;
        }
        usleep(50000);
    }
    return NULL;
}
void SpinnerStart() {
    if (spinnerThread != LWP_THREAD_NULL) return;
    spinnerRunning = true;
    LWP_CreateThread(&spinnerThread, spinner, NULL, NULL, 0, 80);
}

void SpinnerStop() {
    if (spinnerRunning) {
        spinnerRunning = false;
        LWP_JoinThread(spinnerThread, NULL);
        spinnerThread = LWP_THREAD_NULL;
       // WIILIGHT_SetLevel(0);
       // WIILIGHT_TurnOn();
       // WIILIGHT_Toggle();
    }
}
 
/****************************************************************************
 * Connect to SMB
 ***************************************************************************/
bool ConnectSMB() {
	sprintf(SMB_USER,"%s","admin");
	sprintf(SMB_PWD,"%s","admin");
	sprintf(SMB_SHARE,"%s","\\\\Home-PC_Network\\music\\wads");
	sprintf(SMB_IP,"%s","192.168.1.100");
	
    if (connected == 0) {
        SpinnerStart();
        if (Network_Init() < 0) {
            gprintf("init network failed\n");
            return false;
        }
		else
			gprintf("network init ok ... \n");
			
        SpinnerStop();
        gprintf("\n|%s|\n|%s|\n|%s|\n|%s|\n", SMB_USER,
            SMB_PWD, SMB_SHARE, SMB_IP);
        SpinnerStart();
        if (smbInitDevice(fdev->name, SMB_USER,
            SMB_PWD, SMB_SHARE, SMB_IP)) {
            connected = 1;
            gprintf("connected\n");
        }
        SpinnerStop();
    }
    return (connected == 1);
}

void CloseSMB() {
    if (connected) {
        smbClose("smb");
    }
    connected = 0;
}
