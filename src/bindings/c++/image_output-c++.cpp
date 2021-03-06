/*  This file is part of SAIL (https://github.com/smoked-herring/sail)

    Copyright (c) 2020 Dmitry Baryshev

    The MIT License

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include <cstdlib>
#include <cstring>

#include "sail-common.h"
#include "sail.h"
#include "sail-c++.h"

namespace sail
{

class SAIL_HIDDEN image_output::pimpl
{
public:
    pimpl()
        : state(nullptr)
        , sail_io(nullptr)
    {
    }

    sail_status_t ensure_state_is_null()
    {
        if (state != nullptr) {
            SAIL_LOG_ERROR("Writing operation is in progress. Stop it before starting a new one");
            return SAIL_ERROR_CONFLICTING_OPERATION;
        }

        return SAIL_OK;
    }

    void *state;
    struct sail_io *sail_io;
};

image_output::image_output()
    : d(new pimpl)
{
}

image_output::~image_output()
{
    stop();
    delete d;
}

sail_status_t image_output::write(const std::string_view path, const sail::image &image) const
{
    sail_image *sail_image = nullptr;
    SAIL_TRY(image.to_sail_image(&sail_image));

    SAIL_AT_SCOPE_EXIT(
        sail_image->pixels = nullptr;
        sail_destroy_image(sail_image);
    );

    SAIL_TRY(sail_write_file(path.data(), sail_image));

    return SAIL_OK;
}

sail_status_t image_output::write(void *buffer, size_t buffer_length, const sail::image &image) const
{
    SAIL_TRY(write(buffer, buffer_length, image, nullptr));

    return SAIL_OK;
}

sail_status_t image_output::write(void *buffer, size_t buffer_length, const sail::image &image, size_t *written) const
{
    SAIL_CHECK_BUFFER_PTR(buffer);

    sail_image *sail_image = nullptr;
    SAIL_TRY(image.to_sail_image(&sail_image));

    SAIL_AT_SCOPE_EXIT(
        sail_image->pixels = nullptr;
        sail_destroy_image(sail_image);
    );

    SAIL_TRY(sail_write_mem(buffer, buffer_length, sail_image, written));

    return SAIL_OK;
}

sail_status_t image_output::start(const std::string_view path)
{
    SAIL_TRY(d->ensure_state_is_null());

    SAIL_TRY(sail_start_writing_file(path.data(), nullptr, &d->state));

    return SAIL_OK;
}

sail_status_t image_output::start(const std::string_view path, const sail::codec_info &codec_info)
{
    SAIL_TRY(d->ensure_state_is_null());

    SAIL_TRY(sail_start_writing_file(path.data(), codec_info.sail_codec_info_c(), &d->state));

    return SAIL_OK;
}

sail_status_t image_output::start(const std::string_view path, const sail::write_options &write_options)
{
    SAIL_TRY(d->ensure_state_is_null());

    sail_write_options sail_write_options;
    SAIL_TRY(write_options.to_sail_write_options(&sail_write_options));

    SAIL_TRY(sail_start_writing_file_with_options(path.data(), nullptr, &sail_write_options, &d->state));

    return SAIL_OK;
}

sail_status_t image_output::start(const std::string_view path, const sail::codec_info &codec_info, const sail::write_options &write_options)
{
    SAIL_TRY(d->ensure_state_is_null());

    sail_write_options sail_write_options;
    SAIL_TRY(write_options.to_sail_write_options(&sail_write_options));

    SAIL_TRY(sail_start_writing_file_with_options(path.data(), codec_info.sail_codec_info_c(), &sail_write_options, &d->state));

    return SAIL_OK;
}

sail_status_t image_output::start(void *buffer, size_t buffer_length, const sail::codec_info &codec_info)
{
    SAIL_TRY(d->ensure_state_is_null());

    SAIL_TRY(sail_start_writing_mem(buffer,
                                    buffer_length,
                                    codec_info.sail_codec_info_c(),
                                    &d->state));

    return SAIL_OK;
}

sail_status_t image_output::start(void *buffer, size_t buffer_length, const sail::codec_info &codec_info, const sail::write_options &write_options)
{
    SAIL_TRY(d->ensure_state_is_null());

    sail_write_options sail_write_options;
    SAIL_TRY(write_options.to_sail_write_options(&sail_write_options));

    SAIL_TRY(sail_start_writing_mem_with_options(buffer,
                                                 buffer_length,
                                                 codec_info.sail_codec_info_c(),
                                                 &sail_write_options,
                                                 &d->state));

    return SAIL_OK;
}

sail_status_t image_output::start(const sail::io &io, const sail::codec_info &codec_info)
{
    SAIL_TRY(io.to_sail_io(&d->sail_io));
    SAIL_TRY(sail_check_io_valid(d->sail_io));

    SAIL_TRY(sail_start_writing_io_with_options(d->sail_io,
                                                codec_info.sail_codec_info_c(),
                                                nullptr,
                                                &d->state));

    return SAIL_OK;
}

sail_status_t image_output::start(const sail::io &io, const sail::codec_info &codec_info, const sail::write_options &write_options)
{
    SAIL_TRY(io.to_sail_io(&d->sail_io));
    SAIL_TRY(sail_check_io_valid(d->sail_io));

    sail_write_options sail_write_options;
    SAIL_TRY(write_options.to_sail_write_options(&sail_write_options));

    SAIL_TRY(sail_start_writing_io_with_options(d->sail_io,
                                                codec_info.sail_codec_info_c(),
                                                &sail_write_options,
                                                &d->state));

    return SAIL_OK;
}

sail_status_t image_output::next_frame(const sail::image &image) const
{
    sail_image *sail_image = nullptr;
    SAIL_TRY(image.to_sail_image(&sail_image));

    SAIL_AT_SCOPE_EXIT(
        sail_image->pixels = nullptr;
        sail_destroy_image(sail_image);
    );

    SAIL_TRY(sail_write_next_frame(d->state, sail_image));

    return SAIL_OK;
}

sail_status_t image_output::stop()
{
    size_t written;
    SAIL_TRY(stop(&written));

    (void)written;

    return SAIL_OK;
}

sail_status_t image_output::stop(size_t *written)
{
    sail_status_t saved_status = SAIL_OK;
    SAIL_TRY_OR_EXECUTE(sail_stop_writing_with_written(d->state, written),
                        /* on error */ saved_status = __sail_error_result);
    d->state = nullptr;

    sail_destroy_io(d->sail_io);
    d->sail_io = nullptr;

    return saved_status;
}

}
