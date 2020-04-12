#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* libsail-common */
#include "error.h"
#include "log.h"
#include "utils.h"

#include "ini.h"
#include "plugin_info.h"
#include "string_node.h"

/*
 * Private functions.
 */

static sail_error_t alloc_string_node(struct sail_string_node **string_node) {

    *string_node = (struct sail_string_node *)malloc(sizeof(struct sail_string_node));

    if (*string_node == NULL) {
        return SAIL_MEMORY_ALLOCATION_FAILED;
    }

    (*string_node)->value = NULL;
    (*string_node)->next  = NULL;

    return 0;
}

static void destroy_string_node(struct sail_string_node *string_node) {

    if (string_node == NULL) {
        return;
    }

    free(string_node->value);
    free(string_node);
}

static void destroy_string_node_chain(struct sail_string_node *string_node) {

    while (string_node != NULL) {
        struct sail_string_node *string_node_next = string_node->next;

        destroy_string_node(string_node);

        string_node = string_node_next;
    }
}

static sail_error_t split_into_string_node_chain(const char *value, struct sail_string_node **target_string_node) {

    struct sail_string_node *last_string_node = NULL;

    while (*(value += strspn(value, ";")) != '\0') {
        size_t length = strcspn(value, ";");

        struct sail_string_node *extension_node;

        SAIL_TRY(alloc_string_node(&extension_node));

        SAIL_TRY_OR_CLEANUP(sail_strdup_length(value, length, &extension_node->value),
                            /* cleanup */ destroy_string_node(extension_node));

        sail_to_lower(extension_node->value);

        if (*target_string_node == NULL) {
            *target_string_node = last_string_node = extension_node;
        } else {
            last_string_node->next = extension_node;
            last_string_node = extension_node;
        }

        value += length;
    }

    return 0;
}

static int inih_handler(void *data, const char *section, const char *name, const char *value) {

    (void)section;

    struct sail_plugin_info *plugin_info = (struct sail_plugin_info *)data;

    int res;

    if (strcmp(name, "layout") == 0) {
        plugin_info->layout = atoi(value);

        if (plugin_info->layout < 1) {
            SAIL_LOG_ERROR("Failed to convert '%s' to a plugin layout version", value);
            return 0;
        }

        return 1;
    }

    if (plugin_info->layout == 0) {
        SAIL_LOG_ERROR("Plugin layout version is unset. Make sure a plugin layout version is the very first key in the plugin info file");
        return 0;
    }

    if (plugin_info->layout >= 1 && plugin_info->layout <= 2) {
        if (strcmp(name, "version") == 0) {
            if ((res = sail_strdup(value, &plugin_info->version)) != 0) {
                return 0;
            }
        } else if (strcmp(name, "description") == 0) {
            if ((res = sail_strdup(value, &plugin_info->description)) != 0) {
                return 0;
            }
        } else if (strcmp(name, "extensions") == 0) {
            if (split_into_string_node_chain(value, &plugin_info->extension_node) != 0) {
                return 0;
            }
        } else if (strcmp(name, "mime-types") == 0) {
            if (split_into_string_node_chain(value, &plugin_info->mime_type_node) != 0) {
                return 0;
            }
        } else {
            SAIL_LOG_ERROR("Unsupported plugin configuraton key '%s'", name);
            return 0;  /* error */
        }
    } else {
        SAIL_LOG_ERROR("Unsupported plugin layout version %d", plugin_info->layout);
        return 0;
    }

    return 1;
}

/*
 * Public functions.
 */

int sail_alloc_plugin_info(struct sail_plugin_info **plugin_info) {

    *plugin_info = (struct sail_plugin_info *)malloc(sizeof(struct sail_plugin_info));

    if (*plugin_info == NULL) {
        return SAIL_MEMORY_ALLOCATION_FAILED;
    }

    (*plugin_info)->layout         = 0;
    (*plugin_info)->version        = NULL;
    (*plugin_info)->description    = NULL;
    (*plugin_info)->extension_node = NULL;
    (*plugin_info)->mime_type_node = NULL;
    (*plugin_info)->path           = NULL;

    return 0;
}

void sail_destroy_plugin_info(struct sail_plugin_info *plugin_info) {

    if (plugin_info == NULL) {
        return;
    }

    free(plugin_info->version);
    free(plugin_info->description);

    destroy_string_node_chain(plugin_info->extension_node);
    destroy_string_node_chain(plugin_info->mime_type_node);

    free(plugin_info->path);
    free(plugin_info);
}

sail_error_t sail_alloc_plugin_info_node(struct sail_plugin_info_node **plugin_info_node) {

    *plugin_info_node = (struct sail_plugin_info_node *)malloc(sizeof(struct sail_plugin_info_node));

    if (*plugin_info_node == NULL) {
        return SAIL_MEMORY_ALLOCATION_FAILED;
    }

    (*plugin_info_node)->plugin_info = NULL;
    (*plugin_info_node)->next        = NULL;

    return 0;
}

void sail_destroy_plugin_info_node(struct sail_plugin_info_node *plugin_info_node) {

    if (plugin_info_node == NULL) {
        return;
    }

    sail_destroy_plugin_info(plugin_info_node->plugin_info);

    free(plugin_info_node);
}

void sail_destroy_plugin_info_node_chain(struct sail_plugin_info_node *plugin_info_node) {

    while (plugin_info_node != NULL) {
        struct sail_plugin_info_node *plugin_info_node_next = plugin_info_node->next;

        sail_destroy_plugin_info_node(plugin_info_node);

        plugin_info_node = plugin_info_node_next;
    }
}

int sail_plugin_read_info(const char *file, struct sail_plugin_info **plugin_info) {

    if (file == NULL) {
        return SAIL_INVALID_ARGUMENT;
    }

    SAIL_TRY(sail_alloc_plugin_info(plugin_info));

    /*
     * Returns 0 on success, line number of first error on parse error (doesn't
     * stop on first error), -1 on file open error, or -2 on memory allocation
     * error (only when INI_USE_STACK is zero).
     */
    const int code = ini_parse(file, inih_handler, *plugin_info);

    if (code == 0) {
        return 0;
    }

    sail_destroy_plugin_info(*plugin_info);

    switch (code) {
        case -1: return SAIL_FILE_OPEN_ERROR;
        case -2: return SAIL_MEMORY_ALLOCATION_FAILED;

        default: return SAIL_FILE_PARSE_ERROR;
    }
}