// EXT-17 — child process supervision.
//
// CR-EX-1: a run's processes are terminated by the handle that created them, never by image
// name. That is the whole reason this is a component rather than a call to system(): a handle
// cannot address a process this campaign did not start, and an image name can.
//
// Detection by image name is a different act and is allowed — CR-EX-1's third criterion asks
// for exactly it, so that a host left behind by a crashed campaign is a named error in the
// first second rather than twenty contaminated runs in an hour. See `findProcessesByImageName`.
//
// Never throws (constraint C3).
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ext17::proc {

struct StartSpec {
    std::string exePath;
    std::vector<std::string> args;
    std::string workingDirectory;

    // Environment applied on top of this process's own, by name. The two that matter here are
    // measured preconditions, not preferences:
    //   N8RO_RELEASE  - without it the host skips its plugin scan, never registers
    //                   componentPhysics, and refuses every 42-entity scenario load while
    //                   sitting idle rather than failing (M1, docs/m1-lifecycle.md 7a).
    //   PATH          - C:\N8RO\bin must be on it or an SDK-linked binary will not load at all
    //                   (M2 re-check; the DLLs resolve from PATH and from nowhere else).
    std::vector<std::pair<std::string, std::string>> environment;
    std::string pathPrepend;

    // Both are optional. An empty path leaves that stream inherited.
    std::string stdoutPath;
    std::string stderrPath;
};

// A started child, owning its handles. Move-only: the owner of the handle is the only thing
// permitted to end the process.
class Process {
public:
    static std::optional<Process> start(const StartSpec& spec, std::string& error);

    Process(Process&& other) noexcept;
    Process& operator=(Process&& other) noexcept;
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;
    ~Process();

    [[nodiscard]] std::uint32_t pid() const { return pid_; }
    [[nodiscard]] bool valid() const { return handle_ != nullptr; }

    [[nodiscard]] bool isAlive() const;
    // The exit code, once the process has exited; nullopt while it is still running.
    [[nodiscard]] std::optional<std::uint32_t> exitCode() const;
    // Wait up to timeoutMs for exit. Returns true if the process exited within it.
    [[nodiscard]] bool waitFor(int timeoutMs) const;
    // Ask the process to shut down, by console control event addressed to the process group
    // this campaign created for it. Returns false if the request could not be delivered; the
    // caller then falls back to terminate(). Preferred first, because a host killed outright
    // leaves its logger marker behind and the next host start renames the previous run's log
    // as a crash log (measured at M2).
    bool requestStopGracefully();

    // Terminate by handle. Returns true if the process is not running afterwards.
    bool terminate();

    // True if this process was ended by terminate() rather than exiting on its own.
    [[nodiscard]] bool wasTerminatedByUs() const { return terminatedByUs_; }

    // Release handles without ending the process. Used only after a confirmed exit.
    void close();

private:
    Process() = default;

    void* handle_ = nullptr;       // HANDLE
    void* stdoutHandle_ = nullptr; // HANDLE
    void* stderrHandle_ = nullptr; // HANDLE
    std::uint32_t pid_ = 0;
    mutable bool terminatedByUs_ = false;
};

// Detection only. Returns the pids of every running process with this image name, including
// ones this campaign did not start — which is the point.
std::vector<std::uint32_t> findProcessesByImageName(const std::string& imageName);

// Best-effort file utilities used for per-run evidence. Each returns false rather than throwing.
bool createDirectories(const std::string& path);
bool copyFileTo(const std::string& from, const std::string& to);

// Copy `from`'s bytes from `offset` to end into a new file `to`. Written for one measured
// property of the platform: the host appends to a single shared log inside the read-only
// install tree, so run N's copy of it otherwise contains runs 0..N as well. Recording the
// size before the host starts and slicing from there gives one run's log and nothing else.
// If the file is now shorter than `offset` it was rotated underneath us, and the whole of it
// is copied instead. Returns the number of bytes written, or nullopt on failure.
std::optional<std::uint64_t> copyFileTailFrom(const std::string& from, const std::string& to,
                                              std::uint64_t offset);
std::optional<std::uint64_t> fileSizeBytes(const std::string& path);
bool fileExists(const std::string& path);

// Names (not full paths) of the files in `dir` whose name ends with `suffix`, sorted. Used to
// find a run's capture without reproducing the recorder's own file-naming rule, which belongs
// to EXT-08 and is not something this project may depend on.
std::vector<std::string> listFilesWithSuffix(const std::string& dir, const std::string& suffix);

// The file name portion of a path.
std::string baseName(const std::string& path);

// The absolute form of `path`, or `path` unchanged if it cannot be resolved. Needed because a
// child process is started in its own working directory: a relative path handed to it means
// something different there than it did here. Measured at M3, from a probe run given a relative
// --out-dir: the recorder resolved it against the run directory it had just been placed in,
// found nothing, and refused - and the run went on to completion having recorded nothing.
std::string absolutePath(const std::string& path);

// Free space in bytes on the volume holding `path`, for CR-CAP-5's pre-flight check. nullopt
// when the volume cannot be queried, which is reported rather than assumed to be plenty.
std::optional<std::uint64_t> freeSpaceBytes(const std::string& path);

// The total size in bytes of everything under `dir`, recursively. CR-CAP-5's ceiling is over the
// campaign directory rather than over its captures, because logs are what a projection of
// capture size alone leaves out - measured at M3 at 5.4 MB per run against a 24.3 MB capture.
std::uint64_t directorySizeBytes(const std::string& dir);

} // namespace ext17::proc
