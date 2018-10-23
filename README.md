# AppImageLauncherFS

Experimental FUSE filesystem for use with AppImageLauncher.


## About

AppImageLauncherFS is a FUSE filesystem. Its virtual files map to real AppImages on the system. However, AppImageLauncherFS automatically replaces the magic bytes in AppImages with null bytes. This way, AppImages seem like regular ELF files to the system.

Now, you might correctly ask "what is that good for?". As you may or may not know, AppImageLauncher registers itself as "entry point" for all AppImages, using those magic bytes to make the system recognize AppImages and call AppImageLauncher on them on execution using a technology called binfmt_misc. That works very well until the point where AppImageLauncher would like to launch the AppImage itself. If it'd try to "just call the AppImage", the system would of course launch AppImageLauncher again, as it has been told to do so, resulting in an infinite loop of AppImageLauncher windows. Hence, to actually run an AppImage after setting up binfmt_misc, one needs to bypass this integration. And to accomplish that, the trigger for binfmt_misc must be eliminated: the magic bytes.

So far, the techniques for bypassing binfmt_misc involve using an external runtime to run the AppImage bundled with AppImageLauncher, copying the AppImage into a temporary directory and patching out the magic bytes, etc. The former will always mean we're using custom and outdated code to launch the AppImages (possibly missing special runtime features in the embedded runtime of the AppImage), the latter is anything but efficient and generates a lot of file I/O operations.

Ever since, there was the idea to use some sort of "virtual files" or FIFOs or anything like that where a process could intercept the file reads and replace the magic bytes by zeroes on the fly. After looking for alteratives without success, FUSE was chosen to implement such a system.


## Usage

AppImageLauncherFS is alpha grade software. It does work, but you should expect bugs.

AppImageLauncherFS maps virtual files to real AppImages. By writing paths into a virtual file called `register`, new files can be associated with the filesystem: the file is added to the internal map, and a new virtual file is created immediately. The path can be read from a virtual file called `map`. By default, all AppImages in `~/Applications` will be registered once on startup.

The new filesystem will be mounted automatically in `/run/user/<numeric user ID>/appimagelauncherfs`, similar to how other virtual filesystems are mounted.
