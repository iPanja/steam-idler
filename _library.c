#include <sysinfoapi.h>
#include <ShlObj.h>
#include <stdio.h>
#include <regex.h>
#include <stdbool.h>

const char *APP_SECTION_REGEX = "\"apps\"\\{(.+?)\\}";
const char *APP_HERO = "https://steamcdn-a.akamaihd.net/steam/apps/%lu/library_hero.jpg";


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

//https://stackoverflow.com/questions/13084236/function-to-remove-spaces-from-string-char-array-in-c
char *cleanup(char *input){
    int i, j;
    char *output = input;
    for(i = 0, j = 0; i < strlen(input); i++, j++){
        if(!(input[i] == ' ' || input[i] == '\n' || input[i] == '\t'))
            output[j] = input[i];
        else
            j--;
    }
    output[j] = 0;
    return output;
}

void get_steam_master_library(char *buffer){
    SHGetSpecialFolderPath(
        0,
        buffer,
        CSIDL_PROGRAM_FILESX86,
        FALSE
    );
    snprintf(buffer, MAX_PATH, "%s%s", buffer, "\\Steam\\steamapps\\libraryfolders.vdf");
    if(!FileExists(buffer)){
        printf("Steam master library file does not exist!\n\tLooked for it at: %s\n", buffer);
    }
}
void get_steam_library_cache_folder(char *buffer){
    SHGetSpecialFolderPath(
        0,
        buffer,
        CSIDL_PROGRAM_FILESX86,
        FALSE
    );
    snprintf(buffer, MAX_PATH, "%s%s", buffer, "\\Steam\\appcache\\librarycache\\");
    return DirectoryExists(buffer);
}
void read_entire_file(FILE *file, char **buffer){
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek (file, 0, SEEK_SET);
    *buffer = malloc(length+1); //Add extra space for terminator
    if(!*buffer){
        printf("Couldn't allocate memory to load vdf file!");
        fclose(file);
        return;
    }
    fread(*buffer, 1, length, file);
    (*buffer)[length] = '\0'; //Add terminator
}

void dump(unsigned long *array, int length){
    for(int i = 0; i < length; i++){
        printf("%d: %lu\n", i, array[i]);
    }
}


//https://stackoverflow.com/questions/2577193/how-do-you-capture-a-group-with-regex
//https://gist.github.com/ianmackinnon/3294587
void parse_app_ids(char *contents, unsigned long **app_ids, int *app_ids_count){
    //Use regex to get list of alternating app ids inside the "app"{ sections
    //REGEX EXPESSION: "apps"{(.*?)} 
    //  https://regex101.com/r/bKNGsI/1
    //FORMAT: APP_ID        SOME_RANDOM_NUMBER
    size_t maxMatches = 6;
    size_t maxGroups = 6;

    regex_t regexCompiled;
    regmatch_t groupArray[6];
    int err = regcomp(&regexCompiled, APP_SECTION_REGEX, REG_EXTENDED);
    if(err){
        char buffer[100];
        regerror(err, &regexCompiled, buffer, 100);
        printf("Could not compile regular expression: %s\n", buffer);
        return;
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
            /*
            if(g == 1)
                printf("Match %u, Group %u: [%2u-%2u]: %s (len: %d)\n",
                    m, g, groupArray[g].rm_so, groupArray[g].rm_eo,
                    cursorCopy + groupArray[g].rm_so, );
            */
           if(g == 1){
               strncat(final, (cursorCopy + groupArray[g].rm_so), length);
           }
        }
        cursor += offset;
    }
    //Parse the final str, gets next subset split by dobule quotation (")
    char *token = strtok(final, "\"");
    unsigned long *temp;
    bool grab = true;
    *app_ids = NULL; //PARAMETER
    *app_ids_count = 0;
    //Cycle through the final str until we hit the end
    while(token != NULL){
        //printf("%s\t", token);
        //Ignore if token is just a '{'
        if(strcmp(token, "}") == 0){
            token = strtok(NULL, "\"");
            continue;
        }
        if(grab){
            //Increase size of array
            temp = realloc(*app_ids, ++(*app_ids_count) * sizeof(*app_ids));
            if(!temp){
                printf("realloc failed\n");
                return;
            }
            *app_ids = temp;
            //Set APP ID into new (last) slot
            (*app_ids)[*app_ids_count-1] = strtoul(token, NULL, 0);
        }
        //Get next token for the next loop iteration
        token = strtok(NULL, "\"");
        grab = !grab;
    }
    regfree(&regexCompiled);
}


void get_installed_app_ids(unsigned long **app_ids, int *app_ids_count){
    char pf[MAX_PATH];
    get_steam_master_library(pf);

    //Read contents of libraryfolders.vdf which contains all installed APP IDs
    FILE *lib = fopen(pf, "r");
    if(lib == NULL){
        printf("Steam master library file could not be located!\n");
        return;
    }
    char *buffer;
    read_entire_file(lib, &buffer);
    buffer = cleanup(buffer);
    
    //Use regex/magic to get the APP IDs from the file
    parse_app_ids(buffer, app_ids, app_ids_count);
    
    //CLEANUP
    fclose(lib);
    free(buffer);
}

bool get_game_hero(unsigned long app_id, char **library_hero){
    //Check if file already exists in C:\Program Files (x86)\Steam\appcache\librarycache
    static char app_cache[MAX_PATH] = {'\0'};
    *library_hero = malloc(MAX_PATH);
    //Use static so we only attempt to locate the steam installation folder once per program run
    //It's just unncecessary to do so multiple times
    if(app_cache[0] == '\0'){
        char buf[MAX_PATH];
        get_steam_library_cache_folder(buf);
        printf("APP_CACHE: %s\n\n", buf);
        strcpy(app_cache, buf);
    }
    //Assemble library hero image file location
    snprintf(*library_hero, MAX_PATH, "%s%lu_library_hero.jpg", app_cache, app_id);
    //Filename will be: <app_id>_library_hero.jpg
    if(FileExists(*library_hero)){
        return true;
    }else{
        *library_hero = '\0';
        printf("%lu Library hero NOT found!\n\t>Looked for it at: %s\n", app_id, *library_hero);
        return false;
    }
}
bool get_game_icon(unsigned long app_id, char **library_icon){
    //Check if file already exists in C:\Program Files (x86)\Steam\appcache\librarycache
    static char app_cache[MAX_PATH] = {'\0'};
    *library_icon = malloc(MAX_PATH);
    //Use static so we only attempt to locate the steam installation folder once per program run
    //It's just unncecessary to do so multiple times
    if(app_cache[0] == '\0'){
        char buf[MAX_PATH];
        get_steam_library_cache_folder(buf);
        printf("APP_CACHE: %s\n\n", buf);
        strcpy(app_cache, buf);
    }
    //Assemble library hero image file location
    snprintf(*library_icon, MAX_PATH, "%s%lu_icon.jpg", app_cache, app_id);
    //Filename will be: <app_id>_library_hero.jpg
    if(FileExists(*library_icon)){
        return true;
    }else{
        *library_icon = '\0';
        printf("%lu Library hero NOT found!\n\t>Looked for it at: %s\n", app_id, *library_icon);
        return false;
    }
}

/*
int main(){
    unsigned long *app_ids; //Array of Steam installed APP_IDs
    int app_ids_count; //Length of app_ids;
    get_installed_app_ids(&app_ids, &app_ids_count);

    printf("Installed apps: %d\n", app_ids_count);
    for(int i = 0; i < app_ids_count; i++){
        printf("%lu:\t\t", app_ids[i]);
        char *hero_filepath;
        if(get_game_hero(app_ids[i], &hero_filepath)){
            printf("hero located! (%s)\n", hero_filepath);
        }else{
            printf("hero NOT found!\n");
        }

        free(hero_filepath);
    }

    return 0;
}
*/