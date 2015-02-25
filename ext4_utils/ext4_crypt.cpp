#include "ext4_crypt.h"

#include <string>
#include <fstream>
#include <map>

#include <errno.h>
#include <sys/mount.h>
#include <cutils/properties.h>

// ext4enc:TODO Use include paths
#include "../../core/init/log.h"

// ext4enc::TODO remove this duplicated const
static const std::string unencrypted_path = "/unencrypted";

static std::map<std::string, std::string> s_password_store;

bool e4crypt_non_default_key(const char* dir)
{
    int type = e4crypt_get_password_type(dir);
    return type != -1 && type != 1;
}

int e4crypt_get_password_type(const char* path)
{
    auto full_path = std::string() + path + unencrypted_path;
    if (!std::ifstream(full_path + "/key")) {
        INFO("No master key, so not ext4enc\n");
        return -1;
    }

    std::ifstream type(full_path + "/type");
    if (!type) {
        INFO("No password type so default\n");
        return 1; // Default
    }

    int value = 0;
    type >> value;
    INFO("Password type is %d\n", value);
    return value;
}

int e4crypt_change_password(const char* path, int crypt_type,
                            const char* password)
{
    // ext4enc:TODO Encrypt master key with password securely. Store hash of
    // master key for validation
    auto full_path = std::string() + path + unencrypted_path;
    std::ofstream(full_path + "/password") << password;
    std::ofstream(full_path + "/type") << crypt_type;
    return 0;
}

int  e4crypt_crypto_complete(const char* path)
{
    INFO("ext4 crypto complete called on %s\n", path);

    auto full_path = std::string() + path + unencrypted_path;
    if (!std::ifstream(full_path + "/key")) {
        INFO("No master key, so not ext4enc\n");
        return -1;
    }

    return 0;
}

int e4crypt_check_passwd(const char* path, const char* password)
{
    auto full_path = std::string() + path + unencrypted_path;
    if (!std::ifstream(full_path + "/key")) {
        INFO("No master key, so not ext4enc\n");
        return -1;
    }

    std::string actual_password;
    std::ifstream(full_path + "/password") >> actual_password;

    if (actual_password == password) {
        s_password_store[path] = password;
        return 0;
    } else {
        return -1;
    }
}

int e4crypt_restart(const char* path)
{
    int rc = 0;

    INFO("ext4 restart called on %s\n", path);
    property_set("vold.decrypt", "trigger_reset_main");
    INFO("Just asked init to shut down class main\n");
    sleep(2);

    std::string tmp_path = std::string() + path + "/tmp_mnt";

    // ext4enc:TODO add retry logic
    rc = umount(tmp_path.c_str());
    if (rc) {
        ERROR("umount %s failed with rc %d, msg %s\n",
              tmp_path.c_str(), rc, strerror(errno));
        return rc;
    }

    // ext4enc:TODO add retry logic
    rc = umount(path);
    if (rc) {
        ERROR("umount %s failed with rc %d, msg %s\n",
              path, rc, strerror(errno));
        return rc;
    }

    return 0;
}

const char* e4crypt_get_password(const char* path)
{
    // ext4enc:TODO scrub password after timeout
    auto i = s_password_store.find(path);
    if (i == s_password_store.end()) {
        return 0;
    } else {
        return i->second.c_str();
    }
}
