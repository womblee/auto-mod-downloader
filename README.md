# Comfortable mod loader
This tool lets you setup your mods with the help of Nest Rushers in 5 seconds, it literally does everything for you.

Works only for Dying Light, uses Nest Rushers' website for grabbing the mods.

*You don't have to specify any paths or something like that (with steam). This tool automatically finds your game's directory!*

![image](https://user-images.githubusercontent.com/52250786/235461989-3c971c1d-7caf-498b-9ff5-e389fd765bf8.png)

![image](https://user-images.githubusercontent.com/52250786/235461956-0354ff45-1277-4ef8-b970-e833ae887527.png)

![image](https://user-images.githubusercontent.com/52250786/235462256-67c518f6-4eba-4678-beed-ff030dde74a0.png)

# Sleek
After the work is done, it leaves absolutely zero mess (of course, since it cleans up all the leftover files)

# How to setup for your own mod collection
Create ***config.ini*** and change the following:
```ini
[Config]
download_server=https://www.nlog.us/downloads/
download_file=full_archive.zip
```

# How to compile
```
NuGet packages: (curl),
Release x64,
Dynamic Link Library (.dll),
Visual Studio 2022 (v143),
C++ Standart - ISO C++17,
C Standart - Legacy MSVC,
Preprocessor arguments: CURL_STATICLIB, _CRT_SECURE_NO_WARNINGS, _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
Symbols: Unicode.
```

# Additional requirements
7z.dll (you can take it from your 7-zip folder)
