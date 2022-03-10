/*
 * Works on both Windows and UNIX, however the UI tool only works on Windows at the moment
 * To run this on a unix system compile this file and copy and paste your desired libsteam_api.so from /redistributable_bin/ into the same folder as the compiled executable
 * gcc steam_idler.c -o idler -Wall "libsteam_api.so"
*/
//idler.exe <game_id> <duration (milliseconds)>

#ifdef __unix__
    #include <unistd.h>
    #include <stdlib.h>
    #include <stdio.h>
    #include <stdint.h>
    extern uint32_t SteamAPI_Init();
#elif defined(_WIN32) || defined(WIN32)
    #include <stdio.h>
    #include <stdlib.h>
    #include <Windows.h>
    #include <WinBase.h>
    #include <tchar.h>
    __declspec(dllimport) BOOL __cdecl SteamAPI_Init();
#endif

#if defined(_WIN32) || defined(WIN32)
//-1 = failure, 0 = success
int set_environmental_variable(int app_id){
    const TCHAR *lpName = "SteamAppId";
    TCHAR lpValue[32];
    _stprintf(lpValue, _T("%d"), app_id);
    if(SetEnvironmentVariable(lpName, lpValue) == 0){
        printf("Failed to set environmental variables...\n%lu\n", GetLastError());
        return -1;
    }
    return 0;
}
#endif

int main(int argc, char *argv[]){
    //Get steam app_id from first command-line argument
    int app_id;
    int duration; //milliseconds
    if(argc > 1){
        printf("argc: %d\n", argc);
        app_id = atoi(argv[1]);
        duration = atoi(argv[2])*1000;
    }else{
        printf("Specify app_id\n");
        return 1;
    }

    //Start process, different methods depending on OS
    #if defined(_WIN32) || defined(WIN32)
        printf("Windows detected\n");

        //Fake steam process/game
        set_environmental_variable(app_id);

        //Start Steam API (so steam picks it up)
        if(!SteamAPI_Init()){
            printf("Failed to start Steam API!\n");
            return -1;
        }
        
        printf("Idling... (%d)\n", app_id);
        if(duration > 0){
            Sleep(duration);
        }else{
            getchar();
        }
    #elif defined(__unix__)
        //Unix
        printf("Unix detected\n");
        setenv("SteamAppId", argv[1], 1);
        if(!SteamAPI_Init()){
            printf("Failed to start Steam API!\n");
            return -1;
        }
        printf("Idling (%d)...\n", app_id);
        sleep(duration);
    #endif
    return 0;
}