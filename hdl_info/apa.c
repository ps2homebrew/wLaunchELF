//--------------------------------------------------------------
// File name:   apa.c
//--------------------------------------------------------------
#include <thbase.h>
#include <sysclib.h>
#include <cdvdman.h>
#include <iomanX.h>

#include "main.h"
#include "ps2_hdd.h"
#include "hdd.h"
#include "hdl.h"
#include "apa.h"

#define AUTO_DELETE_EMPTY

#define _MB *(1024 * 1024) /* really ugly :-) */

// Remove this line, and uncomment the next line, to reactivate 'apa_check'
// static int apa_check(const apa_partition_table_t *table);

//--------------------------------------------------------------
u_long apa_partition_checksum(const ps2_partition_header_t *part)
{
    const u_long *p = (const u_long *)part;
    register u_long i;
    u_long sum = 0;
    for (i = 1; i < 256; ++i)
        sum += get_u32(p + i);
    return sum;
}
//------------------------------
// endfunc apa_partition_checksum
//--------------------------------------------------------------
static apa_partition_table_t *apa_ptable_alloc(void)
{
    apa_partition_table_t *table = malloc(sizeof(apa_partition_table_t));
    if (table != NULL)
        memset(table, 0, sizeof(apa_partition_table_t));
    return table;
}
//------------------------------
// endfunc apa_ptable_alloc
//--------------------------------------------------------------
void apa_ptable_free(apa_partition_table_t *table)
{
    if (table != NULL) {
        if (table->chunks_map != NULL)
            free(table->chunks_map);
        if (table->parts != NULL)
            free(table->parts);
        free(table);
    }
}
//------------------------------
// endfunc apa_ptable_free
//--------------------------------------------------------------
static int apa_part_add(apa_partition_table_t *table, const ps2_partition_header_t *part, int existing, int linked)
{
    if (table->part_count == table->part_alloc_) { /* grow buffer */
        u_long bytes = (table->part_alloc_ + 16) * sizeof(apa_partition_t);
        apa_partition_t *tmp = malloc(bytes);
        if (tmp != NULL) {
            memset(tmp, 0, bytes);
            if (table->parts != NULL) /* copy existing */
                memcpy(tmp, table->parts, table->part_count * sizeof(apa_partition_t));
            free(table->parts);
            table->parts = tmp;
            table->part_alloc_ += 16;
        } else
            return -2;
    }

    memcpy(&table->parts[table->part_count].header, part, sizeof(ps2_partition_header_t));
    table->parts[table->part_count].existing = existing;
    table->parts[table->part_count].modified = !existing;
    table->parts[table->part_count].linked = linked;
    ++table->part_count;

    return 0;
}
//------------------------------
// endfunc apa_part_add
//--------------------------------------------------------------
/* //Remove this line and a similar one below to reactivate 'apa_setup_statistics'
static int apa_setup_statistics(apa_partition_table_t *table)
{
 u_long i;
 char *map;

 table->total_chunks = table->device_size_in_mb / 128;
 map = malloc(table->total_chunks * sizeof (char));
 if(map != NULL)
 {
  for(i=0; i<table->total_chunks; ++i)
   map [i] = MAP_AVAIL;

  // build occupided/available space map
  table->allocated_chunks = 0;
  table->free_chunks = table->total_chunks;
  for(i=0; i<table->part_count; ++i)
  {
   const ps2_partition_header_t *part = &table->parts [i].header;
   u_long part_no = get_u32(&part->start) / ((128 _MB) / 512);
   u_long num_parts = get_u32(&part->length) / ((128 _MB) / 512);

   // "alloc" num_parts starting at part_no
   while (num_parts)
   {
    if(map[part_no] == MAP_AVAIL)
     map[part_no] = get_u32(&part->main) == 0 ? MAP_MAIN : MAP_SUB;
    else
     map[part_no] = MAP_COLL; // collision
    ++part_no;
    --num_parts;
    ++table->allocated_chunks;
    --table->free_chunks;
   }
  }

  if(table->chunks_map != NULL)
  free(table->chunks_map);
  table->chunks_map = map;

  return 0;
 }
 else return -2;
}
*/
// Remove this line and a similar one below to reactivate 'apa_setup_statistics'
//------------------------------
// endfunc apa_setup_statistics
//--------------------------------------------------------------
int apa_ptable_read_ex(hio_t *hio, apa_partition_table_t **table)
{
    u_long size_in_kb;
    int result = hio->stat(hio, &size_in_kb);
    if (result == 0) {
        u_long total_sectors;
        // limit HDD size to 128GB - 1KB; that is: exclude the last 128MB chunk
        // if (size_in_kb > 128 * 1024 * 1024 - 1)
        // size_in_kb = 128 * 1024 * 1024 - 1;

        total_sectors = size_in_kb * 2; /* 1KB = 2 sectors of 512 bytes, each */

        *table = apa_ptable_alloc();
        if (*table != NULL) {
            u_long sector = 0;
            do {
                u_long bytes;
                ps2_partition_header_t part;
                result = hio->read(hio, sector, sizeof(part) / 512, &part, &bytes);
                if (result == 0) {
                    if (bytes == sizeof(part) &&
                        get_u32(&part.checksum) == apa_partition_checksum(&part) &&
                        memcmp(part.magic, PS2_PARTITION_MAGIC, 4) == 0) {
                        if (get_u32(&part.start) < total_sectors &&
                            get_u32(&part.start) + get_u32(&part.length) < total_sectors) {
                            if ((get_u16(&part.flags) == 0x0000) && (get_u16(&part.type) == 0x1337))
                                result = apa_part_add(*table, &part, 1, 1);
                            if (result == 0)
                                sector = get_u32(&part.next);
                        } else {        /* partition behind end-of-HDD */
                            result = 7; /* data behind end-of-HDD */
                            break;
                        }
                    } else
                        result = 1;
                }
                /* TODO: check whether next partition is not loaded already --
                 * do not deadlock; that is a quick-and-dirty hack */
                if ((*table)->part_count > 10000)
                    result = 7;
            } while (result == 0 && sector != 0);

            if (result == 0) {
                (*table)->device_size_in_mb = size_in_kb / 1024;
                // NB: uncommenting the next lines requires changes elsewhere too
                // result = apa_setup_statistics (*table);
                // if (result == 0)
                // result = apa_check (*table);
            }

            if (result != 0) {
                result = 20000 + (*table)->part_count;
                apa_ptable_free(*table);
            }
        } else
            result = -2;
    }
    return result;
}
//------------------------------
// endfunc apa_ptable_read_ex
//--------------------------------------------------------------
/* //Remove this line and a similar one below to reactivate 'apa_check'
static int apa_check(const apa_partition_table_t *table)
{

    u_long i, j, k;

    const u_long total_sectors = table->device_size_in_mb * 1024 * 2;

    for (i = 0; i < table->part_count; ++i) {
        const ps2_partition_header_t *part = &table->parts[i].header;
        if (get_u32(&part->checksum) != apa_partition_checksum(part))
            return 7;  // bad checksum

        if (get_u32(&part->start) < total_sectors &&
            get_u32(&part->start) + get_u32(&part->length) <= total_sectors)
            ;
        else {
            return 7;  // data behind end-of-HDD
        }

        if ((get_u32(&part->length) % ((128 _MB) / 512)) != 0)
            return 7;  // partition size not multiple to 128MB

        if ((get_u32(&part->start) % get_u32(&part->length)) != 0)
            return 7;  // partition start not multiple on partition size

        if (get_u32(&part->main) == 0 &&
            get_u16(&part->flags) == 0 &&
            get_u32(&part->start) != 0) {  // check sub-partitions
            u_long count = 0;
            for (j = 0; j < table->part_count; ++j) {
                const ps2_partition_header_t *part2 = &table->parts[j].header;
                if (get_u32(&part2->main) == get_u32(&part->start)) {  // sub-partition of current main partition
                    int found;
                    if (get_u16(&part2->flags) != PS2_PART_FLAG_SUB)
                        return 7;

                    found = 0;
                    for (k = 0; k < get_u32(&part->nsub); ++k)
                        if (get_u32(&part->subs[k].start) == get_u32(&part2->start)) {  // in list
                            if (get_u32(&part->subs[k].length) != get_u32(&part2->length))
                                return 7;
                            found = 1;
                            break;
                        }
                    if (!found)
                        return 7;  // not found in the list

                    ++count;
                }
            }
            if (count != get_u32(&part->nsub))
                return 7;  // wrong number of sub-partitions
        }
    }

    // verify double-linked list
    for (i = 0; i < table->part_count; ++i) {
        apa_partition_t *prev = table->parts + (i > 0 ? i - 1 : table->part_count - 1);
        apa_partition_t *curr = table->parts + i;
        apa_partition_t *next = table->parts + (i + 1 < table->part_count ? i + 1 : 0);
        if (get_u32(&curr->header.prev) != get_u32(&prev->header.start) ||
            get_u32(&curr->header.next) != get_u32(&next->header.start))
            return 7;  // bad links
    }

    return 0;
}
*/
// Remove this line and a similar one above to reactivate 'apa_check'
//------------------------------
// endfunc apa_check
//--------------------------------------------------------------
u_long get_u32(const void *buffer)
{
    const u_char *p = buffer;
    return ((((u_long)p[3]) << 24) |
            (((u_long)p[2]) << 16) |
            (((u_long)p[1]) << 8) |
            (((u_long)p[0]) << 0));
}
//------------------------------
// endfunc get_u32
//--------------------------------------------------------------
void set_u32(void *buffer, u_long val)
{
    u_char *p = buffer;
    p[3] = (u_char)(val >> 24);
    p[2] = (u_char)(val >> 16);
    p[1] = (u_char)(val >> 8);
    p[0] = (u_char)(val >> 0);
}
//------------------------------
// endfunc set_u32
//--------------------------------------------------------------
u_short get_u16(const void *buffer)
{
    const u_char *p = buffer;
    return ((((u_short)p[1]) << 8) |
            (((u_short)p[0]) << 0));
}
//------------------------------
// endfunc get_u16
//--------------------------------------------------------------
void set_u16(void *buffer, u_short val)
{
    u_char *p = buffer;
    p[1] = (u_char)(val >> 8);
    p[0] = (u_char)(val >> 0);
}
//------------------------------
// endfunc set_u16
//--------------------------------------------------------------
// End of file: apa.c
//--------------------------------------------------------------
