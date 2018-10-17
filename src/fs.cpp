 // system includes
#include <algorithm>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>
#include <string.h>
#include <unistd.h>

// library includes
#include <boost/filesystem.hpp>

// local includes
#include "fs.h"
#include "error.h"

namespace bf = boost::filesystem;

class AppImageLauncherFS::PrivateData {
public:
    // a.k.a. where the filesystem will be mounted
    // this only needs to be calculated once, on initialization
    std::string mountpoint;

    // registered AppImages are supposed to be executed only by the user and the group
    // furthermore, they must be read-only, as we don't implement any sort of writing (we don't need it)
    static constexpr int mode = 0550;
    // mount point directory must be writable
    static constexpr int mountpointMode = 0750;

    // holds registered AppImages
    // they're indexed by a monotonically increasing counter, IDs may be added or removed at any time, therefore using
    // a map
    // numerical IDs are surely less cryptic than any other sort of identifier
    static std::map<int, bf::path> registeredAppImages;
    static int counter;

    // time of creation of the instance
    // used to display atimes/mtimes of associated directories and the mountpoint
    static const int timeOfCreation;

public:
    PrivateData() {
        mountpoint = generateMountpointPath();

        // make sure new instances free old resources (terminating an old instance) and recreate everything from scratch
        // TODO: disables existing instance check
        // TODO: simple string concatenation = super evil
        system((std::string("fusermount -u ") + mountpoint).c_str());
        system((std::string("rmdir ") + mountpoint).c_str());

        // create mappings for all AppImages in ~/Applications, which are most used
        // TODO: allow "registration" of AppImages in any directory
        auto applicationsDir = std::string(getenv("HOME")) + "/Applications";
        for (bf::directory_iterator it(applicationsDir); it != bf::directory_iterator(); ++it) {
            auto path = it->path();
            auto filename = path.filename();

            if (!bf::is_regular_file(path))
                continue;

            // TODO: implement check whether file is an AppImage

            registeredAppImages[counter++] = path;
        }
    }

private:
    static std::string generateMountpointPath() {
        return std::string("/run/user/") + std::to_string(getuid()) + "/appimagelauncherfs/";
    }

    static std::string generateFilenameForId(int id) {
        std::ostringstream filename;
        filename << std::setfill('0') << std::setw(4) << id << ".AppImage";
        return filename.str();
    }

    static std::string generateTextMap() {
        std::vector<char> map(1, '\0');

        for (const auto& entry : registeredAppImages) {
            auto filename = generateFilenameForId(entry.first);

            std::ostringstream line;
            line << filename << " -> " << entry.second.string() << std::endl;
            auto lineStr = line.str();

            map.resize(map.size() + lineStr.size());
            strcat(map.data(), lineStr.c_str());
        }

        return map.data();
    }

public:
    bool otherInstanceRunning() const {
        // TODO: implement properly (as in, check for stale mountpoint)
        return bf::is_directory(mountpoint);
    }

    static bf::path mapPathToOriginalPath(const std::string& path) {
        std::vector<char> mutablePath(path.size() + 1);
        strcpy(mutablePath.data(), path.c_str());
        auto mutablePathPtr = mutablePath.data();

        char* firstPart = strsep(&mutablePathPtr, ".");
        // skip leading slash
        firstPart++;

        int id;
        try {
            id = std::stoi(firstPart);
        } catch (const std::invalid_argument&) {
            return "";
        }

        // check if filename matches the one we'd generate for parsed id
        // that'll make sure only the listed files in the used scheme are covered by this function
        if (path != ("/" + generateFilenameForId(id)))
            return "";

        return registeredAppImages[id];
    }

    static int getattr(const char* path, struct stat* st) {
        // right now we only handle root
        if (strcmp(path, "/") == 0) {
            st->st_atim = timespec{timeOfCreation, 0};
            st->st_mtim = timespec{timeOfCreation, 0};

            st->st_gid = getgid();
            st->st_uid = getuid();

            st->st_mode = S_IFDIR | mode;

            return 0;
        }

        if (strcmp(path, "/map") == 0) {
            st->st_atim = timespec{timeOfCreation, 0};
            st->st_mtim = timespec{timeOfCreation, 0};

            st->st_gid = getgid();
            st->st_uid = getuid();

            st->st_mode = S_IFREG | 0444;

            auto map = generateTextMap();
            st->st_size = map.size();

            return 0;
        }

        auto originalPath = mapPathToOriginalPath(path);

        if (originalPath.empty() || !bf::is_regular_file(originalPath)) {
            return -1;
        }

        stat(originalPath.c_str(), st);

        // overwrite permissions: read-only executable
        st->st_mode = S_IFREG | 0555;
        return 0;
    }

    static int readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi) {
        filler(buf, ".", nullptr, 0);
        filler(buf, "..", nullptr, 0);
        filler(buf, "map", nullptr, 0);

        for (const auto& entry : PrivateData::registeredAppImages) {
            auto filename = generateFilenameForId(entry.first);
            filler(buf, filename.c_str(), nullptr, 0);
        }

        return 0;
    }

    static int read(const char* path, char* buf, size_t bufsize, off_t offset, struct fuse_file_info* fi) {
        if (strcmp(path, "/map") == 0) {
            auto map = generateTextMap();

            // cannot request more bytes than the file size
            if (offset > map.size())
                return -1;

            int bytesToCopy = (int) std::min(bufsize, map.size() - offset);
            memcpy(buf, map.data(), (size_t) bytesToCopy);
            return bytesToCopy;
        }

        auto originalPath = mapPathToOriginalPath(path);
        auto* f = fopen(originalPath.c_str(), "r");

        if (f == nullptr)
            return -1;

        // fetch size of file for error treatment
        ::fseek(f, 0, SEEK_END);
        auto fullSize = ::ftell(f);
        ::rewind(f);

        // cannot request more bytes than the file size
        if (offset > fullSize)
            return -1;

        ::fseek(f, offset, SEEK_SET);
        auto bytesRead = ::fread(buf, sizeof(char), std::min((size_t) bufsize, (size_t) (fullSize - offset)), f);

        // patch out magic bytes if necessary
        constexpr auto magicBytesBegin = 8;
        constexpr auto magicBytesEnd = 10;
        if (offset <= magicBytesEnd) {
            auto beg = magicBytesBegin - offset;
            auto count = std::min(std::min((size_t) (magicBytesEnd - offset), (size_t) 2), bufsize) + 1;
            memset(buf + beg, '\x00', count);
        }

        if (::fclose(f) != 0)
            return -1;

        return static_cast<int>(bytesRead);
    };

};

int AppImageLauncherFS::PrivateData::counter = 0;
const int AppImageLauncherFS::PrivateData::timeOfCreation = time(NULL);
std::map<int, bf::path> AppImageLauncherFS::PrivateData::registeredAppImages;

AppImageLauncherFS::AppImageLauncherFS() : d(std::make_shared<PrivateData>()) {}

std::shared_ptr<struct fuse_operations> AppImageLauncherFS::operations() {
    auto ops = std::make_shared<struct fuse_operations>();

    // available functionality
    ops->getattr = d->getattr;
    ops->read = d->read;
    ops->readdir = d->readdir;

    return ops;
}

std::string AppImageLauncherFS::mountpoint() {
    return d->mountpoint;
}

int AppImageLauncherFS::run() {
    // create FUSE state instance
    // the filesystem object encapsules all the functionality, and can generate a FUSE operations struct for us
    auto fuseOps = operations();

    // create fake args containing future mountpoint
    std::vector<char*> args;

    auto mp = mountpoint();

    // check whether another instance is running
    if (d->otherInstanceRunning())
        throw AlreadyRunningError("");

    // make sure mountpoint dir exists over lifetime of this object
    bf::create_directories(mp);
    bf::permissions(mp, static_cast<bf::perms>(d->mountpointMode));

    // we need a normal char pointer
    std::vector<char> mpBuf(mp.size() + 1, '\0');
    strcpy(mpBuf.data(), mp.c_str());

    // path to this binary is none of FUSE's concern
    args.push_back("");
    args.push_back(mpBuf.data());

    // force foreground mode
    args.push_back("-f");

    // "sort of debug mode"
    if (getenv("DEBUG") != nullptr) {
        // disable multithreading for better debugging
        args.push_back("-s");

        // enable debug output (implies f)
        args.push_back("-d");
    }

    int fuse_stat = fuse_main(static_cast<int>(args.size()), args.data(), fuseOps.get(), this);

    return fuse_stat;
}

std::shared_ptr<AppImageLauncherFS> AppImageLauncherFS::instance = nullptr;

std::shared_ptr<AppImageLauncherFS> AppImageLauncherFS::getInstance() {
    if (instance == nullptr)
        instance = std::shared_ptr<AppImageLauncherFS>(new AppImageLauncherFS);

    return instance;
}
