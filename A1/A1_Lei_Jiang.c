/*
 * A1_Lei_Jiang.c
 * COMP 8567 - Assignment 1
 *
 * Traverse directory tree using nftw() (ftw.h)
 *
 * 10 functions:
 *  -flist dir
 *  -tcount ext1 [ext2] [ext3] dir
 *  -srchf filename root_dir
 *  -dircnt root_dir
 *  -sumfilesize root_dir
 *  -lfsize dir
 *  -nonwr dir
 *  -copyd source_dir destination_dir
 *  -dmove source_dir destination_dir
 *  -remd root_dir file_extension
 *
 */

#define _XOPEN_SOURCE 700
#include <ftw.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <limits.h>

/**
 * @brief  Print system error message and exit (EXIT_FAILURE).
 *
 * @param  msg  Prompt string for perror.
 *
 * @return This function does not return (calls exit).
 */
static void die(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

/**
 * @brief  Print custom error message and exit (EXIT_FAILURE).
 *
 * @param  msg  Error text (does not append strerror).
 *
 * @return This function does not return (calls exit).
 */
static void die_msg(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(EXIT_FAILURE);
}

/**
 * @brief  Check whether an absolute path is under HOME (or equals HOME).
 *
 * @param  path_abs  Absolute path to check (prefer realpath first).
 * @param  home_abs  Absolute HOME path (after realpath).
 *
 * @return 1 if under HOME or equals HOME; 0 otherwise.
 *
 * @note   Uses prefix matching with boundary checks to avoid treating "/home/user2" as inside "/home/user".
 */
static int path_is_under_home(const char *path_abs, const char *home_abs)
{
    size_t hl = strlen(home_abs);
    if (hl == 0)
        return 0;

    if (strncmp(path_abs, home_abs, hl) != 0) // Compare first hl characters is different
        return 0;
    if (path_abs[hl] == '\0') /* The next of last character of path_abs is '\0', it means path_abs ends exactly at home_abs */
        return 1;
    if (home_abs[hl - 1] == '/') /* The prefix matches but the path continues, make sure the next character is '/'  */
        return 1;
    return path_abs[hl] == '/'; /* Avoid '/home/user2' is inside '/home/user' */
}

/**
 * @brief  Convert input path to canonical absolute path (realpath).
 *
 * @param  input  User input path (relative or absolute).
 *
 * @return On success returns malloc-allocated absolute path; on failure returns NULL.
 *
 * @warning Caller must free the returned string on success.
 */
static char *to_real_abs(const char *input)
{
    char *buf = (char *)malloc(PATH_MAX);
    if (!buf)
        die("malloc");
    if (!realpath(input, buf))
    {
        free(buf);
        return NULL;
    }
    return buf;
}

/**
 * @brief  Check whether a filename ends with the given extension (e.g., ".c", ".txt").
 *
 * @param  name  Filename or last component of a path.
 * @param  ext   Extension string (prefer including '.', e.g., ".txt").
 *
 * @return 1 if match; 0 otherwise.
 */
static int ends_with_ext(const char *name, const char *ext)
{
    if (!name || !ext)
        return 0;
    size_t nl = strlen(name), el = strlen(ext);
    if (el == 0 || nl < el)
        return 0;
    return strcmp(name + (nl - el), ext) == 0;
}

/**
 * @brief   Get the last component of a path without modifying the original string.
 *
 * @param  path  Path string.
 *
 * @return Pointer to a substring inside path (a basename view).
 *
 * @note   The returned pointer points inside the original string and should not be freed.
 */
static const char *base_name_view(const char *path)
{
    const char *p = strrchr(path, '/');
    return p ? (p + 1) : path;
}

/* Structure for collecting file information and dynamic array container */
typedef struct
{
    char *path;  /* Absolute path */
    char *bname; /* Used for comparison (e.g., -lfsize) */
    off_t size;
    time_t t; /* Timestamp, st_mtime is easy to get from stat, so it’s a reliable choice for sorting */
} Item;

/* Dynamic array container */
typedef struct
{
    Item *a;
    size_t n;
    size_t cap;
} ItemVec;

/**
 * @brief  Initialize the dynamic array container.
 *
 * @param  v  ItemVec pointer.
 *
 * @return None.
 */
static void vec_init(ItemVec *v)
{
    v->a = NULL;
    v->n = 0;
    v->cap = 0;
}

/**
 * @brief  Append an element to the dynamic array (auto-grow when needed).
 *
 * @param  v   ItemVec pointer.
 * @param  it  Element to append (struct is copied by value).
 *
 * @return None (die on failure).
 *
 * @warning Pointer members in it should already be allocated (e.g., via strdup) and will be freed by vec_free.
 */
static void vec_push(ItemVec *v, const Item *it)
{
    if (v->n == v->cap)
    {
        size_t newcap = (v->cap == 0) ? 32 : v->cap * 2;
        Item *na = (Item *)realloc(v->a, newcap * sizeof(Item));
        if (!na)
            die("realloc");
        v->a = na;
        v->cap = newcap;
    }
    v->a[v->n++] = *it;
}

/**
 * @brief  Free the dynamic array and heap memory owned by its elements.
 *
 * @param  v  ItemVec pointer.
 *
 * @return None.
 *
 * @note   Frees Item.path and Item.bname (if non-NULL).
 */
static void vec_free(ItemVec *v)
{
    for (size_t i = 0; i < v->n; i++)
    {
        free(v->a[i].path);
        free(v->a[i].bname);
    }
    free(v->a);
    v->a = NULL;
    v->n = 0;
    v->cap = 0;
}

/* Global context for nftw callbacks, configured in main() based on the selected mode and arguments. */
typedef enum
{
    M_NONE = 0,
    M_FLIST,
    M_TCOUNT,
    M_SRCHF,
    M_DIRCNT,
    M_SUMFILESIZE,
    M_LFSIZE,
    M_NONWR,
    M_COPYD,
    M_DMOVE,
    M_REMD,
    M_DMOVE_DELETE_ONLY
} Mode;

typedef struct
{
    Mode mode;

    /* Common: absolute root path (nftw starting point) */
    const char *root_abs;

    /* -flist / -lfsize / -nonwr need to collect items */
    ItemVec items;

    /* -tcount extensions */
    const char *exts[3];
    int extn;
    long counts[3];

    /* -srchf */
    const char *target_name;
    int found_any;

    /* -dircnt */
    long dir_count;

    /* -sumfilesize */
    long long total_bytes;

    /* -remd */
    const char *rem_ext;
    long removed_files;

    /* copy/move */
    const char *src_abs;
    const char *dst_abs;
    const char *src_base; /* last path component of source_dir */
    long copied_files;
    long copied_dirs;
    long copy_failures;

} Ctx;

static Ctx G; /* global context for nftw callbacks */

/**
 * @brief  Used by qsort: sort by time from newest to oldest; if the time is the same, sort by the path in alphabetical order.
 *
 * @param  p1  Pointer to an Item.
 * @param  p2  Pointer to an Item.
 *
 * @return qsort comparison result.
 */
static int cmp_flist(const void *p1, const void *p2)
{
    const Item *a = (const Item *)p1;
    const Item *b = (const Item *)p2;
    if (a->t > b->t)
        return -1;
    if (a->t < b->t)
        return 1;
    return strcmp(a->path, b->path);
}

/**
 * @brief  For qsort: sort by file size descending; tie-break by filename alphabetically.
 *
 * @param  p1  Pointer to Item.
 * @param  p2  Pointer to Item.
 *
 * @return qsort comparison result.
 */
static int cmp_lfsize(const void *p1, const void *p2)
{
    const Item *a = (const Item *)p1;
    const Item *b = (const Item *)p2;
    if (a->size > b->size)
        return -1;
    if (a->size < b->size)
        return 1;
    return strcmp(a->bname, b->bname);
}

/**
 * @brief  Compare function for qsort by path in alphabetical order
 *
 * @param  p1  Pointer to Item.
 * @param  p2  Pointer to Item.
 *
 * @return qsort comparison result.
 */
static int cmp_path_alpha(const void *p1, const void *p2)
{
    const Item *a = (const Item *)p1;
    const Item *b = (const Item *)p2;
    return strcmp(a->path, b->path);
}

/**
 * @brief  Recursively create directories (like mkdir -p).
 *
 * @param  dirpath  Directory path to create.
 * @param  mode     Permissions for mkdir (e.g., 0755).
 *
 * @return 0 on success; -1 on failure (errno set).
 *
 * @note   If directory already exists (EEXIST), treat as success.
 */
static int mkdirs_for_path(const char *dirpath, mode_t mode)
{
    char tmp[PATH_MAX];
    if (strlen(dirpath) >= sizeof(tmp))
        return -1;
    strcpy(tmp, dirpath); // copy dirpath to tmp, ensuring it fits within PATH_MAX

    /* Remove trailing slashes */
    size_t len = strlen(tmp);
    while (len > 1 && tmp[len - 1] == '/') // If the last character of tmp is '/', remove it by setting it to '\0' and decreasing len
        tmp[--len] = '\0';

    for (char *p = tmp + 1; *p; p++)
    {
        if (*p == '/') // When we encounter a '/', we temporarily replace it with '\0' to create a substring for mkdir
        {
            *p = '\0';
            if (mkdir(tmp, mode) != 0) // Try to create the directory specified by tmp. If it fails and the error is not EEXIST, return -1 to indicate failure.
            {
                if (errno != EEXIST)
                    return -1;
            }
            *p = '/'; // Restore the '/' character after attempting to create the directory
        }
    }
    if (mkdir(tmp, mode) != 0) // Finally, attempt to create the full directory specified by tmp. If it fails and the error is not EEXIST, return -1 to indicate failure.
    {
        if (errno != EEXIST)
            return -1;
    }
    return 0;
}

/**
 * @brief  Copy a regular file's contents to destination (overwrite).
 *
 * @param  src   Source file path.
 * @param  dst   Destination file path.
 * @param  mode  Destination permissions (usually src mode & 0777).
 *
 * @return 0 on success; -1 on failure (errno set).
 *
 * @warning Ensure dst's parent directory exists before calling.
 */
static int copy_file_bytes(const char *src, const char *dst, mode_t mode)
{
    int in = open(src, O_RDONLY);
    if (in < 0)
        return -1;

    /* If dst already exists, overwrite it */
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (out < 0)
    {
        close(in);
        return -1;
    }

    char buf[64 * 1024];
    while (1)
    {
        ssize_t r = read(in, buf, sizeof(buf)); // Read up to sizeof(buf) bytes from the input file into buf
        if (r == 0)
            break;
        if (r < 0)
        {
            close(in);
            close(out);
            return -1;
        }

        ssize_t off = 0;
        while (off < r) // Write all bytes read to the output file
        {
            ssize_t w = write(out, buf + off, (size_t)(r - off)); // Write the bytes read to the output file
            if (w < 0)
            {
                close(in);
                close(out);
                return -1;
            }
            off += w; // Update the offset by the number of bytes written to continue writing any remaining bytes
        }
    }

    close(in);
    close(out);
    return 0;
}

/**
 * @brief  Build corresponding destination path from src_path (for copy/move).
 *
 * @param  src_path  Current source path (must be within source_dir subtree).
 * @param  dst_out   Output buffer for destination path.
 * @param  outsz     Output buffer size.
 *
 * @return 0 on success; -1 on failure (not in source tree or buffer too small).
 *
 * Example: src=/home/u/ch4/a/b.txt
 *      source_dir=/home/u/ch4
 *      dst=/home/u/ch9/backup/ch4/a/b.txt
 *
 * @note   Destination root = destination_dir + "/" + basename(source_dir).
 */
static int build_dst_path(const char *src_path, char *dst_out, size_t outsz)
{
    const char *src_root = G.src_abs;
    size_t rl = strlen(src_root);

    if (strncmp(src_path, src_root, rl) != 0) // check if first rl characters are equal
        return -1;
    // Now get the "relative path" part by skipping the src_root prefix
    // Example: src_root="/home/u/ch4", src_path="/home/u/ch4/a/b.txt"
    // Then rel points to "/a/b.txt"
    const char *rel = src_path + rl; /* The rel starts with "" or "/xxx" */
    if (rel[0] == '\0')              // If rel is empty, it means src_path is exactly the root folder itself
        rel = "";                    /* root directory itself => relative part is empty */

    char dst_root[PATH_MAX];
    // Build the destination root: destination_dir + "/" + basename(source_dir)
    // Example: dst_abs="/home/u/ch9/backup", src_base="ch4"
    // dst_root becomes "/home/u/ch9/backup/ch4"
    if (snprintf(dst_root, sizeof(dst_root), "%s/%s", G.dst_abs, G.src_base) >= (int)sizeof(dst_root))
        return -1; // if it doesn't fit, fail to avoid overflow

    // Combine dst_root + rel to get the final destination path
    // Example: dst_root="/home/u/ch9/backup/ch4" + rel="/a/b.txt"
    // dst_out becomes "/home/u/ch9/backup/ch4/a/b.txt"
    if (snprintf(dst_out, outsz, "%s%s", dst_root, rel) >= (int)outsz)
        return -1;

    return 0;
}

/**
 * @brief  Delete a path (unlink for files, rmdir for directories).
 *
 * @param  path      Path to delete.
 * @param  typeflag  Type flag from nftw (FTW_F / FTW_D / FTW_DP, etc.).
 *
 * @return 0 on success; -1 on failure (errno set).
 *
 * @note   Directory deletion requires empty dirs, so move deletion phase uses FTW_DEPTH (post-order).
 */
static int delete_one_path(const char *path, int typeflag)
{
    if (typeflag == FTW_F || typeflag == FTW_SL || typeflag == FTW_SLN) // regular file or symbolic link
    {
        if (unlink(path) != 0) // use unlink to try to delete the file
            return -1;
        return 0;
    }
    if (typeflag == FTW_D || typeflag == FTW_DP) // directory
    {
        if (rmdir(path) != 0) // use rmdir to try to delete the directory
            return -1;
        return 0;
    }
    /* Other types: ignore or treat as files, here we choose to ignore */
    return 0;
}

/**
 * @brief  Process only one level of a directory (using nftw), compatible with platforms without FTW_ACTIONRETVAL.
 *
 * @param  dir_path  Directory path (absolute).
 *
 * @return 0 on success; -1 on failure.
 *
 * @note   When FTW_ACTIONRETVAL is unavailable, call nftw per child entry and
 *         return non-zero on directories to stop subtree traversal, achieving one-level scanning.
 */
/**
 * @brief  nftw callback: perform counting/collection/copy/delete based on current mode (G.mode).
 *
 * @param  fpath    Current path (prefer absolute start so this is absolute too).
 * @param  sb       stat info (type/size/permissions/time).
 * @param  typeflag Type flag from nftw (FTW_F, FTW_D, FTW_DP, etc.).
 * @param  ftwbuf   Level info (ftwbuf->level), etc.
 *
 * @return 0 to continue; non-zero to stop traversal.
 *
 * @note   -flist / -tcount only handle level==1 files to scan one level without relying on FTW_SKIP_SUBTREE.
 */
static int cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    (void)sb; /* Not needed for some branches */

    switch (G.mode)
    {
    case M_FLIST:
    {
        /* Only look at one level: I only handle regular files where level == 1. */
        if (typeflag == FTW_F && ftwbuf && ftwbuf->level == 1 && sb && S_ISREG(sb->st_mode))
        {
            Item it;
            memset(&it, 0, sizeof(it)); /* Initialize the Item structure to zero */
            it.path = strdup(fpath);
            it.size = 0;
            it.t = sb->st_mtime;
            it.bname = NULL;
            if (!it.path)
                die("strdup");
            vec_push(&G.items, &it);
        }
        return 0; //  Other cases are ignored; no need to skip subtree
    }

    case M_TCOUNT:
    {
        /* Only look at one level: I only handle regular files where level == 1. */
        if (typeflag == FTW_F && ftwbuf && ftwbuf->level == 1 && sb && S_ISREG(sb->st_mode))
        {
            const char *bn = base_name_view(fpath); // get the base name of the file
            for (int i = 0; i < G.extn; i++)
            {
                if (ends_with_ext(bn, G.exts[i])) // use ends_with_ext to check if the base name ends with the given extension
                    G.counts[i]++;
            }
        }
        return 0;
    }

    case M_SRCHF:
    {
        /* Search for the specified filename and print its path */
        if (typeflag == FTW_F && sb && S_ISREG(sb->st_mode))
        {
            const char *bn = base_name_view(fpath);
            if (strcmp(bn, G.target_name) == 0) // Compare the base name with the target name
            {
                printf("%s\n", fpath);
                G.found_any = 1;
            }
        }
        return 0;
    }

    case M_DIRCNT:
    {
        if (typeflag == FTW_D || typeflag == FTW_DP) // check if the current path is a directory or a post-order directory
            G.dir_count++;
        return 0;
    }

    case M_SUMFILESIZE:
    {
        if (typeflag == FTW_F && sb && S_ISREG(sb->st_mode)) // check if the current path is a regular file, if so, add its size to total_bytes
        {
            G.total_bytes += (long long)sb->st_size; // accumulate file size to total_bytes
        }
        return 0;
    }

    case M_LFSIZE:
    {
        if (typeflag == FTW_F && sb && S_ISREG(sb->st_mode)) // check if the current path is a regular file
        {
            Item it;
            memset(&it, 0, sizeof(it)); // initialize the Item structure to zero
            it.path = strdup(fpath);    // copy the file path string and assign it to it.path
            it.size = sb->st_size;      // set the size of the item to the file size
            it.t = 0;
            it.bname = strdup(base_name_view(fpath)); // get the base name for comparison in sorting
            if (!it.path || !it.bname)
                die("strdup");
            vec_push(&G.items, &it);
        }
        return 0;
    }

    case M_NONWR:
    {
        if (typeflag == FTW_F && sb && S_ISREG(sb->st_mode)) // check if the current path is a regular file
        {
            /* access checks using the current user’s permissions, so it’s more accurate than only looking at the permission bits. */
            if (access(fpath, W_OK) != 0)
            {
                Item it;
                memset(&it, 0, sizeof(it)); // initialize the Item structure to zero
                it.path = strdup(fpath);    // copy the file path string and assign it to it.path
                it.bname = NULL;
                it.size = 0;
                it.t = 0;
                if (!it.path)
                    die("strdup");
                vec_push(&G.items, &it);
            }
        }
        return 0;
    }

    case M_COPYD:
    {
        char dst[PATH_MAX]; // define a buffer to hold the destination path
        if (build_dst_path(fpath, dst, sizeof(dst)) != 0)
        {
            G.copy_failures++;
            return 0;
        }

        if (typeflag == FTW_D)
        {
            /* Directory: ensure creation */
            mode_t mode = sb ? (sb->st_mode & 0777) : 0755;
            if (mkdirs_for_path(dst, mode) != 0)
            {
                /* Directory creation failure counts as failure, but continue */
                G.copy_failures++;
            }
            else
            {
                G.copied_dirs++;
            }
            return 0;
        }

        if (typeflag == FTW_F && sb && S_ISREG(sb->st_mode))
        {
            /* File: first create parent directory, then copy content */
            char parent[PATH_MAX];
            strncpy(parent, dst, sizeof(parent)); // use strncpy to copy dst to parent, ensuring not to overflow
            parent[sizeof(parent) - 1] = '\0';    // ensure null-termination
            char *slash = strrchr(parent, '/');   // find the last '/' in parent to separate the directory part
            if (slash)
            {
                *slash = '\0';
                if (mkdirs_for_path(parent, 0755) != 0) // Use mkdirs_for_path to create parent directories with mode 0755
                {
                    G.copy_failures++; // If directory creation fails, count as a failure and return
                    return 0;
                }
            }

            mode_t mode = sb->st_mode & 0777;           // get the file mode for the destination file
            if (copy_file_bytes(fpath, dst, mode) != 0) // Use copy_file_bytes to copy the file content from source to destination
            {
                G.copy_failures++; // If copying fails, count as a failure
            }
            else
            {
                G.copied_files++; // If copying succeeds, increment the copied files count
            }
            return 0;
        }

        /* Other types (symbolic links, device files, etc.) are skipped here */
        return 0;
    }

    case M_DMOVE_DELETE_ONLY:
    {
        /* Post-order traversal: delete files first, then directories, to avoid rmdir failure */
        if (delete_one_path(fpath, typeflag) != 0)
        {
            /* Continue even if deletion fails, try to delete as many as possible, but warn */
            fprintf(stderr, "WARN: failed to delete: %s (%s)\n", fpath, strerror(errno));
        }
        return 0;
    }

    case M_REMD:
    {
        if (typeflag == FTW_F && sb && S_ISREG(sb->st_mode)) // check if the current path is a regular file
        {
            const char *bn = base_name_view(fpath); // get the base name of the file
            if (ends_with_ext(bn, G.rem_ext))       // use ends_with_ext to check if the base name ends with the specified extension
            {
                if (unlink(fpath) == 0) // use unlink to try to delete the file
                {
                    G.removed_files++; // increment the count of removed files
                }
                else
                {
                    fprintf(stderr, "WARN: cannot remove %s: %s\n", fpath, strerror(errno)); // print a warning if the file cannot be removed
                }
            }
        }
        return 0;
    }

    default:
        return 0;
    }
}

/**
 * @brief  Print usage to stderr.
 *
 * @param  prog  Program name (usually argv[0]).
 *
 * @return None.
 */
static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage:\n"
            "  %s -flist dir\n"
            "  %s -tcount ext1 [ext2] [ext3] dir\n"
            "  %s -srchf filename root_dir\n"
            "  %s -dircnt root_dir\n"
            "  %s -sumfilesize root_dir\n"
            "  %s -lfsize dir\n"
            "  %s -nonwr dir\n"
            "  %s -copyd source_dir destination_dir\n"
            "  %s -dmove source_dir destination_dir\n"
            "  %s -remd root_dir file_extension\n",
            prog, prog, prog, prog, prog, prog, prog, prog, prog, prog);
}

/**
 * @brief  Entry point: parse args, validate paths, configure context, and run nftw.
 *
 * @param  argc  Argument count.
 * @param  argv  Argument vector.
 *
 * @return 0 on success; non-zero on failure.
 *
 * @note   Paths must be under HOME (~); copy/move also require source/destination not be HOME itself.
 */
int main(int argc, char **argv)
{
    if (argc < 3)
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    /* Get HOME and realpath to absolute path for "must be under ~" restriction */
    const char *home_env = getenv("HOME");
    if (!home_env)
        die_msg("HOME is not set.");

    char *home_abs = to_real_abs(home_env);
    if (!home_abs)
        die("realpath(HOME)");

    memset(&G, 0, sizeof(G)); // initialize the global context to zero
    vec_init(&G.items);       // initialize the items vector

    const char *opt = argv[1];

    /* Normalize: realpath dir/root/source/dest */
    /* Note: realpath requires path to exist; destination_dir should exist for copyd */
    if (strcmp(opt, "-flist") == 0) // List the files in the first level of dir, sorted by newest modified time.
    // Use strcmp to check whether the second command-line argument is -flist.
    {
        if (argc != 3)
        {
            usage(argv[0]);
            return EXIT_FAILURE;
        }

        char *dir_abs = to_real_abs(argv[2]); // Get the third command-line argument (the directory path) and convert it into a real, absolute path.
        if (!dir_abs)
            die("realpath(dir)");
        if (!path_is_under_home(dir_abs, home_abs)) // Use path_is_under_home to check whether dir_abs is inside the home_abs directory.
            die_msg("Error: dir must be under HOME (~).");

        G.mode = M_FLIST;     // set mode of global context to M_FLIST
        G.root_abs = dir_abs; // set root_abs to dir_abs

        /* Use nftw to walk through the directory (it may scan the whole directory tree). */
        if (nftw(G.root_abs, cb, 20, FTW_PHYS) != 0)
            die("nftw");

        // Sort the collected items by time in descending order using qsort.
        qsort(G.items.a, G.items.n, sizeof(Item), cmp_flist);
        for (size_t i = 0; i < G.items.n; i++)
        {
            printf("%s\n", G.items.a[i].path); // Print the path of each item in the sorted order.
        }

        vec_free(&G.items); // Free the memory allocated for the items vector and its elements.
        free(dir_abs);
        free(home_abs);
        return 0;
    }

    if (strcmp(opt, "-tcount") == 0) // Count how many first-level files match each extension (1 to 3 extensions).
    {
        if (argc < 4 || argc > 6)
        {
            usage(argv[0]);
            return EXIT_FAILURE;
        }

        /* Get the last argument, which is always the directory. The arguments in the middle are 1 to 3 extensions. */
        char *dir_abs = to_real_abs(argv[argc - 1]);
        if (!dir_abs)
            die("realpath(dir)");
        if (!path_is_under_home(dir_abs, home_abs)) // Check if dir_abs is under home_abs using path_is_under_home.
            die_msg("Error: dir must be under HOME (~).");

        G.mode = M_TCOUNT; // set mode of global context to M_TCOUNT
        G.root_abs = dir_abs;

        /* Figure out how many extensions we got. argc=4 => 1 extension; argc=6 => 3 extensions. */
        G.extn = argc - 3;
        for (int i = 0; i < G.extn; i++)
        {
            G.exts[i] = argv[2 + i]; // set exts[i] to the corresponding extension argument
            G.counts[i] = 0;         // initialize counts[i] to 0
        }

        /* Use nftw to walk through the directory (it may scan the whole directory tree). */
        if (nftw(G.root_abs, cb, 20, FTW_PHYS) != 0)
            die("nftw");

        for (int i = 0; i < G.extn; i++)
        {
            printf("%s count: %ld\n", G.exts[i], G.counts[i]); // Print the count of files for each extension
        }

        vec_free(&G.items); // Free the memory allocated for the items vector and its elements.
        free(dir_abs);
        free(home_abs);
        return 0;
    }

    if (strcmp(opt, "-srchf") == 0) // Search the whole tree and print the full path of files named filename.
    {
        if (argc != 4)
        {
            usage(argv[0]);
            return EXIT_FAILURE;
        }

        /* Get last argument, which is always the root directory. */
        char *root_abs = to_real_abs(argv[3]); // Convert the root directory argument to a real, absolute path.
        if (!root_abs)
            die("realpath(root_dir)");
        if (!path_is_under_home(root_abs, home_abs)) // Check if root_abs is under home_abs by using path_is_under_home.
            die_msg("Error: root_dir must be under HOME (~).");

        G.mode = M_SRCHF; // set mode of global context to M_SRCHF
        G.root_abs = root_abs;
        G.target_name = argv[2]; // the third argument is the target filename to search for
        G.found_any = 0;         // initialize found_any to 0

        /* Use nftw to walk through the directory (it may scan the whole directory tree). */
        if (nftw(G.root_abs, cb, 20, FTW_PHYS) != 0)
            die("nftw");

        if (!G.found_any)
        {
            fprintf(stderr, "Not found: %s\n", G.target_name);
        }

        free(root_abs);
        free(home_abs);
        return 0;
    }

    if (strcmp(opt, "-dircnt") == 0) // Count how many directories are under the given root directory.
    {
        if (argc != 3)
        {
            usage(argv[0]);
            return EXIT_FAILURE;
        }

        /* Get the root directory argument and convert it to a real, absolute path. */
        char *root_abs = to_real_abs(argv[2]);
        if (!root_abs)
            die("realpath(root_dir)");
        if (!path_is_under_home(root_abs, home_abs)) // Check if root_abs is under home_abs using path_is_under_home.
            die_msg("Error: root_dir must be under HOME (~).");

        G.mode = M_DIRCNT;     // set mode of global context to M_DIRCNT
        G.root_abs = root_abs; // set path you want to count directories under
        G.dir_count = 0;       // initialize dir_count to 0

        /* Counting directories does not require post-order depth traversal */
        if (nftw(G.root_abs, cb, 20, FTW_PHYS) != 0)
            die("nftw");

        printf("Directory count: %ld\n", G.dir_count);

        free(root_abs);
        free(home_abs);
        return 0;
    }

    if (strcmp(opt, "-sumfilesize") == 0) // Sum the sizes of all files under the given root directory.
    {
        if (argc != 3)
        {
            usage(argv[0]);
            return EXIT_FAILURE;
        }

        // Get last argument, which is the root directory, and convert it to a real, absolute path.
        char *root_abs = to_real_abs(argv[2]);
        if (!root_abs)
            die("realpath(root_dir)");
        if (!path_is_under_home(root_abs, home_abs)) // Check if root_abs is under home_abs using path_is_under_home.
            die_msg("Error: root_dir must be under HOME (~).");

        G.mode = M_SUMFILESIZE; // set mode of global context to M_SUMFILESIZE
        G.root_abs = root_abs;  // set root_abs to the directory you want to sum file sizes under
        G.total_bytes = 0;      // initialize total_bytes to 0

        if (nftw(G.root_abs, cb, 20, FTW_PHYS) != 0)
            die("nftw");

        printf("Total file size (bytes): %lld\n", G.total_bytes); // Print the total file size in bytes.

        free(root_abs);
        free(home_abs);
        return 0;
    }

    if (strcmp(opt, "-lfsize") == 0) // List files in the directory sorted by file size.
    {
        if (argc != 3)
        {
            usage(argv[0]);
            return EXIT_FAILURE;
        }

        // Get last argument, which is the directory, and convert it to a real, absolute path.
        char *dir_abs = to_real_abs(argv[2]);
        if (!dir_abs)
            die("realpath(dir)");
        if (!path_is_under_home(dir_abs, home_abs)) // Check if dir_abs is under home_abs using path_is_under_home.
            die_msg("Error: dir must be under HOME (~).");

        G.mode = M_LFSIZE;    // set mode of global context to M_LFSIZE
        G.root_abs = dir_abs; // set root_abs to the directory you want to list files by size under

        if (nftw(G.root_abs, cb, 20, FTW_PHYS) != 0)
            die("nftw");

        qsort(G.items.a, G.items.n, sizeof(Item), cmp_lfsize);
        for (size_t i = 0; i < G.items.n; i++)
        {
            printf("%s\t%lld\n", G.items.a[i].path, (long long)G.items.a[i].size);
        }

        vec_free(&G.items);
        free(dir_abs);
        free(home_abs);
        return 0;
    }

    if (strcmp(opt, "-nonwr") == 0) // List non-writable files in the directory, sorted by path in alphabetical order.
    {
        if (argc != 3)
        {
            usage(argv[0]);
            return EXIT_FAILURE;
        }

        // get last argument, which is the directory, and convert it to a real, absolute path.
        char *dir_abs = to_real_abs(argv[2]);
        if (!dir_abs)
            die("realpath(dir)");
        if (!path_is_under_home(dir_abs, home_abs)) // Check if dir_abs is under home_abs using path_is_under_home.
            die_msg("Error: dir must be under HOME (~).");

        G.mode = M_NONWR;     // set mode of global context to M_NONWR
        G.root_abs = dir_abs; // set root_abs to the directory you want to list non-writable files under

        if (nftw(G.root_abs, cb, 20, FTW_PHYS) != 0)
            die("nftw");

        qsort(G.items.a, G.items.n, sizeof(Item), cmp_path_alpha); // Sort the collected items by path in alphabetical order.
        for (size_t i = 0; i < G.items.n; i++)
        {
            printf("%s\n", G.items.a[i].path); // Print the path of each non-writable file.
        }

        vec_free(&G.items);
        free(dir_abs);
        free(home_abs);
        return 0;
    }

    if (strcmp(opt, "-copyd") == 0 || strcmp(opt, "-dmove") == 0) // Copy/Move the whole source directory tree into the destination folder.
    // -copyd and -dmove are very similar, so I handle them in the same code block.
    {
        if (argc != 4)
        {
            usage(argv[0]);
            return EXIT_FAILURE;
        }

        // get source directory and convert it to a real, absolute path.
        char *src_abs = to_real_abs(argv[2]);
        if (!src_abs)
            die("realpath(source_dir)");

        // get destination directory and convert it to a real, absolute path.
        char *dst_abs = to_real_abs(argv[3]);
        if (!dst_abs)
            die("realpath(destination_dir)");

        if (!path_is_under_home(src_abs, home_abs) || !path_is_under_home(dst_abs, home_abs)) // Check if both src_abs and dst_abs are under home_abs using path_is_under_home.
            die_msg("Error: source_dir and destination_dir must be under HOME (~).");

        /* For copy/move, the source and destination can’t be the HOME folder itself. */
        if (strcmp(src_abs, home_abs) == 0 || strcmp(dst_abs, home_abs) == 0)
            die_msg("Error: source_dir and destination_dir cannot be HOME itself.");

        /* Require source_dir to be a directory */
        struct stat st;
        if (stat(src_abs, &st) != 0) // Check if source_dir exists and get its status.
            die("stat(source_dir)");
        if (!S_ISDIR(st.st_mode))
            die_msg("Error: source_dir is not a directory.");

        /* Require destination_dir to be a directory and exist */
        if (stat(dst_abs, &st) != 0) // Check if destination_dir exists and get its status.
            die("stat(destination_dir)");
        if (!S_ISDIR(st.st_mode))
            die_msg("Error: destination_dir is not a directory.");

        /* Save copy/move context */
        G.src_abs = src_abs;
        G.dst_abs = dst_abs;
        G.src_base = base_name_view(src_abs); /* get the last component of source_dir path */

        G.copied_files = 0; //* initialize copied_files to 0 */
        G.copied_dirs = 0;
        G.copy_failures = 0;

        G.mode = M_COPYD;     // set mode of global context to M_COPYD
        G.root_abs = src_abs; // set root_abs to the source directory you want to copy/move from

        /* First: use nftw to walk through the source folder and copy everything */
        if (nftw(G.root_abs, cb, 20, FTW_PHYS) != 0)
            die("nftw(copy)");

        printf("Copied dirs: %ld\n", G.copied_dirs);
        printf("Copied files: %ld\n", G.copied_files);
        if (G.copy_failures > 0) // print how many folders and files were copied, and also show a warning if any copies failed.
        {
            fprintf(stderr, "WARN: copy failures: %ld\n", G.copy_failures);
        }

        if (strcmp(opt, "-dmove") == 0) // Check if the operation is a move, proceed to delete the source files/directories.
        {
            /*
             * Second: delete the source (must traverse in post-order to avoid "directory not empty" errors)
             * FTW_DEPTH: visit child nodes first, then the directory itself
             */
            G.mode = M_DMOVE_DELETE_ONLY; // set mode of global context to M_DMOVE_DELETE_ONLY for deletion phase
            if (nftw(src_abs, cb, 20, FTW_PHYS | FTW_DEPTH) != 0)
            {
                die("nftw(delete source)"); // If deletion fails, terminate the program with an error message.
            }
            printf("Move done (source removed).\n"); // Indicate that the move operation is complete and the source has been removed.
        }

        free(src_abs);
        free(dst_abs);
        free(home_abs);
        return 0;
    }

    if (strcmp(opt, "-remd") == 0) // Delete all regular files with the given extension in the tree.
    {
        if (argc != 4)
        {
            usage(argv[0]);
            return EXIT_FAILURE;
        }

        // get root directory and convert it to a real, absolute path.
        char *root_abs = to_real_abs(argv[2]);
        if (!root_abs)
            die("realpath(root_dir)");
        if (!path_is_under_home(root_abs, home_abs)) // Check if root_abs is under home_abs using path_is_under_home.
            die_msg("Error: root_dir must be under HOME (~).");

        G.mode = M_REMD; // set mode of global context to M_REMD
        G.root_abs = root_abs;
        G.rem_ext = argv[3]; // get the file extension to remove
        G.removed_files = 0; // initialize removed_files to 0

        if (nftw(G.root_abs, cb, 20, FTW_PHYS) != 0)
            die("nftw");

        printf("Removed files with extension %s: %ld\n", G.rem_ext, G.removed_files);

        free(root_abs);
        free(home_abs);
        return 0;
    }

    /* Unknown option */
    usage(argv[0]);
    free(home_abs);
    return EXIT_FAILURE;
}