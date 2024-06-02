// Generate header system_scrape_id.h based on systemesListe.json grabbed from screenscraper.fr
// Dhani Novan 25 Mei 2024 Cempaka Putih Jakarta
/*
wget "https://api.screenscraper.fr/api2/systemesListe.php?output=json&devid=recalbox&devpassword=C3KbyjX8PKsUgm2tu53y&softname=Emulationstation-Recalbox-9.1&ssid=test&sspassword=test" -O systemesListe.json
g++ gen_system_scrape_id.cpp -o gen_system_scrape_id
./gen_system_scrape_id > system_scrape_id.h
*/

// #include <stdio.h>
#include <dirent.h>
#include <fstream>
#include "json.hpp"
using json = nlohmann::json;

#define ROMS_PATH "/home/deck/Emulation/roms"

int main(int argc, char *argv[])
{
    DIR *dir1;
    struct dirent *entry;

    std::ifstream jsonFile("systemesListe.json");
    if (!jsonFile.is_open())
    {
        printf("Unable to open file systemesListe.json");
        return 1;
    }

    // Read the file into a json object
    json jsonData;
    jsonFile >> jsonData;
    bool found;
    long system_id;

    printf("#include <map>\n#include <string>\n\n");
    printf("std::map<std::string, int> scrapeId{\n");
    dir1 = opendir(ROMS_PATH);
    if (dir1)
    {
        entry = readdir(dir1);
        do
        {
            if (entry->d_type == 4)
            {
                found = false;
                for (const auto &system : jsonData["response"]["systemes"])
                {
                    if (system["noms"].contains("nom_recalbox"))
                    {
                        std::string system_names = system["noms"]["nom_recalbox"];
                        if (strstr(system_names.c_str(), entry->d_name)){
                            found = true;
                            system_id = system["id"];
                            break;
                        }
                    }
                    if (system["noms"].contains("nom_retropie"))
                    {
                        std::string system_names = system["noms"]["nom_retropie"];
                        if (strstr(system_names.c_str(), entry->d_name)){
                            found = true;
                            system_id = system["id"];
                            break;
                        }
                    }
                }
                if (found){
                    printf("{\"%s\",%ld},\n", entry->d_name, system_id);
                }else{
                    printf("//{\"%s\",},\n", entry->d_name);
                }
            }
        } while ((entry = readdir(dir1)) != NULL);
        closedir(dir1);
    }
    printf("};\n");
    return 0;
}