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

class SAIL_HIDDEN image_input::pimpl
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
            SAIL_LOG_ERROR("Reading operation is in progress. Stop it before starting a new one");
            return SAIL_ERROR_CONFLICTING_OPERATION;
        }

        return SAIL_OK;
    }

    void *state;
    struct sail_io *sail_io;
};

image_input::image_input()
    : d(new pimpl)
{
}

image_input::~image_input()
{
    stop();
    delete d;
}

std::tuple<image, codec_info> image_input::probe(const std::string_view path) const
{
    const sail_codec_info *sail_codec_info;
    sail_image *sail_image = nullptr;

    SAIL_AT_SCOPE_EXIT(
        sail_destroy_image(sail_image);
    );

    SAIL_TRY_OR_EXECUTE(sail_probe_file(path.data(), &sail_image, &sail_codec_info),
                        /* on error */ return {});

    return { image(sail_image), codec_info(sail_codec_info) };
}

std::tuple<image, codec_info> image_input::probe(const void *buffer, size_t buffer_length) const
{
    const sail_codec_info *sail_codec_info;
    sail_image *sail_image = nullptr;

    SAIL_AT_SCOPE_EXIT(
        sail_destroy_image(sail_image);
    );

    SAIL_TRY_OR_EXECUTE(sail_probe_mem(buffer, buffer_length, &sail_image, &sail_codec_info),
                        /* on error */ return {});

    return { image(sail_image), codec_info(sail_codec_info) };
}

std::tuple<image, codec_info> image_input::probe(const sail::io &io) const
{
    SAIL_TRY_OR_EXECUTE(io.verify_valid(),
                        /* on error */ return {});

    struct sail_io *sail_io = nullptr;
    const sail_codec_info *sail_codec_info;
    sail_image *sail_image = nullptr;

    SAIL_TRY_OR_EXECUTE(io.to_sail_io(&sail_io),
                        /* on error */ return {});

    SAIL_AT_SCOPE_EXIT(
        sail_destroy_image(sail_image);
        sail_destroy_io(sail_io);
    );

    SAIL_TRY_OR_EXECUTE(sail_probe_io(sail_io, &sail_image, &sail_codec_info),
                        /* on error */ return {});

    return { image(sail_image), codec_info(sail_codec_info) };
}

image image_input::read(const std::string_view path) const
{
    sail_image *sail_image = nullptr;

    SAIL_AT_SCOPE_EXIT(
        sail_destroy_image(sail_image);
    );

    SAIL_TRY_OR_EXECUTE(sail_read_file(path.data(), &sail_image),
                        /* on error */ return {});

    const sail::image image(sail_image);
    sail_image->pixels = nullptr;

    return image;
}

image image_input::read(const void *buffer, size_t buffer_length) const
{
    sail_image *sail_image = nullptr;

    SAIL_AT_SCOPE_EXIT(
        sail_destroy_image(sail_image);
    );

    SAIL_TRY_OR_EXECUTE(sail_read_mem(buffer, buffer_length, &sail_image),
                        /* on error */ return {});

    const sail::image image(sail_image);
    sail_image->pixels = nullptr;

    return image;
}

sail_status_t image_input::start(const std::string_view path)
{
    SAIL_TRY(d->ensure_state_is_null());

    SAIL_TRY(sail_start_reading_file(path.data(), nullptr, &d->state));

    return SAIL_OK;
}

sail_status_t image_input::start(const std::string_view path, const sail::codec_info &codec_info)
{
    SAIL_TRY(d->ensure_state_is_null());

    SAIL_TRY(sail_start_reading_file(path.data(), codec_info.sail_codec_info_c(), &d->state));

    return SAIL_OK;
}

sail_status_t image_input::start(const std::string_view path, const sail::codec_info &codec_info, const sail::read_options &read_options)
{
    SAIL_TRY(d->ensure_state_is_null());

    sail_read_options sail_read_options;
    SAIL_TRY(read_options.to_sail_read_options(&sail_read_options));

    SAIL_TRY(sail_start_reading_file_with_options(path.data(), codec_info.sail_codec_info_c(), &sail_read_options, &d->state));

    return SAIL_OK;
}

sail_status_t image_input::start(const void *buffer, size_t buffer_length)
{
    SAIL_TRY(d->ensure_state_is_null());

    SAIL_TRY(sail_start_reading_mem(buffer, buffer_length, nullptr, &d->state));

    return SAIL_OK;
}

sail_status_t image_input::start(const void *buffer, size_t buffer_length, const sail::codec_info &codec_info)
{
    SAIL_TRY(d->ensure_state_is_null());

    SAIL_TRY(sail_start_reading_mem(buffer, buffer_length, codec_info.sail_codec_info_c(), &d->state));

    return SAIL_OK;
}

sail_status_t image_input::start(const void *buffer, size_t buffer_length, const sail::read_options &read_options)
{
    SAIL_TRY(d->ensure_state_is_null());

    sail_read_options sail_read_options;
    SAIL_TRY(read_options.to_sail_read_options(&sail_read_options));

    SAIL_TRY(sail_start_reading_mem_with_options(buffer, buffer_length, nullptr, &sail_read_options, &d->state));

    return SAIL_OK;
}

sail_status_t image_input::start(const void *buffer, size_t buffer_length, const sail::codec_info &codec_info, const sail::read_options &read_options)
{
    SAIL_TRY(d->ensure_state_is_null());

    sail_read_options sail_read_options;
    SAIL_TRY(read_options.to_sail_read_options(&sail_read_options));

    SAIL_TRY(sail_start_reading_mem_with_options(buffer, buffer_length, codec_info.sail_codec_info_c(), &sail_read_options, &d->state));

    return SAIL_OK;
}

sail_status_t image_input::start(const sail::io &io)
{
    SAIL_TRY(d->ensure_state_is_null());

    SAIL_TRY(io.to_sail_io(&d->sail_io));
    SAIL_TRY(sail_check_io_valid(d->sail_io));

    SAIL_TRY(sail_start_reading_io(d->sail_io, nullptr, &d->state));

    return SAIL_OK;
}

sail_status_t image_input::start(const sail::io &io, const sail::codec_info &codec_info)
{
    SAIL_TRY(d->ensure_state_is_null());

    SAIL_TRY(io.to_sail_io(&d->sail_io));
    SAIL_TRY(sail_check_io_valid(d->sail_io));

    SAIL_TRY(sail_start_reading_io(d->sail_io, codec_info.sail_codec_info_c(), &d->state));

    return SAIL_OK;
}

sail_status_t image_input::start(const sail::io &io, const sail::read_options &read_options)
{
    SAIL_TRY(d->ensure_state_is_null());

    SAIL_TRY(io.to_sail_io(&d->sail_io));
    SAIL_TRY(sail_check_io_valid(d->sail_io));

    sail_read_options sail_read_options;
    SAIL_TRY(read_options.to_sail_read_options(&sail_read_options));

    SAIL_TRY(sail_start_reading_io_with_options(d->sail_io, nullptr, &sail_read_options, &d->state));

    return SAIL_OK;
}

sail_status_t image_input::start(const sail::io &io, const sail::codec_info &codec_info, const sail::read_options &read_options)
{
    SAIL_TRY(d->ensure_state_is_null());

    SAIL_TRY(io.to_sail_io(&d->sail_io));
    SAIL_TRY(sail_check_io_valid(d->sail_io));

    sail_read_options sail_read_options;
    SAIL_TRY(read_options.to_sail_read_options(&sail_read_options));

    SAIL_TRY(sail_start_reading_io_with_options(d->sail_io, codec_info.sail_codec_info_c(), &sail_read_options, &d->state));

    return SAIL_OK;
}

sail_status_t image_input::next_frame(sail::image *image)
{
    SAIL_CHECK_IMAGE_PTR(image);

    sail_image *sail_image = nullptr;

    SAIL_AT_SCOPE_EXIT(
        sail_destroy_image(sail_image);
    );

    SAIL_TRY(sail_read_next_frame(d->state, &sail_image));

    *image = sail::image(sail_image);
    sail_image->pixels = nullptr;

    return SAIL_OK;
}

sail_status_t image_input::stop()
{
    sail_status_t saved_status = SAIL_OK;
    SAIL_TRY_OR_EXECUTE(sail_stop_reading(d->state),
                        /* on error */ saved_status = __sail_error_result);
    d->state = nullptr;

    sail_destroy_io(d->sail_io);
    d->sail_io = nullptr;

    return saved_status;
}

}
