#ifndef _CDVDHDD_H_
#define _CDVDHDD_H_

#define HDD_SECTOR_SIZE 512 /* HDD sector size in bytes */

/* HD Loader I/O interface */

typedef struct hio_type hio_t;

typedef int (*hio_probe_t)(const char *path,
                           hio_t **hio);

typedef int (*hio_stat_t)(hio_t *hio,
                          u_int32_t *size_in_kb);

typedef int (*hio_read_t)(hio_t *hio,
                          u_int32_t start_sector,
                          u_int32_t num_sectors,
                          void *output,
                          u_int32_t *bytes);

typedef int (*hio_write_t)(hio_t *hio,
                           u_int32_t start_sector,
                           u_int32_t num_sectors,
                           const void *input,
                           u_int32_t *bytes);

typedef int (*hio_flush_t)(hio_t *hio);

typedef int (*hio_poweroff_t)(hio_t *hio);

typedef int (*hio_close_t)(hio_t *hio);

/* return last error text in a memory buffer, that would be freed by calling hio_dispose_error_t */
typedef char *(*hio_last_error_t)(hio_t *hio);
typedef void (*hio_dispose_error_t)(hio_t *hio,
                                    char *error);

struct hio_type
{
    hio_stat_t stat;
    hio_read_t read;
    hio_write_t write;
    hio_flush_t flush;
    hio_close_t close;
    hio_poweroff_t poweroff;
    hio_last_error_t last_error;
    hio_dispose_error_t dispose_error;
};

typedef struct
{
    char Partition_Name[32 + 1];
    char Name[64 + 1];
    char Startup[8 + 1 + 3 + 1];
    int Is_Dvd;
} GameInfo;

extern int HdlGetGameInfo(const char *PartName, GameInfo *GameInf);
extern int HdlRenameGame(const char *OldName, const char *NewName);

#endif
