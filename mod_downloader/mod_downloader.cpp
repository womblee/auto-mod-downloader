#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Normaliz.lib")
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Wldap32.lib")
#pragma comment(lib, "libcurl_a.lib")
#pragma comment(lib, "bit7z.lib")
#pragma comment(lib, "ole32.lib")

#include <iostream>
#include <string>
#include <vector>
#include <Windows.h>
#include <thread>
#include <filesystem>

#include <curl/curl.h>
#include "bit7z/bitfileextractor.hpp"
#include "bit7z/bitarchivereader.hpp"
#include "vdf_parser.hpp"
#include "ini.hpp"

#define NEWLINE std::cout << std::endl;
#define DEFAULT_CONFIG "[Config]\ndownload_server=https://www.nlog.us/downloads/\ndownload_file=full_archive.zip"

// Needed
std::string download_server;
std::string download_file;
bool download_complete = false;

// Termination
int terminate_process(int delay)
{
    // Delay
    if (delay > 0)
        std::this_thread::sleep_for(std::chrono::seconds(delay));

    // Terminate
    return TerminateProcess(GetCurrentProcess(), 0);
}

// Custom error function
int throw_error(const char* error, int delay = 3, int clear = 0)
{
    // Clear?
    if (clear >= 1)
        system("cls");

    // Print the error
    std::cout << error;
    std::cout << "\n";

    // Terminate
    terminate_process(delay);

    return -1;

}
bool is_answer_positive(std::string answer)
{
    std::vector <std::string> positive_answers
    {
        "Y",
        "y",
        "+",
        "1",
    };

    // Characters found
    int hits = 0;

    // Loop
    for (const auto& rs : positive_answers)
    {
        if (answer.find(rs) != std::string::npos)
            hits++;
    }

    // Return
    return hits > 0;
}

// Callback
std::size_t write_data(void* ptr, std::size_t size, std::size_t nmemb, FILE* stream)
{
    std::size_t written;
    written = fwrite(ptr, size, nmemb, stream);
    return written;
}

// File
void get_bonzo()
{
    // Folder/file
    std::string site(download_server + download_file);

    // File variable
    FILE* fp;

    // File request
    CURL* curl = curl_easy_init();
    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, site.c_str());
        curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_SSLv3);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);

        // Open
        fp = fopen(download_file.c_str(), "wb");
        if (fp)
        {
            // Write
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

            // Perform
            curl_easy_perform(curl);
            fclose(fp);
        }

        // Complete
        download_complete = true;
    }
}

// Split string
std::vector<std::string> split(std::string s, std::string delimiter)
{
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    std::string token;
    std::vector<std::string> res;

    while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
        token = s.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back(token);
    }

    res.push_back(s.substr(pos_start));
    return res;
}

// Deep clean
void deep_clean(bool log)
{
    // Already present?
    std::vector<std::string> bad_ones
    {
        "out",
        "full_archive.zip",
        "_placeholder.ini",
        "dide_mod.ini"
    };

    for (const auto& loser : bad_ones)
    {
        // The odd one's out
        std::filesystem::path to_remove = std::filesystem::current_path();
        to_remove /= loser;

        if (log)
            std::cout << "* Cleaning " + to_remove.filename().string() + "..." << std::endl;

        if (std::filesystem::exists(to_remove)) std::filesystem::is_directory(to_remove) ? std::filesystem::remove_all(to_remove) : std::filesystem::remove(to_remove);
    }
}

int main()
{
    // Title
    SetConsoleTitleA("Comfortable and automatic mod installer by Nest Rushers.");

    // First, ask
    std::cout << "Welcome to the automatic mod downloader!\n\nIt pulls mods directly from Nest Rushers discord and automatically sets them up for your game.\n\nShould I start my work here? (yes): ";
    std::string input_proceed;

    std::getline(std::cin, input_proceed);

    // Proceed
    if (!is_answer_positive(input_proceed))
        throw_error("Unfortunately, you disagreed.");
  
    // Do a cleaning before downloading anything
    deep_clean(false);

    // We don't have a config?
    if (!std::filesystem::exists("config.ini"))
    {
        // Create
        std::ofstream file("config.ini");
        file << DEFAULT_CONFIG << std::endl;

        // Close
        file.close();
    }

    // Read download information
    {
        mINI::INIFile file("config.ini");
        mINI::INIStructure ini;

        // Read
        file.read(ini);

        // Server information
        download_server = ini["Config"]["download_server"];
        download_file = ini["Config"]["download_file"];

        // Validation
        if (download_server.empty()) throw_error("Missing download_server in config.ini, please make a proper configuration.");
        if (download_file.empty()) throw_error("Missing download_file in config.ini, please make a proper configuration.");
    }
    
    // Download
    get_bonzo();

    // Wow, completed
    if (download_complete)
    {
        // Attempt
        try {
            bit7z::Bit7zLibrary lib{ "7z.dll" };
            bit7z::BitArchiveReader reader{ lib, download_file, bit7z::BitFormat::Zip };

            // Extracting the archive
            reader.extract("out/");
        }
        catch (const bit7z::BitException& ex) { throw_error(ex.what()); }

        // Collection
        struct entry
        {
            int num;
            std::filesystem::path path;
        };

        std::vector<entry> paks{};

        // Iteration for visuals
        int i = 0;

        for (const auto& dir : std::filesystem::directory_iterator("out"))
        {
            if (std::filesystem::is_directory(dir))
            {
                for (const auto& file : std::filesystem::directory_iterator(dir.path()))
                {
                    // Ignore certain folders
                    if (file.path().filename().string().find(".ignore_this") != std::string::npos)
                        break;

                    if (file.is_regular_file() && file.path().extension() == ".pak")
                    {
                        entry data;
                        data.num = i;
                        data.path = file.path(); // For later

                        paks.push_back(data);

                        i++;
                    }
                }
            }
        }

        // Safety check
        if (i == 0)
            throw_error("Couldn't parse the downloadable files, please consider trying again later.");

        // Ask the user
        std::cout << "\nWhich mods would you like to install?" << std::endl;

        for (const auto& pak : paks)
            std::cout << std::string(std::to_string(pak.num) + ": " + pak.path.filename().string()) << std::endl;

        // Input which one he wants
        std::cout << "Please enter wanted mods (separate by commas for multiple numbers): ";
        std::string input_numbers;

        std::getline(std::cin, input_numbers);

        // Remove spaces
        std::remove_if(input_numbers.begin(), input_numbers.end(), isspace);

        // Validate to evade trolls
        if (input_numbers.length() > 2 && !strstr(input_numbers.c_str(), ","))
            throw_error("If you plan to enter something big without commas, that's probably not the right answer.");

        // Split
        std::vector<std::string> splitted = split(input_numbers, ",");

        // Purge existing ones
        std::sort(splitted.begin(), splitted.end());
        splitted.erase(unique(splitted.begin(), splitted.end()), splitted.end());

        // Purge unexisting ones
        for (const auto& verify : splitted)
        {
            int hits = 0;

            for (const auto& pak : paks)
            {
                if (pak.num == std::atoi(verify.c_str()))
                    hits++;
            }

            if (hits == 0 || hits > 1)
            {
                // Purge this iteration
                auto itr = std::find(splitted.begin(), splitted.end(), verify);

                if (itr != splitted.end()) splitted.erase(itr);
            }
        }

        // Newline before messages
        NEWLINE

        // Ask about the platform
        std::cout << "Steam or Other platform? (steam / other): ";
        std::string input_platform;

        std::getline(std::cin, input_platform);

        // Choice
        bool steam = false;

        if (input_platform.find("steam") != std::string::npos) steam = true; 

        // Variable, will be used later
        std::string game_path;
        std::string input_path;

        // Steam installation folder detection
        if (steam)
        {
            // Steam path variable that we will use
            std::string steam_path;

            // Find platform path
            HKEY h_key = 0;

            std::cout << "Attempting to find the game's installation path..." << std::endl;

            // Data
            if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", 0, KEY_READ, &h_key) == ERROR_SUCCESS)
            {
                WCHAR szBuffer[512];
                DWORD dwBufferSize = sizeof(szBuffer);

                if (ERROR_SUCCESS == RegQueryValueEx(h_key, L"InstallPath", 0, NULL, (LPBYTE)szBuffer, &dwBufferSize))
                {
                    std::wstring little(szBuffer);
                    std::transform(little.begin(), little.end(), std::back_inserter(steam_path), [](wchar_t c)
                        {
                            return (char)c;
                        });

                    steam_path = std::string(little.begin(), little.end());
                }

                RegCloseKey(h_key);
            }

            if (!std::filesystem::exists(steam_path))
                throw_error("Couldn't find your steam's installation path.");

            // Parse .vdf file
            std::ifstream main(std::string(steam_path + "\\steamapps\\libraryfolders.vdf"));

            if (main.fail())
                throw_error("Couldn't check your library's metadata, make sure 'libraryfolders.vdf' exists in your steamapps folder.");

            auto root = tyti::vdf::read(main);

            for (const auto& child : root.childs)
            {
                for (const auto& mini_child : root.childs[child.first].get()->childs)
                {
                    for (const auto& test : mini_child.second.get()->attribs)
                    {
                        // Dying Light's ID
                        if (test.first == "239140")
                        {
                            if (root.childs[child.first].get()->attribs.find("path") != root.childs[child.first].get()->attribs.end())
                                game_path = root.childs[child.first].get()->attribs["path"];
                        }
                    }
                }
            }
        }
        else
        {
            NEWLINE

            // Ask about the actual path
            std::cout << "Dying Light's installation folder (ex: C:\\Program Files\\Epic Games\\DyingLight): ";
            std::getline(std::cin, input_path);

            // Validate
            if (!std::filesystem::exists(input_path))
                throw_error("That path unfortunaly doesn't exist, did you specify the path correctly?");

            // Directory or not?
            if (!std::filesystem::is_directory(input_path))
                throw_error("The path you specified is not a directory, please double check if you've entered everything correctly.");
        }

        // Actual game path
        std::filesystem::path actual_path = steam ? game_path + "\\steamapps\\common\\Dying Light" : input_path;

        if (!std::filesystem::exists(actual_path))
            throw_error("Unfortunately, Dying Light directory is not available.");

        // DW path
        std::filesystem::path dw_path = actual_path;
        dw_path /= "DW";

        if (!std::filesystem::exists(dw_path))
            throw_error("Unfortunately, Dying Light\\DW directory is not available.");

        std::cout << "Copying desired datapaks into DW..." << std::endl;

        // Copy the datapak into DW
        for (const auto& choice : splitted)
        {
            // Already existing?
            std::filesystem::path existing = dw_path;
            existing /= paks.at(std::atoi(choice.c_str())).path.filename();
            
            if (std::filesystem::exists(existing)) std::filesystem::remove(existing);

            // Copy from temporary
            std::filesystem::path temporary = paks.at(std::atoi(choice.c_str())).path;

            std::filesystem::copy(temporary, dw_path, std::filesystem::copy_options::overwrite_existing);

            // Output
            std::cout << std::string("* Copied " + temporary.string() + " into DW folder") << std::endl;
        }

        std::cout << "Doing dide_mod setup..." << std::endl;

        // Search for dide_mod and dsound
        for (const auto& dir : std::filesystem::directory_iterator("out"))
        {
            if (std::filesystem::is_directory(dir))
            {
                for (const auto& file : std::filesystem::directory_iterator(dir.path()))
                {
                    std::string name = file.path().filename().string();

                    if (name == "dide_mod.ini" || name == "dsound.dll")
                    {
                        // Not the best code...
                        if (name == "dide_mod.ini")
                        {
                            std::filesystem::path copy_path = std::filesystem::current_path();
                            copy_path /= "_placeholder.ini";

                            std::filesystem::copy(file, copy_path, std::filesystem::copy_options::overwrite_existing);
                        }
                        else
                            std::filesystem::copy(file, actual_path, std::filesystem::copy_options::overwrite_existing);
                    }
                }
            }
        }

        // INI
        mINI::INIFile file("_placeholder.ini");
        mINI::INIStructure ini;

        // Read
        file.read(ini);

        // Write
        std::cout << "Writing datapaks into dide_mod..." << std::endl;

        for (const auto& choice : splitted)
            ini["CustomPak"][std::string(dw_path.string() + "\\" + paks.at(std::atoi(choice.c_str())).path.filename().string())] = "1";

        file.generate(ini);

        // Replace and delete placeholder
        std::filesystem::path dide_path = actual_path;
        dide_path /= "dide_mod.ini";

        std::filesystem::copy("_placeholder.ini", dide_path, std::filesystem::copy_options::overwrite_existing);

        // Another deep clean so we don't have our space being eaten
        std::cout << "Cleaning out the mess..." << std::endl;

        deep_clean(true);

        // Congratulations
        throw_error("= Successfull installation! You can now enter the game.", 6);
    }
    else
    {
        // Fail :(
        throw_error("Failed to download the mod collection.");
    }
}