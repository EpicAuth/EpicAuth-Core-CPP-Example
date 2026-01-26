#include <Windows.h>
#include "auth.hpp"
#include <string>
#include <thread>
#include "utils.hpp"
#include "skStr.h"
#include <iostream>
#include <filesystem>
#include <iomanip>

std::string tm_to_readable_time(tm ctx);
static std::time_t string_to_timet(std::string timestamp);
static std::tm timet_to_tm(time_t timestamp);
void sessionMonitor();

using namespace EpicAuth;

// Application info
std::string name = skCrypt("name").decrypt();
std::string ownerid = skCrypt("ownerid").decrypt();
std::string version = skCrypt("1.0").decrypt();
std::string url = skCrypt("http://epicauth.cc/api/1.3/").decrypt();
std::string path = skCrypt("").decrypt();

api EpicAuthApp(name, ownerid, version, url, path);

// ==============================
// ====== CONSOLE UI HELPERS ====
// ==============================
void printHeader()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::cout << u8"╔══════════════════════════════════════════════╗\n";
    std::cout << u8"║           EPIC AUTH CONSOLE LOADER           ║\n";
    std::cout << u8"║               Best Auth System               ║\n";
    std::cout << u8"╚══════════════════════════════════════════════╝\n";
}

void printDivider()
{
    std::cout << "------------------------------------------\n";
}

void printUserData()
{
    printDivider();
    std::cout << "User Information:\n";
    std::cout << "  Username   : " << EpicAuthApp.user_data.username << "\n";
    std::cout << "  IP Address : " << EpicAuthApp.user_data.ip << "\n";
    std::cout << "  HWID       : " << EpicAuthApp.user_data.hwid << "\n";
    std::cout << "  Created    : " << tm_to_readable_time(timet_to_tm(string_to_timet(EpicAuthApp.user_data.createdate))) << "\n";
    std::cout << "  Last Login : " << tm_to_readable_time(timet_to_tm(string_to_timet(EpicAuthApp.user_data.lastlogin))) << "\n";

    std::cout << "  Subscriptions:\n";
    for (auto& sub : EpicAuthApp.user_data.subscriptions)
    {
        std::cout << "    - " << sub.name
            << " (Expires: " << tm_to_readable_time(timet_to_tm(string_to_timet(sub.expiry))) << ")\n";
    }
    printDivider();
}

// ==============================
// ====== MAIN FUNCTION =========
// ==============================
int main()
{
    std::string consoleTitle = "EpicAuth Loader - " + std::string(__DATE__) + " " + std::string(__TIME__);
    SetConsoleTitleA(consoleTitle.c_str());

    printHeader();
    std::cout << "Connecting to EpicAuth server...\n";
    EpicAuthApp.init();

    if (!EpicAuthApp.response.success)
    {
        std::cout << "\n[Error] " << EpicAuthApp.response.message << "\n";
        Sleep(1500);
        exit(1);
    }

    // Check auto-login file
    if (std::filesystem::exists("auth.json"))
    {
        if (!CheckIfJsonKeyExists("auth.json", "username"))
        {
            std::string key = ReadFromJson("auth.json", "license");
            EpicAuthApp.license(key);

            if (!EpicAuthApp.response.success)
            {
                std::remove("auth.json");
                std::cout << "\n[Error] " << EpicAuthApp.response.message << "\n";
                Sleep(1500);
                exit(1);
            }
            std::cout << "\n[Success] Automatically logged in via license.\n";
        }
        else
        {
            std::string username = ReadFromJson("auth.json", "username");
            std::string password = ReadFromJson("auth.json", "password");
            EpicAuthApp.login(username, password);

            if (!EpicAuthApp.response.success)
            {
                std::remove("auth.json");
                std::cout << "\n[Error] " << EpicAuthApp.response.message << "\n";
                Sleep(1500);
                exit(1);
            }
            std::cout << "\n[Success] Automatically logged in via credentials.\n";
        }
    }
    else
    {
        std::cout << "\nChoose an option:\n";
        std::cout << "  [1] Login\n";
        std::cout << "  [2] Register\n";
        std::cout << "  [3] Upgrade\n";
        std::cout << "  [4] License Key Only\n";
        std::cout << "Enter choice: ";

        int option;
        std::string username, password, key;
        std::cin >> option;

        switch (option)
        {
        case 1:
            std::cout << "Enter username: "; std::cin >> username;
            std::cout << "Enter password: "; std::cin >> password;
            EpicAuthApp.login(username, password);
            break;
        case 2:
            std::cout << "Enter username: "; std::cin >> username;
            std::cout << "Enter password: "; std::cin >> password;
            std::cout << "Enter license key: "; std::cin >> key;
            EpicAuthApp.Register(username, password, key);
            break;
        case 3:
            std::cout << "Enter username: "; std::cin >> username;
            std::cout << "Enter license key: "; std::cin >> key;
            EpicAuthApp.upgrade(username, key);
            break;
        case 4:
            std::cout << "Enter license key: "; std::cin >> key;
            EpicAuthApp.license(key);
            break;
        default:
            std::cout << "\n[Error] Invalid selection!\n";
            Sleep(1500);
            exit(1);
        }

        if (!EpicAuthApp.response.success)
        {
            std::cout << "\n[Error] " << EpicAuthApp.response.message << "\n";
            Sleep(1500);
            exit(1);
        }

        // Create auto-login file
        if (!username.empty() && !password.empty())
        {
            WriteToJson("auth.json", "username", username, true, "password", password);
        }
        else
        {
            WriteToJson("auth.json", "license", key, false, "", "");
        }

        std::cout << "\n[Success] Auto-login file created.\n";
    }

    // Launch session monitor thread
    std::thread run(checkAuthenticated, ownerid);
    std::thread monitor(sessionMonitor);

    // Show user info
    printUserData();

    std::cout << "\nApplication will close in 5 seconds...\n";
    Sleep(5000);

    return 0;
}

// ==============================
// ====== SESSION MONITOR =======
// ==============================
void sessionMonitor()
{
    EpicAuthApp.check();
    if (!EpicAuthApp.response.success) exit(0);

    while (EpicAuthApp.response.isPaid)
    {
        Sleep(20000);
        EpicAuthApp.check();
        if (!EpicAuthApp.response.success) exit(0);
    }
}

// ==============================
// ====== TIME HELPERS ==========
// ==============================
std::string tm_to_readable_time(tm ctx)
{
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%a %m/%d/%y %H:%M:%S", &ctx);
    return std::string(buffer);
}

static std::time_t string_to_timet(std::string timestamp)
{
    return static_cast<time_t>(strtol(timestamp.c_str(), nullptr, 10));
}

static std::tm timet_to_tm(time_t timestamp)
{
    std::tm context;
    localtime_s(&context, &timestamp);
    return context;
}
