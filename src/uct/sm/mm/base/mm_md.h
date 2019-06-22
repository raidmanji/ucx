/**
* Copyright (C) Mellanox Technologies Ltd. 2001-2015.  ALL RIGHTS RESERVED.
* Copyright (c) UT-Battelle, LLC. 2014-2015. ALL RIGHTS RESERVED.
*
* See file LICENSE for terms.
*/

#ifndef UCT_MM_MD_H_
#define UCT_MM_MD_H_

#include "mm_def.h"

#include <uct/base/uct_md.h>
#include <ucs/config/types.h>
#include <ucs/debug/memtrack.h>
#include <ucs/type/status.h>


/* Shared memory ID */
typedef uint64_t uct_mm_id_t;

extern ucs_config_field_t uct_mm_md_config_table[];

/*
 * Descriptor of the mapped memory
 */
struct uct_mm_remote_seg {
    uct_mm_remote_seg_t *next;
    uct_mm_id_t mmid;        /**< mmid of the remote memory chunk */
    void        *address;    /**< local memory address */
    uint64_t    cookie;      /**< cookie for mmap, xpmem, etc. */
    size_t      length;      /**< size of the memory */
};

/*
 * Memory mapper operations - MM uses them to implement MD and TL functionality.
 */
typedef struct uct_mm_mapper_ops {

    ucs_status_t (*query)();

    size_t       (*get_path_size)(uct_md_h md);

    uint8_t      (*get_priority)();

    ucs_status_t (*reg)(void *address, size_t size, 
                        uct_mm_id_t *mmid_p);

    ucs_status_t (*dereg)(uct_mm_id_t mm_id);

    ucs_status_t (*alloc)(uct_md_h md, size_t *length_p, ucs_ternary_value_t hugetlb,
                          unsigned flags, const char *alloc_name, void **address_p,
                          uct_mm_id_t *mmid_p, const char **path_p, int *is_hugetlb);

    ucs_status_t (*attach)(uct_mm_id_t mmid, size_t length,
                           void *remote_address, void **address, uint64_t *cookie,
                           const char *path);

    ucs_status_t (*detach)(uct_mm_remote_seg_t *mm_desc);

    ucs_status_t (*free)(void *address, uct_mm_id_t mm_id, size_t length,
                         const char *path);

} uct_mm_mapper_ops_t;


/**
 * MM component
 */
typedef struct uct_mm_component {
    uct_component_t       super;
    uct_mm_mapper_ops_t   *ops;
} uct_mm_component_t;


/* Extract mapper ops from MD component */
#define uct_mm_mdc_mapper_ops(_component) \
    (ucs_derived_of(_component, uct_mm_component_t)->ops)


/* Extract mapped ops from MD */
#define uct_mm_md_mapper_ops(_md) \
    uct_mm_mdc_mapper_ops((_md)->component)


#define UCT_MM_COMPONENT_DEFINE(_var, _name, _md_ops, _cfg_prefix) \
    \
    static uct_mm_component_t _var = { \
        .super = { \
            .query_md_resources = uct_mm_query_md_resources, \
            .md_open            = uct_mm_md_open, \
            .rkey_unpack        = uct_mm_rkey_unpack, \
            .rkey_ptr           = uct_mm_rkey_ptr, \
            .rkey_release       = uct_mm_rkey_release, \
            .name               = # _name, \
            .md_config          = { \
                .name           = #_name " memory domain", \
                .prefix         = _cfg_prefix, \
                .table          = uct_##_name##_md_config_table, \
                .size           = sizeof(uct_##_name##_md_config_t), \
            }, \
            .tl_list            = UCT_COMPONENT_TL_LIST_INITIALIZER( \
                                      &(_var).super) \
       }, \
       .ops                     = _md_ops \
    }; \
    UCT_COMPONENT_REGISTER(&(_var).super); \


/**
 * Local memory segment structure.
 */
typedef struct uct_mm_seg {
    uct_mm_id_t      mmid;       /* Shared memory ID */
    void             *address;   /* Virtual address */
    size_t           length;     /* Size of the memory */
    const char       *path;      /* Path to the backing file when using posix */
    int              is_hugetlb; /* If hugetlb was used for memory allocation */
} uct_mm_seg_t;


/**
 * Packed remote key
 */
typedef struct uct_mm_packed_rkey {
    uct_mm_id_t      mmid;         /* Shared memory ID */
    uintptr_t        owner_ptr;    /* VA of in allocating process */
    size_t           length;       /* Size of the memory */
    char             path[0];      /* path to the backing file when using posix */
} uct_mm_packed_rkey_t;


/**
 * MM MD
 */
typedef struct uct_mm_md {
    uct_md_t           super;
    uct_mm_md_config_t *config;
} uct_mm_md_t;


ucs_status_t uct_mm_query_md_resources(uct_component_t *component,
                                       uct_md_resource_desc_t **resources_p,
                                       unsigned *num_resources_p);

ucs_status_t uct_mm_mem_alloc(uct_md_h md, size_t *length_p, void **address_p,
                              unsigned flags, const char *alloc_name,
                              uct_mem_h *memh_p);

ucs_status_t uct_mm_mem_free(uct_md_h md, uct_mem_h memh);

ucs_status_t uct_mm_mem_reg(uct_md_h md, void *address, size_t length,
                            unsigned flags, uct_mem_h *memh_p);

ucs_status_t uct_mm_mem_dereg(uct_md_h md, uct_mem_h memh);

ucs_status_t uct_mm_md_query(uct_md_h md, uct_md_attr_t *md_attr);

ucs_status_t uct_mm_mkey_pack(uct_md_h md, uct_mem_h memh, void *rkey_buffer);

ucs_status_t uct_mm_rkey_unpack(uct_md_component_t *mdc, const void *rkey_buffer,
                                uct_rkey_t *rkey_p, void **handle_p);

ucs_status_t uct_mm_rkey_ptr(uct_md_component_t *mdc, uct_rkey_t rkey,
                             void *handle, uint64_t raddr, void **laddr_p);

void uct_mm_rkey_release(uct_md_component_t *mdc, uct_rkey_t rkey, void *handle);

ucs_status_t uct_mm_md_open(uct_component_t *component, const char *md_name,
                            const uct_md_config_t *config, uct_md_h *md_p);

#endif
