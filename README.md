# Comfortable mod loader
This tool lets you setup your mods in a matter of 5 seconds, it literally does everything for you.

Works only for Dying Light, uses the website specified in *config.ini* / default one in DEFAULT_CONFIG var for grabbing the mods.

**You don't have to specify any paths or something like that (steam & epic games path detection). This tool automatically finds your game's directory!**

# Screenshots (Outdated, need to retake)

![image](https://user-images.githubusercontent.com/52250786/235461989-3c971c1d-7caf-498b-9ff5-e389fd765bf8.png)

![image](https://user-images.githubusercontent.com/52250786/235461956-0354ff45-1277-4ef8-b970-e833ae887527.png)

![image](https://user-images.githubusercontent.com/52250786/235462256-67c518f6-4eba-4678-beed-ff030dde74a0.png)

# Sleek
After the work is done, it leaves absolutely zero mess (of course, since it cleans up all the leftover files)

# How to setup for your own mod collection
Create ***config.ini*** and modify it however you want.
```ini
[Config]
download_file=https://www.nlog.us/downloads/full_archive.zip
download_copyright=Nest Rushers Discord
```
This is how my configuration looks like, just a simple example.

![image](https://user-images.githubusercontent.com/52250786/235478951-bfc81558-ad6f-478d-80c3-d9079f447e2c.png)

Explanation of the downloadable .zip archive structure.
# How to compile
```
NuGet packages: (curlpp, nlohmann.json),
Release x64,
Dynamic Link Library (.dll),
Visual Studio 2022 (v143),
C++ Standart - ISO C++17,
C Standart - Legacy MSVC,
Preprocessor arguments: CURL_STATICLIB, _CRT_SECURE_NO_WARNINGS, _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
Symbols: Unicode.
```

# Additional requirements
7z.dll (you can take it from your 7-zip's folder)
