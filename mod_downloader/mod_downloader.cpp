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

#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include "bit7z/bitfileextractor.hpp"
#include "bit7z/bitarchivereader.hpp"
#include "vdf_parser.hpp"
#include "ini.hpp"

#define NEWLINE std::cout << std::endl;
#define CONFIGURE(ini) ini["General"]["EnableMod"] = "1"; ini["Features"]["DeveloperMenu"] = "0"; ini["Features"]["CustomPak"] = "1";
#define TRANSFORM(buffer, str) std::wstring little(buffer); std::transform(little.begin(), little.end(), std::back_inserter(str), [](wchar_t c) { return (char)c; }); str = std::string(little.begin(), little.end());

#define DEFAULT_CONFIG "[Config]\ndownload_file=https://www.nlog.us/downloads/full_archive.zip\ndownload_copyright=Nest Rushers Discord"

// Needed
std::string download_copyright;
std::string download_file;
std::string file_name;
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

// Positive answer
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
    // URL to file
    std::string site(download_file);

    // File variable
    FILE* fp;

    // File request
    CURL* curl = curl_easy_init();
    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, site.c_str());
        curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_SSLv3);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);

        // Extract file name from URL
        file_name = download_file.substr(download_file.find_last_of("/") + 1).c_str();

        // Open
        fp = fopen(file_name.c_str(), "wb");
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

    while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos)
    {
        token = s.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back(token);
    }

    res.push_back(s.substr(pos_start));
    return res;
}

// Function which checks if string ends with a certain string
bool has_ending(const std::string& full_string, const std::string& ending)
{
    if (full_string.length() >= ending.length())
        return (0 == full_string.compare(full_string.length() - ending.length(), ending.length(), ending));
    else
        return false;
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

        if (std::filesystem::exists(to_remove))
        {
            if (log)
                std::cout << "* Cleaning " + to_remove.filename().string() + "..." << std::endl;

            std::filesystem::is_directory(to_remove) ? std::filesystem::remove_all(to_remove) : std::filesystem::remove(to_remove);
        }
    }
}

// Verification before running
void verification()
{
    // Needed things
    std::vector<std::string> needed
    {
        "7z.dll",
        "curlpp.dll"
    };

    for (const auto& file : needed)
    {
        // The odd one's out
        std::filesystem::path needed_file = std::filesystem::current_path();
        needed_file /= file;

        if (!std::filesystem::exists(needed_file))
            throw_error("Make sure you have '7z.dll' and 'curlpp.dll' included with the program, they are required.");
    }
}

int main()
{
    // Title
    SetConsoleTitleA("Automatic mod installer");

    // Choices
    static const char* choices[] =
    {
        "install",
        "remove"
    };

    // The two kings
    std::string inputs = " (" + std::string(choices[0]) + " / " + std::string(choices[1]) + "): ";

    // First, ask
    std::string to_cout = "Welcome to the automatic mod downloader!\n\nIt pulls mods directly from the download server and automatically sets them up for your game.\n\nDo you want me to install or uninstall mods?";
    to_cout += inputs;
    std::cout << to_cout;
    std::string input_proceed;

    std::getline(std::cin, input_proceed);

    // Verify
    int verifications = 0;

    for (int i = 0; i < sizeof(choices) / sizeof(*choices); i++)
    {
        if (input_proceed == choices[i])
            verifications++;
    }

    if (verifications == 0)
        throw_error("You should pick one of the two choices, run the program again...");

    // Decision made, now do the shit
    if (input_proceed == choices[1])
    {
        NEWLINE

            // Won't bother to add automatic detection here, so just ask for it
            std::cout << "Uninstall chosen, enter your Dying Light's installation folder...\n(e.g C:\\Program Files (x86)\\Steam\\steamapps\\common\\Dying Light)\n(e.g C:\\Program Files\\Epic Games\\Dying Light)\n\nEnter (path): " << std::endl;
        std::string input_path;

        std::getline(std::cin, input_path);

        // DW folder
        std::string dide_direct = input_path + "\\dide_mod.ini";

        if (!std::filesystem::exists(dide_direct))
            throw_error("Failed to find dide_mod.ini inside this folder, please input a proper one");

        // Prompt that everything is gonna go into waste
        NEWLINE

            std::cout << "Do you really want to disable every mod? (yes): " << std::endl;
        std::string disable_answer;

        std::getline(std::cin, disable_answer);

        // Here we go
        if (is_answer_positive(disable_answer))
        {
            // Update
            mINI::INIFile file(dide_direct);
            mINI::INIStructure ini;

            // Read
            file.read(ini);

            // Disable
            for (auto const& it : ini)
            {
                // mINI has everything in lowercase, first tried CustomPak, wasted my time...
                if (it.first == "custompak")
                {
                    for (auto const& it2 : it.second)
                        ini[it.first][it2.first] = "0";
                }
            }

            // Write update to file
            file.write(ini);

            // An error suited this the best!
            throw_error("Remember, you can always enable the mods again by changing 'EnableMod' to '1' in dide_mod.ini...", 5, 1);
        }
        else
            throw_error("Okay, maybe you'll uninstall the mods another time soon.");
    }

    // Make sure that the user has everything before doing anything
    verification();

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
        download_file = ini["Config"]["download_file"];

        // Misc
        download_copyright = ini["Config"]["download_copyright"];

        // Validation
        if (download_file.empty() || download_copyright.empty())
            throw_error("Missing 'download_file' or 'download_copyright' in config.ini, please make a proper configuration.");
    }

    // Notify the user
    std::cout << std::string("\nDownloading, please wait... (provided by " + download_copyright + ")") << std::endl;

    // Download
    get_bonzo();

    // Wow, completed
    if (download_complete)
    {
        // Attempt
        try
        {
            bit7z::Bit7zLibrary lib{ "7z.dll" };
            bit7z::BitArchiveReader reader{ lib, file_name, bit7z::BitFormat::Zip };

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

        // Empty, ahh
        if (input_numbers.empty())
            throw_error("Please input something before proceeding, honestly.");

        // Remove spaces
        std::remove_if(input_numbers.begin(), input_numbers.end(), isspace);

        // Validate to evade mistakes
        if (input_numbers.length() > 2 && !strstr(input_numbers.c_str(), ","))
            throw_error("Don't enter your choice without commas, I recommend trying again.");

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
        std::cout << "Steam or Epic Games? (steam / epic): ";
        std::string input_platform;

        std::getline(std::cin, input_platform);

        // Choice
        bool steam = false;

        if (input_platform.find("steam") != std::string::npos) steam = true;

        // Variable, will be used later
        std::string game_path;

        // Steam installation folder detection
        if (steam)
        {
            // Steam path variable that we will use
            std::string steam_path;

            // Find platform path
            HKEY h_key = 0;

            std::cout << "[Steam] Attempting to find the game's installation path..." << std::endl;

            // Data
            if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", 0, KEY_READ, &h_key) == ERROR_SUCCESS)
            {
                WCHAR szBuffer[512]; DWORD dwBufferSize = sizeof(szBuffer);

                if (ERROR_SUCCESS == RegQueryValueEx(h_key, L"InstallPath", 0, NULL, (LPBYTE)szBuffer, &dwBufferSize))
                {
                    // Macros
                    TRANSFORM(szBuffer, steam_path)
                }

                RegCloseKey(h_key);
            }

            if (!std::filesystem::exists(steam_path))
                throw_error("Couldn't find your steam's installation path.");

            // Read .vdf file
            std::ifstream main(std::string(steam_path + "\\steamapps\\libraryfolders.vdf"));

            if (main.fail())
                throw_error("Couldn't check your library's metadata, make sure 'libraryfolders.vdf' exists in your steamapps folder.");

            // Open .vdf file
            auto root = tyti::vdf::read(main);

            // Loop through the file
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
            // Epic Games manifest path
            std::string manifest_path;

            // Status, used to decide if we want to use the #2 method
            int status = 1;

            // Manifest is inside thos registry keys
            HKEY h_key = 0;

            std::cout << "[Epic] Attempting to detect game's installation path..." << std::endl;

            // 1st method
            if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Epic Games\\EOS", 0, KEY_READ, &h_key) == ERROR_SUCCESS)
            {
                WCHAR szBuffer[512]; DWORD dwBufferSize = sizeof(szBuffer);

                if (ERROR_SUCCESS == RegQueryValueEx(h_key, L"ModSdkMetadataDir", 0, NULL, (LPBYTE)szBuffer, &dwBufferSize))
                {
                    // Macros
                    TRANSFORM(szBuffer, manifest_path)
                }
                else
                    status = 0; // Failed

                RegCloseKey(h_key);
            }

            // 2nd method in case the first one failed
            if (status == 0)
            {
                // Null out before doing anything
                h_key = 0;

                if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Epic Games\\EpicGamesLauncher", 0, KEY_READ, &h_key) == ERROR_SUCCESS)
                {
                    WCHAR szBuffer[512]; DWORD dwBufferSize = sizeof(szBuffer);

                    if (ERROR_SUCCESS == RegQueryValueEx(h_key, L"AppDataPath", 0, NULL, (LPBYTE)szBuffer, &dwBufferSize))
                    {
                        // Macros
                        TRANSFORM(szBuffer, manifest_path)
                    }
                    else
                        status = 2; // Failed again

                    RegCloseKey(h_key);
                }
            }

            // Thrilling moment when all of our methods failed
            if (status == 2)
                throw_error("Couldn't detect the location of Epic Games manifest, please contact the developer."); // This is so sad

            // Add 'Manifests' to path in case there are none
            if (has_ending(manifest_path, "\\Data"))
                manifest_path += "\\Manifests";

            // Validate directory
            if (!std::filesystem::exists(manifest_path))
                throw_error("Epic Games manifest directory doesn't exist, please re-install the launcher in an attempt to solve this.");

            // Parse directory for each item
            for (const auto& entry : std::filesystem::directory_iterator(manifest_path))
            {
                // Manifests have the .item extension
                if (entry.path().extension() == ".item")
                {
                    // Attempt to parse
                    std::ifstream file(entry.path());

                    nlohmann::json data = nlohmann::json::parse(file);

                    // Validate #1
                    if (data.find("DisplayName") == data.end())
                        continue;

                    // Most people have 'Dying Light Enhanced Edition' as the game's name, but it's better to include Dying Light only, just in case
                    std::string display_name = data.at("DisplayName");

                    if (display_name.empty())
                        continue;

                    if (display_name.find("Dying Light") == std::string::npos)
                        continue;

                    // Validate #2
                    if (data.find("InstallLocation") == data.end())
                        continue;

                    // Installation path
                    std::string install_location = data.at("InstallLocation");

                    if (install_location.empty())
                        continue;

                    // Kaboosh, our path is cool
                    game_path = install_location;

                    // Finish
                    break;
                }
            }
        }

        // Actual game path (steam ? steam : epic)
        std::filesystem::path actual_path = steam ? game_path + "\\steamapps\\common\\Dying Light" : game_path;

        if (!std::filesystem::exists(actual_path))
            throw_error("Unfortunately, the program has failed to find Dying Light's directory.");

        // DW path
        std::filesystem::path dw_path = actual_path;
        dw_path /= "DW";

        // Create directory if none present
        if (!std::filesystem::exists(dw_path))
            std::filesystem::create_directory(dw_path);

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
            std::cout << std::string("\\ Copied " + temporary.string() + " into DW folder") << std::endl;
        }

        std::cout << "Doing dide_mod setup..." << std::endl;

        // No need to install new one if you already have it
        bool already_present = false;

        // 2 hits are success
        int hits = 0;

        for (const auto& file : std::filesystem::directory_iterator(actual_path))
        {
            std::string name = file.path().filename().string();

            if (name == "dide_mod.ini" || name == "dsound.dll")
                hits++;
        }

        // Yes, the user already has it
        if (hits == 2)
            already_present = true;

        // Don't make a fresh one
        if (!already_present)
        {
            // Notify
            std::cout << "Dide mod not present, installing..." << std::endl;

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

            // Ideally you should already have those in a ready one but meh
            CONFIGURE(ini);

            // Write
            std::cout << "Writing datapaks into dide_mod..." << std::endl;

            for (const auto& choice : splitted)
                ini["CustomPak"][std::string(dw_path.string() + "\\" + paks.at(std::atoi(choice.c_str())).path.filename().string())] = "1";

            file.generate(ini);

            // Replace and delete placeholder
            std::filesystem::path dide_path = actual_path;
            dide_path /= "dide_mod.ini";

            std::filesystem::copy("_placeholder.ini", dide_path, std::filesystem::copy_options::overwrite_existing);
        }
        else
        {
            // Dide path
            std::filesystem::path dide_path = actual_path;
            dide_path /= "dide_mod.ini";

            // INI
            mINI::INIFile file(dide_path.string().c_str());
            mINI::INIStructure ini;

            // Read
            file.read(ini);

            // So there will be no issues
            CONFIGURE(ini);

            // Adding new ones
            std::cout << "Adding new datapaks into dide_mod..." << std::endl;

            for (const auto& choice : splitted)
                ini["CustomPak"][std::string(dw_path.string() + "\\" + paks.at(std::atoi(choice.c_str())).path.filename().string())] = "1";

            // Write updates to file
            file.write(ini);
        }

        // Another deep clean so we don't have our space being eaten
        std::cout << "Cleaning out the mess..." << std::endl;

        deep_clean(true);

        // Congratulations
        throw_error("@ Successfull installation! You can now enter the game.", 6);
    }
    else
    {
        // Fail :(
        throw_error("Failed to download the mod collection.");
    }
}