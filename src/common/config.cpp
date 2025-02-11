#include "config.hpp"
#include "file_util.hpp"

#include <iostream>
#include <fstream>
#include <type_traits>
#include <unistd.h>
#include <limits.h>

#include "storage/posix_module.hpp"
#include "storage/posix_agg_module.hpp"

#ifdef WITH_AXL
#include "storage/axl_module.hpp"
#else
using axl_module_t = storage_module_t;
#endif

#ifdef WITH_DAOS
#include "storage/daos_module.hpp"
#else
using daos_module_t = storage_module_t;
#endif

//#define __DEBUG
#include "debug.hpp"

std::ostream *logger = &std::cout;

config_t::config_t(const std::string &f, bool is_backend) : cfg_file(f), reader(cfg_file) {
    if (!cfg_file.empty() && reader.ParseError() < 0)
	FATAL("cannot open config file: " << cfg_file);
    if (reader.ParseError() > 0)
	FATAL("error parsing config file " << cfg_file << " at line " << reader.ParseError());

    // configure logging
    std::string log_prefix = "/dev/shm";
    if (get_optional("log_prefix", log_prefix) || is_backend) {
        DBG("log prefix=" << log_prefix);
	std::string log_file = log_prefix + "/"
	    + (is_backend ? "veloc-backend-" : "veloc-client-")
	    + unique_suffix() + ".log";
        try {
            logger = new std::ofstream(log_file, std::ofstream::out | std::ofstream::trunc);
        } catch(std::exception &e) {
            FATAL("cannot log to " << log_file << ", error: " << e.what());
        }
    }

    std::string val, scratch, persistent;
    // set sync or async mode
    if (get_optional("mode", val)) {
        if (val != "sync" && val != "async")
            FATAL("mode of operation " << val << " is invalid, must be sync/async!");
        sync_mode = (val == "sync");
    }
    // initialize scratch directory
    if (!check_dir(scratch = get("scratch")))
	FATAL("scratch directory " << scratch << " inaccessible!");

    // configure persistent storage
    if (get_optional("daos_pool", persistent) && get_optional("daos_cont", val)) {
        if constexpr(!std::is_same<daos_module_t, storage_module_t>::value) {
            INFO("using DAOS to interact with persistent storage, pool/container: " << persistent << "/" << val);
            sm = new daos_module_t(scratch, persistent, val);
        } else
            FATAL("DAOS requested but not available at compile time, please link with DAOS");
    } else if (get_optional("persistent", persistent)) {
        if (get_optional("axl_type", val)) {
            if constexpr(!std::is_same<axl_module_t, storage_module_t>::value) {
                INFO("using AXL to interact with persistent storage, AXL type: " << val);
                sm = new axl_module_t(scratch, persistent, val);
            } else
                FATAL("AXL requested but not available at compile time, please link with AXL");
        } else {
            if (get_optional("aggregated", false)) {
                INFO("using POSIX to interact with persistent storage in aggregated file mode, path: " << persistent);
                std::string meta;
                get_optional("meta", meta);
                sm = new posix_agg_module_t(scratch, persistent, meta);
            } else {
                INFO("using POSIX to interact with persistent storage in single file mode, path: " << persistent);
                sm = new posix_module_t(scratch, persistent);
            }
        }
    }
}

config_t::~config_t() {
    delete sm;
    if (logger != &std::cout) {
        delete logger;
        logger = &std::cout;
    }
}

std::string config_t::env_param(const std::string &param) {
    std::string ret = param;
    for (auto &c: ret)
	c = toupper(c);
    char *env = getenv(("VELOC_" + ret).c_str());
    if (env == NULL)
	return "";
    return env;
}

std::string config_t::get(const std::string &param) const {
    std::string ret;
    if (!get_optional(param, ret))
        FATAL("config parameter " << param << " missing or empty");
    return ret;
}

bool config_t::get_optional(const std::string &param, std::string &value) const {
    auto ret = env_param(param);
    if (ret.empty())
	ret = reader.Get("", param, "");
    if (ret.empty())
	return false;
    value = ret;
    return true;
}

bool config_t::get_optional(const std::string &param, int &value) const {
    std::string ret;
    if (!get_optional(param, ret))
	return false;
    try {
	value = std::stoi(ret);
    } catch (std::exception &e) {
	return false;
    }
    return true;
}

bool config_t::get_optional(const std::string &param, bool def) const {
    std::string ret;
    if (!get_optional(param, ret))
	return false;
    for (auto &c: ret)
	c = tolower(c);
    return ret == "true" ? true : false;
}
