/*
 * fib_filewatch.h - Plain C recursive inotify file watcher
 *
 * Adapted from recursive_inotify.hpp for the Fiberus FFI.
 * Uses Linux inotify(7) with IN_NONBLOCK for non-blocking polling.
 *
 * Usage:
 *   fib_filewatch_init(callback)     — register the event callback
 *   fib_filewatch_add_watch(path)    — add a directory to watch recursively
 *   fib_filewatch_remove_watch(path) — remove a watched directory
 *   fib_filewatch_update()           — poll for events (non-blocking)
 *   fib_filewatch_shutdown()         — clean up all resources
 */

#ifndef FIB_FILEWATCH_H
#define FIB_FILEWATCH_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Event types matching the linc_filewatch convention */
typedef enum {
    FIB_FW_UNKNOWN     = 0,
    FIB_FW_MODIFY      = 1,
    FIB_FW_REMOVE      = 2,
    FIB_FW_CREATE      = 3,
    FIB_FW_RENAMED_OLD = 4,
    FIB_FW_RENAMED_NEW = 5,
} FibFilewatchEvent;

/*
 * Callback signature:
 *   type:     FibFilewatchEvent value
 *   root:     the root watch path (absolute)
 *   rel_path: relative path from root to the changed file
 */
typedef void (*FibFilewatchCallback)(int type, const char* root, const char* rel_path);

/*
 * fib_filewatch_init - Initialize the file watcher and register the callback.
 *
 * Must be called once before any other fib_filewatch_* function.
 * Returns true on success, false on failure (e.g., inotify_init failed).
 */
bool fib_filewatch_init(FibFilewatchCallback callback);

/*
 * fib_filewatch_add_watch - Add a directory to watch recursively.
 *
 * path: Absolute path to a directory. The watcher will recursively watch
 *       all subdirectories. Relative paths are resolved to absolute.
 */
void fib_filewatch_add_watch(const char* path);

/*
 * fib_filewatch_remove_watch - Remove a watched directory.
 *
 * path: The path that was previously passed to fib_filewatch_add_watch.
 * Returns true if the path was found and removed, false otherwise.
 */
bool fib_filewatch_remove_watch(const char* path);

/*
 * fib_filewatch_update - Poll for file system events (non-blocking).
 *
 * Reads pending inotify events and fires the callback for each.
 * Must be called periodically (e.g., each frame or tick).
 * Returns immediately if no events are pending.
 */
void fib_filewatch_update(void);

/*
 * fib_filewatch_shutdown - Shut down the file watcher and free all resources.
 *
 * Removes all watches, closes the inotify file descriptor, and frees
 * all internal data structures. The callback will not be called after this.
 */
void fib_filewatch_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* FIB_FILEWATCH_H */
