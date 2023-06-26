#ifndef VELOC_LIBPRESSIO
#define VELOC_LIBPRESSIO

#include <iostream>
#include <functional>
#include <libpressio_ext/cpp/libpressio.h>

namespace veloc::libpressio {
template <typename T> std::function<void (std::ostream &)> serializer(pressio_compressor& comp, T* data, std::vector<size_t> dims) {
    return [data, dims, comp](std::ostream &out) {

        pressio_data input = pressio_data::nonowning(
                pressio_dtype_from_type<T>(),
                data,
                dims
                );
        pressio_data compressed = pressio_data::empty(pressio_byte_dtype, {});
        if(comp->compress(&input, &compressed) > 0) {
            //TODO handle error
            std::cerr << comp->error_msg() << std::endl;
        }
        uint64_t size = static_cast<uint64_t>(compressed.size_in_bytes());
        out.write((char*)&size, sizeof(uint64_t));
        out.write((char*)compressed.data(), compressed.size_in_bytes());
    };
}

template <typename T> std::function<bool (std::istream &)> deserializer(pressio_compressor& comp, T* data, std::vector<size_t> dims) {
    return [data, dims, comp](std::istream &in) {
        try {
            size_t compressed_size = 0;
            in.read((char*)&compressed_size, sizeof(uint64_t));
            std::vector<char> input_data(compressed_size);

            in.read(input_data.data(), compressed_size);
            pressio_data input = pressio_data::nonowning(pressio_byte_dtype, input_data.data(), {compressed_size});
            pressio_data output = pressio_data::empty(
                    pressio_dtype_from_type<T>(),
                    dims
                    );
            if(comp->decompress(&input, &output) > 0) {
                //TODO handle error
                return false;
            }
            memcpy(data, output.data(), output.size_in_bytes());
        } catch (std::exception &e) {
            return false;
        }
        return true;
    };
}
}


#endif /* end of include guard: VELOC_LIBPRESSIO */
