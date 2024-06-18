// ROMs Downloader using ImGui Library
// Dhani Novan 25 Mei 2024 Cempaka Putih Jakarta
// TODO:
// 1. (done) Check target path if exists confirm to continue download or cancel download
// Note: now if target path if exists, it won't displayed in result
// 2. (done) Clipping for result
// 3. (done) Scrape
// 4. Build database using lua script
// 5. DB support for nopaystation tsv 
// 6. (done) Read/Write setting from imgui.ini
// 7. (done) Filtered system for searching

// Fix:
// - (done) handle when scrape found none

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include "imgui_internal.h"
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <iostream>
#include <queue>
#include <map>
#include <curl/curl.h>
#include <SDL.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "json.hpp"
#include "system_scrape_id.h"
using json = nlohmann::json;

#if !SDL_VERSION_ATLEAST(2,0,17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif
#define MAX_LINE 2048
#define DB_PATH "db"
#define SCRAPED_PATH "scraped"
#define DEFAULT_VIEW_RESULT_LIMIT 17
#define DEFAULT_ROMS_PATH_1 "/home/deck/Emulation/roms"
#define DEFAULT_ROMS_PATH_2 "/storage/roms"
#define DEFAULT_ROMS_PATH_3 "/userdata/roms"
#define DEFAULT_ROMS_PATH_4 "/roms"
#define DEFAULT_ROMS_PATH_5 "/roms2"
#define DEFAULT_NEW_CSV_SELECTED 1
#define DEFAULT_NEW_TSV_SELECTED 2

struct RDL_Setting {
    unsigned int view_result_limit;
    char roms_path[1024];
};

struct URLSystem {
    std::string url;
    std::string system;
    std::string size;

    // Constructor
    URLSystem(const std::string& u, const std::string& s, const std::string& sz) : url(u), system(s), size(sz) {}
};

class tSearchResult {
public:
    tSearchResult(const std::string& system, const std::string& title, const std::string& url, const std::string& desc, const std::string& size): system(system), title(title), url(url), desc(desc), size(size) {}
    std::string system;
    std::string title;
    std::string url;
    std::string desc;
    std::string size;
};

std::map<std::string, int> db_selected;

struct RDL_Setting AppSetting = { 0, "" };

bool isDirectoryExists(const char *path) {
    struct stat statbuf;

    // Check if path exists
    if (stat(path, &statbuf) != 0) {
        return false; // Path does not exist or error occurred
    }

    // Check if path is a directory
    return S_ISDIR(statbuf.st_mode);
}

static void MyAppHandler_ClearAll(ImGuiContext* ctx, ImGuiSettingsHandler*){
    // printf("MyAppHandler_ClearAll:\n");
}
static void MyAppHandler_ApplyAll(ImGuiContext* ctx, ImGuiSettingsHandler*){
    // printf("MyAppHandler_ApplyAll:\n");

    // Apply default value for 1st initialization
    if (AppSetting.view_result_limit == 0) AppSetting.view_result_limit = DEFAULT_VIEW_RESULT_LIMIT;
    if (!AppSetting.roms_path[0]){
        if (isDirectoryExists(DEFAULT_ROMS_PATH_1))
            strcpy(AppSetting.roms_path, DEFAULT_ROMS_PATH_1);
        else if (isDirectoryExists(DEFAULT_ROMS_PATH_2))
            strcpy(AppSetting.roms_path, DEFAULT_ROMS_PATH_2);
        else if (isDirectoryExists(DEFAULT_ROMS_PATH_3))
            strcpy(AppSetting.roms_path, DEFAULT_ROMS_PATH_3);
        else if (isDirectoryExists(DEFAULT_ROMS_PATH_4))
            strcpy(AppSetting.roms_path, DEFAULT_ROMS_PATH_4);
        else{
            std::filesystem::create_directory("roms");
            strcpy(AppSetting.roms_path, "roms");
        }
    }

    DIR *dir1;
    struct dirent *entry;

    dir1 = opendir(DB_PATH);
    if (dir1){
        entry = readdir(dir1);
        do {
            if (strstr(entry->d_name, ".csv")) {
                if (db_selected.find(entry->d_name) == db_selected.end()) {   // New csv found
                    db_selected[entry->d_name] = DEFAULT_NEW_CSV_SELECTED;
                    printf("Found %s as CSV\n", entry->d_name);
                }
            }else if (strstr(entry->d_name, ".tsv")) {
                if (db_selected.find(entry->d_name) == db_selected.end()) {   // New tsv found
                    db_selected[entry->d_name] = DEFAULT_NEW_TSV_SELECTED;
                    printf("Found %s as TSV\n", entry->d_name);
                }
            }
        } while ((entry = readdir(dir1)) != NULL);
        closedir(dir1);
    }
}
static void* MyAppHandler_ReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name){
    // printf("MyAppHandler_ReadOpen: %s\n", name);

    if (strcmp(name, "RDL") == 0)
        return (void*)&AppSetting;
    else
        return (void*)&db_selected;
}
static void MyAppHandler_ReadLine(ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line){
    // struct RDL_Setting *AppSetting = (struct RDL_Setting *)entry;
    int i;
    char s[256];
    // printf("MyAppHandler_ReadLine: %s\n", line);
    if (sscanf(line, "ViewResultLimit=%d", &i) == 1) {
        AppSetting.view_result_limit = i;
    }else if (sscanf(line, "RomsPath=%256s", s) == 1) {
        strcpy(AppSetting.roms_path, s);
    }else if (sscanf(line, "%255[^=]=%d", s, &i) == 2) {
        db_selected[s]=i;
    }
}
static void MyAppHandler_WriteAll(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf){
    buf->reserve(buf->size() + 500); // ballpark reserve
    buf->appendf("[%s][RDL]\n", handler->TypeName);
    if (AppSetting.view_result_limit){
        buf->appendf("ViewResultLimit=%d\n", AppSetting.view_result_limit);
    }
    if (AppSetting.roms_path[0]){
        buf->appendf("RomsPath=%s\n", AppSetting.roms_path);
    }

    buf->appendf("\n[%s][DB]\n", handler->TypeName);
    for (const auto& pair : db_selected) {
        buf->appendf("%s=%d\n", pair.first.c_str(), pair.second);
    }
    // printf("MyAppHandler_WriteAll: %s\n", buf->c_str());
}

static void MyAppHandler_ReadInit(ImGuiContext* ctx, ImGuiSettingsHandler*){
    // printf("MyAppHandler_ReadInit:\n");
}

void initialize_settings_export()
{
    ImGuiSettingsHandler ini_handler;
    ini_handler.TypeName = "MyApp";
    ini_handler.TypeHash = ImHashStr( "MyApp" );
    ini_handler.ClearAllFn = MyAppHandler_ClearAll;
    ini_handler.ReadInitFn = MyAppHandler_ReadInit;
    ini_handler.ApplyAllFn = MyAppHandler_ApplyAll;
    ini_handler.ReadOpenFn = MyAppHandler_ReadOpen;
    ini_handler.ReadLineFn = MyAppHandler_ReadLine;
    ini_handler.WriteAllFn = MyAppHandler_WriteAll;
    ImGui::AddSettingsHandler( &ini_handler );
}

bool LoadTextureFromFile(const char* filename, SDL_Texture** texture_ptr, int& width, int& height, SDL_Renderer* renderer) {
    int channels;
    unsigned char* data = stbi_load(filename, &width, &height, &channels, 0);

    if (data == nullptr) {
        fprintf(stderr, "Failed to load image: %s\n", stbi_failure_reason());
        return false;
    }

    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom((void*)data, width, height, channels * 8, channels * width,
                                                    0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);

    if (surface == nullptr) {
        fprintf(stderr, "Failed to create SDL surface: %s\n", SDL_GetError());
        return false;
    }

    *texture_ptr = SDL_CreateTextureFromSurface(renderer, surface);

    if ((*texture_ptr) == nullptr) {
        fprintf(stderr, "Failed to create SDL texture: %s\n", SDL_GetError());
    }

    SDL_FreeSurface(surface);
    stbi_image_free(data);

    return true;
}

// Function to check if a file exists
int isFileExists(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file != NULL) {
        fclose(file);
        return 1; // File exists
    }
    return 0; // File does not exist
}

std::string Format(const char* _string, ...) {
	va_list	args;
	va_list copy;

	va_start(args, _string);

	va_copy(copy, args);
	const int length = vsnprintf(nullptr, 0, _string, copy);
	va_end(copy);

	std::string result;
	result.resize(length);
	va_copy(copy, args);
	vsnprintf((char*)result.c_str(), (size_t)length + 1, _string, copy);
	va_end(copy);

	va_end(args);

	return result;
}

// Split and allocate memory
// After using the result, free it with 'free_word'
char **split_word(const char *keyword) {
	char *internal_keyword, *word = NULL, **p, **lword;
	const char *start;
	int nword = 0;

	// Skip space in keyword
	start = keyword;
	while (*start == ' ')
		start++;

	// Count word
	internal_keyword = strdup(start);
	word = strtok(internal_keyword, " ");
	while (word != NULL) {
		nword++;
		word = strtok(NULL, " ");	//	Subsequent calls to strtok() use NULL as the input, which tells the function to continue splitting from where it left off
	}
	free(internal_keyword);

	// Allocate list of word
	internal_keyword = strdup(start);
	lword = (char **)malloc(sizeof(char *) * (nword + 1));
	p = lword;
	word = strtok(internal_keyword, " ");
	while (word != NULL) {
		*p = word;
		word = strtok(NULL, " ");	//	Subsequent calls to strtok() use NULL as the input, which tells the function to continue splitting from where it left off
		p++;
	}
	*p = NULL;
	return lword;
}

// Free memory used by internal_keyword and list of word
void free_word(char **lword) {
	free(*lword);	// free internal_keyword (1st entry)
	free(lword);	// free list of word
}

// Find keyword(char **) in string line
int find_keyword2(char *line, char **lword) {
	char **p = lword;
	char *word, *in_line;
	int found = 1;

	in_line = strdup(line);
	SDL_strlwr(in_line);	// make 'input line' lower
	while (*p) {
		word = *p;
		if (*word == '-') {
			word++;
			if (strstr(in_line, word)) {
				found = found & 0;
				break;
			}else{
				found = found & 1;
			}
		}else if (*word == '@') {
            //skip;
        }else{
			if (strstr(in_line, word)) {
				found = found & 1;
			}else{
				found = found & 0;
				break;
			}
		}
		p++;
	}
	free(in_line);
	return found;
}

void formatFileSize(const char *inputSize, char *outputSize) {
    // Convert input size from string to long long integer
    long long sizeInBytes = atoll(inputSize);

    // Define unit sizes
    const double KB = 1024.0;             // Number of bytes in a kilobyte
    const double MB = KB * 1024;          // Number of bytes in a megabyte
    const double GB = MB * 1024;          // Number of bytes in a gigabyte
    const double TB = GB * 1024;          // Number of bytes in a terabyte

    // Determine appropriate unit and format the result
    if (sizeInBytes < KB) {
        sprintf(outputSize, "%lld B", sizeInBytes);
    } else if (sizeInBytes < MB) {
        sprintf(outputSize, "%.1f KB", sizeInBytes / KB);
    } else if (sizeInBytes < GB) {
        sprintf(outputSize, "%.1f MB", sizeInBytes / MB);
    } else if (sizeInBytes < TB) {
        sprintf(outputSize, "%.1f GB", sizeInBytes / GB);
    } else {
        sprintf(outputSize, "%.1f TB", sizeInBytes / TB);
    }
}

// Function to format a number with commas
void formatWithCommas(const char *inputNumber, char *outputNumber) {
    int inputLength = strlen(inputNumber);      // Length of the input string
    int outputLength = inputLength + (inputLength - 1) / 3; // Calculate the length of the output string including commas
    outputNumber[outputLength] = '\0';          // Null-terminate the output string

    // Indices for input and output strings
    int inputIndex = inputLength - 1;           // Start from the end of input string
    int outputIndex = outputLength - 1;         // Start from the end of output string
    int commaCounter = 0;                       // Counter to track positions for commas

    // Loop backwards through the input string, copying characters and inserting commas
    while (inputIndex >= 0) {
        if (commaCounter == 3) {
            outputNumber[outputIndex--] = ',';  // Insert comma after every 3 digits
            commaCounter = 0;                   // Reset counter after inserting a comma
        }
        outputNumber[outputIndex--] = inputNumber[inputIndex--]; // Copy digit
        commaCounter++;                         // Increment the comma counter
    }
}

// Find keyword(char **) in string line
int find_keyword3(char *line, char **lword) {
	char **p = lword;
	char *word, *in_line;
	int found = 1;

    // printf("Line %d: %s\n", __LINE__, line);
	in_line = strdup(line);
    // printf("Line %d: \n", __LINE__);
	SDL_strlwr(in_line);	// make 'input line' lower
	while (*p) {
		word = *p;
		if (*word == '-') {
			word++;
			if (strstr(in_line, word)) {
				found = found & 0;
				break;
			}else{
				found = found & 1;
			}
		}else{
			if (strstr(in_line, word)) {
				found = found & 1;
			}else{
				found = found & 0;
				break;
			}
		}
		p++;
	}
    // printf("Line %d: \n", __LINE__);
	free(in_line);
	return found;
}

char *my_strtok(char *str, char delimiter) {
    static char *token; // Static variable to keep track of the current token
    if (str != NULL) {
        token = str; // Initialize or reset the token if str is not NULL
    } else if (token == NULL || *token == '\0') {
        return NULL; // No more tokens
    }

    // Find the next occurrence of the delimiter in the current token
    char *delimiterPtr = strchr(token, delimiter);

    if (delimiterPtr != NULL) {
        *delimiterPtr = '\0'; // Replace delimiter with '\0' to terminate the current substring
        char *result = token;
        token = delimiterPtr + 1; // Move to the next character after the delimiter
        return result;
    } else {
        // If no more delimiters are found in the current token
        char *result = token;
        token = NULL; // Signal the end of tokens
        return result;
    }
}

int Search_PSV_GAMES(std::vector<tSearchResult>& result, const char *tsv_fname, char **lword, unsigned int start_no) {
	FILE *f;
	char line[MAX_LINE];
    char target[MAX_LINE];
	char *category = NULL, *base_url = NULL, *p;
	char *a_title_id;
    char *a_region;
    char *a_name;
    char *a_pkg_link;
    char *a_zrif;
    char *a_content_id;
    char *a_last_update;
    char *a_original_name;
    char *a_file_size;
    char *a_sha256;
    char *a_fw;
    char *a_version;
    char a_desc[300];
    char formattedNumber[32];

	f = fopen(tsv_fname, "r");
	if (!f) {
		return start_no;
	}

    if (lword[0][0] == '@'){    // check if filtered request by system
        char *selected_system = lword[0];
        selected_system++; // skip first char @
        if (strcmp(selected_system, "psvita")){    // skip this db if selected_system != "psvita"
            fclose(f);
            return start_no;
        }
    }
	
    // printf("Line %d: \n", __LINE__);
    // Title ID|Region|Name|PKG direct link|zRIF|Content ID|Last Modification Date|Original Name|File Size|SHA256|Required FW|App Version
	while (fgets(line, MAX_LINE, f)) { // Process next line: the real csv data
		// a_name = my_strtok(line, '\t');
		// a_title = my_strtok(NULL, '\t');
        // a_desc = my_strtok(NULL, '\t');
        // a_size = my_strtok(NULL, '\t');
        // printf("Line %d: %s\n", __LINE__, line);
        a_title_id = my_strtok(line, '\t');
        // printf("Line %d: %s\n", __LINE__, a_title_id);
		a_region = my_strtok(NULL, '\t');
        // printf("Line %d: %s\n", __LINE__, a_region);
        a_name = my_strtok(NULL, '\t');
        // printf("Line %d: %s\n", __LINE__, a_name);
        a_pkg_link = my_strtok(NULL, '\t');
        a_zrif = my_strtok(NULL, '\t');
        a_content_id = my_strtok(NULL, '\t');
        a_last_update = my_strtok(NULL, '\t');
        a_original_name = my_strtok(NULL, '\t');
        a_file_size = my_strtok(NULL, '\t');
        a_sha256 = my_strtok(NULL, '\t');
        a_fw = my_strtok(NULL, '\t');
        a_version = my_strtok(NULL, '\t');
        // printf("Line %d: \n", __LINE__);
        
        // add to list if keyword is found in title
        if (find_keyword3(a_name, lword)) {
			start_no++;
			
			// remove trailing end-of-line in a_size
			p = strrchr(a_version, '\r'); // windows end of line
			if (p) {
				*p = '\0';	// replace \r to \0
			}else{
				p = strrchr(a_version, '\n');
				if (p) *p = '\0';	// replace \n to \0
			}
      
            // snprintf(target, MAX_LINE, "%s/%s", base_url, a_name);
            // result.emplace_back(category, a_title, target, a_desc, a_size);
            formatWithCommas(a_file_size, formattedNumber);
            snprintf(a_desc, 299, "Size: %s (", formattedNumber);
            formatFileSize(a_file_size, formattedNumber);
            strcat(a_desc,formattedNumber);
            strcat(a_desc,")");
            // strcat(a_desc,a_region);
            if (strlen(a_pkg_link)>10){
                // printf("%s, %s (%s), %s, %s, %s\n", "psvita", a_name, a_region, "a_pkg_link", a_desc, a_file_size);
                result.emplace_back("psvita", Format("%s (%s)", a_name, a_region), a_pkg_link, a_desc, a_file_size);
            }
		}
	}
    // printf("Line %d: \n", __LINE__);
	fclose(f);
    // printf("Line %d: \n", __LINE__);
	if (category) free(category);
	if (base_url) free(base_url);
	return start_no;
}

int Search_PSP_GAMES(std::vector<tSearchResult>& result, const char *tsv_fname, char **lword, unsigned int start_no) {
	FILE *f;
	char line[MAX_LINE];
    char target[MAX_LINE];
	char *category = NULL, *base_url = NULL, *p;
	char *a_title_id;
    char *a_region;
    char *a_type;
    char *a_name;
    char *a_pkg_link;
    char *a_content_id;
    char *a_last_update;
    char *a_rap;
    char *a_dl_rap;
    char *a_file_size;
    char *a_sha256;
    char a_desc[300];
    char formattedNumber[32];

	f = fopen(tsv_fname, "r");
	if (!f) {
		return start_no;
	}

    if (lword[0][0] == '@'){    // check if filtered request by system
        char *selected_system = lword[0];
        selected_system++; // skip first char @
        if (strcmp(selected_system, "psp")){    // skip this db if selected_system != "psp"
            fclose(f);
            return start_no;
        }
    }
	
    // Title ID|Region|Type|Name|PKG direct link|Content ID|Last Modification Date|RAP|Download .RAP file|File Size|SHA256
	while (fgets(line, MAX_LINE, f)) { // Process next line: the real csv data
        a_title_id = my_strtok(line, '\t');
        // printf("Line %d: %s\n", __LINE__, a_title_id);
		a_region = my_strtok(NULL, '\t');
        a_type = my_strtok(NULL, '\t');
        // printf("Line %d: %s\n", __LINE__, a_region);
        a_name = my_strtok(NULL, '\t');
        // printf("Line %d: %s\n", __LINE__, a_name);
        a_pkg_link = my_strtok(NULL, '\t');
        a_content_id = my_strtok(NULL, '\t');
        a_last_update = my_strtok(NULL, '\t');
        a_rap = my_strtok(NULL, '\t');
        a_dl_rap = my_strtok(NULL, '\t');
        a_file_size = my_strtok(NULL, '\t');
        a_sha256 = my_strtok(NULL, '\t');
        
        // add to list if keyword is found in title
        if (find_keyword3(a_name, lword)) {
			start_no++;
			
			// remove trailing end-of-line in a_size
			p = strrchr(a_sha256, '\r'); // windows end of line
			if (p) {
				*p = '\0';	// replace \r to \0
			}else{
				p = strrchr(a_sha256, '\n');
				if (p) *p = '\0';	// replace \n to \0
			}
      
            // snprintf(target, MAX_LINE, "%s/%s", base_url, a_name);
            // result.emplace_back(category, a_title, target, a_desc, a_size);
            formatWithCommas(a_file_size, formattedNumber);
            snprintf(a_desc, 299, "Size: %s (", formattedNumber);
            formatFileSize(a_file_size, formattedNumber);
            strcat(a_desc,formattedNumber);
            strcat(a_desc,")");
            // strcat(a_desc,a_region);
            if (!strcmp(a_type,"Go")){
                strcat(a_desc," Type: PSP");
                strcat(a_desc,a_type);
            }

            // printf("%s, %s (%s), %s, %s, %s\n", "psp", a_name, a_region, "a_pkg_link", a_desc, a_file_size);
            if (strlen(a_pkg_link)>10)
                result.emplace_back("psp", Format("%s (%s)", a_name, a_region), a_pkg_link, a_desc, a_file_size);
		}
	}
    // printf("Line %d: \n", __LINE__);
	fclose(f);
    // printf("Line %d: \n", __LINE__);
	if (category) free(category);
	if (base_url) free(base_url);
	return start_no;
}

int Search_PSX_GAMES(std::vector<tSearchResult>& result, const char *tsv_fname, char **lword, unsigned int start_no) {
	FILE *f;
	char line[MAX_LINE];
    char target[MAX_LINE];
	char *p;
	char *a_title_id;
    char *a_region;
    char *a_name;
    char *a_pkg_link;
    char *a_content_id;
    char *a_last_update;
    char *a_original_name;
    char *a_file_size;
    char *a_sha256;
    char a_desc[300];
    char formattedNumber[32];

	f = fopen(tsv_fname, "r");
	if (!f) {
		return start_no;
	}

    if (lword[0][0] == '@'){    // check if filtered request by system
        char *selected_system = lword[0];
        selected_system++; // skip first char @
        if (strcmp(selected_system, "psx")){    // skip this db if selected_system != "psx"
            fclose(f);
            return start_no;
        }
    }
	
    // printf("Line %d: \n", __LINE__);
    // Title ID|Region|Name|PKG direct link|Content ID|Last Modification Date|Original Name|File Size|SHA256
	while (fgets(line, MAX_LINE, f)) { // Process next line: the real csv data
        a_title_id = my_strtok(line, '\t');
        // printf("Line %d: %s\n", __LINE__, a_title_id);
		a_region = my_strtok(NULL, '\t');
        // printf("Line %d: %s\n", __LINE__, a_region);
        a_name = my_strtok(NULL, '\t');
        // printf("Line %d: %s\n", __LINE__, a_name);
        a_pkg_link = my_strtok(NULL, '\t');
        a_content_id = my_strtok(NULL, '\t');
        a_last_update = my_strtok(NULL, '\t');
        a_original_name = my_strtok(NULL, '\t');
        a_file_size = my_strtok(NULL, '\t');
        a_sha256 = my_strtok(NULL, '\t');
        // printf("Line %d: \n", __LINE__);
        
        // add to list if keyword is found in title
        if (find_keyword3(a_name, lword)) {
			start_no++;
			
			// remove trailing end-of-line in a_size
			p = strrchr(a_sha256, '\r'); // windows end of line
			if (p) {
				*p = '\0';	// replace \r to \0
			}else{
				p = strrchr(a_sha256, '\n');
				if (p) *p = '\0';	// replace \n to \0
			}
      
            // snprintf(target, MAX_LINE, "%s/%s", base_url, a_name);
            // result.emplace_back(category, a_title, target, a_desc, a_size);
            formatWithCommas(a_file_size, formattedNumber);
            snprintf(a_desc, 299, "Size: %s (", formattedNumber);
            formatFileSize(a_file_size, formattedNumber);
            strcat(a_desc,formattedNumber);
            strcat(a_desc,")");
            // strcat(a_desc,a_region);
            if (strlen(a_pkg_link)>10)
                result.emplace_back("psx", Format("%s (%s)", a_name, a_region), a_pkg_link, a_desc, a_file_size);
                // printf("%s, %s (%s), %s, %s, %s\n", "psx", a_name, a_region, "a_pkg_link", a_desc, a_file_size);
		}
	}
    // printf("Line %d: \n", __LINE__);
	fclose(f);
    // printf("Line %d: \n", __LINE__);
	return start_no;
}

int SearchTSV(std::vector<tSearchResult>& result, const char *tsv_fname, char **lword, unsigned int start_no) {
    if (!strcmp(tsv_fname, "db/PSV_GAMES.tsv")){
        Search_PSV_GAMES(result, tsv_fname, lword, start_no);
    }else if (!strcmp(tsv_fname, "db/PSP_GAMES.tsv")){
        Search_PSP_GAMES(result, tsv_fname, lword, start_no);
    }else if (!strcmp(tsv_fname, "db/PSX_GAMES.tsv")){
        Search_PSX_GAMES(result, tsv_fname, lword, start_no);
    }
}

int SearchCSV(std::vector<tSearchResult>& result, const char *csv_fname, char **lword, unsigned int start_no) {
	FILE *f;
	char line[MAX_LINE];
    char target[MAX_LINE];
	char *category = NULL, *base_url = NULL, *p;
	char *a_name, *a_title, *a_desc, *a_size;

	f = fopen(csv_fname, "r");
	if (!f) {
		return start_no;
	}
	
	fgets(line, MAX_LINE, f);	// Line 1: #category=snes\r\n
	if ( (p = strchr(line, '=') )) {
		category = strdup(p+1);
		p = strrchr(category, '\r'); if (p) *p = '\0';	// replace \r to \0
		p = strrchr(category, '\n'); if (p) *p = '\0';	// replace \n to \0

        if (lword[0][0] == '@'){    // check if filtered request by system
            char *selected_system = lword[0];
            selected_system++; // skip first char @
            if (strcmp(selected_system, category)){    // skip this db if category != selected_system
                fclose(f);
                free(category);
                return start_no;
            }
        }
	}
	
	fgets(line, MAX_LINE, f);		// Line 2: #url=https://archive.org/download/cylums or #url=https://archive.org/download/cylums_collection.zip/ 
	if ((p = strchr(line, '='))) {
		base_url = strdup(p + 1);
		p = strrchr(base_url, '\r'); // windows end of line
		if (p) {
			*p = '\0';	// replace \r to \0
			if (*(p-1) == '/') *(p-1) = '\0';	// remove trailing /
		}else{
			p = strrchr(base_url, '\n');
			*p = '\0';	// replace \n to \0
			if (*(p-1) == '/') *(p-1) = '\0';	// remove trailing /
		}
	}

	if (!base_url) {
		fclose(f);
		if (category) free(category);
		return start_no;
	}
	while (fgets(line, MAX_LINE, f)) { // Process next line: the real csv data
		a_name = my_strtok(line, '|');
		a_title = my_strtok(NULL, '|');
        a_desc = my_strtok(NULL, '|');
        a_size = my_strtok(NULL, '|');
        
        // Use snprintf to safely concatenate str1 and str2 into target
        // snprintf(target, MAX_LINE, "%s/%s/%s", AppSetting.roms_path, category, a_name);
        // Ensure null-termination and truncate if overflow
        // target[MAX_LINE - 1] = '\0';

        // add to list if keyword is found in title and rom file doesn't exists
		// if (find_keyword2(a_title, lword) && !isFileExists(target)) {
        if (find_keyword2(a_title, lword)) {
			start_no++;
			
			// remove trailing end-of-line in a_size
			p = strrchr(a_size, '\r'); // windows end of line
			if (p) {
				*p = '\0';	// replace \r to \0
			}else{
				p = strrchr(a_size, '\n');
				if (p) *p = '\0';	// replace \n to \0
			}
      
            snprintf(target, MAX_LINE, "%s/%s", base_url, a_name);
            result.emplace_back(category, a_title, target, a_desc, a_size);
		}
	}
	fclose(f);
	if (category) free(category);
	if (base_url) free(base_url);
	return start_no;
}

// Global Variable
std::mutex downloadMutex_1;
bool downloadDone_1 = true;
float downloadProgress_1 = 0.0f;
curl_off_t downloadTotalSize = 0;
curl_off_t downloadedSize = 0;
std::string downloadFilename;
CURL* g_curl = NULL;
struct curl_slist *headers = NULL;
int scrapeStatus = 0;   // 0 = done 1 = step 1 dst
std::string scrapeString, scrapeString2;

std::string decodeUrl(const std::string& encodedUrl) {
    std::ostringstream decoded;
    std::size_t length = encodedUrl.length();

    for (std::size_t i = 0; i < length; ++i) {
        if (encodedUrl[i] == '%' && i + 2 < length) {
            std::istringstream hexStream(encodedUrl.substr(i + 1, 2));
            int hexValue;
            if (hexStream >> std::hex >> hexValue) {
                decoded << static_cast<char>(hexValue);
                i += 2;
            } else {
                decoded << '%';
            }
        } else if (encodedUrl[i] == '+') {
            decoded << ' ';
        } else {
            decoded << encodedUrl[i];
        }
    }

    return decoded.str();
}

std::string encodeUrl(const std::string &value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        // Keep alphanumeric and other accepted characters intact
        if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            // Any other characters are percent-encoded
            escaped << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(c));
        }
    }

    return escaped.str();
}

std::string getFileName(const std::string& url) {
    // Find the last slash in the URL
    std::size_t lastSlashPos = url.find_last_of('/');
    
    // Extract the substring after the last slash
    if (lastSlashPos != std::string::npos) {
        return url.substr(lastSlashPos + 1);
    }
    
    // If no slash is found, return the whole URL (unlikely case for a valid URL)
    return url;
}

std::string getFileNameWithoutExtension(const std::string& filePath) {
    // Find the last occurrence of the path separator
    size_t lastSlash = filePath.find_last_of("/\\");
    // Extract the filename from the filePath
    std::string fileName = (lastSlash == std::string::npos) ? filePath : filePath.substr(lastSlash + 1);

    // Find the last occurrence of the dot to find the extension
    size_t lastDot = fileName.find_last_of('.');
    // Extract the filename without extension
    if (lastDot == std::string::npos) {
        return fileName; // No extension found
    } else {
        return fileName.substr(0, lastDot);
    }
}

// Write callback to write received data to a file
size_t WriteFileCallback(void* ptr, size_t size, size_t nmemb, void* stream) {
    std::ofstream* out = static_cast<std::ofstream*>(stream);
    size_t totalSize = size * nmemb;
    out->write(static_cast<const char*>(ptr), totalSize);
    return totalSize;
}

// Callback function to write data to a std::string
size_t WriteStringCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t totalSize = size * nmemb;
    userp->append((char*)contents, totalSize);
    return totalSize;
}

// Progress callback to display progress
int ProgressCallback(void* ptr, curl_off_t totalToDownload, curl_off_t nowDownloaded, curl_off_t, curl_off_t) {
    if (totalToDownload > 0) {
        std::lock_guard<std::mutex> lock(downloadMutex_1);
        downloadProgress_1 = ((float)nowDownloaded / (float)totalToDownload);
        downloadTotalSize = totalToDownload;
        downloadedSize = nowDownloaded;
    }
    return 0;
}

// Function that request http from url and return the content as string
std::string httpRequestAsString(CURL* curl, const std::string& source_url) {
    std::string content = "";
    // CURLcode res;

    printf("\nURL=%s\n", source_url.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, source_url.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Opera/9.80 (J2ME/MIDP; Opera Mini/7.1.32052/29.3417; U; en) Presto/2.8.119 Version/11.10");
    curl_easy_setopt(curl, CURLOPT_HTTPGET, (long)1);

    // Set write callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteStringCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &content);

    // Perform the request
    curl_easy_perform(curl);
    return content;
}

// Callback function to process headers
size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    size_t numbytes = size * nitems;
    curl_off_t *file_size = (curl_off_t *)userdata;

    // Check if the header contains "Content-Length"
    if (strncasecmp(buffer, "Content-Length:", 15) == 0) {
        // Parse the file size from the header
        // printf("strncasecmp:\n%s", buffer);
        *file_size = strtoll(buffer + 15, NULL, 10);
    }
    return numbytes;
}

curl_off_t urlFilesize(CURL *curl, const char *url) {
    CURLcode res;
    curl_off_t file_size = -1;

    // Set the URL
    printf("get URL size:\n%s\n",url);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Opera/9.80 (J2ME/MIDP; Opera Mini/7.1.32052/29.3417; U; en) Presto/2.8.119 Version/11.10");
    curl_easy_setopt(curl, CURLOPT_HTTPGET, (long)1);
    
    // Set the header callback function
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    
    // Pass the file_size variable to the callback function
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &file_size);
    
    // Only fetch the headers
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    
    // Perform the request
    res = curl_easy_perform(curl);
    
    if(res != CURLE_OK) {
        return -1;
    } else {
        return file_size;
    }
}

// function that request http from url and save to target_path
CURLcode httpRequest(CURL* curl, const std::string& source_url,const std::string& target_path, void *ProgressCallback = NULL) {
    CURLcode res;
    std::ofstream outFile;
    std::int64_t fileSize;

    // printf("source_url: %s\n",source_url.c_str());
    // printf("target_path: %s\n", target_path.c_str());

    // Check file size
    std::ifstream inFile(target_path, std::ios::binary | std::ios::ate);
    if (!inFile.is_open()) {
        fileSize = -1;
    }else{
        fileSize = static_cast<std::int64_t>(inFile.tellg());
        inFile.close();
    }

    if (fileSize == -1){
        // Open new file for writing
        outFile.open(target_path, std::ios::binary);
        // printf("Create new target_path\n");
    }else{
        // Open a file for append
        outFile.open(target_path, std::ios::binary | std::ios::app);
        // printf("Open target_path and continue from %ld\n", fileSize);
    }
    if (!outFile) {
        std::cerr << "Error: Unable to open file " << target_path << std::endl;
        return CURLE_READ_ERROR;
    }

    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, source_url.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Opera/9.80 (J2ME/MIDP; Opera Mini/7.1.32052/29.3417; U; en) Presto/2.8.119 Version/11.10");
    curl_easy_setopt(curl, CURLOPT_HTTPGET, (long)1);

    // Set write callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteFileCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outFile);
    if (fileSize == -1){
        curl_easy_setopt(curl, CURLOPT_RANGE, NULL);
        curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, (curl_off_t)0);
    }else{
        curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, (curl_off_t)fileSize);
    }
    
    // Set progress callback
    if (ProgressCallback){
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, NULL);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    }

    // Perform the request
    res = curl_easy_perform(curl);
    outFile.close();

    // Check for errors
    if (res != CURLE_OK) {
        std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
    }
    return res;
}

// Thread function that download from url and save to target_path
void downloadThreat(const std::string& source_url,const std::string& target_path) {
    CURL* curl;
    CURLcode res;
    std::ofstream outFile;
    std::int64_t fileSize;

    // Use httpRequest with callbak

    printf("source_url: %s\n",source_url.c_str());
    printf("target_path: %s\n", target_path.c_str());

    // Check file size
    std::ifstream inFile(target_path, std::ios::binary | std::ios::ate);
    if (!inFile.is_open()) {
        fileSize = -1;
    }else{
        fileSize = static_cast<std::int64_t>(inFile.tellg());
        inFile.close();
    }

    if (fileSize == -1){
        // Open new file for writing
        outFile.open(target_path, std::ios::binary);
        printf("Create new target_path\n");
    }else{
        // Open a file for append
        outFile.open(target_path, std::ios::binary | std::ios::app);
        printf("Open target_path and continue from %ld\n", fileSize);
    }
    if (!outFile) {
        std::cerr << "Error: Unable to open file " << target_path << std::endl;
        {
            std::lock_guard<std::mutex> lock(downloadMutex_1);
            downloadDone_1 = true;
            downloadProgress_1 = 0.0f;
            downloadTotalSize = 0;
            downloadedSize = 0;
        }
        return;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    res = httpRequest(curl, source_url, target_path, (void *)ProgressCallback);
    if (res != CURLE_OK){
        std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
    }
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    outFile.close();
    {
        std::lock_guard<std::mutex> lock(downloadMutex_1);
        downloadDone_1 = true;
        downloadProgress_1 = 0.0f;
        downloadTotalSize = 0;
        downloadedSize = 0;
    }
    return;
}

// Thread function that scrape based on url
// void scrapeThreat(const std::string& source_url,const std::string& system,const std::string& file_size){
void scrapeThreat(const tSearchResult& res_item){
    CURL* curl;
    // CURLcode res;
    // curl_off_t file_size;
    std::string res;
    std::string file_name;
    std::string scrape_url = "https://api.screenscraper.fr/api2/jeuInfos.php?output=json&devid=recalbox&devpassword=C3KbyjX8PKsUgm2tu53y&softname=Emulationstation-Recalbox-9.1&ssid=test&sspassword=test&romtype=rom";

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (!curl) return;
    // if (strstr(source_url.c_str(),".zip/") || strstr(source_url.c_str(),".7z/") ){
    //     file_size = -1;    
    // }else
    //     file_size = urlFilesize(curl, source_url.c_str());
    {std::lock_guard<std::mutex> lock(downloadMutex_1); scrapeStatus = 2;}
    file_name = getFileName(decodeUrl(res_item.url));
    // printf("file_name.length=%ld\n", file_name.length());
    if (file_name.substr(file_name.length() - 4) == ".pkg"){    // for tsv db format, adjust filename from source_url to its title
        file_name = res_item.title + ".pkg";
    }
    // printf("Scraping Filename=%s Filesize=%ld\n", file_name.c_str(), file_size);
    if (!res_item.size.empty()){
        scrape_url.append("&romtaille=");
        // scrape_url.append(std::to_string(file_size));
        scrape_url.append(res_item.size);
        scrape_url.append("&romnom=");
        scrape_url.append(encodeUrl(file_name));
    }else{
        scrape_url = "https://api.screenscraper.fr/api2/jeuRecherche.php?output=json&devid=recalbox&devpassword=C3KbyjX8PKsUgm2tu53y&softname=Emulationstation-Recalbox-9.1&ssid=test&sspassword=test";
        scrape_url.append("&recherche=");
        scrape_url.append(encodeUrl(getFileNameWithoutExtension(decodeUrl(res_item.url))));
    }
    scrape_url.append("&systemeid=");
    scrape_url.append(std::to_string(scrapeId[res_item.system]));
    // std::cout << "Scrape=" << scrape_url << std::endl;
    res = httpRequestAsString(curl, scrape_url);
    {std::lock_guard<std::mutex> lock(downloadMutex_1); scrapeStatus = 3;}
    // printf("reslength=%ld\n", res.length());
    if (res.compare(0, 6, "Erreur") == 0) {
        puts(res.c_str());
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        std::lock_guard<std::mutex> lock(downloadMutex_1);
        scrapeStatus = 0;
        scrapeString = "ERROR:\nSelected rom isn't supported for scraping.\nProcess can't be completed\n";
        return;
    }

    // Save scrape response to json file
    {
        std::ofstream outFile(SCRAPED_PATH "/" + file_name+".json");
        if (outFile) {
            outFile << res;
        }
        outFile.close();
    }
    json jsonData = json::parse(res);

    if (res_item.size.empty()){
        // std::array jeux = jsonData["response"]["jeux"];
        if (jsonData["response"]["jeux"][0].size() == 0) {
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            std::lock_guard<std::mutex> lock(downloadMutex_1);
            scrapeStatus = 0;
            scrapeString = "ERROR:\nScrape found NOTHING.\n";
            return;
        }
    }
    for (const auto& media : !res_item.size.empty()?jsonData["response"]["jeu"]["medias"]:jsonData["response"]["jeux"][0]["medias"]) {
        if (media["type"] == "ss"){
            httpRequest(curl, media["url"], SCRAPED_PATH "/" + decodeUrl(file_name)+".1.ss.png");
            break;
        }
    }
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    {
        std::lock_guard<std::mutex> lock(downloadMutex_1);
        scrapeStatus = 0;
        scrapeString.append("Genres:");
        for (const auto& genre : !res_item.size.empty()?jsonData["response"]["jeu"]["genres"]:jsonData["response"]["jeux"][0]["genres"]) {
            scrapeString.append(" ");
            scrapeString.append(genre["noms"][!res_item.size.empty()?0:1]["text"]);
        }
        if (!(!res_item.size.empty()?jsonData["response"]["jeu"]["dates"][0]["text"]:jsonData["response"]["jeux"][0]["dates"][0]["text"]).empty()){
            scrapeString.append("\nRelease: ");
            scrapeString.append(!res_item.size.empty()?jsonData["response"]["jeu"]["dates"][0]["text"]:jsonData["response"]["jeux"][0]["dates"][0]["text"]);
        }
        if (!(!res_item.size.empty()?jsonData["response"]["jeu"]["systeme"]["text"]:jsonData["response"]["jeux"][0]["systeme"]["text"]).empty()){
            scrapeString.append("\nSystem: ");
            scrapeString.append(!res_item.size.empty()?jsonData["response"]["jeu"]["systeme"]["text"]:jsonData["response"]["jeux"][0]["systeme"]["text"]);
        }
        if (!(!res_item.size.empty()?jsonData["response"]["jeu"]["joueurs"]["text"]:jsonData["response"]["jeux"][0]["joueurs"]["text"]).empty()){
            scrapeString.append("\nPlayer: ");
            scrapeString.append(!res_item.size.empty()?jsonData["response"]["jeu"]["joueurs"]["text"]:jsonData["response"]["jeux"][0]["joueurs"]["text"]);
        }
        if (!(!res_item.size.empty()?jsonData["response"]["jeu"]["developpeur"]["text"]:jsonData["response"]["jeux"][0]["developpeur"]["text"]).empty()){
            scrapeString.append("\nDeveloper: ");
            scrapeString.append(!res_item.size.empty()?jsonData["response"]["jeu"]["developpeur"]["text"]:jsonData["response"]["jeux"][0]["developpeur"]["text"]);
        }
        if (!(!res_item.size.empty()?jsonData["response"]["jeu"]["editeur"]["text"]:jsonData["response"]["jeux"][0]["editeur"]["text"]).empty()){
            scrapeString.append("\nPublisher: ");
            scrapeString.append(!res_item.size.empty()?jsonData["response"]["jeu"]["editeur"]["text"]:jsonData["response"]["jeux"][0]["editeur"]["text"]);
        }
        if (!(!res_item.size.empty()?jsonData["response"]["jeu"]["resolution"]:jsonData["response"]["jeux"][0]["resolution"]).empty()){
            scrapeString.append("\nResolution: ");
            scrapeString.append(!res_item.size.empty()?jsonData["response"]["jeu"]["resolution"]:jsonData["response"]["jeux"][0]["resolution"]);
        }
        scrapeString2.append(!res_item.size.empty()?jsonData["response"]["jeu"]["synopsis"][0]["text"]:jsonData["response"]["jeux"][0]["synopsis"][1]["text"]);
    }
}

// Main code
int main(int, char**)
{
    // system("mkdir -p scraped");
    std::filesystem::create_directory("scraped");
    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    // Create window with SDL_Renderer graphics context
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("ROMs Downloader", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, window_flags);
    if (window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return -1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr)
    {
        SDL_Log("Error creating SDL_Renderer!");
        return 0;
    }
    //SDL_RendererInfo info;
    //SDL_GetRendererInfo(renderer, &info);
    //SDL_Log("Current SDL_Renderer: %s", info.name);

    SDL_Texture* my_texture;
    int my_image_width, my_image_height;

    // Setup Dear ImGui context
    // initialize_settings_export();
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    initialize_settings_export();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    // ImGui::StyleColorsDark();
    ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != nullptr);

    // Our state
    // bool show_demo_window = true;
    // bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    g_curl = curl_easy_init();
    if (g_curl){
        curl_easy_setopt(g_curl, CURLOPT_TCP_KEEPALIVE, 1L);
        // curl_easy_setopt(g_curl, CURLOPT_TCP_KEEPIDLE, 120L);
        // curl_easy_setopt(g_curl, CURLOPT_TCP_KEEPINTVL, 60L);
        headers = curl_slist_append(headers, "Connection: keep-alive");
        curl_easy_setopt(g_curl, CURLOPT_HTTPHEADER, headers);
    }

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // Start the Dear ImGui frame
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static bool show_app = true;
            static char rom_name[1024] = "adventure";
            const char* systems[] = { "all", "naomi", "ps2", "psx", "n64", "snes", "nds", "3ds", "wii", "gba", "megadrive", "fbneo" };
            static int system_current = 0;
            static char query[1030] = "";
            static std::vector<tSearchResult> result;
            static std::vector<URLSystem> downloadQueue;
            char **lword;
            static unsigned int n_found = 0;
            static bool image_loaded = false;

            static bool use_work_area = true;
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(use_work_area ? viewport->WorkPos : viewport->Pos);
            ImGui::SetNextWindowSize(use_work_area ? viewport->WorkSize : viewport->Size);

            ImGui::Begin("Main", &show_app, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove); // Create a window and append into it.
            ImGui::Text("Query for searching rom:");            // Display some text (you can use a format strings too)
            ImGui::InputText("Rom's Name", rom_name, IM_ARRAYSIZE(rom_name));
            ImGui::Combo("System", &system_current, systems, IM_ARRAYSIZE(systems));
            if (ImGui::Button("Search")) {
                char fullpath[MAX_LINE], *p;

                result.clear();
                n_found = 0;
                if (system_current){
                    snprintf(query, 1030, "@%s %s", systems[system_current], rom_name);
                }else{
                    strcpy(query, rom_name);
                }
                p = query;
                while (*p){
                    *p = tolower(*p);
                    p++;
                }

                lword = split_word(query);
                for (const auto& pair : db_selected) {
                    if (pair.second == DEFAULT_NEW_CSV_SELECTED){
                        // printf("Searching in %s\n", pair.first.c_str());
                        sprintf(fullpath, DB_PATH"/%s", pair.first.c_str());
                        n_found = SearchCSV(result, fullpath, lword, n_found);
                    }else if (pair.second == DEFAULT_NEW_TSV_SELECTED){
                        // printf("Searching in %s\n", pair.first.c_str());
                        sprintf(fullpath, DB_PATH"/%s", pair.first.c_str());
                        n_found = SearchTSV(result, fullpath, lword, n_found);
                    }else{
                        // printf("%s skipped\n", pair.first.c_str());
                    }
                }
                free_word(lword);
            }
            if (query[0]) {
                ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable;
                ImVec2 outer_size = ImVec2(0.0f, 0.0f);
                ImGui::Text("Found = %d", n_found);
                if (n_found > AppSetting.view_result_limit) {
                    flags = flags | ImGuiTableFlags_ScrollY;
                    outer_size = ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * AppSetting.view_result_limit);
                }
                if (ImGui::BeginTable("Result_Table", 3, flags, outer_size)) {
                    int row = 0;
                    static std::string dest_path;
        
                    // Set up columns
                    ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
                    ImGui::TableSetupColumn("[System]Title", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Description (Size/Genre/Publisher)", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 40.0f);
                    // Add the headers row
                    ImGui::TableHeadersRow();
                    for (const auto& res_item : result) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::Text("%d.[%s] %s", row+1, res_item.system.c_str(), res_item.title.c_str());
                        ImGui::TableNextColumn();
                        ImGui::Text(res_item.desc.c_str());
                        ImGui::TableNextColumn();
                        ImGui::PushID(row); // Push a unique identifier for each button based on row index
                        if (ImGui::SmallButton("Get")) {
                            // auto filesize = urlFilesize(g_curl, res_item.url.c_str());
                            // printf("URL filesize = %ld\n", filesize);
                            dest_path = std::string(AppSetting.roms_path) + "/" + res_item.system + "/" + getFileName(decodeUrl(res_item.url));
                            if (isFileExists(dest_path.c_str())){
                                printf("%s already exists.\n", dest_path.c_str());
                                ImGui::OpenPopup("Resume_download");
                            }else{
                                if (downloadDone_1){    // No Queue
                                    {
                                    std::lock_guard<std::mutex> lock(downloadMutex_1);
                                    downloadDone_1 = false;
                                    downloadProgress_1 = 0.0f;
                                    downloadTotalSize = 0;
                                    downloadedSize = 0;
                                    downloadFilename = getFileName(decodeUrl(res_item.url));
                                    }
                                    std::thread thread_1(downloadThreat, res_item.url, dest_path);
                                    thread_1.detach();
                                }else{  // Queue download
                                    downloadQueue.emplace(downloadQueue.begin(), res_item.url, dest_path, res_item.size);
                                }
                            }
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton("..")) {
                            scrapeStatus = 1;
                            scrapeString = "";
                            scrapeString2 = "";
                            // scrapeString.append("\nFile: " + std::string(ROMS_PATH) + "/" + res_item.system + "/" + getFileName(decodeUrl(res_item.url)));
                            // std::cout << httpRequestAsString(g_curl, res_item.url.c_str());
                            // std::cout << httpRequestAsString(g_curl, "https://api.screenscraper.fr/api2/ssuserInfos.php?devid=xxx&devpassword=yyy&softname=zzz&output=xml&ssid=leonkasovan&sspassword=rikadanR1");
                            // std::thread thread_2(scrapeThreat, res_item.url, res_item.system, res_item.size);
                            std::thread thread_2(scrapeThreat, res_item);
                            thread_2.detach();
                            ImGui::OpenPopup("ViewScrape");
                        }
                        if (ImGui::BeginPopupModal("Resume_download", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                            ImGui::Text("%s already exists.\nResume download?", dest_path.c_str());
                            ImGui::Separator();

                            if (ImGui::Button("Yes")) {
                                ImGui::CloseCurrentPopup();
                                if (downloadDone_1){    // No Queue
                                    {
                                    std::lock_guard<std::mutex> lock(downloadMutex_1);
                                    downloadDone_1 = false;
                                    downloadProgress_1 = 0.0f;
                                    downloadTotalSize = 0;
                                    downloadedSize = 0;
                                    downloadFilename = getFileName(decodeUrl(res_item.url));
                                    }
                                    std::thread thread_1(downloadThreat, res_item.url, dest_path);
                                    thread_1.detach();
                                }else{  // Queue download
                                    downloadQueue.emplace(downloadQueue.begin(), res_item.url, dest_path, res_item.size);
                                }
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("No")) {
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::EndPopup();
                        }
                        if (ImGui::BeginPopupModal("ViewScrape")) {
                            if ((scrapeStatus == 0) && !image_loaded && (scrapeString.compare(0, 3, "ERR") != 0)){
                                std::string file_name = getFileName(decodeUrl(res_item.url));
                                // printf("file_name.length=%ld\n", file_name.length());
                                if (file_name.substr(file_name.length() - 4) == ".pkg"){    // for tsv db format, adjust filename from source_url to its title
                                    file_name = res_item.title + ".pkg";
                                }
                                if (!LoadTextureFromFile((SCRAPED_PATH "/" + file_name +".1.ss.png").c_str(), &my_texture, my_image_width, my_image_height, renderer)){
                                    printf("Error: LoadTextureFromFile\n");
                                    image_loaded = false;
                                    // return -1;
                                }else{
                                    image_loaded = true;
                                    // printf("This is should be loaded once\n");
                                }
                            }

                            ImGui::Text("%s (%s)", res_item.title.c_str(), res_item.system.c_str());
                            ImGui::Separator();
                            if (image_loaded) {
                                ImGui::Image((void*) my_texture, ImVec2((my_image_width * 200) / my_image_height, 200));
                                ImGui::SameLine();
                            }
                            if (scrapeStatus == 0) {
                                ImGui::TextWrapped(scrapeString.c_str());
                                ImGui::Separator();
                                ImGui::TextWrapped(scrapeString2.c_str());
                            } else if (scrapeStatus == 1) {
                                ImGui::Text("\nScraping rom size...\nPlease wait.");
                            } else if (scrapeStatus == 2) {
                                ImGui::Text("\nScraping rom info...\nPlease wait.");
                            } else if (scrapeStatus == 3) {
                                ImGui::Text("\nDownloading media (screenshot)...\nPlease wait.");
                            }
                            ImGui::Separator();
                            if (ImGui::Button("Close")) {
                                ImGui::CloseCurrentPopup();
                                if (image_loaded) SDL_DestroyTexture(my_texture);
                                image_loaded = false;
                            }
                            ImGui::EndPopup();
                        }
                        ImGui::PopID(); // Pop the unique identifier
                        row++;
                    }
                    ImGui::EndTable();
                }
            }

            if (!downloadQueue.empty()){
                ImGui::Text("Download Queue: %ld", downloadQueue.size());
                if (ImGui::BeginTable("Download_Queue", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
                    // Set up columns
                    ImGui::TableSetupColumn("System", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("ROM", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableHeadersRow();
                    for (const auto& item : downloadQueue) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::Text(item.system.c_str());
                        ImGui::TableNextColumn();
                        ImGui::Text(getFileName(decodeUrl(item.url)).c_str());
                    }
                }
                ImGui::EndTable();

                if (downloadDone_1){    // there is no downloading = downloadDone then request in the download queue
                    auto item = downloadQueue.back();
                    downloadFilename = getFileName(decodeUrl(item.url));
                    {
                    std::lock_guard<std::mutex> lock(downloadMutex_1);
                    downloadDone_1 = false;
                    downloadProgress_1 = 0.0f;
                    downloadTotalSize = 0;
                    downloadedSize = 0;
                    downloadQueue.pop_back();
                    }
                    std::thread thread_1(downloadThreat, item.url, item.system);
                    thread_1.detach();
                }
            }
            if (!downloadDone_1){
                // ImVec2 size = ImVec2(-1, 0);
                {
                std::lock_guard<std::mutex> lock(downloadMutex_1);
                ImGui::Text("Filename: %s", downloadFilename.c_str());
                ImGui::SameLine();
                ImGui::Text("Filesize: %ld byte", downloadTotalSize);
                ImGui::SameLine();
                ImGui::Text("Downloaded: %ld byte", downloadedSize);
                ImGui::ProgressBar(downloadProgress_1); // Render the progress bar
                }
            }
            // ImGui::Image((void*) my_texture, ImVec2(my_image_width, my_image_height));
            // ImGui::Image((void*) my_texture, ImVec2(100, 50));
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColor(renderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255), (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    // Cleanup
    curl_slist_free_all(headers);
    if (g_curl) curl_easy_cleanup(g_curl);
    curl_global_cleanup();
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    // SDL_DestroyTexture(my_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
