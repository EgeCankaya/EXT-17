#include "Process.h"

#include "../common/Log.h"

#include <algorithm>
#include <cstring>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>

namespace ext17::proc {
namespace {

std::wstring widen(const std::string& s) {
    if (s.empty()) { return std::wstring(); }
    const int n = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) { return std::wstring(); }
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

std::string narrow(const std::wstring& w) {
    if (w.empty()) { return std::string(); }
    const int n = ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()),
                                        nullptr, 0, nullptr, nullptr);
    if (n <= 0) { return std::string(); }
    std::string s(static_cast<std::size_t>(n), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()),
                          s.data(), n, nullptr, nullptr);
    return s;
}

std::string lastErrorText() {
    const DWORD e = ::GetLastError();
    return "win32 error " + std::to_string(static_cast<unsigned long>(e));
}

// Windows command-line quoting, as CommandLineToArgvW parses it. An argument is quoted when it
// is empty or holds a space, a tab or a quote; backslashes immediately before a quote double.
void appendQuoted(std::wstring& out, const std::wstring& arg) {
    const bool needsQuotes = arg.empty() ||
        arg.find_first_of(L" \t\n\v\"") != std::wstring::npos;
    if (!needsQuotes) {
        out += arg;
        return;
    }
    out += L'"';
    for (auto it = arg.begin();; ++it) {
        std::size_t backslashes = 0;
        while (it != arg.end() && *it == L'\\') { ++it; ++backslashes; }
        if (it == arg.end()) {
            out.append(backslashes * 2, L'\\');
            break;
        }
        if (*it == L'"') {
            out.append(backslashes * 2 + 1, L'\\');
        } else {
            out.append(backslashes, L'\\');
        }
        out += *it;
    }
    out += L'"';
}

bool equalsNoCase(const std::wstring& a, const std::wstring& b) {
    if (a.size() != b.size()) { return false; }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (::towupper(a[i]) != ::towupper(b[i])) { return false; }
    }
    return true;
}

// Build the child's environment block: this process's, with the spec's names overridden and
// PATH prepended. Returns a double-NUL-terminated wide block.
std::wstring buildEnvironmentBlock(const StartSpec& spec) {
    std::vector<std::pair<std::wstring, std::wstring>> vars;

    LPWCH block = ::GetEnvironmentStringsW();
    if (block) {
        for (LPWCH p = block; *p; ) {
            const std::wstring entry(p);
            p += entry.size() + 1;
            // Skip the "=C:=..." drive-current-directory entries: they begin with '='.
            if (entry.empty() || entry[0] == L'=') { continue; }
            const std::size_t eq = entry.find(L'=', 1);
            if (eq == std::wstring::npos) { continue; }
            vars.emplace_back(entry.substr(0, eq), entry.substr(eq + 1));
        }
        ::FreeEnvironmentStringsW(block);
    }

    const auto setVar = [&vars](const std::wstring& name, const std::wstring& value) {
        for (auto& v : vars) {
            if (equalsNoCase(v.first, name)) { v.second = value; return; }
        }
        vars.emplace_back(name, value);
    };
    const auto getVar = [&vars](const std::wstring& name) -> std::wstring {
        for (const auto& v : vars) {
            if (equalsNoCase(v.first, name)) { return v.second; }
        }
        return std::wstring();
    };

    for (const auto& kv : spec.environment) {
        setVar(widen(kv.first), widen(kv.second));
    }
    if (!spec.pathPrepend.empty()) {
        const std::wstring prepend = widen(spec.pathPrepend);
        const std::wstring current = getVar(L"PATH");
        setVar(L"PATH", current.empty() ? prepend : (prepend + L";" + current));
    }

    std::wstring out;
    for (const auto& v : vars) {
        out += v.first;
        out += L'=';
        out += v.second;
        out += L'\0';
    }
    out += L'\0';
    return out;
}

void* openForWrite(const std::string& path) {
    if (path.empty()) { return nullptr; }
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof sa;
    sa.bInheritHandle = TRUE;
    const HANDLE h = ::CreateFileW(widen(path).c_str(), GENERIC_WRITE,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    return h == INVALID_HANDLE_VALUE ? nullptr : h;
}

} // namespace

std::optional<Process> Process::start(const StartSpec& spec, std::string& error) {
    error.clear();

    std::wstring cmdline;
    appendQuoted(cmdline, widen(spec.exePath));
    for (const auto& a : spec.args) {
        cmdline += L' ';
        appendQuoted(cmdline, widen(a));
    }

    Process p;
    p.stdoutHandle_ = openForWrite(spec.stdoutPath);
    p.stderrHandle_ = openForWrite(spec.stderrPath);
    if (!spec.stdoutPath.empty() && !p.stdoutHandle_) {
        error = "could not open stdout file " + spec.stdoutPath + ": " + lastErrorText();
        return std::nullopt;
    }
    if (!spec.stderrPath.empty() && !p.stderrHandle_) {
        error = "could not open stderr file " + spec.stderrPath + ": " + lastErrorText();
        return std::nullopt;
    }

    STARTUPINFOW si{};
    si.cb = sizeof si;
    const bool redirecting = p.stdoutHandle_ || p.stderrHandle_;
    if (redirecting) {
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdInput  = ::GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = p.stdoutHandle_ ? static_cast<HANDLE>(p.stdoutHandle_)
                                        : ::GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError  = p.stderrHandle_ ? static_cast<HANDLE>(p.stderrHandle_)
                                        : ::GetStdHandle(STD_ERROR_HANDLE);
    }

    std::wstring env = buildEnvironmentBlock(spec);
    const std::wstring cwd = widen(spec.workingDirectory);

    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> mutableCmdline(cmdline.begin(), cmdline.end());
    mutableCmdline.push_back(L'\0');

    const BOOL ok = ::CreateProcessW(
        nullptr,
        mutableCmdline.data(),
        nullptr, nullptr,
        redirecting ? TRUE : FALSE,
        CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_PROCESS_GROUP,
        env.data(),
        cwd.empty() ? nullptr : cwd.c_str(),
        &si, &pi);

    if (!ok) {
        error = "CreateProcess failed for " + spec.exePath + ": " + lastErrorText();
        return std::nullopt;
    }

    ::CloseHandle(pi.hThread);
    p.handle_ = pi.hProcess;
    p.pid_ = static_cast<std::uint32_t>(pi.dwProcessId);
    return std::optional<Process>(std::move(p));
}

Process::Process(Process&& other) noexcept {
    *this = std::move(other);
}

Process& Process::operator=(Process&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = other.handle_;
        stdoutHandle_ = other.stdoutHandle_;
        stderrHandle_ = other.stderrHandle_;
        pid_ = other.pid_;
        terminatedByUs_ = other.terminatedByUs_;
        other.handle_ = nullptr;
        other.stdoutHandle_ = nullptr;
        other.stderrHandle_ = nullptr;
        other.pid_ = 0;
    }
    return *this;
}

Process::~Process() {
    close();
}

bool Process::isAlive() const {
    if (!handle_) { return false; }
    return ::WaitForSingleObject(static_cast<HANDLE>(handle_), 0) == WAIT_TIMEOUT;
}

std::optional<std::uint32_t> Process::exitCode() const {
    if (!handle_) { return std::nullopt; }
    if (isAlive()) { return std::nullopt; }
    DWORD code = 0;
    if (!::GetExitCodeProcess(static_cast<HANDLE>(handle_), &code)) { return std::nullopt; }
    return static_cast<std::uint32_t>(code);
}

bool Process::waitFor(int timeoutMs) const {
    if (!handle_) { return true; }
    const DWORD r = ::WaitForSingleObject(static_cast<HANDLE>(handle_),
                                          timeoutMs < 0 ? INFINITE
                                                        : static_cast<DWORD>(timeoutMs));
    return r == WAIT_OBJECT_0;
}

bool Process::requestStopGracefully() {
    if (!handle_ || !isAlive()) { return true; }
    // The child was created with CREATE_NEW_PROCESS_GROUP, so its group id is its pid and the
    // event reaches it alone - never this process, and never a host the campaign did not start.
    if (!::GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, static_cast<DWORD>(pid_))) {
        log::line("teardown", "CTRL_BREAK to pid " + std::to_string(pid_) + " not delivered: "
                                  + lastErrorText());
        return false;
    }
    return true;
}

bool Process::terminate() {
    if (!handle_) { return true; }
    if (!isAlive()) { return true; }
    terminatedByUs_ = true;
    if (!::TerminateProcess(static_cast<HANDLE>(handle_), 1)) {
        log::line("teardown", "TerminateProcess failed for pid " + std::to_string(pid_) + ": "
                                  + lastErrorText());
        return false;
    }
    return waitFor(10000);
}

void Process::close() {
    if (handle_) { ::CloseHandle(static_cast<HANDLE>(handle_)); handle_ = nullptr; }
    if (stdoutHandle_) { ::CloseHandle(static_cast<HANDLE>(stdoutHandle_)); stdoutHandle_ = nullptr; }
    if (stderrHandle_) { ::CloseHandle(static_cast<HANDLE>(stderrHandle_)); stderrHandle_ = nullptr; }
}

std::vector<std::uint32_t> findProcessesByImageName(const std::string& imageName) {
    std::vector<std::uint32_t> pids;
    const HANDLE snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) { return pids; }
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof entry;
    const std::wstring wanted = widen(imageName);
    if (::Process32FirstW(snap, &entry)) {
        do {
            if (equalsNoCase(std::wstring(entry.szExeFile), wanted)) {
                pids.push_back(static_cast<std::uint32_t>(entry.th32ProcessID));
            }
        } while (::Process32NextW(snap, &entry));
    }
    ::CloseHandle(snap);
    return pids;
}

bool createDirectories(const std::string& path) {
    const std::wstring w = widen(path);
    if (w.empty()) { return false; }
    std::wstring accum;
    for (std::size_t i = 0; i <= w.size(); ++i) {
        if (i == w.size() || w[i] == L'\\' || w[i] == L'/') {
            if (!accum.empty() && accum.back() != L':') {
                ::CreateDirectoryW(accum.c_str(), nullptr);
            }
        }
        if (i < w.size()) { accum += w[i]; }
    }
    const DWORD attr = ::GetFileAttributesW(w.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool copyFileTo(const std::string& from, const std::string& to) {
    return ::CopyFileW(widen(from).c_str(), widen(to).c_str(), FALSE) != 0;
}

std::optional<std::uint64_t> copyFileTailFrom(const std::string& from, const std::string& to,
                                              std::uint64_t offset) {
    const HANDLE in = ::CreateFileW(widen(from).c_str(), GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (in == INVALID_HANDLE_VALUE) { return std::nullopt; }

    LARGE_INTEGER size{};
    if (!::GetFileSizeEx(in, &size)) {
        ::CloseHandle(in);
        return std::nullopt;
    }
    if (static_cast<std::uint64_t>(size.QuadPart) < offset) {
        offset = 0;  // rotated underneath us; take the whole file
    }

    LARGE_INTEGER move{};
    move.QuadPart = static_cast<LONGLONG>(offset);
    if (!::SetFilePointerEx(in, move, nullptr, FILE_BEGIN)) {
        ::CloseHandle(in);
        return std::nullopt;
    }

    const HANDLE out = ::CreateFileW(widen(to).c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                     CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (out == INVALID_HANDLE_VALUE) {
        ::CloseHandle(in);
        return std::nullopt;
    }

    std::uint64_t copied = 0;
    std::vector<char> buffer(64 * 1024);
    for (;;) {
        DWORD read = 0;
        if (!::ReadFile(in, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)) {
            ::CloseHandle(in);
            ::CloseHandle(out);
            return std::nullopt;
        }
        if (read == 0) { break; }
        DWORD written = 0;
        if (!::WriteFile(out, buffer.data(), read, &written, nullptr) || written != read) {
            ::CloseHandle(in);
            ::CloseHandle(out);
            return std::nullopt;
        }
        copied += written;
    }
    ::CloseHandle(in);
    ::CloseHandle(out);
    return copied;
}

std::optional<std::uint64_t> fileSizeBytes(const std::string& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!::GetFileAttributesExW(widen(path).c_str(), GetFileExInfoStandard, &data)) {
        return std::nullopt;
    }
    return (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
}

bool fileExists(const std::string& path) {
    return ::GetFileAttributesW(widen(path).c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::vector<std::string> listFilesWithSuffix(const std::string& dir, const std::string& suffix) {
    std::vector<std::string> names;
    WIN32_FIND_DATAW data{};
    const HANDLE h = ::FindFirstFileW(widen(dir + "\\*").c_str(), &data);
    if (h == INVALID_HANDLE_VALUE) { return names; }
    do {
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { continue; }
        std::string name = narrow(std::wstring(data.cFileName));
        if (name.size() >= suffix.size() &&
            name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) {
            names.push_back(std::move(name));
        }
    } while (::FindNextFileW(h, &data));
    ::FindClose(h);
    std::sort(names.begin(), names.end());
    return names;
}

std::string baseName(const std::string& path) {
    const std::size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string absolutePath(const std::string& path) {
    char buf[MAX_PATH * 4];
    const DWORD n = GetFullPathNameA(path.c_str(), static_cast<DWORD>(sizeof buf), buf, nullptr);
    if (n == 0 || n >= sizeof buf) return path;
    return std::string(buf, n);
}

std::optional<std::uint64_t> freeSpaceBytes(const std::string& path) {
    ULARGE_INTEGER available{};
    if (!GetDiskFreeSpaceExA(path.c_str(), &available, nullptr, nullptr)) return std::nullopt;
    return static_cast<std::uint64_t>(available.QuadPart);
}

std::uint64_t directorySizeBytes(const std::string& dir) {
    std::uint64_t total = 0;
    WIN32_FIND_DATAA find{};
    const HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &find);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        const std::string name = find.cFileName;
        if (name == "." || name == "..") continue;
        if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            total += directorySizeBytes(dir + "\\" + name);
        } else {
            ULARGE_INTEGER size{};
            size.LowPart = find.nFileSizeLow;
            size.HighPart = find.nFileSizeHigh;
            total += size.QuadPart;
        }
    } while (FindNextFileA(h, &find));
    FindClose(h);
    return total;
}

} // namespace ext17::proc
