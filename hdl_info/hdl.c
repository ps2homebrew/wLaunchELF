//--------------------------------------------------------------
//File name:   hdl.c
//--------------------------------------------------------------
#include <thbase.h>
#include <stdio.h>
#include <sysclib.h>
#include <cdvdman.h>
#include <iomanX.h>

#include "main.h"
#include "ps2_hdd.h"
#include "hdd.h"
#include "hdl.h"
#include "apa.h"

//--------------------------------------------------------------
void hdl_glist_free(hdl_games_list_t *glist)
{
    if (glist != NULL) {
        free(glist->games);
        free(glist);
    }
}
//------------------------------
//endfunc hdl_glist_free
//--------------------------------------------------------------
static int hdl_ginfo_read(hio_t *hio, const ps2_partition_header_t *part, hdl_game_info_t *ginfo)
{
    u_long i, size;
    /* data we're interested in starts @ 0x101000 and is header
 * plus information for up to 65 partitions
 * (1 main + 64 sub) by 12 bytes each */
    const u_long offset = 0x101000;
    char buffer[1024];
    int result;
    u_long bytes;

    result = hio->read(hio, get_u32(&part->start) + offset / 512, 2, buffer, &bytes);
    if (result == 0) {
        if (bytes == 1024) {
            /* calculate total size */
            size = get_u32(&part->length);
            for (i = 0; i < get_u32(&part->nsub); ++i)
                size += get_u32(&part->subs[i].length);

            memcpy(ginfo->partition_name, part->id, PS2_PART_IDMAX);
            ginfo->partition_name[PS2_PART_IDMAX] = '\0';
            strcpy(ginfo->name, buffer + 8);
            strcpy(ginfo->startup, buffer + 0xac);
            ginfo->compat_flags = buffer[0xa8];
            ginfo->is_dvd = buffer[0xec] == 0x14;
            ginfo->start_sector = get_u32(&part->start);
            ginfo->total_size_in_kb = size / 2;
        } else
            result = -1;
    }
    return (result);
}
//------------------------------
//endfunc hdl_ginfo_read
//--------------------------------------------------------------
int hdl_glist_read(hio_t *hio, hdl_games_list_t **glist)
{
    apa_partition_table_t *ptable;
    int result;

    result = apa_ptable_read_ex(hio, &ptable);
    if (result == 0) {
        u_long i, count = 0;
        void *tmp;
        for (i = 0; i < ptable->part_count; ++i)
            count += (get_u16(&ptable->parts[i].header.flags) == 0x00 &&
                      get_u16(&ptable->parts[i].header.type) == 0x1337);

        tmp = malloc(sizeof(hdl_game_info_t) * count);
        if (tmp != NULL) {
            memset(tmp, 0, sizeof(hdl_game_info_t) * count);
            *glist = malloc(sizeof(hdl_games_list_t));
            if (*glist != NULL) {
                u_long index = 0;
                memset(*glist, 0, sizeof(hdl_games_list_t));
                (*glist)->count = count;
                (*glist)->games = tmp;
                (*glist)->total_chunks = ptable->total_chunks;
                (*glist)->free_chunks = ptable->free_chunks;
                for (i = 0; result == 0 && i < ptable->part_count; ++i) {
                    const ps2_partition_header_t *part = &ptable->parts[i].header;
                    if (get_u16(&part->flags) == 0x00 && get_u16(&part->type) == 0x1337)
                        result = hdl_ginfo_read(hio, part, (*glist)->games + index++);
                }
                if (result != 0)
                    free(*glist);
            } else
                result = -2;
            if (result != 0)
                free(tmp);
        } else
            result = -2;

        apa_ptable_free(ptable);
    } else {  //apa_ptable_read_ex failed
    }
    return result;
}
//------------------------------
//endfunc hdl_glist_read
//--------------------------------------------------------------
static int hdl_ginfo_write(hio_t *hio, const ps2_partition_header_t *part, hdl_game_info_t *ginfo)
{
    const u_long offset = 0x101000;
    char buffer[1024];
    int result;
    u_long bytes;

    result = hio->read(hio, get_u32(&part->start) + offset / 512, 2, buffer, &bytes);

    memset(buffer + 8, 0, PS2_PART_NAMEMAX);
    memcpy(buffer + 8, ginfo->name, PS2_PART_NAMEMAX);

    result = hio->write(hio, get_u32(&part->start) + offset / 512, 2, buffer, &bytes);

    return result;
}
//------------------------------
//endfunc hdl_ginfo_write
//--------------------------------------------------------------
int hdl_glist_write(hio_t *hio, hdl_game_info_t *ginfo)
{
    hdl_games_list_t *tmplist;
    apa_partition_table_t *ptable;
    int result;

    result = apa_ptable_read_ex(hio, &ptable);
    if (result == 0) {
        u_long i, count = 0;
        void *tmp;
        for (i = 0; i < ptable->part_count; ++i)
            count += (get_u16(&ptable->parts[i].header.flags) == 0x00 &&
                      get_u16(&ptable->parts[i].header.type) == 0x1337);

        tmp = malloc(sizeof(hdl_game_info_t) * count);
        if (tmp != NULL) {
            memset(tmp, 0, sizeof(hdl_game_info_t) * count);
            tmplist = malloc(sizeof(hdl_games_list_t));
            if (tmplist != NULL) {
                u_long index = 0;
                memset(tmplist, 0, sizeof(hdl_games_list_t));
                tmplist->count = count;
                tmplist->games = tmp;
                tmplist->total_chunks = ptable->total_chunks;
                tmplist->free_chunks = ptable->free_chunks;
                for (i = 0; result == 0 && i < ptable->part_count; ++i) {
                    const ps2_partition_header_t *part = &ptable->parts[i].header;
                    if (get_u16(&part->flags) == 0x00 && get_u16(&part->type) == 0x1337) {
                        result = hdl_ginfo_read(hio, part, tmplist->games + index++);
                        if (!strcmp(tmplist->games[index - 1].partition_name, ginfo->partition_name)) {
                            result = hdl_ginfo_write(hio, part, ginfo);
                            break;
                        }
                    }
                }
                free(tmplist);
            } else
                result = -2;
            if (result != 0)
                free(tmp);
        } else
            result = -2;

        apa_ptable_free(ptable);
    }
    return result;
}
//------------------------------
//endfunc hdl_glist_write
//--------------------------------------------------------------
//End of file: hdl.c
//--------------------------------------------------------------
