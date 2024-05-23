#ifndef _CDVDHDL_H_
#define _CDVDHDL_H_

#include "ps2_hdd.h"
#define HDL_GAME_NAME_MAX 64

typedef struct hdl_game_info_type
{
    char partition_name[PS2_PART_IDMAX + 1];
    char name[HDL_GAME_NAME_MAX + 1];
    char startup[8 + 1 + 3 + 1];
    u_char compat_flags;
    int is_dvd;
    u_long start_sector;
    u_long total_size_in_kb;
} hdl_game_info_t;

typedef struct hdl_games_list_type
{
    u_long count;
    hdl_game_info_t *games;
    u_long total_chunks;
    u_long free_chunks;
} hdl_games_list_t;

void hdl_glist_free(hdl_games_list_t *glist);
int hdl_glist_read(hio_t *hio, hdl_games_list_t **glist);
int hdl_glist_write(hio_t *hio, const hdl_game_info_t *ginfo);

#endif /* _CDVDHDL_H_ */
