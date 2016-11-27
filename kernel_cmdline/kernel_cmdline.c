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

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

#include "../lib/log.h"
#include "../lib/util.h"

#include <libbootimg.h>

#define KERNEL_TMP "/tmp/kernel.new.tmp"

int kernel_cmdline(const char *kernel_path, const char *cmd_append, char debug)
{
    int res = -1;
    struct bootimg img;
    int cmd_append_size;
    int cmdline_size;
    char* cmdline_write;
    char* cmdline_write_tmp;

    if (libbootimg_init_load(&img, kernel_path, LIBBOOTIMG_LOAD_ALL) < 0)
    {
        ERROR("Could not open boot image (%s)!\n", kernel_path);
        return -1;
    }

    if (debug)
    {
        printf("kernel_cmdline: Original \"%s\"\n", img.hdr.cmdline);
    }

    if (strstr((char*)img.hdr.cmdline, cmd_append) == NULL)
    {
        cmd_append_size = strlen(cmd_append);
        cmdline_size = strlen((char*)img.hdr.cmdline);
        cmdline_write = memchr(img.hdr.cmdline, 0x00, BOOT_ARGS_SIZE);
        cmdline_write_tmp = memchr(img.hdr.cmdline, 0x0A, BOOT_ARGS_SIZE);

        if (cmdline_write_tmp != NULL && cmdline_write_tmp < cmdline_write)
        {
            cmdline_write = cmdline_write_tmp;
        }

        if (cmdline_write != (char*)img.hdr.cmdline)
        {
            *cmdline_write = ' ';
            ++cmdline_write;
            if (img.is_elf)
            {
                img.hdr_info->cmdline_size += 1;
            }
        }

        strcpy(cmdline_write, cmd_append);
        cmdline_write[cmd_append_size] = 0x00;
        if (img.is_elf)
        {
            img.hdr_info->cmdline_size += cmd_append_size;
        }

        if (debug)
        {
            printf("kernel_cmdline: Patched  \"%s\"\n", img.hdr.cmdline);
        }

        if (libbootimg_write_img(&img, KERNEL_TMP) >= 0)
        {
            INFO("Writing boot.img updated with kernel\n");
            if (copy_file(KERNEL_TMP, kernel_path) < 0)
                ERROR("Failed to copy %s to %s!\n", KERNEL_TMP, kernel_path);
            else
                res = 0;
            remove(KERNEL_TMP);
        }
        else
        {
            ERROR("Failed to libbootimg_write_img to %s!\n", KERNEL_TMP);
        }
    }
    else if (debug)
    {
        printf("kernel_cmdline: cmdline already patched\n");
    }

exit:
    libbootimg_destroy(&img);
    return res;
}

int main(int argc, char *argv[])
{
    int i, res;
    static char *const cmd[] = { "/init", NULL };
    char *inject_path = NULL;
    char *cmd_append = NULL;
    char debug = 0;

    for (i = 1; i < argc; ++i)
    {
        if (strstartswith(argv[i], "--inject="))
        {
            inject_path = argv[i] + strlen("--inject=");
        }
        else if (strstartswith(argv[i], "--cmd_append="))
        {
            cmd_append = argv[i] + strlen("--cmd_append=");
        }
        else if (strstartswith(argv[i], "--debug"))
        {
            debug = 1;
        }
    }

    if (!inject_path || !cmd_append)
    {
        printf("kernel_cmdline usage:\n");
        printf(" --inject=[path to bootimage to patch] needed\n");
        printf(" --cmd_append=[cmdline string to append] needed\n");
        printf(" --debug optional\n");
        fflush(stdout);
        return 1;
    }

    mrom_set_log_tag("kernel_cmdline");
    return kernel_cmdline(inject_path, cmd_append, debug);
}
