/*
 * This file is part of MultiROM.
 *
 * MultiROM is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * MultiROM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MultiROM.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "inject.h"
#include "mrom_data.h"
#include "log.h"
#include "util.h"
#include "../version.h"

// clone libbootimg to /system/extras/ from
// https://github.com/Tasssadar/libbootimg.git
#include <libbootimg.h>

#if LIBBOOTIMG_VERSION  < 0x000200
#error "libbootimg version 0.2.0 or higher is required. Please update libbootimg."
#endif

#define TMP_RD_UNPACKED_DIR "/mrom_rd"
#define TMP_RD2_UNPACKED_DIR TMP_RD_UNPACKED_DIR "/second"
#define TMP_RD2 TMP_RD2_UNPACKED_DIR "/ramdisk.cpio"

static int get_img_trampoline_ver(struct bootimg *img)
{
    int ver = 0;
    if (strncmp((char*)img->hdr.name, "tr_ver", 6) == 0)
        ver = atoi((char*)img->hdr.name + 6);
    return ver;
}

static int copy_rd_files(UNUSED const char *path, UNUSED const char *busybox_path)
{
    char buf[256];
    const int *secRD = access(TMP_RD2, F_OK);

    if ((secRD != -1 && access(TMP_RD2_UNPACKED_DIR "/main_init", F_OK) < 0 &&
            rename(TMP_RD2_UNPACKED_DIR "/init", TMP_RD2_UNPACKED_DIR "/main_init")) ||
            (secRD == -1 && access(TMP_RD_UNPACKED_DIR "/main_init", F_OK) < 0 &&
            rename(TMP_RD_UNPACKED_DIR "/init", TMP_RD_UNPACKED_DIR "/main_init") < 0))
    {
        ERROR("Failed to move /init to /main_init!\n");
        return -1;
    }

    snprintf(buf, sizeof(buf), "%s/trampoline", mrom_dir());
    if ((secRD != -1 && copy_file(buf, TMP_RD2_UNPACKED_DIR "/init") < 0) ||
            (secRD == -1 && copy_file(buf, TMP_RD_UNPACKED_DIR "/init") < 0))
    {
        ERROR("Failed to copy trampoline to /init!\n");
        return -1;
    }

    if (secRD != -1)
    {
        chmod(TMP_RD2_UNPACKED_DIR "/init", 0750);

        remove(TMP_RD2_UNPACKED_DIR "/sbin/ueventd");
        remove(TMP_RD2_UNPACKED_DIR "/sbin/watchdogd");
        symlink(TMP_RD2_UNPACKED_DIR "/main_init", TMP_RD2_UNPACKED_DIR "/sbin/ueventd");
        symlink(TMP_RD2_UNPACKED_DIR "/main_init", TMP_RD2_UNPACKED_DIR "/sbin/watchdogd");

#ifdef MR_USE_MROM_FSTAB
        snprintf(buf, sizeof(buf), "%s/mrom.fstab", mrom_dir());
        copy_file(buf, TMP_RD2_UNPACKED_DIR "/mrom.fstab");
#else
        remove(TMP_RD2_UNPACKED_DIR "/mrom.fstab");
#endif
    } else {
        chmod(TMP_RD_UNPACKED_DIR "/init", 0750);

        remove(TMP_RD_UNPACKED_DIR "/sbin/ueventd");
        remove(TMP_RD_UNPACKED_DIR "/sbin/watchdogd");
        symlink(TMP_RD_UNPACKED_DIR "/main_init", TMP_RD_UNPACKED_DIR "/sbin/ueventd");
        symlink(TMP_RD_UNPACKED_DIR "/main_init", TMP_RD_UNPACKED_DIR "/sbin/watchdogd");

#ifdef MR_USE_MROM_FSTAB
        snprintf(buf, sizeof(buf), "%s/mrom.fstab", mrom_dir());
        copy_file(buf, TMP_RD_UNPACKED_DIR "/mrom.fstab");
#else
        remove(TMP_RD_UNPACKED_DIR "/mrom.fstab");
#endif
    }

#ifdef MR_ENCRYPTION
    if (secRD != -1)
        remove_dir(TMP_RD2_UNPACKED_DIR "/mrom_enc");
    else
        remove_dir(TMP_RD_UNPACKED_DIR "/mrom_enc");

    if ((secRD != -1 && mr_system("busybox cp -a \"%s/enc\" \"%s/mrom_enc\"", mrom_dir(), TMP_RD2_UNPACKED_DIR) != 0) ||
            (secRD == -1 && mr_system("busybox cp -a \"%s/enc\" \"%s/mrom_enc\"", mrom_dir(), TMP_RD_UNPACKED_DIR) != 0))
    {
        ERROR("Failed to copy encryption files!\n");
        return -1;
    }
#endif
    return 0;
}

#define RD_GZIP 1
#define RD_LZ4  2
static int inject_second_rd(const char *path, const char *second_path)
{
    int result = -1;
    uint32_t magic = 0;

    FILE *f = fopen(path, "re");
    if (!f)
    {
        ERROR("Couldn't open %s!\n", path);
        return -1;
    }
    fread(&magic, sizeof(magic), 1, f);
    fclose(f);

    mkdir(TMP_RD2_UNPACKED_DIR, 0755);

    // Decompress initrd
    int type;
    char buff[256];
    char busybox_path[256];
    snprintf(busybox_path, sizeof(busybox_path), "%s/busybox", mrom_dir());

    char *cmd[] = { busybox_path, "sh", "-c", buff, NULL };

    snprintf(buff, sizeof(buff), "B=\"%s\"; cd \"%s\"; \"$B\" cat \"%s\" | \"$B\" cpio -i", busybox_path, TMP_RD2_UNPACKED_DIR, second_path);

    int r;
    char *out = run_get_stdout_with_exit(cmd, &r);
    if (r != 0)
    {
        ERROR("Output: %s\n", out);
        ERROR("Failed to unpack second ramdisk!%s\n", buff);
        goto fail;
    }

    // Update files
    if (copy_rd_files(second_path, busybox_path) < 0)
        goto fail;

    // Pack initrd again
    snprintf(buff, sizeof(buff), "B=\"%s\"; cd \"%s\"; \"$B\" find . | \"$B\" cpio -o -H newc > \"%s\"", busybox_path, TMP_RD2_UNPACKED_DIR, second_path);

    out = run_get_stdout_with_exit(cmd, &r);
    if (r != 0)
    {
        ERROR("Output: %s\n", out);
        ERROR("Failed to pack ramdisk.cpio!\n");
        goto fail;
    }
success:
    result = 0;
fail:
    remove_dir(TMP_RD2_UNPACKED_DIR);
    return result;
}

static int inject_rd(const char *path, const char *second_path)
{
    int result = -1;
    uint32_t magic = 0;

    FILE *f = fopen(path, "re");
    if (!f)
    {
        ERROR("Couldn't open %s!\n", path);
        return -1;
    }
    fread(&magic, sizeof(magic), 1, f);
    fclose(f);

    remove_dir(TMP_RD_UNPACKED_DIR);
    mkdir(TMP_RD_UNPACKED_DIR, 0755);

    // Decompress initrd
    int type;
    char buff[256];
    char busybox_path[256];
    snprintf(busybox_path, sizeof(busybox_path), "%s/busybox", mrom_dir());

    char *cmd[] = { busybox_path, "sh", "-c", buff, NULL };

    if ((magic & 0xFFFF) == 0x8B1F)
    {
        type = RD_GZIP;
        snprintf(buff, sizeof(buff), "B=\"%s\"; cd \"%s\"; \"$B\" gzip -d -c \"%s\" | \"$B\" cpio -i", busybox_path, TMP_RD_UNPACKED_DIR, path);
    }
    else if (magic == 0x184C2102)
    {
        type = RD_LZ4;
        snprintf(buff, sizeof(buff), "cd \"%s\"; \"%s/lz4\" -d \"%s\" stdout | \"%s\" cpio -i", TMP_RD_UNPACKED_DIR, mrom_dir(), path, busybox_path);
    }
    else
    {
        ERROR("Unknown ramdisk magic 0x%08X, can't update trampoline\n", magic);
        goto success;
    }

    int r = run_cmd(cmd);
    if (r != 0)
    {
        ERROR("Failed to unpack ramdisk! %s\n", buff);
        goto fail;
    }

    if (access(TMP_RD2, F_OK) != -1) {
        if (inject_second_rd(path, second_path) < 0)
            goto fail;
    } else {
        if (copy_rd_files(path, busybox_path) < 0)
            goto fail;
    }

    // Pack ramdisk
    switch(type)
    {
        case RD_GZIP:
            snprintf(buff, sizeof(buff), "B=\"%s\"; cd \"%s\"; \"$B\" find . | \"$B\" cpio -o -H newc | \"$B\" gzip > \"%s\"", busybox_path, TMP_RD_UNPACKED_DIR, path);
            break;
        case RD_LZ4:
            snprintf(buff, sizeof(buff), "B=\"%s\"; cd \"%s\"; \"$B\" find . | \"$B\" cpio -o -H newc | \"%s/lz4\" stdin \"%s\"", busybox_path, TMP_RD_UNPACKED_DIR, mrom_dir(), path);
            break;
    }

    r = run_cmd(cmd);
    if (r != 0)
    {
        ERROR("Failed to pack ramdisk!\n");
        goto fail;
    }

success:
    result = 0;
fail:
    remove_dir(TMP_RD_UNPACKED_DIR);
    return result;
}

int inject_bootimg(const char *img_path, int force)
{
    int res = -1;
    struct bootimg img;
    int img_ver;
    char initrd_path[256];
    static const char *initrd_tmp_name = "/inject-initrd.img";
    static const char *initrd2_tmp_name = "/mrom_rd/sbin/ramdisk.cpio";

    if (libbootimg_init_load(&img, img_path, LIBBOOTIMG_LOAD_ALL) < 0)
    {
        ERROR("Could not open boot image (%s)!\n", img_path);
        return -1;
    }

    img_ver = get_img_trampoline_ver(&img);
    if (!force && img_ver == VERSION_TRAMPOLINE)
    {
        INFO("No need to update trampoline.\n");
        res = 0;
        goto exit;
    }

    INFO("Updating trampoline from ver %d to %d\n", img_ver, VERSION_TRAMPOLINE);

    if (libbootimg_dump_ramdisk(&img, initrd_tmp_name) < 0)
    {
        ERROR("Failed to dump ramdisk to %s!\n", initrd_path);
        goto exit;
    }

    if (inject_rd(initrd_tmp_name, initrd2_tmp_name) >= 0)
    {
        // Update the boot.img
        snprintf((char*)img.hdr.name, BOOT_NAME_SIZE, "tr_ver%d", VERSION_TRAMPOLINE);
#ifdef MR_RD_ADDR
        img.hdr.ramdisk_addr = MR_RD_ADDR;
#endif

        if (libbootimg_load_ramdisk(&img, initrd_tmp_name) < 0)
        {
            ERROR("Failed to load ramdisk from %s!\n", initrd_tmp_name);
            goto exit;
        }

        char tmp[256];
        strcpy(tmp, img_path);
        strcat(tmp, ".new");
        if (libbootimg_write_img(&img, tmp) >= 0)
        {
            INFO("Writing boot.img updated with trampoline v%d\n", VERSION_TRAMPOLINE);
            if (copy_file(tmp, img_path) < 0)
                ERROR("Failed to copy %s to %s!\n", tmp, img_path);
            else
                res = 0;
            remove(tmp);
        }
        else
            ERROR("Failed to libbootimg_write_img!\n");
    }

exit:
    libbootimg_destroy(&img);
    remove("/inject-initrd.img");
    return res;
}
