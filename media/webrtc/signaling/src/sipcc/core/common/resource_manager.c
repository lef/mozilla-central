/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_types.h"
#include "cpr_stdlib.h"
#include "resource_manager.h"
#include "phone_debug.h"

#define RM_NUM_ELEMENTS_PER_MAP 32
#define rm_get_table_index(a) (a / RM_NUM_ELEMENTS_PER_MAP)
#define rm_get_map_offset(a)  (a % RM_NUM_ELEMENTS_PER_MAP)

/*
 * rm_clear_all_elements
 *
 * Description:
 *    This function clears all members of the specified resource manager
 *
 * Parameters:
 *    rm_p - pointer to the resource manager to be cleared
 *
 * Returns:
 *    None
 */
void
rm_clear_all_elements (resource_manager_t *rm_p)
{
    static const char fname[] = "rm_clear_all_elements";
    uint16_t i;

    if (!rm_p) {
        PLAT_ERROR(PLAT_COMMON_F_PREFIX"null resource manager received.\n", fname);
        return;
    }

    for (i = 0; i < rm_p->max_index; i++) {
        rm_p->table[i] = 0;
    }
}

/*
 * rm_clear_element
 *
 * Description:
 *    This function clears a single element from the specified resource manager
 *
 * Parameters:
 *    rm_p    - pointer to the resource manager to be cleared
 *    element - element id of element to be cleared
 *
 * Returns:
 *    None
 */
void
rm_clear_element (resource_manager_t * rm_p, int16_t element)
{
    static const char fname[] = "rm_clear_elements";

    if (!rm_p) {
        PLAT_ERROR(PLAT_COMMON_F_PREFIX"null resource manager received.\n", fname);
        return;
    }

    if (element < 0 || element >= rm_p->max_element) {
        PLAT_ERROR(PLAT_COMMON_F_PREFIX"element value %d invalid. Max value is %d.\n",
               fname, element, rm_p->max_element - 1);
        return;
    }

    rm_p->table[rm_get_table_index(element)] &=
        (~(1 << rm_get_map_offset(element)));
}

/*
 * rm_set_element
 *
 * Description:
 *    This function sets the bit representing the specified element
 *    in the specified resource manager.
 *
 * Parameters:
 *    rm_p    - pointer to the resource manager
 *    element - element id of element to be set
 *
 * Returns:
 *    None
 */
void
rm_set_element (resource_manager_t *rm_p, int16_t element)
{
    static const char fname[] = "rm_set_element";

    if (!rm_p) {
        PLAT_ERROR(PLAT_COMMON_F_PREFIX"null resource manager received.\n", fname);
        return;
    }

    if (element < 0 || element >= rm_p->max_element) {
        PLAT_ERROR(PLAT_COMMON_F_PREFIX"element value %d invalid. Max value %d.\n",
               fname, element, rm_p->max_element - 1);
        return;
    }

    rm_p->table[rm_get_table_index(element)] |=
        (1 << rm_get_map_offset(element));
}

/*
 * rm_is_element_set
 *
 * Description:
 *    This function checks if the specified element in the specified
 *    resource manager is set.
 *
 * Parameters:
 *    rm_p    - pointer to the resource manager.
 *    element - element id of element to be checked.
 *
 * Returns:
 *    TRUE if element is set, else FALSE
 */
boolean
rm_is_element_set (resource_manager_t *rm_p, int16_t element)
{
    static const char fname[] = "rm_is_element_set";

    if (!rm_p) {
        PLAT_ERROR(PLAT_COMMON_F_PREFIX"null resource manager received.\n", fname);
        return FALSE;
    }

    if (element < 0 || element >= rm_p->max_element) {
        PLAT_ERROR(PLAT_COMMON_F_PREFIX"element value %d invalid. Max value %d.\n",
               fname, element, rm_p->max_element - 1);
        return FALSE;
    }

    if (rm_p->table[rm_get_table_index(element)] &
        (1 << rm_get_map_offset(element))) {
        return TRUE;
    }

    return FALSE;
}

/*
 * rm_get_free_element
 *
 * Description:
 *    This function walks through the members of the resource manager and
 *    attempts to locate a free element. If a free element is found, the
 *    element's associated bit is set in the resource manager and the
 *    element id is returned.
 *
 * Parameters:
 *    rm_p - pointer to the resource manager.
 *
 * Returns:
 *    If an element is available, a element id (from zero to max element)
 *    If no element is available, -1 is returned.
 */
int16_t
rm_get_free_element (resource_manager_t *rm_p)
{
    static const char fname[] = "rm_get_free_element";
    int16_t element = -1;
    uint16_t i, j;
    uint32_t max_map = 0;

    max_map = ~max_map;

    if (!rm_p) {
        PLAT_ERROR(PLAT_COMMON_F_PREFIX"null resource manager received.\n", fname);
        return -1;
    }

    for (i = 0; i < rm_p->max_index && element == -1; i++) {
        if (rm_p->table[i] != max_map) {
            for (j = 0; j < RM_NUM_ELEMENTS_PER_MAP && element == -1; j++) {
                if (!(rm_p->table[i] & (1 << j))) {
                    element = i * RM_NUM_ELEMENTS_PER_MAP + j;
                    if (element < rm_p->max_element) {
                        rm_set_element(rm_p, element);
                    }
                }
            }
        }
    }

    if (element >= rm_p->max_element) {
        element = -1;
    }
    return (element);
}

/*
 * rm_show
 *
 * Description:
 *    Utility function used to dump the contents of the resource manager.
 *
 * Parameters:
 *    rm_p - pointer to the resource manager.
 *
 * Returns:
 *    none
 */
void
rm_show (resource_manager_t *rm_p)
{
    static const char fname[] = "rm_show";
    int16_t element = 0;
    uint16_t i, j;

    if (!rm_p) {
        PLAT_ERROR(PLAT_COMMON_F_PREFIX"null resource manager received.\n", fname);
        return;
    }

    for (i = 0; i < rm_p->max_index; i++) {
        for (j = 0; j < RM_NUM_ELEMENTS_PER_MAP; j++) {
            if (rm_p->table[i] & (1 << j)) {
                element = (i * RM_NUM_ELEMENTS_PER_MAP) + j;
                TNP_DEBUG(DEB_F_PREFIX"rm map: %d\n", DEB_F_PREFIX_ARGS(RM, fname), element);
            }
        }
    }
}

/*
 * rm_create
 *
 * Description:
 *    Allocates and initializes a new resource manager
 *
 * Parameters:
 *    max_element - Maximum number of elements the resource manager
 *                  is required to track
 *
 * Returns:
 *    If successful, pointer to the newly allocated resource manager
 *    If not successful, NULL
 */
resource_manager_t *
rm_create (int16_t max_element)
{
    static const char fname[] = "rm_create";
    resource_manager_t *rm_p;

    if (max_element < 0) {
        PLAT_ERROR(PLAT_COMMON_F_PREFIX"invalid max element %d received.\n", fname,
               max_element);
        return NULL;
    }

    rm_p = (resource_manager_t *) cpr_malloc(sizeof(resource_manager_t));
    if (!rm_p) {
        PLAT_ERROR(PLAT_COMMON_F_PREFIX"unable to allocate resource manager.\n", fname);
        return NULL;
    }

    rm_p->max_element = max_element;
    rm_p->max_index = max_element / RM_NUM_ELEMENTS_PER_MAP + 1;

    rm_p->table = (uint32_t *)
        cpr_malloc(rm_p->max_index * RM_NUM_ELEMENTS_PER_MAP);
    if (!rm_p->table) {
        free(rm_p);
        return NULL;
    }
    rm_clear_all_elements(rm_p);
    return rm_p;
}

/*
 * rm_free
 *
 * Description:
 *    This function frees the memory allocated for the specified resource manager.
 *
 * Parameters:
 *    rm_p - pointer to the resource manager.
 *
 * Returns:
 *    none
 */
void
rm_destroy (resource_manager_t *rm_p)
{
    static const char fname[] = "rm_destroy";

    if (!rm_p) {
        PLAT_ERROR(PLAT_COMMON_F_PREFIX"null resource manager received.\n", fname);
        return;
    }

    cpr_free(rm_p->table);
    cpr_free(rm_p);
}
