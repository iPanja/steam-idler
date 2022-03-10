#include <sysinfoapi.h>
#include <ShlObj.h>
#include <stdio.h>
#include <regex.h>
#include <stdbool.h>

const char *APP_SECTION_REGEX = "\"apps\"\\{(.+?)\\}";

//https://stackoverflow.com/questions/6218325/how-do-you-check-if-a-directory-exists-on-windows-in-c
bool FileExists(LPCTSTR szPath){
  DWORD dwAttrib = GetFileAttributes(szPath);

  return (dwAttrib != INVALID_FILE_ATTRIBUTES && 
         !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}
bool DirectoryExists(LPCTSTR szPath){
  DWORD dwAttrib = GetFileAttributes(szPath);

  return (dwAttrib != INVALID_FILE_ATTRIBUTES && 
         (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

//Remove spaces, newlines, tabs, and carriage returns to compact the input
//Returns compacted result
char *compact(char *input){
    int i, j;
    char *output = input;
    for(i = 0, j = 0; i < strlen(input); i++, j++){
        if(input[i] == ' ' || input[i] == '\n' || input[i] == '\t' || input[i] == '\r')
            j--;
        else
            output[j] = input[i];
    }
    output[j] = 0;
    return output;
}

// [WINDOWS] Locates the libraryfolders.vdf file which contains all installed APP IDs
//Returns success status
bool locate_steam_master_library_file(char *buffer){
    SHGetSpecialFolderPath(
        0,
        buffer,
        CSIDL_PROGRAM_FILESX86,
        FALSE
    );
    snprintf(buffer, MAX_PATH, "%s%s", buffer, "\\Steam\\steamapps\\libraryfolders.vdf");
    return FileExists(buffer);
}

// [WINDOWS] Locates the steam cache folder which contains all icon and hero images for installed games\
//Returns success status
bool locate_steam_cache_folder(char *buffer){
    SHGetSpecialFolderPath(
        0,
        buffer,
        CSIDL_PROGRAM_FILESX86,
        FALSE
    );
    snprintf(buffer, MAX_PATH, "%s%s", buffer, "\\Steam\\appcache\\librarycache\\");
    return DirectoryExists(buffer);
}

//Reads the entire file into buffer
bool read_entire_file(FILE *file, char **buffer){
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    *buffer = malloc(length + 1);
    fseek(file, 0, SEEK_SET);
    if(!*buffer){
        fprintf(stderr, "[ERROR] Could not read file (read_entire_file)");
        fclose(file);
        return false;
    }
    fread(*buffer, 1, length, file);
    (*buffer)[length] = '\0';
    return true;
}

//Read from contents (should be compacted) and extract the list of app_ids (unsigned longs)
//Returns success status
bool parse_app_ids(char *contents, unsigned long **app_ids, int *app_ids_count){
    //Use regex to get list of alternating app ids inside the "app"{ sections
    //FORMAT: APP_ID        SOME_RANDOM_NUMBER
    size_t maxMatches = 6; //Some arbitrary number
    size_t maxGroups = 6;  //Some arbitrary number
    regex_t regexCompiled;
    regmatch_t groupArray[maxGroups];

    //Compile regular expression
    int err = regcomp(&regexCompiled, APP_SECTION_REGEX, REG_EXTENDED);
    if(err){
        char buffer[100];
        regerror(err, &regexCompiled, buffer, 100);
        fprintf(stderr, "[ERROR] Could not compile regular expression: %s\n", buffer);
        return false;
    }
    unsigned int m = 0;
    char *cursor = contents;
    char final[strlen(cursor)+1];
    memset(&final, 0, sizeof(final));

    //Get captures from matches and store into a single string, final
    for(m = 0; m < maxMatches; m++){
        if(regexec(&regexCompiled, cursor, maxGroups, groupArray, 0))
            break;
        unsigned int g = 0;
        unsigned int offset = 0;
        for(g = 0; g < maxGroups; g++){
            if(groupArray[g].rm_so == (size_t)-1)
                break;
            if(g == 0)
                offset = groupArray[g].rm_eo;
            
            const size_t length = strlen(cursor) + 1;
            char cursorCopy[length];
            strcpy(cursorCopy, cursor);
            cursorCopy[groupArray[g].rm_eo] = 0;
            if(g == 1){
                strncat(final, (cursorCopy + groupArray[g].rm_so), length);
            }
        }
        cursor += offset;
    }
    //Parse the final str for every other number, which is an APP ID
    char *token = strtok(final, "\""); //Gets the next subset split by double quotation
    unsigned long *temp;
    bool grab = true;
    *app_ids = NULL; //PARAMETER
    *app_ids_count = 0;
    //Cycle through the final str until we hit the end
    while(token != NULL){
        //Ignore if token is just a '{'
        if(strcmp(token, "}") == 0){
            token = strtok(NULL, "\"");
            continue;
        }
        if(grab){
            //Increase size of array
            temp = realloc(*app_ids, ++(*app_ids_count) * sizeof(*app_ids));
            if(!temp){
                fprintf(stderr, "[ERROR] realloc failed\n");
                return false;
            }
            *app_ids = temp;
            //Store new APP ID
            (*app_ids)[*app_ids_count-1] = strtoul(token, NULL, 0);
        }
        //Get next token for the next loop iteration
        token = strtok(NULL, "\"");
        grab = !grab;
    }
    regfree(&regexCompiled);

    return true;
}

//One stop function to get the list of installed APP IDs
//Returns success status
bool get_installed_app_ids(unsigned long **app_ids, int *app_ids_count){
    //Locate library file
    char library_file_path[MAX_PATH];
    if(!locate_steam_master_library_file(library_file_path)){
        fprintf(stderr, "[ERROR] Could not locate steam master library file (get_installed_app_ids)\n");
        return false;
    }

    //Read contents of file into memory and compact it (remove spaces, newlines, etc for regex)
    FILE *library_file = fopen(library_file_path, "r");
    if(library_file == NULL){
        fprintf(stderr, "[ERROR] Could not open steam master library file (get_installed_app_ids)\n");
        return false;
    }
    char *buffer;
    if(!read_entire_file(library_file, &buffer)){
        fprintf(stderr, "[ERROR] read_entire_file failed (get_installed_app_ids)\n");
        return false;
    }
    buffer = compact(buffer);

    //Locate the sections containing the APP IDs using regex
    if(!parse_app_ids(buffer, app_ids, app_ids_count)){
        fprintf(stderr, "[ERROR] Could not parse APP IDs from compacted file (get_installed_app_ids)\n");
        return false;
    }

    //Cleanup
    fclose(library_file);
    free(buffer);

    return true;
}

// [WINDOWS] Locates the APP ID's hero image (wide display image) for the specified APP ID
//Returns success status
bool locate_app_id_hero_path(unsigned long app_id, char **library_hero_path){
    //Use static so this method only attempts to locate the library cache once per runtime
    static char app_cache_folder_path[MAX_PATH] = {'\0'};
    if(app_cache_folder_path[0] == '\0'){
        if(!locate_steam_cache_folder(app_cache_folder_path)){
            fprintf(stderr, "[ERROR] Could not locate steam cache folder (locate_app_id_hero_path)\n");
            return false;
        }
    }

    //Compile path to where we expect the library hero image to be located
    *library_hero_path = malloc(MAX_PATH);
    snprintf(*library_hero_path, MAX_PATH, "%s%lu_library_hero.jpg", app_cache_folder_path, app_id);

    //Verify if that file exists
    if(!FileExists(*library_hero_path)){
        *library_hero_path = '\0';
        fprintf(stderr, "[ERROR] APP ID's hero image does not exist (locate_app_id_hero_path)\n");
        return false;
    }

    return true;
}

// [WINDOWS] Locates the APP ID's icon (16x16 pixels) for the specified APP ID
//Returns success status
bool locate_app_id_icon_path(unsigned long app_id, char **library_hero_path){
    //Use static so this method only attempts to locate the library cache once per runtime
    static char app_cache_folder_path[MAX_PATH] = {'\0'};
    if(app_cache_folder_path[0] == '\0'){
        if(!locate_steam_cache_folder(app_cache_folder_path)){
            fprintf(stderr, "[ERROR] Could not locate steam cache folder (locate_app_id_icon_path)\n");
            return false;
        }
    }

    //Compile path to where we expect the library hero image to be located
    *library_hero_path = malloc(MAX_PATH);
    snprintf(*library_hero_path, MAX_PATH, "%s%lu_icon.jpg", app_cache_folder_path, app_id);

    //Verify if that file exists
    if(!FileExists(*library_hero_path)){
        *library_hero_path = '\0';
        fprintf(stderr, "[ERROR] APP ID's icon image does not exist (locate_app_id_icon_path)\n");
        return false;
    }

    return true;
}