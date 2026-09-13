#include "ProcessLauncher.hpp"

#include <string>
#include <vector>

#if defined (WINDOWS)
#include "Win32Interface.hpp"
#else
#include <spawn.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
extern char** environ;
#endif

namespace {

std::string ToUtf8 (const GS::UniString& str)
{
    return std::string (str.ToCStr (CC_UTF8).Get ());
}

#if defined (WINDOWS)

std::wstring ToWide (const GS::UniString& str)
{
    const std::string utf8 = ToUtf8 (str);
    if (utf8.empty ()) {
        return std::wstring ();
    }
    const int len = MultiByteToWideChar (CP_UTF8, 0, utf8.c_str (), -1, nullptr, 0);
    std::wstring result (static_cast<size_t> (len), L'\0');
    MultiByteToWideChar (CP_UTF8, 0, utf8.c_str (), -1, &result[0], len);
    result.resize (static_cast<size_t> (len) - 1);
    return result;
}

// Quotes one argument according to the MSVC CommandLineToArgvW rules.
std::wstring QuoteArgument (const std::wstring& arg)
{
    if (!arg.empty () && arg.find_first_of (L" \t\n\v\"") == std::wstring::npos) {
        return arg;
    }
    std::wstring result = L"\"";
    size_t backslashes = 0;
    for (wchar_t ch : arg) {
        if (ch == L'\\') {
            ++backslashes;
            continue;
        }
        if (ch == L'"') {
            result.append (backslashes * 2 + 1, L'\\');
            result.push_back (ch);
        } else {
            result.append (backslashes, L'\\');
            result.push_back (ch);
        }
        backslashes = 0;
    }
    result.append (backslashes * 2, L'\\');
    result.push_back (L'"');
    return result;
}

#endif

}

namespace ProcessLauncher {

LaunchResult LaunchDetached (const GS::UniString& executable,
                             const GS::Array<GS::UniString>& arguments,
                             const GS::UniString& workingDirectory)
{
    LaunchResult result;

#if defined (WINDOWS)
    std::wstring commandLine = QuoteArgument (ToWide (executable));
    for (const GS::UniString& arg : arguments) {
        commandLine += L" ";
        commandLine += QuoteArgument (ToWide (arg));
    }
    result.commandLine = GS::UniString (reinterpret_cast<const GS::UniChar*> (commandLine.c_str ()));

    std::wstring workDir = ToWide (workingDirectory);
    std::vector<wchar_t> mutableCommandLine (commandLine.begin (), commandLine.end ());
    mutableCommandLine.push_back (L'\0');

    STARTUPINFOW startupInfo = {};
    startupInfo.cb = sizeof (startupInfo);
    PROCESS_INFORMATION processInfo = {};
    const BOOL ok = CreateProcessW (nullptr, mutableCommandLine.data (), nullptr, nullptr, FALSE,
                                    CREATE_NEW_CONSOLE | CREATE_UNICODE_ENVIRONMENT, nullptr,
                                    workDir.empty () ? nullptr : workDir.c_str (),
                                    &startupInfo, &processInfo);
    if (!ok) {
        result.errorMessage = GS::UniString::Printf ("CreateProcess failed with error %lu", GetLastError ());
        return result;
    }
    result.processId = static_cast<Int64> (processInfo.dwProcessId);
    CloseHandle (processInfo.hThread);
    CloseHandle (processInfo.hProcess);
    result.success = true;
#else
    std::vector<std::string> argStorage;
    argStorage.push_back (ToUtf8 (executable));
    for (const GS::UniString& arg : arguments) {
        argStorage.push_back (ToUtf8 (arg));
    }
    std::vector<char*> argv;
    for (std::string& s : argStorage) {
        argv.push_back (&s[0]);
    }
    argv.push_back (nullptr);

    GS::UniString joined;
    for (const std::string& s : argStorage) {
        if (!joined.IsEmpty ()) {
            joined += " ";
        }
        joined += GS::UniString (s.c_str (), CC_UTF8);
    }
    result.commandLine = joined;

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init (&actions);
    const std::string workDir = ToUtf8 (workingDirectory);
    if (!workDir.empty ()) {
        posix_spawn_file_actions_addchdir_np (&actions, workDir.c_str ());
    }
    pid_t pid = 0;
    const int rc = posix_spawnp (&pid, argv[0], &actions, nullptr, argv.data (), environ);
    posix_spawn_file_actions_destroy (&actions);
    if (rc != 0) {
        result.errorMessage = GS::UniString::Printf ("posix_spawnp failed: %s", strerror (rc));
        return result;
    }
    result.processId = static_cast<Int64> (pid);
    result.success = true;
#endif

    return result;
}

}
