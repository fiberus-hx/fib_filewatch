# fib_filewatch

Recursive filesystem watching library for the Fiberus runtime. Provides Haxe bindings to a plain C implementation using Linux `inotify(7)` with `IN_NONBLOCK` for non-blocking event polling. Adapted from [linc_filewatch](https://github.com/snowkit/linc_filewatch) (snowkit, MIT license), rewritten from C++ to plain C for the Fiberus FFI.

## Architecture

```
Haxe code -> filewatch.Filewatch (@:native extern, @:gcBlocking)
          -> fib_filewatch.c (plain C, inotify-based)
          -> Linux inotify(7) with IN_NONBLOCK
```

The watcher uses a polling model: call `update()` each frame/tick to drain pending inotify events. The callback fires synchronously during `update()`. Directories are watched recursively -- new subdirectories are automatically added to the watch set.

## Directory Structure

```
fib_filewatch/
  lib/
    fib_filewatch.c        C implementation (inotify, recursive directory tracking)
    fib_filewatch.h        Public C API
  src/filewatch/
    Filewatch.hx           Haxe extern class (init, addWatch, removeWatch, update, shutdown)
  test/
    TestMain.hx            Test entry point
    TestFilewatch.hx       Test suite (5 test cases)
    Test.hx                Base test class
    TestFramework.hx       Lightweight assertion framework
    compile.hxml           Haxe compiler flags
    Makefile               Build targets
  reference/
    linc_filewatch/        Original upstream C++ reference (MIT license)
```

## Usage

```haxe
import filewatch.Filewatch;

// Initialize with a callback
Filewatch.init((type, rootPath, relPath) -> {
    trace('Event: $type $rootPath/$relPath');
});

// Add a directory to watch recursively
Filewatch.addWatch("/path/to/watch");

// In your main loop: poll for events (non-blocking)
Filewatch.update();

// When done:
Filewatch.shutdown();
```

## API

### filewatch.Filewatch

| Method | Description |
|--------|-------------|
| `init(callback:(Int, String, String) -> Void):Bool` | Initialize with event callback. Returns true on success. |
| `addWatch(path:String):Void` | Add a directory to watch recursively. Resolves to absolute path. |
| `removeWatch(path:String):Bool` | Remove a watched directory. Returns true if found. |
| `update():Void` | Poll for events (non-blocking). Fires callback for each pending event. |
| `shutdown():Void` | Shut down and free all resources. |

### Event Types

The callback receives `(eventType:Int, rootPath:String, relativePath:String)`:

| Value | Constant | Description |
|-------|----------|-------------|
| 0 | `FIB_FW_UNKNOWN` | Unknown event |
| 1 | `FIB_FW_MODIFY` | File modified |
| 2 | `FIB_FW_REMOVE` | File deleted |
| 3 | `FIB_FW_CREATE` | File created |
| 4 | `FIB_FW_RENAMED_OLD` | File renamed (old name) |
| 5 | `FIB_FW_RENAMED_NEW` | File renamed (new name) |

## Building and Testing

```bash
# From libraries/fib_filewatch/test/

make debug          # Debug build with FIBERUS_DEBUG
make release        # Release build
make run            # Run tests
make clean          # Remove build artifacts
make cache_clear    # Clear cached .a files
```

The test suite covers: init/shutdown lifecycle, file creation detection, file modification detection, callback string argument correctness, and multiple update cycles without duplicate events.

## Platform

Linux only. Uses `inotify(7)` with `IN_NONBLOCK`. The inotify watch mask includes `IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE_SELF`. Queue overflow triggers automatic reinitialization of all watches.

## Memory Model

- Internal watch entries and root paths are malloc-allocated and freed by `shutdown()`.
- The inotify file descriptor is opened with `inotify_init1(IN_NONBLOCK)` and closed on shutdown.
- All functions are marked `@:gcBlocking` to enter the GC-free zone during native calls.
