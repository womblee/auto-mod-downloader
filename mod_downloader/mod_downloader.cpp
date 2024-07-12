#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Normaliz.lib")
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Wldap32.lib")
#pragma comment(lib, "libcurl_a.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "zip.lib")

#include <iostream>
#include <string>
#include <vector>
#include <Windows.h>
#include <thread>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <zip.h>
#include "vdf_parser.hpp"
#include "ini.hpp"

#define NEWLINE std::cout << std::endl;
#define CONFIGURE(ini) ini["General"]["EnableMod"] = "1"; ini["Features"]["DeveloperMenu"] = "0"; ini["Features"]["CustomPak"] = "1";
#define TRANSFORM(buffer, str) \
    do { \
        std::wstring little(buffer); \
        str.clear(); \
        std::transform(little.begin(), little.end(), std::back_inserter(str), [](wchar_t c) { return static_cast<char>(c); }); \
    } while (0)

const std::string DEFAULT_CONFIG = R"(
[Config]
download_file=https://www.nlog.us/dl/full_archive.zip
download_copyright=Nest Rushers Discord
)";

// Needed
std::string download_copyright;
std::string download_file;
std::string file_name;

bool download_complete = false;
double download_progress = 0.0;

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

// Function to extract a ZIP archive
void extract_zip(const std::string& zip_path, const std::string& extract_path)
{
    int err = 0;
    zip* za = zip_open(zip_path.c_str(), 0, &err);
    if (za == nullptr)
        throw_error(("Failed to open ZIP archive: " + zip_path).c_str());

    // Get the number of entries in the archive
    zip_int64_t num_entries = zip_get_num_entries(za, 0);
    if (num_entries < 0)
    {
        zip_close(za);
        throw_error(("Failed to get number of entries in ZIP archive: " + zip_path).c_str());
    }

    // Iterate over each entry in the archive
    for (zip_uint64_t i = 0; i < num_entries; ++i)
    {
        struct zip_stat st;
        zip_stat_init(&st);
        if (zip_stat_index(za, i, 0, &st) != 0)
        {
            zip_close(za);
            throw_error(("Failed to get entry information in ZIP archive: " + zip_path).c_str());
        }

        const char* name = st.name;
        if (name[strlen(name) - 1] == '/')
        {
            // This is a directory, create it
            std::filesystem::create_directories(extract_path + "/" + name);
        }

        else
        {
            // This is a file, extract it
            zip_file* zf = zip_fopen_index(za, i, 0);
            if (zf == nullptr)
            {
                zip_close(za);
                throw_error(("Failed to open file in ZIP archive: " + std::string(name)).c_str());
            }

            std::ofstream ofs(extract_path + "/" + name, std::ios::binary);
            if (!ofs.is_open())
            {
                zip_fclose(zf);
                zip_close(za);
                throw_error(("Failed to create output file: " + extract_path + "/" + name).c_str());
            }

            char buffer[4096];
            zip_int64_t bytes_read = 0;
            while ((bytes_read = zip_fread(zf, buffer, sizeof(buffer))) > 0)
                ofs.write(buffer, bytes_read);

            ofs.close();
            zip_fclose(zf);

            if (bytes_read < 0)
            {
                zip_close(za);
                throw_error(("Failed to read file in ZIP archive: " + std::string(name)).c_str());
            }
        }
    }

    zip_close(za);
}

// Positive answer
bool is_answer_positive(std::string answer)
{
    std::vector <std::string> positive_answers { "Y", "y", "+", "1" };

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

// Lower case
std::string to_lower(const std::string& str)
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return std::tolower(c); });

    return result;
}

// Write
size_t write_data(void* ptr, size_t size, size_t nmemb, FILE* stream)
{
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

// Callback
int progress_callback(void* clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
    if (dltotal > 0)
        download_progress = dlnow / dltotal * 100.0;

    return 0;
}

// Cool progress bar
void display_progress()
{
    int width = 50;
    std::cout << "[";

    int pos = static_cast<int>(width * download_progress / 100.0);
    for (int i = 0; i < width; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }

    std::cout << "] " << static_cast<int>(download_progress) << "%\r";
    std::cout.flush();
}

bool get_bonzo(const std::string& download_file)
{
    CURL* curl = curl_easy_init();
    if (!curl)
    {
        std::cerr << "Failed to initialize curl" << std::endl;
        return false;
    }

    // Assign to variable which we're gonna use later
    file_name = download_file.substr(download_file.find_last_of("/") + 1);

    FILE* fp = fopen(file_name.c_str(), "wb");
    if (!fp)
    {
        std::cerr << "Failed to open file for writing" << std::endl;
        curl_easy_cleanup(curl);

        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, download_file.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, progress_callback);

    CURLcode res = curl_easy_perform(curl);

    fclose(fp);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
    {
        std::cerr << "Download failed: " << curl_easy_strerror(res) << std::endl;
        return false;
    }

    download_complete = true;
    return true;
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
    std::vector<std::string> bad_ones { "out", "full_archive.zip", "_placeholder.ini", "dide_mod.ini" };

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
    std::vector<std::string> needed { "curlpp.dll" };

    for (const auto& file : needed)
    {
        // The odd one's out
        std::filesystem::path needed_file = std::filesystem::current_path();
        needed_file /= file;

        if (!std::filesystem::exists(needed_file))
            throw_error("Make sure you have 'curlpp.dll' in the same folder as the program.");
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

        // Ask about mod usage method
        std::cout << "Are you using DLML or Regular Online mod usage? (dlml/regular): ";
        std::string mod_usage_method;
        std::getline(std::cin, mod_usage_method);

        bool use_dlml = (mod_usage_method == "dlml");

        if (use_dlml)
        {
            // DLML uninstall
            std::filesystem::path custom_paks_path = std::filesystem::path(input_path) / "CustomPaks";

            if (std::filesystem::exists(custom_paks_path))
            {
                std::cout << "Do you really want to delete all mods in the CustomPaks folder? (yes/no): ";
                std::string confirm;
                std::getline(std::cin, confirm);

                if (is_answer_positive(confirm))
                {
                    for (const auto& entry : std::filesystem::directory_iterator(custom_paks_path))
                    {
                        if (entry.is_regular_file() && entry.path().extension() == ".pak")
                        {
                            std::filesystem::remove(entry.path());
                            std::cout << "Deleted: " << entry.path().filename() << std::endl;
                        }
                    }

                    throw_error("All .pak files have been deleted from the CustomPaks folder.", 5);
                }
                else
                    throw_error("Uninstall cancelled.");
            }
            else
                throw_error("CustomPaks folder not found. No mods to uninstall.");
        }
        else
        {
            // Regular Online mod usage uninstall
            std::string dide_direct = input_path + "\\dide_mod.ini";

            if (!std::filesystem::exists(dide_direct))
                throw_error("Failed to find dide_mod.ini inside this folder, please input a proper one");

            // Prompt that everything is gonna go into waste
            NEWLINE

            std::cout << "Do you really want to disable every mod in dide_mod.ini? (yes/no): " << std::endl;
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

                throw_error("All mods have been disabled in dide_mod.ini.\nRemember, you can always enable the mods again by changing 'EnableMod' to '1' in dide_mod.ini...");
            }
            else
                throw_error("Uninstall cancelled.");
        }
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
    std::thread download_thread(get_bonzo, download_file);

    while (!download_complete)
    {
        display_progress();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    download_thread.join();

    std::cout << std::endl << "Download complete!" << std::endl;

    // Attempt
    try
    {
        extract_zip(file_name, "out");
        std::cout << "Extraction completed successfully!" << std::endl;
    }
    catch (const std::exception& ex) { throw_error(ex.what()); }

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
    input_numbers.erase(std::remove_if(input_numbers.begin(), input_numbers.end(), ::isspace), input_numbers.end());

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
                TRANSFORM(szBuffer, steam_path);

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

        // Needed for additional verification via appmanifest
        bool found = false;

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
                        {
                            game_path = root.childs[child.first].get()->attribs["path"];

                            found = true;
                        }
                    }
                }
            }
        }


        // This was not required in the first place, but some people had issues, and I had to do it
        // Attempt to read appmanifest_239140.acf
        if (found)
        {
            std::ifstream main_add(std::string(game_path + "\\steamapps\\appmanifest_239140.acf"));

            if (main_add.fail())
                throw_error("Dying Light's app manifest doesn't exist! Make sure 'appmanifest_239140.acf' exists in your steamapps folder.");

            // Parse installdir
            auto root_add = tyti::vdf::read(main_add);

            game_path += "\\steamapps\\common\\" + root_add.attribs["installdir"];
        }
        else
            throw_error("Failed to find Dying Light (239140) in your 'libraryfolders.vdf' file which is inside of the steamapps folder.");
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
                TRANSFORM(szBuffer, manifest_path);
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
                    TRANSFORM(szBuffer, manifest_path);
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

    // Verify if we can proceed further
    if (!std::filesystem::exists(game_path))
        throw_error("Unfortunately, the program has failed to find Dying Light's directory.");

    // Ask about mod usage method
    std::cout << "Do you want to use DLML or Regular Online mod usage? (dlml/regular): ";
    std::string mod_usage_method;
    std::getline(std::cin, mod_usage_method);

    bool use_dlml = (mod_usage_method == "dlml");

    std::cout << "Doing mod setup..." << std::endl;

    if (use_dlml)
    {
        // Status
        bool dlml_found = false;
        std::filesystem::path dlml_dsound;

        for (const auto& dir : std::filesystem::directory_iterator("out"))
        {
            if (std::filesystem::is_directory(dir))
            {
                std::string dir_name = to_lower(dir.path().string()); // We have to to_lower the directory name because the search for 'dlml' is case-sensitive

                if (dir_name.find("dlml") != std::string::npos)
                {
                    bool has_dsound = false;
                    bool has_dide_mod = false;

                    for (const auto& file : std::filesystem::directory_iterator(dir.path()))
                    {
                        std::string name = file.path().filename().string();
                        if (name == "dsound.dll")
                        {
                            has_dsound = true;
                            dlml_dsound = file.path();
                        }
                        else if (name == "dide_mod.ini")
                            has_dide_mod = true;
                    }

                    if (has_dsound && !has_dide_mod)
                    {
                        dlml_found = true;
                        break; // Found the DLML folder, no need to continue searching
                    }
                }
            }
        }

        if (!dlml_found)
            throw_error("Couldn't find the DLML online mod loader in the mod collection.");

        // Paks setup
        std::filesystem::path custom_paks_path = std::filesystem::path(game_path) / "CustomPaks";
        std::filesystem::create_directories(custom_paks_path);

        std::cout << "Copying desired datapaks into CustomPaks..." << std::endl;

        for (const auto& choice : splitted)
        {
            std::filesystem::path source = paks.at(std::atoi(choice.c_str())).path;
            std::filesystem::path destination = custom_paks_path / source.filename();

            std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing);

            std::cout << "Copied " << source.filename() << " into CustomPaks folder" << std::endl;
        }

        // Copy dsound.dll for DLML
        if (std::filesystem::exists(dlml_dsound))
        {
            std::filesystem::copy_file(dlml_dsound, std::filesystem::path(game_path) / "dsound.dll", std::filesystem::copy_options::overwrite_existing);
            std::cout << "Copied DLML dsound.dll to game directory" << std::endl;
        }
        else
            throw_error("DLML dsound.dll not found in the archive.");
    }
    else
    {
        // Regular Online Mod Loader setup
        std::filesystem::path dw_path = game_path;
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

            if (std::filesystem::exists(existing))
                std::filesystem::remove(existing);

            // Copy from temporary
            std::filesystem::path temporary = paks.at(std::atoi(choice.c_str())).path;
            std::filesystem::copy(temporary, dw_path, std::filesystem::copy_options::overwrite_existing);

            // Output
            std::cout << std::string("\\ Copied " + temporary.string() + " into DW folder") << std::endl;
        }

        // No need to install new one if you already have it
        bool already_present = false;

        // 2 hits are success
        int hits = 0;

        for (const auto& file : std::filesystem::directory_iterator(game_path))
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
                                std::filesystem::copy(file, game_path, std::filesystem::copy_options::overwrite_existing);
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
            std::filesystem::path dide_path = game_path;
            dide_path /= "dide_mod.ini";

            std::filesystem::copy("_placeholder.ini", dide_path, std::filesystem::copy_options::overwrite_existing);
        }
        else
        {
            // Dide path
            std::filesystem::path dide_path = game_path;
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
    }

    // Another deep clean so we don't have our space being eaten
    std::cout << "Cleaning out the mess..." << std::endl;

    deep_clean(true);

    // Congratulations
    throw_error("@ Successfull installation! You can now enter the game.", 6);
}