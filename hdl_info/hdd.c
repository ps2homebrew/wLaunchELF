//--------------------------------------------------------------
//File name:   hdd.c
//--------------------------------------------------------------
#include <thbase.h>
#include <sysclib.h>
#include <stdio.h>
#include <dev9.h>
#include <atad.h>
#include <poweroff.h>

#include "main.h"
#include "ps2_hdd.h"
#include "hdd.h"
#include "hdl.h"
#include "apa.h"

static hdl_games_list_t *games = NULL;
static hio_t *hio = NULL;

//--------------------------------------------------------------
static int iop_stat(hio_t *hio, u_long *size_in_kb)
{
    hio_iop_t *iop = (hio_iop_t *)hio;
    *size_in_kb = iop->size_in_sectors / 2;
    return 0;
}
//------------------------------
//endfunc iop_stat
//--------------------------------------------------------------
static int iop_read(hio_t *hio, u_long start_sector, u_long num_sectors, void *output, u_long *bytes)
{
    hio_iop_t *iop = (hio_iop_t *)hio;
    int result = ata_device_sector_io(iop->unit, output, start_sector, num_sectors, ATA_DIR_READ);
    if (result == 0) {
        *bytes = num_sectors * HDD_SECTOR_SIZE;
        return 0;
    } else
        return -1;
}
//------------------------------
//endfunc iop_read
//--------------------------------------------------------------
static int iop_write(hio_t *hio, u_long start_sector, u_long num_sectors, const void *input, u_long *bytes)
{
    hio_iop_t *iop = (hio_iop_t *)hio;
    int result = ata_device_sector_io(iop->unit, (char *)input, start_sector, num_sectors, ATA_DIR_WRITE);
    if (result == 0) {
        *bytes = num_sectors * HDD_SECTOR_SIZE;
        return 0;
    }
    return -1;
}
//------------------------------
//endfunc iop_write
//--------------------------------------------------------------
static int iop_flush(hio_t *hio)
{
    hio_iop_t *iop = (hio_iop_t *)hio;
    int result = ata_device_flush_cache(iop->unit);
    return result;
}
//------------------------------
//endfunc iop_flush
//--------------------------------------------------------------
static int iop_close(hio_t *hio)
{
    free(hio);
    return 0;
}
//------------------------------
//endfunc iop_close
//--------------------------------------------------------------
static int iop_poweroff(hio_t *hio)
{                        //Prerequisites: all files on the HDD must be saved & all partitions unmounted.
    dev9Shutdown();      //Power off DEV9
    PoweroffShutdown();  //Power off PlayStation 2
    return 0;
}
//------------------------------
//endfunc iop_poweroff
//--------------------------------------------------------------
static hio_t *iop_alloc(int unit, size_t size_in_sectors)
{
    hio_iop_t *iop = malloc(sizeof(hio_iop_t));
    if (iop != NULL) {
        hio_t *hio = &iop->hio;
        hio->stat = &iop_stat;
        hio->read = &iop_read;
        hio->write = &iop_write;
        hio->flush = &iop_flush;
        hio->close = &iop_close;
        hio->poweroff = &iop_poweroff;
        iop->unit = unit;
        iop->size_in_sectors = size_in_sectors;
    }
    return ((hio_t *)iop);
}
//------------------------------
//endfunc iop_alloc
//--------------------------------------------------------------
int hio_iop_probe(const char *path, hio_t **hio)
{
    if (path[0] == 'h' &&
        path[1] == 'd' &&
        path[2] == 'd' &&
        (path[3] >= '0' && path[3] <= '9') &&
        path[4] == ':' &&
        path[5] == '\0') {
        int unit = path[3] - '0';
        ata_devinfo_t *dev_info = ata_get_devinfo(unit);
        if (dev_info != NULL && dev_info->exists) {
            *hio = iop_alloc(unit, dev_info->total_sectors);
            if (*hio != NULL)
                return (0);
            else
                return -2;
        }
    }
    return 14;
}
//------------------------------
//endfunc hio_iop_probe
//--------------------------------------------------------------
int HdlGetGameInfo(char *PartName, GameInfo *GameInf)
{

    int i, count = 0, err;

    hdl_glist_free(games);
    games = NULL;
    if (hio != NULL)
        hio->close(hio);
    hio = NULL;

    if (hio_iop_probe("hdd0:", &hio) == 0) {
        if ((err = hdl_glist_read(hio, &games)) == 0) {
            for (i = 0; i < games->count; ++i) {
                const hdl_game_info_t *game = &games->games[i];

                if (!strcmp(PartName, game->partition_name)) {
                    strcpy(GameInf->Partition_Name, game->partition_name);
                    strcpy(GameInf->Name, game->name);
                    strcpy(GameInf->Startup, game->startup);
                    GameInf->Is_Dvd = game->is_dvd;
                    return 0;  //Return flag for no error
                }
                ++count;
            }           /* for */
            return -3;  //Return error flag for 'Game not found'
        }               /* if */
        return err;     //Return error flag for 'hdl_glist_read failed'
    }                   /* if */
    return -1;          //Return error flag for 'hio_iop_probe failed'
}
//------------------------------
//endfunc HdlGetGameInfo
//--------------------------------------------------------------

int HdlRenameGame(void *Data)
{

    int i, count = 0, err;

    int *Pointer = Data;
    Rpc_Packet_Send_Rename *Packet = (Rpc_Packet_Send_Rename *)Pointer;

    hdl_glist_free(games);
    games = NULL;
    if (hio != NULL)
        hio->close(hio);
    hio = NULL;

    if (hio_iop_probe("hdd0:", &hio) == 0) {
        if ((err = hdl_glist_read(hio, &games)) == 0) {
            for (i = 0; i < games->count; ++i) {
                hdl_game_info_t *game = &games->games[i];

                if (!strcmp(Packet->OldName, game->name)) {
                    printf("Renaming Game %s To %s.\n", game->name, Packet->NewName);
                    strcpy(game->name, Packet->NewName);
                    if ((err = hdl_glist_write(hio, game)) == 0)
                        return 0;  //Return flag for no error
                    else
                        return err;  //Return error flag for 'hdl_glist_write failed'
                }
                ++count;
            }           /* for */
            return -3;  //Return error flag for 'Game not found'
        }               /* if */
        return err;     //Return error flag for 'hdl_glist_read failed'
    }                   /* if */
    return -1;          //Return error flag for 'hio_iop_probe failed'
}
//------------------------------
//endfunc HdlRenameGame
//--------------------------------------------------------------
//End of file: hdd.c
//--------------------------------------------------------------
