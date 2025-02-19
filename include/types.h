/*
 * Copyright © 2019 Red Hat, Inc.
 * Author(s): David Cantrell <dcantrell@redhat.com>
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see
 * <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

/*
 * This header defines types used by librpminspect
 */

#include <regex.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/capability.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmfi.h>
#include <libkmod.h>
#include "secrules.h"
#include "queue.h"
#include "uthash.h"

#ifndef _LIBRPMINSPECT_TYPES_H
#define _LIBRPMINSPECT_TYPES_H

/*
 * List of strings. Used by some of the inspections.
 */
typedef struct _string_entry_t {
    char *data;
    TAILQ_ENTRY(_string_entry_t) items;
} string_entry_t;

typedef TAILQ_HEAD(string_entry_s, _string_entry_t) string_list_t;

/*
 * List of string pairs. Used to later convert in to a newly allocated hash table.
 */
typedef struct _pair_entry_t {
    char *key;
    char *value;
    TAILQ_ENTRY(_pair_entry_t) items;
} pair_entry_t;

typedef TAILQ_HEAD(pair_entry_s, _pair_entry_t) pair_list_t;

/*
 * A file is information about a file in an RPM payload.
 *
 * If fullpath is not NULL, it is the absolute path of the unpacked
 * file.  Not every file is unpacked (e.g., block and char special
 * files are skipped).  The ownership and permissions of the unpacked
 * file may not match the intended owner and mode from the RPM
 * metadata.
 *
 * "localpath" is file path from the RPM payload, and "st" is the
 * metadata about the file, as described by the RPM payload. localpath
 * and st may not necessarily match the description of the file in the
 * RPM header.
 *
 * The rpm_header field is shared by multiple files. Each file entry
 * must call headerFree to dereference the header.
 *
 * idx is the index for this file into the RPM array tags such as
 * RPMTAG_FILESIZES.
 *
 * type is the MIME type string that you would get from 'file
 * --mime-type'.
 *
 * cap is the getcap() value for the file.
 *
 * checksum is a string containing the human-readable checksum digest
 *
 * moved_path is true if the file moved path locations between the
 * before and after build, false otherwise
 *
 * moved_subpackage is true if the file moved between subpackages
 * between the before and after build, false otherwise.
 */
typedef struct _rpmfile_entry_t {
    Header rpm_header;
    char *fullpath;
    char *localpath;
    struct stat st;
    int idx;
    char *type;
    char *checksum;
    cap_t cap;
    rpmfileAttrs flags;
    struct _rpmfile_entry_t *peer_file;
    bool moved_path;
    bool moved_subpackage;
    TAILQ_ENTRY(_rpmfile_entry_t) items;
} rpmfile_entry_t;

typedef TAILQ_HEAD(rpmfile_s, _rpmfile_entry_t) rpmfile_t;

/*
 * A peer is a mapping of a built RPM from the before and after builds.
 * We can expand this struct as necessary based on what tests need to
 * reference.
 */
typedef struct _rpmpeer_entry_t {
    Header before_hdr;        /* RPM header of the before package */
    Header after_hdr;         /* RPM header of the after package */
    char *before_rpm;         /* full path to the before RPM */
    char *after_rpm;          /* full path to the after RPM */
    char *before_root;        /* full path to the before RPM extracted root dir */
    char *after_root;         /* full path to the after RPM extracted root dir */
    rpmfile_t *before_files;  /* list of files in the payload of the before RPM */
    rpmfile_t *after_files;   /* list of files in the payload of the after RPM */
    TAILQ_ENTRY(_rpmpeer_entry_t) items;
} rpmpeer_entry_t;

typedef TAILQ_HEAD(rpmpeer_s, _rpmpeer_entry_t) rpmpeer_t;

/*
 * And individual inspection result and the list to hold them.
 */
typedef enum _severity_t {
    RESULT_NULL   = 0,      /* used to indicate internal error */
    RESULT_OK     = 1,
    RESULT_INFO   = 2,
    RESULT_VERIFY = 3,
    RESULT_BAD    = 4,
    RESULT_SKIP   = 5       /* not reported, used to skip output */
} severity_t;

typedef enum _waiverauth_t {
    NOT_WAIVABLE         = 0,
    WAIVABLE_BY_ANYONE   = 1,
    WAIVABLE_BY_SECURITY = 2
} waiverauth_t;

typedef enum _verb_t {
    VERB_NIL = 0,       /* not used, same as "not set" */
    VERB_ADDED = 1,     /* new file or metadata */
    VERB_REMOVED = 2,   /* removed file or metadata */
    VERB_CHANGED = 3,   /* changed file or metadata */
    VERB_FAILED = 4     /* check failing */
} verb_t;

/*
 * struct to make it easier to make multiple calls to add_result()
 * See the results_entry_t for details.
 */
struct result_params {
    severity_t severity;
    waiverauth_t waiverauth;
    const char *header;
    char *msg;
    char *details;
    char *remedy;
    verb_t verb;
    const char *noun;
    const char *arch;
    const char *file;
};

typedef struct _results_entry_t {
    severity_t severity;      /* see results.h */
    waiverauth_t waiverauth;  /* who can waive an inspection result */
    const char *header;       /* header string for reporting */
    char *msg;                /* the result message */
    char *details;            /* details (optional, can be NULL) */
    char *remedy;             /* suggested correction for the result */
    verb_t verb;              /* verb indicating what happened */
    char *noun;               /* noun impacted by 'verb', one line
                                 (e.g., a file path or an RPM dependency
                                        string) */
    char *arch;               /* architecture impacted (${ARCH}) */
    char *file;               /* file impacted (${FILE}) */
    TAILQ_ENTRY(_results_entry_t) items;
} results_entry_t;

typedef TAILQ_HEAD(results_s, _results_entry_t) results_t;

/*
 * Known types of Koji builds
 */
typedef enum _koji_build_type_t {
    KOJI_BUILD_NULL = 0,       /* initializer, not an actual build */
    KOJI_BUILD_IMAGE = 1,      /* not supported */
    KOJI_BUILD_MAVEN = 2,      /* not supported */
    KOJI_BUILD_MODULE = 3,
    KOJI_BUILD_RPM = 4,
    KOJI_BUILD_WIN = 5         /* not supported */
} koji_build_type_t;

/*
 * fileinfo for a product release. Used by some of the inspections.
 */
typedef enum _fileinfo_field_t {
    MODE = 0,
    OWNER = 1,
    GROUP = 2,
    FILENAME = 3
} fileinfo_field_t;

typedef struct _fileinfo_entry_t {
    mode_t mode;
    char *owner;
    char *group;
    char *filename;
    TAILQ_ENTRY(_fileinfo_entry_t) items;
} fileinfo_entry_t;

typedef TAILQ_HEAD(fileinfo_entry_s, _fileinfo_entry_t) fileinfo_t;

/*
 * caps list for a product release.  Used by some of the inspections.
 */
typedef enum _caps_field_t {
    PACKAGE = 0,
    FILEPATH = 1,
    EQUAL = 2,
    CAPABILITIES = 3
} caps_field_t;

typedef struct _caps_filelist_entry_t {
    char *path;
    char *caps;
    TAILQ_ENTRY(_caps_filelist_entry_t) items;
} caps_filelist_entry_t;

typedef TAILQ_HEAD(caps_filelist_entry_s, _caps_filelist_entry_t) caps_filelist_t;

typedef struct _caps_entry_t {
    char *pkg;
    caps_filelist_t *files;
    TAILQ_ENTRY(_caps_entry_t) items;
} caps_entry_t;

typedef TAILQ_HEAD(caps_entry_s, _caps_entry_t) caps_t;

/* Spec filename matching types */
typedef enum _specname_match_t {
    MATCH_NULL = 0,
    MATCH_FULL = 1,
    MATCH_PREFIX = 2,
    MATCH_SUFFIX = 3
} specname_match_t;

typedef enum _specname_primary_t {
    PRIMARY_NULL = 0,
    PRIMARY_NAME = 1,
    PRIMARY_FILENAME = 2
} specname_primary_t;

/* RPM header cache so we don't balloon out our memory */
typedef struct _header_cache_entry_t {
    char *pkg;
    Header hdr;
    TAILQ_ENTRY(_header_cache_entry_t) items;
} header_cache_entry_t;

typedef TAILQ_HEAD(header_cache_entry_s, _header_cache_entry_t) header_cache_t;

/* Product release string favoring */
typedef enum _favor_release_t {
    FAVOR_NONE = 0,
    FAVOR_OLDEST = 1,
    FAVOR_NEWEST = 2
} favor_release_t;

/*
 * Politics list
 */
typedef struct _politics_entry_t {
    char *pattern;
    char *digest;
    bool allowed;
    TAILQ_ENTRY(_politics_entry_t) items;
} politics_entry_t;

typedef TAILQ_HEAD(politics_entry_s, _politics_entry_t) politics_list_t;

typedef enum _politics_field_t {
    PATTERN = 0,
    DIGEST = 1,
    PERMISSION = 2,
} politics_field_t;

/* Commands used by rpminspect at runtime. */
struct command_paths {
    char *diff;
    char *diffstat;
    char *msgunfmt;
    char *desktop_file_validate;
    char *annocheck;
    char *abidiff;
    char *kmidiff;
};

/* Hash table used for key/value situations where each is a string. */
typedef struct _string_map_t {
    char *key;
    char *value;
    UT_hash_handle hh;
} string_map_t;

/* Hash table with a string key and a string_list_t value. */
typedef struct _string_list_map_t {
    char *key;
    string_list_t *value;
    UT_hash_handle hh;
} string_list_map_t;

/*
 * Configuration and state instance for librpminspect run.
 * Applications using librpminspect should initialize the
 * library and retain this structure through the run of
 * their program.  You need to free this struct on exit.
 */
struct rpminspect {
    char *progname;            /* Full path to the program */
    string_list_t *cfgfiles;   /* list of full path config files read (in order) */
    char *workdir;             /* full path to working directory */
    char *profiledir;          /* full path to profiles directory */
    char *worksubdir;          /* within workdir, where these builds go */

    /* Commands */
    struct command_paths commands;

    /* Vendor data */
    char *vendor_data_dir;     /* main vendor data directory */
    char *licensedb;           /* name of file under licenses/ to use */
    favor_release_t favor_release;

    /* Populated at runtime for the product release */
    char *fileinfo_filename;
    fileinfo_t *fileinfo;
    caps_t *caps;
    char *caps_filename;
    string_list_t *rebaseable;
    char *rebaseable_filename;
    politics_list_t *politics;
    char *politics_filename;
    security_list_t *security;
    char *security_filename;
    bool security_initialized;

    /* Koji information (from config file) */
    char *kojihub;             /* URL of Koji hub */
    char *kojiursine;          /* URL to access packages built in Koji */
    char *kojimbs;             /* URL to access module packages in Koji */

    /* Information used by different tests */
    string_list_t *badwords;   /* Space-delimited list of words prohibited
                                * from certain package strings.
                                */
    char *vendor;              /* Required vendor string */

    /* Required subdomain for buildhosts -- multiple subdomains allowed */
    string_list_t *buildhost_subdomain;

    /*
     * Optional: if not NULL, contains list of path prefixes for files
     * that are of security concern.
     */
    string_list_t *security_path_prefix;

    /*
     * Optional: if not NULL, contains a list of filename extensions
     * for C and C++ header files.
     */
    string_list_t *header_file_extensions;

    /*
     * Optional: Lists of path substrings and directories to forbid.
     */
    string_list_t *forbidden_path_prefixes;
    string_list_t *forbidden_path_suffixes;
    string_list_t *forbidden_directories;

    /*
     * Optional: if not NULL, contains a list of forbidden function
     * names.
     */
    string_list_t *bad_functions;

    /* Optional: if not NULL, contains list of architectures */
    /* if not specified on the command line, this becomes the list of
       all architectures downloaded */
    string_list_t *arches;

    regex_t *elf_path_include;
    regex_t *elf_path_exclude;
    regex_t *manpage_path_include;
    regex_t *manpage_path_exclude;
    regex_t *xml_path_include;
    regex_t *xml_path_exclude;

    /* copies of regex pattern strings used for debug mode output */
    char *elf_path_include_pattern;
    char *elf_path_exclude_pattern;
    char *manpage_path_include_pattern;
    char *manpage_path_exclude_pattern;
    char *xml_path_include_pattern;
    char *xml_path_exclude_pattern;

    /* Where desktop entry files live */
    char *desktop_entry_files_dir;

    /* Executable path prefixes and required ownership */
    string_list_t *bin_paths;
    char *bin_owner;
    char *bin_group;

    /* Optional: Forbidden file owners and groups */
    string_list_t *forbidden_owners;
    string_list_t *forbidden_groups;

    /* List of shells to check script syntax */
    string_list_t *shells;

    /* Optional: file size change threshold for inc/dec reporting (%) */
    long int size_threshold;

    /* Optional: ELF LTO symbol prefixes */
    string_list_t *lto_symbol_name_prefixes;

    /* Spec filename matching type */
    specname_match_t specmatch;
    specname_primary_t specprimary;

    /* hash table of product release -> JVM major versions */
    string_map_t *jvm;

    /* hash table of annocheck tests */
    string_map_t *annocheck;

    /* hash table of path migrations */
    string_map_t *pathmigration;
    string_list_t *pathmigration_excluded_paths;

    /* hash table of product release regexps */
    string_map_t *products;

    /* list of paths to ignore (these strings allow glob(3) syntax) */
    string_list_t *ignores;

    /* list of forbidden path references for %files sections */
    string_list_t *forbidden_paths;

    /* name of the optional ABI suppression file in the SRPM */
    char *abidiff_suppression_file;

    /* path where debuginfo files are found in packages */
    char *abidiff_debuginfo_path;

    /* path where header filesa re found in packages */
    char *abidiff_include_path;

    /* extra arguments for abidiff(1) */
    char *abidiff_extra_args;

    /* ABI compat level security reporting threshold */
    long int abi_security_threshold;

    /* name of the optional ABI suppression file in the SRPM */
    char *kmidiff_suppression_file;

    /* path where debuginfo files are found in packages */
    char *kmidiff_debuginfo_path;

    /* extra arguments for kmidiff(1) */
    char *kmidiff_extra_args;

    /* list of valid kernel executable filenames (e.g., "vmlinux") */
    string_list_t *kernel_filenames;

    /*
     * directory where kernel ABI (KABI) files are kept (in any
     * subpackage in a kernel build)
     */
    char *kabi_dir;

    /*
     * name of KABI files in kabi_dir, can use $ARCH or ${ARCH} for
     * architecture
     */
    char *kabi_filename;

    /* list of patches to ignore in the 'patches' inspection */
    string_list_t *patch_ignore_list;

    /* file count reporting threshold in the 'patches' inspection */
    long int patch_file_threshold;

    /* line count reporting threshold in the 'patches' inspection */
    long int patch_line_threshold;

    /* runpath inspection lists */
    string_list_t *runpath_allowed_paths;
    string_list_t *runpath_allowed_origin_paths;
    string_list_t *runpath_origin_prefix_trim;

    /* optional per-inspection ignore lists */
    /*
     * NOTE: the 'ignores' struct member is a global list of ignore
     * globs.  This hash table provides per-inspection ignore globs.
     */
    string_list_map_t *inspection_ignores;

    /* Optional list of expected RPMs with empty payloads */
    string_list_t *expected_empty_rpms;

    /* Options specified by the user */
    char *before;              /* before build ID arg given on cmdline */
    char *after;               /* after build ID arg given on cmdline */
    uint64_t tests;            /* which tests to run (default: ALL) */
    bool verbose;              /* verbose inspection output? */
    bool rebase_detection;     /* Is rebase detection enabled for
                                  builds? (default true) */

    /* Failure threshold */
    severity_t threshold;
    severity_t worst_result;

    /* The product release we are inspecting against */
    char *product_release;

    /* The type of Koji build we are looking at */
    /*
     * NOTE: rpminspect works with RPMs at the lowest level, so
     * build types that are other collections of RPMs may be
     * supported. But supporting non-RPM containers in rpminspect
     * is not really in scope.
     */
    koji_build_type_t buildtype;

    /* accumulated data of the build set */
    rpmpeer_t *peers;               /* list of packages */
    header_cache_t *header_cache;   /* RPM header cache */
    char *before_rel;               /* before Release w/o %{?dist} */
    char *after_rel;                /* after Release w/o ${?dist} */
    int rebase_build;               /* indicates if this is a rebased build */

    /* used by ELF symbol checks */
    string_map_t *fortifiable;

    /* spec file macros */
    pair_list_t *macros;

    /* inspection results */
    results_t *results;
};

/*
 * Definition for an output format.
 */
struct format {
    /* The output format type from output.h */
    int type;

    /* short name of the format */
    char *name;

    /* output driver function */
    void (*driver)(const results_t *, const char *, const severity_t);
};

/*
 * Definition for an inspection.  Inspections are assigned a flag (see
 * inspect.h), a short name, and a function pointer to the driver.  The
 * driver function needs to take a struct rpminspect pointer as the only
 * argument.  The driver returns true on success and false on failure.
 */
struct inspect {
    /* the inspection flag from inspect.h */
    uint64_t flag;

    /* short name of inspection */
    char *name;

    /*
     * Does this inspection require a before and after package?
     * Some inspections run just against a single build, which
     * we can use when the user wants to run rpminspect against
     * a single build.
     *
     * True if this inspection is for a single build (which is
     * always the 'after' build throughout the code).
     */
    bool single_build;

    /* the driver function for the inspection */
    bool (*driver)(struct rpminspect *);
};

/*
 * List of RPMs from a Koji build.  Only the information we need.
 */
typedef struct _koji_rpmlist_entry_t {
    char *arch;
    char *name;
    char *version;
    char *release;
    int epoch;
    long long int size;
    TAILQ_ENTRY(_koji_rpmlist_entry_t) items;
} koji_rpmlist_entry_t;

typedef TAILQ_HEAD(koji_rpmlist_s, _koji_rpmlist_entry_t) koji_rpmlist_t;

/*
 * List of build IDs from a Koji build.
 */
typedef struct _koji_buildlist_entry_t {
    int build_id;
    char *package_name;
    char *owner_name;
    int task_id;
    int state;
    char *nvr;
    char *start_time;
    int creation_event_id;
    char *creation_time;
    char *epoch;
    int tag_id;
    char *completion_time;
    char *tag_name;
    char *version;
    int volume_id;
    char *release;
    int package_id;
    int owner_id;
    int id;
    char *volume_name;
    char *name;

    /* List of RPMs in this build */
    koji_rpmlist_t *rpms;

    TAILQ_ENTRY(_koji_buildlist_entry_t) builditems;
} koji_buildlist_entry_t;

typedef TAILQ_HEAD(koji_buildlist_s, _koji_buildlist_entry_t) koji_buildlist_t;

/*
 * Koji build structure.  This is determined by looking at the
 * output of a getBuild XMLRPC call to a Koji hub.
 *
 * You can examine example values by running xmlrpc on the command
 * line and giving it a build specification, e.g.:
 *     xmlrpc KOJI_HUB_URL getBuild NVR
 * Not all things returned are represented in this struct.
 */
struct koji_build {
    /* These are all relevant to the name of the build */
    char *package_name;
    int epoch;
    char *name;
    char *version;
    char *release;
    char *nvr;

    /* The source used to drive this build (usually a VCS link) */
    char *source;

    /* Koji-specific information about the build */
    char *creation_time;
    char *completion_time;
    int package_id;
    int id;
    int state;
    double completion_ts;
    int owner_id;
    char *owner_name;
    char *start_time;
    int creation_event_id;
    double start_ts;
    double creation_ts;
    int task_id;

    /* Where to find the resulting build artifacts */
    int volume_id;
    char *volume_name;

    /*
     * Original source URL, for some reason
     * (not present for module builds)
     */
    char *original_url;

    /* Content Generator information (currently not used in rpminspect) */
    int cg_id;
    char *cg_name;

    /* Module metadata -- only if this build is a module */
    char *modulemd_str;
    char *module_name;
    char *module_stream;
    int module_build_service_id;
    char *module_version;
    char *module_context;
    char *module_content_koji_tag;

    /* List of build IDs associated with this build */
    koji_buildlist_t *builds;
};

/*
 * Koji task structure.  This is determined by looking at the
 * output of a getTaskInfo XMLRPC call to a Koji hub.
 *
 * You can examine example values by running xmlrpc on the command
 * line and giving it a task ID, e.g.:
 *     xmlrpc KOJI_HUB_URL getTaskInfo ID
 * Not all things returned are represented in this struct.
 */
typedef TAILQ_HEAD(koji_task_list_s, _koji_task_entry_t) koji_task_list_t;

struct koji_task {
    /* members returned from getTaskInfo */
    double weight;
    int parent;
    char *completion_time;
    char *start_time;
    double start_ts;
    bool waiting;
    bool awaited;
    char *label;
    int priority;
    int channel_id;
    int state;
    char *create_time;
    double create_ts;
    int owner;
    int host_id;
    char *method;
    double completion_ts;
    char *arch;
    int id;

    /* Descendent tasks (where files are) */
    koji_task_list_t *descendents;
};

/* A generic list of koji tasks */
typedef struct _koji_task_entry_t {
    /* main task information */
    struct koji_task *task;

    /* results from getTaskResult */
    int brootid;
    string_list_t *srpms;
    string_list_t *rpms;
    string_list_t *logs;

    TAILQ_ENTRY(_koji_task_entry_t) items;
} koji_task_entry_t;

/* Types of files -- used by internal nftw() callbacks */
typedef enum _filetype_t {
    FILETYPE_NULL = 0,
    FILETYPE_EXECUTABLE = 1,
    FILETYPE_ICON = 2
} filetype_t;

/* Kernel module handling */
typedef void (*modinfo_to_entries)(string_list_t *, const struct kmod_list *);
typedef void (*module_alias_callback)(const char *, const string_list_t *, const string_list_t *, void *);

/* mapping of an alias string to a module name */
typedef struct _kernel_alias_data_t {
    char *alias;
    string_list_t *modules;
    UT_hash_handle hh;
} kernel_alias_data_t;

/* Types of workdirs */
typedef enum _workdir_t {
    NULL_WORKDIR = 0,          /* unused                    */
    LOCAL_WORKDIR = 1,         /* locally cached koji build */
    TASK_WORKDIR = 2,          /* like for scratch builds   */
    BUILD_WORKDIR = 3          /* remote koji build spec    */
} workdir_t;

/**
 * @brief Callback function to pass to foreach_peer_file.
 *
 * Given the program's main struct rpminspect and a single
 * rpmfile_entry_t, perform a defined action and return true if the
 * action was successful and false otherwise.  This is used for
 * callback functions in inspection functions.  You can iterate over
 * each file in a package and perform the inspection.  True means
 * everything passed, false means something failed.  Since the
 * callback received the struct rpminspect, you can add results as the
 * program returns and not worry about losing inspection details.
 */
typedef bool (*foreach_peer_file_func)(struct rpminspect *, rpmfile_entry_t *);

/* Types of ELF information we can return */
typedef enum _elfinfo_t {
    ELF_TYPE    = 0,
    ELF_MACHINE = 1
} elfinfo_t;

/*
 * Exit status for abidiff and abicompat tools.  It's actually a bit
 * mask.  The value of each enumerator is a power of two.
 *
 * (This is lifted from abg-tools-utils.h in libabigail because it
 * cannot be included directly because libabigail is C++.)
 */
enum abidiff_status {
  /*
   * This is for when the compared ABIs are equal.
   * Its numerical value is 0.
   */
  ABIDIFF_OK = 0,

  /*
   * This bit is set if there an application error.
   * Its numerical value is 1.
   */
  ABIDIFF_ERROR = 1,

  /*
   * This bit is set if the tool is invoked in an non appropriate
   * manner.
   * Its numerical value is 2.
   */
  ABIDIFF_USAGE_ERROR = 1 << 1,

  /*
   * This bit is set if the ABIs being compared are different.
   * Its numerical value is 4.
   */
  ABIDIFF_ABI_CHANGE = 1 << 2,

  /*
   * This bit is set if the ABIs being compared are different *and*
   * are incompatible.
   *
   * Its numerical value is 8.
   */
  ABIDIFF_ABI_INCOMPATIBLE_CHANGE = 1 << 3
};

/*
 * ABI compatibility level types
 */
typedef struct _abi_t {
    char *pkg;
    int level;
    bool all;
    string_list_t *dsos;
    UT_hash_handle hh;
} abi_t;

/*
 * diffstat(1) findings for reporting in the patches inspection
 */
typedef struct _diffstat_t {
    long int files;
    long int lines;
} diffstat_t;

#endif
