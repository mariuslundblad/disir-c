
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include <disir/disir.h>
#include <disir/io.h>

#include "log.h"
#include "mold.h"
#include "config.h"
#include "mqueue.h"
#include "disir_private.h"

enum disir_status
disir_register_input (struct disir_instance *disir, const char *type,
                      const char *description, struct disir_input_plugin *plugin)
{
    enum disir_status status;
    struct disir_input *input;

    if (type == NULL || description == NULL || plugin == NULL)
    {
        log_debug (0, "invoked with NULL pointer(s). (%p %p %p)",
                   type, description, plugin);
        return DISIR_STATUS_INVALID_ARGUMENT;
    }

    if (plugin->in_struct_size != sizeof (struct disir_input_plugin))
    {
        // TODO: Log disir
        disir_error_set(disir, "%s input plugin structure mismatched structure size (%d vs %d)",
                  type, plugin->in_struct_size, sizeof (struct disir_input_plugin));
        return DISIR_STATUS_INVALID_ARGUMENT;
    }

    // TODO: Check existance of type in disir
    // return DISIR_STATUS_EXIST

    input = calloc (1, sizeof (struct disir_input));
    if (input == NULL)
    {
        status = DISIR_STATUS_NO_MEMORY;
        goto error;
    }

    input->di_type = strndup (type, DISIR_IO_TYPE_MAXLENGTH);
    if (input->di_type == NULL)
    {
        status = DISIR_STATUS_NO_MEMORY;
        goto error;
    }

    input->di_description = strndup (description, DISIR_IO_DESCRIPTION_MAXLENGTH);
    if (input->di_description == NULL)
    {
        status = DISIR_STATUS_NO_MEMORY;
        goto error;
    }

    // Copy the number of bytes, validated, from the input structure size member
    memcpy (&input->di_input, plugin, plugin->in_struct_size);

    MQ_PUSH (disir->dio_input_queue, input);

    return DISIR_STATUS_OK;
error:
    if (input)
    {
        free (input);
    }
    return status;
}

enum disir_status
disir_register_output (struct disir_instance *disir, const char *type, const char *description,
                       struct disir_output_plugin *plugin)
{
    enum disir_status status;
    struct disir_output *output;

    if (type == NULL || description == NULL || plugin == NULL)
    {
        log_debug (0, "invoked with NULL pointer(s). (%p %p %p)",
                   type, description, plugin);
        return DISIR_STATUS_INVALID_ARGUMENT;
    }

    if (plugin->out_struct_size != sizeof (struct disir_output_plugin))
    {
        // TODO: Log disir
        disir_error_set (disir, "%s output plugin structure mismatched structure size (%d vs %d)",
                  type, plugin->out_struct_size, sizeof (struct disir_output_plugin));
        return DISIR_STATUS_INVALID_ARGUMENT;
    }

    output = calloc (1, sizeof (struct disir_output));
    if (output == NULL)
    {
        status = DISIR_STATUS_NO_MEMORY;
        goto error;
    }

    output->do_type = strndup (type, DISIR_IO_TYPE_MAXLENGTH);
    if (output->do_type == NULL)
    {
        status = DISIR_STATUS_NO_MEMORY;
        goto error;
    }

    output->do_description = strndup (description, DISIR_IO_DESCRIPTION_MAXLENGTH);
    if (output->do_description == NULL)
    {
        status = DISIR_STATUS_NO_MEMORY;
        goto error;
    }

    // Copy the number of bytes, validated, from the input structure size member
    memcpy (&output->do_output, plugin, plugin->out_struct_size);

    MQ_PUSH (disir->dio_output_queue, output);

    return DISIR_STATUS_OK;
error:
    if (output)
    {
        free (output);
    }
    return status;
}

//! PUBLIC API
enum disir_status
disir_config_input (struct disir_instance *disir, const char *type, const char *id,
                    struct disir_mold *mold, struct disir_config **config)
{
    enum disir_status status;
    struct disir_input *input;

    input = MQ_FIND (disir->dio_input_queue,
                      (strncmp(entry->di_type, type, DISIR_IO_TYPE_MAXLENGTH) == 0));
    if (input == NULL)
    {
        disir_error_set (disir, "no input type '%s' registered.", type);
        status = DISIR_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        status = input->di_input.in_config_read (disir, id, mold, config);
    }
    return status;
}

//! PUBLIC API
enum disir_status
disir_mold_input (struct disir_instance *instance, const char *type, const char *id,
                    struct disir_mold **mold)
{
    enum disir_status status;
    struct disir_input *input;

    if (instance == NULL || type == NULL || id == NULL || mold == NULL)
    {
        log_debug (0, "invoked with NULL pointer(s) (%p %p %p %p)", instance, type, id, mold);
        return DISIR_STATUS_INVALID_ARGUMENT;
    }

    input = MQ_FIND (instance->dio_input_queue,
                      (strncmp(entry->di_type, type, DISIR_IO_TYPE_MAXLENGTH) == 0));
    if (input== NULL)
    {
        disir_error_set (instance, "no input type '%s' registered.", type);
        status = DISIR_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        status = input->di_input.in_mold_read (instance, id, mold);
    }
    return status;
}

//! PUBLIC API
enum disir_status
disir_config_output (struct disir_instance *disir, const char *type, const char *id,
                     struct disir_config *config)
{
    enum disir_status status;
    struct disir_output *output;

    if (disir == NULL || type == NULL || config == NULL || id == NULL)
    {
        log_debug (0, "invoked with NULL parameters (%p %p %p %p)",
                   disir, type, id, config);
        return DISIR_STATUS_INVALID_ARGUMENT;
    }

    output = MQ_FIND (disir->dio_output_queue,
                      (strncmp(entry->do_type, type, DISIR_IO_TYPE_MAXLENGTH) == 0));
    if (output == NULL)
    {
        disir_error_set (disir, "no input type '%s' registered.", type);
        status = DISIR_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        status = output->do_output.out_config_write (disir, id, config);
    }
    return status;
}

//! PUBLIC API
enum disir_status
disir_mold_output (struct disir_instance *disir, const char *type, const char *id,
                     struct disir_mold *mold)
{
    enum disir_status status;
    struct disir_output *output;

    if (disir == NULL || type == NULL || mold == NULL || id == NULL)
    {
        log_debug (0, "invoked with NULL parameters (%p %p %p %p)",
                   disir, type, id, mold);
        return DISIR_STATUS_INVALID_ARGUMENT;
    }

    output = MQ_FIND (disir->dio_output_queue,
                      (strncmp(entry->do_type, type, DISIR_IO_TYPE_MAXLENGTH) == 0));
    if (output == NULL)
    {
        disir_error_set (disir, "no input type '%s' registered.", type);
        status = DISIR_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        status = output->do_output.out_mold_write (disir, id, mold);
    }
    return status;
}

//! PUBLIC API
enum disir_status
disir_config_list (struct disir_instance *disir, const char *type,
                   struct disir_collection **collection)
{
    enum disir_status status;
    struct disir_input  *input;

    if (disir == NULL || type == NULL || collection == NULL)
    {
        log_debug (0, "invoked with NULL parameters (%p %p %p)",
                   disir, type, collection);
        return DISIR_STATUS_INVALID_ARGUMENT;
    }

    input = MQ_FIND (disir->dio_input_queue,
                      (strncmp(entry->di_type, type, DISIR_IO_TYPE_MAXLENGTH) == 0));

    if (input == NULL)
    {
        disir_error_set (disir, "no input type '%s' registered.", type);
        status = DISIR_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        status = input->di_input.in_config_list (disir, collection);
    }

    return status;
}

//! PUBLIC API
enum disir_status
disir_input_plugin_list (struct disir_instance *disir, struct disir_collection **collection)
{
    struct disir_collection *col;
    struct disir_context *context;

    context = NULL;

    if (disir == NULL || collection == NULL)
    {
        log_debug (0, "invoked with NULL pointers (%p %p)", disir, collection);
        return DISIR_STATUS_INVALID_ARGUMENT;
    }

    // No registered entries
    if (disir->dio_input_queue == NULL)
    {
        return DISIR_STATUS_EXHAUSTED;
    }

    col = dc_collection_create ();
    if (col == NULL)
    {
        return DISIR_STATUS_NO_MEMORY;
    }

    MQ_FOREACH (disir->dio_input_queue,
    ({
        dc_free_text_create (entry->di_type, &context);
        dc_collection_push_context (col, context);
        dc_putcontext (&context);
    }));

    *collection = col;
    return DISIR_STATUS_OK;
}

//! PUBLIC API
enum disir_status
disir_config_finished (struct disir_config **config)
{
    enum disir_status status;
    struct disir_context *context;

    context = (*config)->cf_context;
    status = dc_destroy (&context);
    if (status == DISIR_STATUS_OK)
        *config = NULL;
    return status;
}

//! PUBLIC API
enum disir_status
disir_mold_finished (struct disir_mold **mold)
{
    enum disir_status status;
    struct disir_context *context;

    if (mold == NULL || *mold == NULL)
    {
        log_debug (0, "invoked with NULL mold pointer");
        return DISIR_STATUS_INVALID_ARGUMENT;
    }

    TRACE_ENTER ("mold: %p", *mold);

    context = (*mold)->mo_context;
    status = dc_destroy (&context);
    if (status == DISIR_STATUS_OK)
        *mold = NULL;

    TRACE_EXIT ("status: %s", disir_status_string (status));
    return status;
}

//! INTERNAL API
struct disir_output *
dx_disir_output_create (void)
{
    struct disir_output *output;

    output = calloc (1, sizeof (struct disir_output));
    if (output == NULL)
    {
        return NULL;
    }

    return output;
}

//! INTERNAL API
enum disir_status
dx_disir_output_destroy (struct disir_output **output)
{
    if (output || *output)
    {
        free (*output);
        *output  = NULL;
    }

    // TODO: REMOVE FROM PARENT

    return DISIR_STATUS_OK;
}

//! INTERNAL API
struct disir_input *
dx_disir_input_create (void)
{
    struct disir_input *input;

    input = calloc (1, sizeof (struct disir_input));
    if (input == NULL)
    {
        return NULL;
    }

    return input;
}

//! INTERNAL API
enum disir_status
dx_disir_input_destroy (struct disir_input **input)
{
    if (input || *input)
    {
        free (*input);
        *input= NULL;
    }

    // TODO: REMOVE FROM PARENT

    return DISIR_STATUS_OK;
}
