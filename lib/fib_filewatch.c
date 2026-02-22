/*
 * fib_filewatch.c - Plain C recursive inotify file watcher implementation
 *
 * Adapted from recursive_inotify.hpp for the Fiberus FFI.
 * Uses Linux inotify(7) with IN_NONBLOCK for non-blocking polling.
 *
 * Internal data structures:
 * - watch_entry: maps inotify watch descriptor to directory path
 * - Stored in a simple dynamic array (watches grow linearly, rarely exceed ~1000)
 * - Root paths tracked separately for relative path computation
 */

#include "fib_filewatch.h"

#include <sys/inotify.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* ============================================================================
 * Internal Data Structures
 * ============================================================================ */

/* Map an inotify watch descriptor to a directory path */
typedef struct {
    int wd;
    char path[PATH_MAX];
} watch_entry;

/* Dynamic array of watch entries */
static watch_entry* g_watches = NULL;
static int g_watch_count = 0;
static int g_watch_cap = 0;

/* Root paths (directories passed to add_watch) */
static char** g_roots = NULL;
static int g_root_count = 0;
static int g_root_cap = 0;

/* Inotify file descriptor */
static int g_inotify_fd = -1;

/* User callback */
static FibFilewatchCallback g_callback = NULL;

/* Event buffer */
#define EVENT_BUF_LEN (1024 * (sizeof(struct inotify_event) + 16))

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/* Find a watch entry by watch descriptor. Returns index or -1. */
static int find_watch_by_wd(int wd) {
    for (int i = 0; i < g_watch_count; i++) {
        if (g_watches[i].wd == wd) return i;
    }
    return -1;
}

/* Find a watch entry by path. Returns index or -1. */
static int find_watch_by_path(const char* path) {
    for (int i = 0; i < g_watch_count; i++) {
        if (strcmp(g_watches[i].path, path) == 0) return i;
    }
    return -1;
}

/* Add a watch entry */
static void add_watch_entry(int wd, const char* path) {
    if (find_watch_by_wd(wd) >= 0 || find_watch_by_path(path) >= 0) return;

    if (g_watch_count >= g_watch_cap) {
        int new_cap = g_watch_cap == 0 ? 64 : g_watch_cap * 2;
        watch_entry* new_arr = (watch_entry*)realloc(g_watches, new_cap * sizeof(watch_entry));
        if (!new_arr) return;
        g_watches = new_arr;
        g_watch_cap = new_cap;
    }

    g_watches[g_watch_count].wd = wd;
    strncpy(g_watches[g_watch_count].path, path, PATH_MAX - 1);
    g_watches[g_watch_count].path[PATH_MAX - 1] = '\0';
    g_watch_count++;
}

/* Remove a watch entry by index (swap with last) */
static void remove_watch_entry(int idx) {
    if (idx < 0 || idx >= g_watch_count) return;
    inotify_rm_watch(g_inotify_fd, g_watches[idx].wd);
    g_watches[idx] = g_watches[g_watch_count - 1];
    g_watch_count--;
}

/* Remove all watches whose path starts with prefix */
static void zap_subtree(const char* dir) {
    size_t dir_len = strlen(dir);
    for (int i = g_watch_count - 1; i >= 0; i--) {
        const char* p = g_watches[i].path;
        if (strcmp(p, dir) == 0 ||
            (strncmp(p, dir, dir_len) == 0 && p[dir_len] == '/')) {
            inotify_rm_watch(g_inotify_fd, g_watches[i].wd);
            g_watches[i] = g_watches[g_watch_count - 1];
            g_watch_count--;
        }
    }
}

/* Add a root path */
static void add_root(const char* path) {
    /* Check if already in roots */
    for (int i = 0; i < g_root_count; i++) {
        if (strcmp(g_roots[i], path) == 0) return;
    }

    if (g_root_count >= g_root_cap) {
        int new_cap = g_root_cap == 0 ? 8 : g_root_cap * 2;
        char** new_arr = (char**)realloc(g_roots, new_cap * sizeof(char*));
        if (!new_arr) return;
        g_roots = new_arr;
        g_root_cap = new_cap;
    }

    g_roots[g_root_count] = strdup(path);
    g_root_count++;
}

/* Remove a root path */
static void remove_root(const char* path) {
    for (int i = 0; i < g_root_count; i++) {
        if (strcmp(g_roots[i], path) == 0) {
            free(g_roots[i]);
            g_roots[i] = g_roots[g_root_count - 1];
            g_root_count--;
            return;
        }
    }
}

/* Check if path is a root */
static int is_root(const char* path) {
    for (int i = 0; i < g_root_count; i++) {
        if (strcmp(g_roots[i], path) == 0) return 1;
    }
    return 0;
}

/* Find the root path for a given directory. Returns root index or -1. */
static int find_root_for(const char* dir) {
    for (int i = 0; i < g_root_count; i++) {
        size_t root_len = strlen(g_roots[i]);
        if (strcmp(dir, g_roots[i]) == 0 ||
            (strncmp(dir, g_roots[i], root_len) == 0 && dir[root_len] == '/')) {
            return i;
        }
    }
    return -1;
}

/* Compute relative path from root to full_path.
 * Writes result into rel_buf (must be PATH_MAX). */
static void compute_relative(const char* root, const char* dir, const char* filename, char* rel_buf) {
    size_t root_len = strlen(root);
    const char* rel_dir = dir + root_len;
    if (*rel_dir == '/') rel_dir++;

    if (*rel_dir == '\0') {
        /* File is directly in the root dir */
        strncpy(rel_buf, filename, PATH_MAX - 1);
    } else {
        snprintf(rel_buf, PATH_MAX, "%s/%s", rel_dir, filename);
    }
    rel_buf[PATH_MAX - 1] = '\0';
}

/* Add an inotify watch for a single directory */
static void add_inotify_watch(const char* path, int is_root_path) {
    uint32_t mask = IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE_SELF;
    if (is_root_path) mask |= IN_MOVE_SELF;

    int wd = inotify_add_watch(g_inotify_fd, path, mask);
    if (wd == -1) {
        if (errno != ENOENT) {
            /* Silently ignore ENOENT (race condition: dir deleted between readdir and watch) */
#ifdef FIBERUS_DEBUG
            fprintf(stderr, "[FILEWATCH] inotify_add_watch(%s): %s\n", path, strerror(errno));
#endif
        }
        return;
    }

    add_watch_entry(wd, path);
}

/* Recursively add watches for a directory tree */
static void recursive_add_watches(const char* dir, int is_root_path) {
    add_inotify_watch(dir, is_root_path);

    DIR* dp = opendir(dir);
    if (!dp) return;

    struct dirent* entry;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.' &&
            (entry->d_name[1] == '\0' ||
             (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue; /* Skip . and .. */
        }

        char child_path[PATH_MAX];
        snprintf(child_path, PATH_MAX, "%s/%s", dir, entry->d_name);

        struct stat st;
        if (stat(child_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            recursive_add_watches(child_path, 0);
        }
    }

    closedir(dp);
}

/* Fire the callback for an event */
static void fire_callback(int event_type, const char* dir, const char* filename) {
    if (!g_callback) return;

    int root_idx = find_root_for(dir);
    if (root_idx < 0) return; /* No root found — shouldn't happen */

    char rel_buf[PATH_MAX];
    compute_relative(g_roots[root_idx], dir, filename, rel_buf);

    g_callback(event_type, g_roots[root_idx], rel_buf);
}

/* Process a single inotify event. Returns bytes consumed. */
static size_t process_event(char* ptr, char* end) {
    struct inotify_event* ev = (struct inotify_event*)ptr;
    size_t len = sizeof(struct inotify_event) + ev->len;

    /* Queue overflow */
    if (ev->wd == -1) {
        if (ev->mask & IN_Q_OVERFLOW) {
#ifdef FIBERUS_DEBUG
            fprintf(stderr, "[FILEWATCH] Queue overflow — reinitializing\n");
#endif
            /* Reinitialize: close and reopen inotify, re-add all roots */
            close(g_inotify_fd);
            g_inotify_fd = inotify_init1(IN_NONBLOCK);
            g_watch_count = 0;
            for (int i = 0; i < g_root_count; i++) {
                recursive_add_watches(g_roots[i], 1);
            }
        }
        return len;
    }

    int idx = find_watch_by_wd(ev->wd);
    if (idx < 0) return len; /* Unknown/stale watch */

    const char* dir = g_watches[idx].path;

    /* IN_IGNORED: watch was removed (explicitly or because file was deleted) */
    if (ev->mask & IN_IGNORED) return len;

    /* IN_DELETE_SELF: the watched directory itself was deleted */
    if (ev->mask & IN_DELETE_SELF) {
        if (is_root(dir)) remove_root(dir);
        remove_watch_entry(idx);
        return len;
    }

    /* IN_MOVE_SELF: root directory was moved */
    if (ev->mask & IN_MOVE_SELF) {
        return len; /* Ignore for now; could reinitialize */
    }

    /* Build the full path of the affected entry */
    char full_path[PATH_MAX];
    snprintf(full_path, PATH_MAX, "%s/%s", dir, ev->name);

    /* IN_DELETE: file or directory deleted */
    if (ev->mask & IN_DELETE) {
        if (ev->mask & IN_ISDIR) {
            if (is_root(full_path)) remove_root(full_path);
            zap_subtree(full_path);
        } else {
            fire_callback(FIB_FW_REMOVE, dir, ev->name);
        }
        return len;
    }

    /* IN_MODIFY: file modified */
    if (ev->mask & IN_MODIFY) {
        if (!(ev->mask & IN_ISDIR)) {
            fire_callback(FIB_FW_MODIFY, dir, ev->name);
        }
        return len;
    }

    /* IN_MOVED_FROM: file/dir moved away (treat as delete unless paired with MOVED_TO) */
    if (ev->mask & IN_MOVED_FROM) {
        if (ev->mask & IN_ISDIR) {
            /* Check for paired MOVED_TO (rename within tree) */
            char* next_ptr = ptr + len;
            if (next_ptr < end) {
                struct inotify_event* next_ev = (struct inotify_event*)next_ptr;
                if ((next_ev->mask & IN_MOVED_TO) && next_ev->cookie == ev->cookie) {
                    /* Paired rename — update internal state */
                    int next_idx = find_watch_by_wd(next_ev->wd);
                    if (next_idx >= 0) {
                        /* This is a rename within the watched tree — just update paths */
                        /* For simplicity, zap and re-add */
                        char new_full[PATH_MAX];
                        snprintf(new_full, PATH_MAX, "%s/%s",
                                 g_watches[next_idx].path, next_ev->name);
                        zap_subtree(full_path);
                        recursive_add_watches(new_full, 0);
                    }
                    return len + sizeof(struct inotify_event) + next_ev->len;
                }
            }
            /* Unpaired: moved out of tree */
            if (is_root(full_path)) remove_root(full_path);
            zap_subtree(full_path);
        } else {
            fire_callback(FIB_FW_REMOVE, dir, ev->name);
        }
        return len;
    }

    /* IN_MOVED_TO: file/dir moved in (treat as create) */
    if (ev->mask & IN_MOVED_TO) {
        if (ev->mask & IN_ISDIR) {
            recursive_add_watches(full_path, 0);
        } else {
            fire_callback(FIB_FW_CREATE, dir, ev->name);
        }
        return len;
    }

    /* IN_CREATE: file/dir created */
    if (ev->mask & IN_CREATE) {
        if (ev->mask & IN_ISDIR) {
            recursive_add_watches(full_path, 0);
        } else {
            fire_callback(FIB_FW_CREATE, dir, ev->name);
        }
        return len;
    }

    return len;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

bool fib_filewatch_init(FibFilewatchCallback callback) {
    if (g_inotify_fd >= 0) return true; /* Already initialized */

    g_inotify_fd = inotify_init1(IN_NONBLOCK);
    if (g_inotify_fd == -1) {
        fprintf(stderr, "[FILEWATCH] inotify_init1 failed: %s\n", strerror(errno));
        return false;
    }

    g_callback = callback;
    return true;
}

void fib_filewatch_add_watch(const char* path) {
    if (g_inotify_fd < 0) return;

    /* Resolve to absolute path */
    char abs_path[PATH_MAX];
    if (realpath(path, abs_path) == NULL) {
        fprintf(stderr, "[FILEWATCH] realpath(%s) failed: %s\n", path, strerror(errno));
        return;
    }

    add_root(abs_path);
    recursive_add_watches(abs_path, 1);
}

bool fib_filewatch_remove_watch(const char* path) {
    if (g_inotify_fd < 0) return false;

    char abs_path[PATH_MAX];
    if (realpath(path, abs_path) == NULL) {
        strncpy(abs_path, path, PATH_MAX - 1);
        abs_path[PATH_MAX - 1] = '\0';
    }

    int was_root = is_root(abs_path);
    if (was_root) {
        zap_subtree(abs_path);
        remove_root(abs_path);
        return true;
    }
    return false;
}

void fib_filewatch_update(void) {
    if (g_inotify_fd < 0) return;

    char buf[EVENT_BUF_LEN];
    ssize_t num_read = read(g_inotify_fd, buf, EVENT_BUF_LEN);
    if (num_read <= 0) return;

    char* ptr = buf;
    char* end = buf + num_read;
    while (ptr < end) {
        size_t advance = process_event(ptr, end);
        if (advance == 0) break; /* Safety: avoid infinite loop */
        ptr += advance;
    }
}

void fib_filewatch_shutdown(void) {
    /* Remove all inotify watches */
    for (int i = 0; i < g_watch_count; i++) {
        inotify_rm_watch(g_inotify_fd, g_watches[i].wd);
    }

    /* Free watch array */
    free(g_watches);
    g_watches = NULL;
    g_watch_count = 0;
    g_watch_cap = 0;

    /* Free root paths */
    for (int i = 0; i < g_root_count; i++) {
        free(g_roots[i]);
    }
    free(g_roots);
    g_roots = NULL;
    g_root_count = 0;
    g_root_cap = 0;

    /* Close inotify fd */
    if (g_inotify_fd >= 0) {
        close(g_inotify_fd);
        g_inotify_fd = -1;
    }

    g_callback = NULL;
}
