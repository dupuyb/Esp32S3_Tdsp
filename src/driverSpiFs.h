#ifndef DRIVERSPIFS_H
#define DRIVERSPIFS_H

#include "Arduino.h"
#include "lvgl.h"
// File FS SPI Flash File System
#include <FS.h>
#include <SPIFFS.h>

/* Create a type to store the required data about your file.*/
typedef File lv_spiffs_file_t;
typedef File lv_spiffs_dir_t;

void *open_SPIFFS_file(struct _lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    (void)drv; /*Unused*/
    fs::File *spiFile = new fs::File();
    *spiFile = SPIFFS.open(path, mode == LV_FS_MODE_WR ? FILE_WRITE : FILE_READ);
    // LV_LOG_WARN("open_SPIFFS_file for %s", path);
    if (!*spiFile)
    {
        return NULL;
    }
    else if (spiFile->isDirectory())
    {
        return NULL;
    }
    else
    {
        return (void *)&*spiFile;
    }
}

lv_fs_res_t read_SPIFFS_file(struct _lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br)
{
    (void)drv; /*Unused*/
    lv_spiffs_file_t *fp = (lv_spiffs_file_t *)file_p;
    lv_spiffs_file_t file = *fp;
    if (!file)
    {
        return LV_FS_RES_NOT_EX;
    }
    else
    {
        *br = (uint32_t)file.readBytes((char *)buf, btr);
        // LV_LOG_WARN("read_SPIFFS_file *br=%d btr=%d", *br, btr);
        return LV_FS_RES_OK;
    }
}

lv_fs_res_t close_SPIFFS_file(struct _lv_fs_drv_t *drv, void *file_p)
{
    (void)drv; /*Unused*/
    lv_spiffs_file_t file = *(lv_spiffs_file_t *)file_p;
    if (!file)
    {
        return LV_FS_RES_NOT_EX;
    }
    else if (file.isDirectory())
    {
        return LV_FS_RES_UNKNOWN;
    }
    else
    {
        file.close();
        return LV_FS_RES_OK;
    }
}

lv_fs_res_t seek_SPIFFS_file(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence)
{
    (void)drv; /*Unused*/
    lv_spiffs_file_t file = *(lv_spiffs_file_t *)file_p;
    if (!file)
    {
        return LV_FS_RES_NOT_EX;
    }
    else
    {
        //  LV_LOG_WARN("seek_SPIFFS_file at %d", pos);
        file.seek(pos, SeekSet);
        return LV_FS_RES_OK;
    }
}

lv_fs_res_t tell_SPIFFS_file(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    (void)drv; /*Unused*/
    lv_spiffs_file_t file = *(lv_spiffs_file_t *)file_p;
    if (!file)
    {
        return LV_FS_RES_NOT_EX;
    }
    else
    {
        *pos_p = (uint32_t)file.position();
        //  LV_LOG_WARN("seek_SPIFFS_file at %d", *pos_p);
        return LV_FS_RES_OK;
    }
}

bool my_ready_cb(struct _lv_fs_drv_t *drv)
{
    return true;
}

void *my_dir_open_cb(struct _lv_fs_drv_t *drv, const char *path)
{
    path = "/";
    return NULL;
}

lv_fs_res_t my_dir_read_cb(struct _lv_fs_drv_t *drv, void *rddir_p, char *fn)
{
    return LV_FS_RES_OK;
}

bool init_file_system_driver()
{
    static lv_fs_drv_t drv;
    lv_fs_drv_init(&drv); /*Basic initialization*/

    drv.letter = 'P';                 /*An uppercase letter to identify the drive */
    drv.ready_cb = my_ready_cb;       /*Callback to tell if the drive is ready to use */
    drv.open_cb = open_SPIFFS_file;   /*Callback to open a file */
    drv.close_cb = close_SPIFFS_file; /*Callback to close a file */
    drv.read_cb = read_SPIFFS_file;   /*Callback to read a file */
    drv.write_cb = NULL;              /*Callback to write a file */
    drv.seek_cb = seek_SPIFFS_file;   /*Callback to seek in a file (Move cursor) */
    drv.tell_cb = tell_SPIFFS_file;   /*Callback to tell the cursor position  */

    drv.dir_open_cb = my_dir_open_cb; /*Callback to open directory to read its content */
    drv.dir_read_cb = my_dir_read_cb; /*Callback to read a directory's content */
    drv.dir_close_cb = NULL;          /*Callback to close a directory */

    lv_fs_drv_register(&drv); /*Finally register the drive*/

    if (!SPIFFS.begin(true))
    {
        return false;
    }
    return true;
}
#endif