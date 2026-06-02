#ifndef _CDVDAPA_H_
#define _CDVDAPA_H_

/* chunks_map */
static const char MAP_AVAIL = '.';
static const char MAP_MAIN = 'M';
static const char MAP_SUB = 's';
static const char MAP_COLL = 'x';
static const char MAP_ALLOC = '*';

typedef struct apa_partition_type
{
    int existing;
    int modified;
    int linked;
    ps2_partition_header_t header;
} apa_partition_t;


typedef struct apa_partition_table_type
{
    u_int32_t device_size_in_mb;
    u_int32_t total_chunks;
    u_int32_t allocated_chunks;
    u_int32_t free_chunks;

    char *chunks_map;

    /* existing partitions */
    u_int32_t part_alloc_;
    u_int32_t part_count;
    apa_partition_t *parts;
} apa_partition_table_t;

void apa_ptable_free(apa_partition_table_t *table);

u_int32_t apa_partition_checksum(const ps2_partition_header_t *part);

int apa_ptable_read_ex(hio_t *hio, apa_partition_table_t **table);

u_int32_t get_u32(const void *buffer);
void set_u32(void *buffer, u_int32_t val);

u_int16_t get_u16(const void *buffer);
void set_u16(void *buffer, u_int16_t val);


#endif /* _APA_H_ */
