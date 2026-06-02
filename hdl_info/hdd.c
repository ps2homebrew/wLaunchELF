//--------------------------------------------------------------
// File name:   hdd.c
//--------------------------------------------------------------
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <hdd-ioctl.h>
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>

#include "ps2_hdd.h"
#include "hdd.h"
#include "hdl.h"
#include "apa.h"

static hdl_games_list_t *games = NULL;
static hio_t *g_hio = NULL;

static u8 IOBuffer[2048];

//--------------------------------------------------------------
static int fxio_stat(hio_t *hio, u_int32_t *size_in_kb)
{
    *size_in_kb = fileXioDevctl("hdd:", HDIOC_TOTALSECTOR, NULL, 0, NULL, 0) / 2;
    return 0;
}
//------------------------------
// endfunc fxio_stat
//--------------------------------------------------------------
static int fxio_read(hio_t *hio, u_int32_t start_sector, u_int32_t num_sectors, void *output, u_int32_t *bytes)
{
    hddAtaTransfer_t *args = (hddAtaTransfer_t *)IOBuffer;
    u32 lba_offset;

    // filexio is limited to 2048 (4 sectors) bytes both arg and buf buffers.
    for (lba_offset = 0; lba_offset < num_sectors; lba_offset += args->size) {
        args->lba = start_sector + lba_offset;
        args->size = (num_sectors - lba_offset) > 4 ? 4 : (num_sectors - lba_offset);

        if (fileXioDevctl("hdd:", HDIOC_READSECTOR, args, sizeof(hddAtaTransfer_t), ((u8 *)output) + (lba_offset * 512), args->size * 512) != 0)
            return -1;
    }
    *bytes = num_sectors * HDD_SECTOR_SIZE;
    return 0;
}
//------------------------------
// endfunc fxio_read
//--------------------------------------------------------------
static int fxio_write(hio_t *hio, u_int32_t start_sector, u_int32_t num_sectors, const void *input, u_int32_t *bytes)
{
    static u8 WriteBuffer[3 * 512 + sizeof(hddAtaTransfer_t)];  // Has to be a different buffer from IOBuffer (input can be in IOBuffer).
    hddAtaTransfer_t *args = (hddAtaTransfer_t *)WriteBuffer;
    u32 lba_offset;

    // filexio is limited to 2048 (4 sectors) bytes both arg and buf buffers.
    for (lba_offset = 0; lba_offset < num_sectors; lba_offset += args->size) {
        args->lba = start_sector + lba_offset;
        args->size = (num_sectors - lba_offset) > 3 ? 3 : (num_sectors - lba_offset);

        memcpy(args->data, ((u8 *)input) + (lba_offset * 512), args->size * 512);

        if (fileXioDevctl("hdd:", HDIOC_WRITESECTOR, args, sizeof(hddAtaTransfer_t) + (args->size * 512), NULL, 0) != 0)
            return -1;
    }
    *bytes = num_sectors * HDD_SECTOR_SIZE;
    return 0;
}
//------------------------------
// endfunc fxio_write
//--------------------------------------------------------------
static int fxio_flush(hio_t *hio)
{
    return (fileXioDevctl("hdd:", HDIOC_FLUSH, NULL, 0, NULL, 0) != 0) ? -1 : 0;
}
//------------------------------
// endfunc fxio_flush
//--------------------------------------------------------------
static int fxio_close(hio_t *hio)
{
    free(hio);
    return 0;
}
//------------------------------
// endfunc fxio_close
//--------------------------------------------------------------
static int fxio_poweroff(hio_t *hio)
{
    return 0;
}
//------------------------------
// endfunc fxio_poweroff
//--------------------------------------------------------------
static hio_t *fxio_alloc(void)
{
    hio_t *hio = malloc(sizeof(hio_t));
    if (hio != NULL) {
        hio->stat = &fxio_stat;
        hio->read = &fxio_read;
        hio->write = &fxio_write;
        hio->flush = &fxio_flush;
        hio->close = &fxio_close;
        hio->poweroff = &fxio_poweroff;
    }
    return hio;
}
//------------------------------
// endfunc fxio_alloc
//--------------------------------------------------------------
int hio_fxio_probe(const char *path, hio_t **hio)
{
    if (path[0] == 'h' &&
        path[1] == 'd' &&
        path[2] == 'd' &&
        (path[3] >= '0' && path[3] <= '9') &&
        path[4] == ':' &&
        path[5] == '\0') {
        if (fileXioDevctl("hdd:", HDIOC_STATUS, NULL, 0, NULL, 0) == 0) {
            *hio = fxio_alloc();
            return (*hio != NULL) ? 0 : -2;
        }
    }
    return 14;
}
//------------------------------
// endfunc hio_fxio_probe
//--------------------------------------------------------------
int HdlGetGameInfo(const char *PartName, GameInfo *GameInf)
{
    hdl_glist_free(games);
    games = NULL;
    if (g_hio != NULL)
        g_hio->close(g_hio);
    g_hio = NULL;

    if (hio_fxio_probe("hdd0:", &g_hio) == 0) {
        int err;

        if ((err = hdl_glist_read(g_hio, &games)) == 0) {
            int i;

            for (i = 0; i < games->count; ++i) {
                const hdl_game_info_t *game = &games->games[i];

                if (!strcmp(PartName, game->partition_name)) {
                    strcpy(GameInf->Partition_Name, game->partition_name);
                    strcpy(GameInf->Name, game->name);
                    strcpy(GameInf->Startup, game->startup);
                    GameInf->Is_Dvd = game->is_dvd;
                    return 0;  // Return flag for no error
                }
            } /* for */
            return -3;  // Return error flag for 'Game not found'
        } /* if */
        return err;  // Return error flag for 'hdl_glist_read failed'
    } /* if */
    return -1;  // Return error flag for 'hio_fxio_probe failed'
}
//------------------------------
// endfunc HdlGetGameInfo
//--------------------------------------------------------------

int HdlRenameGame(const char *OldName, const char *NewName)
{
    hdl_glist_free(games);
    games = NULL;
    if (g_hio != NULL)
        g_hio->close(g_hio);
    g_hio = NULL;

    if (hio_fxio_probe("hdd0:", &g_hio) == 0) {
        int err;

        if ((err = hdl_glist_read(g_hio, &games)) == 0) {
            int i;

            for (i = 0; i < games->count; ++i) {
                hdl_game_info_t *game = &games->games[i];

                if (!strcmp(OldName, game->name)) {
                    printf("Renaming Game %s To %s.\n", game->name, NewName);
                    strcpy(game->name, NewName);
                    if ((err = hdl_glist_write(g_hio, game)) == 0)
                        return 0;  // Return flag for no error
                    else
                        return err;  // Return error flag for 'hdl_glist_write failed'
                }
            } /* for */
            return -3;  // Return error flag for 'Game not found'
        } /* if */
        return err;  // Return error flag for 'hdl_glist_read failed'
    } /* if */
    return -1;  // Return error flag for 'hio_fxio_probe failed'
}
//------------------------------
// endfunc HdlRenameGame
//--------------------------------------------------------------
// End of file: hdd.c
//--------------------------------------------------------------
