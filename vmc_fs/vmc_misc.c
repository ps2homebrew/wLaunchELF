#include "vmc.h"


// Misc fonctions

//----------------------------------------------------------------------------
// Search a direntry corresponding to a path
//----------------------------------------------------------------------------
unsigned int getDirentryFromPath(struct direntry *retval, const char *path, struct gen_privdata *gendata, int unit)
{

    PROF_START(getDirentryFromPathProf);

    DEBUGPRINT(6, "vmc_fs: Searching Direntry corresponding to path: %s\n", path);

    // Skip past the first slash if they opened a file such as
    // vmc0: / file.txt ( which means the path passed here will be / file.txt, we
    // want to skip that first slash, to ease comparisons.
    int pathoffset = 0;

    if (path[0] == '/')
        pathoffset = 1;

    if ((path[0] == '/' || path[0] == '.') && path[1] == '\0')  // rootdirectory
    {

        gendata->dirent_page = 0;
        readPage(gendata->fd, (unsigned char *)retval, gendata->first_allocatable * g_Vmc_Image[unit].header.pages_per_cluster);

        PROF_END(getDirentryFromPathProf);

        return ROOT_CLUSTER;
    }

    struct direntry dirent;  // Our temporary directory entry

    int status = 0;                    // The status of our search, if 0 at the end, we didn't find path
    unsigned int current_cluster = 0;  // The cluster we are currently scanning for directory entries
    int length = 1;                    // The number of items in the current directory
    int i = 0;

    // Our main loop that goes through files / folder searching for the right one
    for (i = 0; i < length; i++) {

        gendata->dirent_page = i % g_Vmc_Image[unit].header.pages_per_cluster;

        DEBUGPRINT(6, "vmc_fs: Reading in allocatable cluster %u / page %u\n", (current_cluster + gendata->first_allocatable), (current_cluster + gendata->first_allocatable) * g_Vmc_Image[unit].header.pages_per_cluster + gendata->dirent_page);

        // Reads either the first or second page of a cluster, depending
        // on the value currently stored in i
        readPage(gendata->fd, (unsigned char *)&dirent, (current_cluster + gendata->first_allocatable) * g_Vmc_Image[unit].header.pages_per_cluster + gendata->dirent_page);

        // Check if this was the first entry ( aka the rootdirectory )
        if (i == 0 && current_cluster == 0)
            length = dirent.length;

        DEBUGPRINT(5, "vmc_fs: Direntry Informations\n");
        DEBUGPRINT(5, "vmc_fs: Object Type    : %s\n", (dirent.mode & DF_DIRECTORY) ? "Folder" : "File");
        DEBUGPRINT(5, "vmc_fs: Object Name    : %s\n", dirent.name);
        DEBUGPRINT(5, "vmc_fs: Object Exists  : %s\n", (dirent.mode & DF_EXISTS) ? "Yes" : "No");
        DEBUGPRINT(5, "vmc_fs: Object Length  : %u\n", dirent.length);
        DEBUGPRINT(5, "vmc_fs: Object Cluster : %u\n", dirent.cluster);

        // Now that we have a pages worth of data, check if it is the
        // Directory we are searching for.

        if (memcmp(dirent.name, path + pathoffset, strlen(dirent.name)) == 0) {

            // Increase the path offset by the length of the directory
            pathoffset += strlen(dirent.name);

            // If the next item in the pathname is the null terminator,
            // we must be at the end of the path string, and that means
            // we found the correct entry, so we can break.
            if (path[pathoffset] == '\0' || (path[pathoffset] == '/' && path[pathoffset + 1] == '\0')) {

                DEBUGPRINT(6, "vmc_fs: Breaking from function\n");
                DEBUGPRINT(6, "vmc_fs: dir_cluster = %u\n", dirent.cluster);
                DEBUGPRINT(6, "vmc_fs: dirent.name = %s\n", dirent.name);
                DEBUGPRINT(6, "vmc_fs: dirent.length = %u\n", dirent.length);

                status = 1;

                break;

            }
            // Otherwise we found the subfolder, but not the
            // requested entry, keep going
            else {

                DEBUGPRINT(6, "vmc_fs: Recursing into subfolder\n");
                DEBUGPRINT(6, "vmc_fs: dir_cluster = %u\n", dirent.cluster);
                DEBUGPRINT(6, "vmc_fs: dirent.name = %s\n", dirent.name);
                DEBUGPRINT(6, "vmc_fs: dirent.length = %u\n", dirent.length);

                i = -1;                            // will be 0 when we continue, essentially starting the loop over again
                current_cluster = dirent.cluster;  // dirent.cluster refer to fat table and current_cluster to allocatable place
                length = dirent.length;            // set length to current directory length
                pathoffset++;                      // add one to skip past the / in the folder name

                continue;
            }
        }

        // If we just read the second half of a cluster, we need to set
        // current_cluster to the next cluster in the chain.
        if (i % g_Vmc_Image[unit].header.pages_per_cluster) {

            current_cluster = getFatEntry(gendata->fd, current_cluster, gendata->indir_fat_clusters, FAT_VALUE);
        }
    }

    // When we get here, that means one of two things:
    // 1 ) We found the requested folder / file
    // 2 ) We searched through all files / folders, and could not find it.
    // To determine which one it was, check the status variable.
    if (status == 0) {

        PROF_END(getDirentryFromPathProf)

        return NOFOUND_CLUSTER;
    }

    // Copy the last directory entry's contents into 'retval'
    memcpy(retval, &dirent, sizeof(dirent));

    PROF_END(getDirentryFromPathProf)

    // Return the cluster where the desired directory entry can be found
    return current_cluster;
}


//----------------------------------------------------------------------------
// Add 2 pseudo entries for a new directory
//----------------------------------------------------------------------------
unsigned int addPseudoEntries(struct gen_privdata *gendata, struct direntry *parent, int unit)
{

    // Get a free cluster we can use to store the entries '.' and '..'
    unsigned int pseudo_entries_cluster = getFreeCluster(gendata, unit);

    if (pseudo_entries_cluster == ERROR_CLUSTER) {

        DEBUGPRINT(2, "vmc_fs: Not enough free space to add pseudo entries.  Aborting.\n");

        return ERROR_CLUSTER;
    }

    // Create the first 2 psuedo entries for the folder, and write them
    DEBUGPRINT(5, "vmc_fs: Adding pseudo entries into fat table cluster %u\n", pseudo_entries_cluster);

    struct direntry pseudo_entries;

    memset(&pseudo_entries, 0, sizeof(pseudo_entries));

    // fill pseudo entries
    strcpy(pseudo_entries.name, ".");
    pseudo_entries.dir_entry = parent->length;
    pseudo_entries.length = 0;
    pseudo_entries.cluster = parent->cluster;
    pseudo_entries.mode = DF_EXISTS | DF_0400 | DF_DIRECTORY | DF_READ | DF_WRITE | DF_EXECUTE;  // 0x8427

    getPs2Time(&pseudo_entries.created);
    getPs2Time(&pseudo_entries.modified);

    // write first pseudo entry
    writePage(gendata->fd, (unsigned char *)&pseudo_entries, (pseudo_entries_cluster + gendata->first_allocatable) * g_Vmc_Image[unit].header.pages_per_cluster);

    strcpy(pseudo_entries.name, "..");
    pseudo_entries.dir_entry = 0;
    pseudo_entries.cluster = 0;

    // write second pseudo entry
    writePage(gendata->fd, (unsigned char *)&pseudo_entries, (pseudo_entries_cluster + gendata->first_allocatable) * g_Vmc_Image[unit].header.pages_per_cluster + 1);

    // set cluster as EOF
    setFatEntry(gendata->fd, pseudo_entries_cluster, EOF_CLUSTER, gendata->indir_fat_clusters, FAT_SET);

    return pseudo_entries_cluster;
}


//----------------------------------------------------------------------------
// Add an object into vmc image.
// This fonction get a free cluster where it can insert the object, insert it and return the cluster number.
// Object can be a file or a folder.
//----------------------------------------------------------------------------
unsigned int addObject(struct gen_privdata *gendata, unsigned int parent_cluster, struct direntry *parent, struct direntry *dirent, int unit)
{

    int i = 0;
    unsigned int retval = ERROR_CLUSTER;
    unsigned int current_cluster = parent->cluster;

    // Find to the last cluster in the list of files / folder to add our new object after it
    for (i = 0; i < parent->length; i += g_Vmc_Image[unit].header.pages_per_cluster) {

        DEBUGPRINT(6, "vmc_fs: Following fat table cluster: %u\n", current_cluster);

        if (getFatEntry(gendata->fd, current_cluster, gendata->indir_fat_clusters, FAT_VALUE) == EOF_CLUSTER) {

            DEBUGPRINT(6, "vmc_fs: Last used cluster in fat table %u\n", current_cluster);
            break;
        }

        current_cluster = getFatEntry(gendata->fd, current_cluster, gendata->indir_fat_clusters, FAT_VALUE);
    }

    // Check if we need a new cluster or if we can add our object at the end of an allocatable cluster
    if (parent->length % g_Vmc_Image[unit].header.pages_per_cluster == 0)  // if its even, we need a new cluster for our file
    {

        // Get a free cluster because our object require an additional cluster
        unsigned int nextfree_cluster = getFreeCluster(gendata, unit);

        if (nextfree_cluster == ERROR_CLUSTER) {

            DEBUGPRINT(2, "vmc_fs: Not enough free space.  Aborting.\n");

            return ERROR_CLUSTER;
        }

        DEBUGPRINT(6, "vmc_fs: Added new object in fat table cluster %u ( Page %u ) \n", current_cluster, current_cluster * g_Vmc_Image[unit].header.pages_per_cluster);

        setFatEntry(gendata->fd, current_cluster, nextfree_cluster, gendata->indir_fat_clusters, FAT_SET);  // update the last cluster in the directory entry list to point to our new cluster
        setFatEntry(gendata->fd, nextfree_cluster, EOF_CLUSTER, gendata->indir_fat_clusters, FAT_SET);      // set the free cluster we just found to be EOF

        // If the object is a folder, we have to add 2 psuedoentries
        if (dirent->mode & DF_DIRECTORY) {

            // Link the new folder to the entries we just created
            dirent->cluster = addPseudoEntries(gendata, parent, unit);

            if (dirent->cluster == ERROR_CLUSTER)
                return ERROR_CLUSTER;

            // Set the length of the directory to 2, for the 2 psuedoentries we just added
            dirent->length = 2;
        }

        // write the page containing our direntry object information
        writePage(gendata->fd, (unsigned char *)dirent, (nextfree_cluster + gendata->first_allocatable) * g_Vmc_Image[unit].header.pages_per_cluster);

        // now we need to update the length of the parent directory
        parent->length++;
        writePage(gendata->fd, (unsigned char *)parent, (parent_cluster + gendata->first_allocatable) * g_Vmc_Image[unit].header.pages_per_cluster + gendata->dirent_page);

        gendata->dirent_page = 0;
        retval = nextfree_cluster;

    } else  // otherwise we can just add the directory entry to the end of the last cluster
    {

        DEBUGPRINT(5, "vmc_fs: Added new object in fat table cluster %u ( Page %u ) \n", current_cluster, current_cluster * g_Vmc_Image[unit].header.pages_per_cluster + 1);

        // If the object is a folder, we have to add 2 psuedoentries
        if (dirent->mode & DF_DIRECTORY) {

            // Link the new folder to the entries we just created
            dirent->cluster = addPseudoEntries(gendata, parent, unit);

            if (dirent->cluster == ERROR_CLUSTER)
                return ERROR_CLUSTER;

            // Set the length of the directory to 2, for the 2 psuedoentries we just added
            dirent->length = 2;
        }

        // write the page containing our direntry object information
        writePage(gendata->fd, (unsigned char *)dirent, (current_cluster + gendata->first_allocatable) * g_Vmc_Image[unit].header.pages_per_cluster + 1);  // write the page containing our directory information ( + 1 because we are writing the second page in the cluster )

        // now we need to update the length of the parent directory
        parent->length++;
        writePage(gendata->fd, (unsigned char *)parent, (parent_cluster + gendata->first_allocatable) * g_Vmc_Image[unit].header.pages_per_cluster + gendata->dirent_page);

        gendata->dirent_page = 1;
        retval = current_cluster;
    }

    return retval;
}


//----------------------------------------------------------------------------
// Remove an object from vmc image.
// Object can be a file or a directory.
// Follow fat table to find the last cluster of the direntry chain cluster. ( EOF_CLUSTER )
// Change bit mask of tested cluster if it's not EOF. ( only for files )
// Set as free the last cluster of the direntry.
// Finaly, set deleted flag of the direntry. ( DF_EXISTS flag )
//----------------------------------------------------------------------------
void removeObject(struct gen_privdata *gendata, unsigned int dirent_cluster, struct direntry *dirent, int unit)
{

    unsigned int last_cluster = dirent->cluster;

    DEBUGPRINT(3, "vmc_fs: Searching last cluster of direntry\n");

    while (1) {
        unsigned int current_cluster;

        current_cluster = getFatEntry(gendata->fd, last_cluster, gendata->indir_fat_clusters, FAT_VALUE);

        if (current_cluster == FREE_CLUSTER) {

            // FREE_CLUSTER mean nothing to remove or error, so return
            DEBUGPRINT(10, "vmc_fs: Testing cluster %u ... value is FREE_CLUSTER\n", last_cluster);

            return;

        } else if (current_cluster == EOF_CLUSTER) {

            // EOF_CLUSTER mean last cluster of the direntry is found
            DEBUGPRINT(3, "vmc_fs: Last cluster of direntry at %u\n", last_cluster);

            break;

        } else {

            // Otherwise change bit mask of tested cluster
            DEBUGPRINT(10, "vmc_fs: Testing cluster %u ... value is %u\n", last_cluster, current_cluster);

            setFatEntry(gendata->fd, last_cluster, current_cluster, gendata->indir_fat_clusters, FAT_RESET);
        }

        last_cluster = current_cluster;
    }

    // Set last cluster of direntry as free.
    setFatEntry(gendata->fd, last_cluster, FREE_CLUSTER, gendata->indir_fat_clusters, FAT_RESET);  // set the last cluster of the file as free

    // Set object as deleted. ( Remove DF_EXISTS flag )
    dirent->mode = dirent->mode ^ DF_EXISTS;
    writePage(gendata->fd, (unsigned char *)dirent, (dirent_cluster + gendata->first_allocatable) * g_Vmc_Image[unit].header.pages_per_cluster + gendata->dirent_page);
}


//----------------------------------------------------------------------------
// Return a free cluster.
//----------------------------------------------------------------------------
unsigned int getFreeCluster(struct gen_privdata *gendata, int unit)
{

    unsigned int i;
    unsigned int cluster_mask;

    for (i = g_Vmc_Image[unit].last_free_cluster; i < gendata->last_allocatable; i++) {
        unsigned int value;

        value = getFatEntry(gendata->fd, i - gendata->first_allocatable, gendata->indir_fat_clusters, FAT_VALUE);

        if (value == FREE_CLUSTER) {

            DEBUGPRINT(10, "vmc_fs: Testing fat table cluster %d ... value is FREE_CLUSTER\n", i - gendata->first_allocatable);

            DEBUGPRINT(6, "vmc_fs: Free cluster found at %d in fat table\n", i - gendata->first_allocatable);
            g_Vmc_Image[unit].last_free_cluster = i;

            return (i - gendata->first_allocatable);

        } else if (value == EOF_CLUSTER) {

            DEBUGPRINT(10, "vmc_fs: Testing fat table cluster %d ... value is EOF_CLUSTER\n", i - gendata->first_allocatable);

        } else {

            DEBUGPRINT(10, "vmc_fs: Testing fat table cluster %d ... value is %d\n", i - gendata->first_allocatable, value);

            cluster_mask = getFatEntry(gendata->fd, i - gendata->first_allocatable, gendata->indir_fat_clusters, FAT_MASK);

            if (cluster_mask != MASK_CLUSTER) {

                DEBUGPRINT(6, "vmc_fs: Free cluster found at %d in fat table\n", i - gendata->first_allocatable);
                g_Vmc_Image[unit].last_free_cluster = i;

                return (i - gendata->first_allocatable);
            }
        }
    }

    return ERROR_CLUSTER;
}


//----------------------------------------------------------------------------
// Set default specification to superblock header when card is not formatted
//----------------------------------------------------------------------------
int setDefaultSpec(int unit)
{

    DEBUGPRINT(4, "vmc_fs: SuperBlock set by default\n");

    memset(&g_Vmc_Image[unit].header, g_Vmc_Image[unit].erase_byte, sizeof(struct superblock));

    strcpy((char *)g_Vmc_Image[unit].header.magic, "Sony PS2 Memory Card Format 1.2.0.0");

    DEBUGPRINT(4, "vmc_fs: SuperBlock Info: magic[40]             : %s\n", g_Vmc_Image[unit].header.magic);

    g_Vmc_Image[unit].header.page_size = 0x200;
    g_Vmc_Image[unit].header.pages_per_cluster = 0x2;
    g_Vmc_Image[unit].header.pages_per_block = 0x10;
    g_Vmc_Image[unit].header.unused0 = 0xFF00;

    g_Vmc_Image[unit].cluster_size = g_Vmc_Image[unit].header.page_size * g_Vmc_Image[unit].header.pages_per_cluster;
    g_Vmc_Image[unit].erase_byte = 0xFF;
    g_Vmc_Image[unit].last_idc = EOF_CLUSTER;
    g_Vmc_Image[unit].last_cluster = EOF_CLUSTER;

    if (g_Vmc_Image[unit].card_size % (g_Vmc_Image[unit].header.page_size + 0x10) == 0) {

        g_Vmc_Image[unit].ecc_flag = TRUE;
        g_Vmc_Image[unit].total_pages = g_Vmc_Image[unit].card_size / (g_Vmc_Image[unit].header.page_size + 0x10);
        g_Vmc_Image[unit].header.clusters_per_card = g_Vmc_Image[unit].card_size / ((g_Vmc_Image[unit].header.page_size + 0x10) * g_Vmc_Image[unit].header.pages_per_cluster);

    } else if (g_Vmc_Image[unit].card_size % g_Vmc_Image[unit].header.page_size == 0) {

        g_Vmc_Image[unit].ecc_flag = FALSE;
        g_Vmc_Image[unit].total_pages = g_Vmc_Image[unit].card_size / g_Vmc_Image[unit].header.page_size;
        g_Vmc_Image[unit].header.clusters_per_card = g_Vmc_Image[unit].card_size / (g_Vmc_Image[unit].header.page_size * g_Vmc_Image[unit].header.pages_per_cluster);

    } else {

        // Size error
        return 0;
    }

    g_Vmc_Image[unit].header.mc_type = 0x2;
    g_Vmc_Image[unit].header.mc_flag = 0x2B;

    DEBUGPRINT(4, "vmc_fs: Image file Info: Number of pages       : %d\n", g_Vmc_Image[unit].total_pages);
    DEBUGPRINT(4, "vmc_fs: Image file Info: Size of a cluster     : %d bytes\n", g_Vmc_Image[unit].cluster_size);
    DEBUGPRINT(4, "vmc_fs: Image file Info: ECC shunk found       : %s\n", g_Vmc_Image[unit].ecc_flag ? "YES" : "NO");
    DEBUGPRINT(3, "vmc_fs: Image file Info: Vmc card type         : %s MemoryCard.\n", (g_Vmc_Image[unit].header.mc_type == PSX_MEMORYCARD ? "PSX" : (g_Vmc_Image[unit].header.mc_type == PS2_MEMORYCARD ? "PS2" : "PDA")));
    DEBUGPRINT(4, "vmc_fs: SuperBlock Info: page_size             : 0x%02x\n", g_Vmc_Image[unit].header.page_size);
    DEBUGPRINT(4, "vmc_fs: SuperBlock Info: pages_per_cluster     : 0x%02x\n", g_Vmc_Image[unit].header.pages_per_cluster);
    DEBUGPRINT(4, "vmc_fs: SuperBlock Info: pages_per_block       : 0x%02x\n", g_Vmc_Image[unit].header.pages_per_block);
    DEBUGPRINT(4, "vmc_fs: SuperBlock Info: clusters_per_card     : 0x%02x\n", g_Vmc_Image[unit].header.clusters_per_card);

    g_Vmc_Image[unit].header.first_allocatable = ((((g_Vmc_Image[unit].total_pages - 1) * (g_Vmc_Image[unit].cluster_size / g_Vmc_Image[unit].header.page_size)) / g_Vmc_Image[unit].cluster_size) + 1) + 8 + ((g_Vmc_Image[unit].total_pages - 1) / (g_Vmc_Image[unit].total_pages * g_Vmc_Image[unit].header.pages_per_cluster * (g_Vmc_Image[unit].cluster_size / g_Vmc_Image[unit].header.page_size))) + 1;
    g_Vmc_Image[unit].header.last_allocatable = (g_Vmc_Image[unit].header.clusters_per_card - g_Vmc_Image[unit].header.first_allocatable) - ((g_Vmc_Image[unit].header.pages_per_block / g_Vmc_Image[unit].header.pages_per_cluster) * g_Vmc_Image[unit].header.pages_per_cluster);
    g_Vmc_Image[unit].header.root_cluster = 0;

    g_Vmc_Image[unit].header.backup_block1 = (g_Vmc_Image[unit].total_pages / g_Vmc_Image[unit].header.pages_per_block) - 1;
    g_Vmc_Image[unit].header.backup_block2 = g_Vmc_Image[unit].header.backup_block1 - 1;
    memset(g_Vmc_Image[unit].header.unused1, g_Vmc_Image[unit].erase_byte, 8);

    DEBUGPRINT(4, "vmc_fs: SuperBlock Info: first_allocatable     : 0x%02x\n", g_Vmc_Image[unit].header.first_allocatable);
    DEBUGPRINT(4, "vmc_fs: SuperBlock Info: last_allocatable      : 0x%02x\n", g_Vmc_Image[unit].header.last_allocatable);
    DEBUGPRINT(4, "vmc_fs: SuperBlock Info: root_cluster          : 0x%02x\n", g_Vmc_Image[unit].header.root_cluster);
    DEBUGPRINT(4, "vmc_fs: SuperBlock Info: backup_block1         : 0x%02x\n", g_Vmc_Image[unit].header.backup_block1);
    DEBUGPRINT(4, "vmc_fs: SuperBlock Info: backup_block2         : 0x%02x\n", g_Vmc_Image[unit].header.backup_block2);

    unsigned int last_blk_sector = 8;
    int i = 0;

    for (i = 0; i <= ((g_Vmc_Image[unit].total_pages - 1) / 65536); i++) {
        if ((((last_blk_sector + i) * (g_Vmc_Image[unit].cluster_size / g_Vmc_Image[unit].header.page_size)) % g_Vmc_Image[unit].header.pages_per_block) == 0) {
            last_blk_sector = last_blk_sector + i;
        }
        g_Vmc_Image[unit].header.indir_fat_clusters[i] = last_blk_sector + i;
    }

    for (i = 0; g_Vmc_Image[unit].header.indir_fat_clusters[i] != 0; i++)
        DEBUGPRINT(4, "vmc_fs: SuperBlock Info: indir_fat_clusters[%d] : 0x%02x\n", i, g_Vmc_Image[unit].header.indir_fat_clusters[i]);

    memset(g_Vmc_Image[unit].header.bad_block_list, g_Vmc_Image[unit].erase_byte, sizeof(unsigned int) * 32);

    g_Vmc_Image[unit].header.unused2 = 0;
    g_Vmc_Image[unit].header.unused3 = 0x100;
    g_Vmc_Image[unit].header.size_in_megs = (g_Vmc_Image[unit].total_pages * g_Vmc_Image[unit].header.page_size) / (g_Vmc_Image[unit].cluster_size * g_Vmc_Image[unit].cluster_size);
    g_Vmc_Image[unit].header.unused4 = 0xFFFFFFFF;
    memset(g_Vmc_Image[unit].header.unused5, g_Vmc_Image[unit].erase_byte, 12);
    g_Vmc_Image[unit].header.max_used = (97 * g_Vmc_Image[unit].header.clusters_per_card) / 100;
    memset(g_Vmc_Image[unit].header.unused6, g_Vmc_Image[unit].erase_byte, 8);
    g_Vmc_Image[unit].header.unused7 = 0xFFFFFFFF;

    DEBUGPRINT(4, "vmc_fs: SuperBlock Info: mc_type               : 0x%02x\n", g_Vmc_Image[unit].header.mc_type);
    DEBUGPRINT(4, "vmc_fs: SuperBlock Info: mc_flag               : 0x%02x\n", g_Vmc_Image[unit].header.mc_flag);

    g_Vmc_Image[unit].last_free_cluster = g_Vmc_Image[unit].header.first_allocatable;

    return 1;
}


//----------------------------------------------------------------------------
// Used for timing functions, use this to optimize stuff
//----------------------------------------------------------------------------
#ifdef PROFILING

iop_sys_clock_t iop_clock_finished;  // Global clock finished data

void profilerStart(iop_sys_clock_t *iopclock)
{

    GetSystemTime(iopclock);
}

void profilerEnd(const char *function, const char *name, iop_sys_clock_t *iopclock1)
{

    // Make this the first item, so we don't add time that need not be added
    GetSystemTime(&iop_clock_finished);

    unsigned int sec1, usec1;
    unsigned int sec2, usec2;

    SysClock2USec(&iop_clock_finished, &sec2, &usec2);
    SysClock2USec(iopclock1, &sec1, &usec1);

    printf("vmc_fs: Profiler[ %s ]: %s %u. %u seconds\n", function, name, sec2 - sec1, (usec2 - usec1) / 1000);
}

#endif


//----------------------------------------------------------------------------
// Get date and time from cdvd fonction and put them into a "vmc_datetime" struct pointer.
//----------------------------------------------------------------------------
int getPs2Time(vmc_datetime *tm)
{

    sceCdCLOCK cdtime;
    s32 tmp;

    static vmc_datetime timeBuf = {
        0, 0x00, 0x00, 0x0A, 0x01, 0x01, 2008  // used if can not get time...
    };

    if (sceCdReadClock(&cdtime) != 0 && cdtime.stat == 0) {

        tmp = cdtime.second >> 4;
        timeBuf.sec = (unsigned int)(((tmp << 2) + tmp) << 1) + (cdtime.second & 0x0F);
        tmp = cdtime.minute >> 4;
        timeBuf.min = (((tmp << 2) + tmp) << 1) + (cdtime.minute & 0x0F);
        tmp = cdtime.hour >> 4;
        timeBuf.hour = (((tmp << 2) + tmp) << 1) + (cdtime.hour & 0x0F);
        tmp = cdtime.day >> 4;
        timeBuf.day = (((tmp << 2) + tmp) << 1) + (cdtime.day & 0x0F);
        tmp = (cdtime.month >> 4) & 0x7F;
        timeBuf.month = (((tmp << 2) + tmp) << 1) + (cdtime.month & 0x0F);
        tmp = cdtime.year >> 4;
        timeBuf.year = (((tmp << 2) + tmp) << 1) + (cdtime.year & 0xF) + 2000;
    }

    memcpy(tm, &timeBuf, sizeof(vmc_datetime));

    return 0;
}


// clang-format off
//----------------------------------------------------------------------------
// XOR table use for Error Correcting Code ( ECC ) calculation.
//----------------------------------------------------------------------------
const unsigned char ECC_XOR_Table[256] = {
    0x00, 0x87, 0x96, 0x11, 0xA5, 0x22, 0x33, 0xB4,
    0xB4, 0x33, 0x22, 0xA5, 0x11, 0x96, 0x87, 0x00,
    0xC3, 0x44, 0x55, 0xD2, 0x66, 0xE1, 0xF0, 0x77,
    0x77, 0xF0, 0xE1, 0x66, 0xD2, 0x55, 0x44, 0xC3,
    0xD2, 0x55, 0x44, 0xC3, 0x77, 0xF0, 0xE1, 0x66,
    0x66, 0xE1, 0xF0, 0x77, 0xC3, 0x44, 0x55, 0xD2,
    0x11, 0x96, 0x87, 0x00, 0xB4, 0x33, 0x22, 0xA5,
    0xA5, 0x22, 0x33, 0xB4, 0x00, 0x87, 0x96, 0x11,
    0xE1, 0x66, 0x77, 0xF0, 0x44, 0xC3, 0xD2, 0x55,
    0x55, 0xD2, 0xC3, 0x44, 0xF0, 0x77, 0x66, 0xE1,
    0x22, 0xA5, 0xB4, 0x33, 0x87, 0x00, 0x11, 0x96,
    0x96, 0x11, 0x00, 0x87, 0x33, 0xB4, 0xA5, 0x22,
    0x33, 0xB4, 0xA5, 0x22, 0x96, 0x11, 0x00, 0x87,
    0x87, 0x00, 0x11, 0x96, 0x22, 0xA5, 0xB4, 0x33,
    0xF0, 0x77, 0x66, 0xE1, 0x55, 0xD2, 0xC3, 0x44,
    0x44, 0xC3, 0xD2, 0x55, 0xE1, 0x66, 0x77, 0xF0,
    0xF0, 0x77, 0x66, 0xE1, 0x55, 0xD2, 0xC3, 0x44,
    0x44, 0xC3, 0xD2, 0x55, 0xE1, 0x66, 0x77, 0xF0,
    0x33, 0xB4, 0xA5, 0x22, 0x96, 0x11, 0x00, 0x87,
    0x87, 0x00, 0x11, 0x96, 0x22, 0xA5, 0xB4, 0x33,
    0x22, 0xA5, 0xB4, 0x33, 0x87, 0x00, 0x11, 0x96,
    0x96, 0x11, 0x00, 0x87, 0x33, 0xB4, 0xA5, 0x22,
    0xE1, 0x66, 0x77, 0xF0, 0x44, 0xC3, 0xD2, 0x55,
    0x55, 0xD2, 0xC3, 0x44, 0xF0, 0x77, 0x66, 0xE1,
    0x11, 0x96, 0x87, 0x00, 0xB4, 0x33, 0x22, 0xA5,
    0xA5, 0x22, 0x33, 0xB4, 0x00, 0x87, 0x96, 0x11,
    0xD2, 0x55, 0x44, 0xC3, 0x77, 0xF0, 0xE1, 0x66,
    0x66, 0xE1, 0xF0, 0x77, 0xC3, 0x44, 0x55, 0xD2,
    0xC3, 0x44, 0x55, 0xD2, 0x66, 0xE1, 0xF0, 0x77,
    0x77, 0xF0, 0xE1, 0x66, 0xD2, 0x55, 0x44, 0xC3,
    0x00, 0x87, 0x96, 0x11, 0xA5, 0x22, 0x33, 0xB4,
    0xB4, 0x33, 0x22, 0xA5, 0x11, 0x96, 0x87, 0x00,
};
// clang-format on


//----------------------------------------------------------------------------
// Calculate ECC for a 128 bytes chunk of data
//----------------------------------------------------------------------------
static void calculateECC(u8 *ECC_Chunk, const u8 *Data_Chunk)
{
    int i;

    ECC_Chunk[0] = ECC_Chunk[1] = ECC_Chunk[2] = 0;

    for (i = 0; i < 0x80; i++) {
        int c;

        c = ECC_XOR_Table[Data_Chunk[i]];

        ECC_Chunk[0] ^= c;

        if (c & 0x80) {

            ECC_Chunk[1] ^= ~i;
            ECC_Chunk[2] ^= i;
        }
    }

    ECC_Chunk[0] = ~ECC_Chunk[0];
    ECC_Chunk[0] &= 0x77;
    ECC_Chunk[1] = ~ECC_Chunk[1];
    ECC_Chunk[1] &= 0x7f;
    ECC_Chunk[2] = ~ECC_Chunk[2];
    ECC_Chunk[2] &= 0x7f;
}


//----------------------------------------------------------------------------
// Build ECC from a complet page of data
//----------------------------------------------------------------------------
void buildECC(int unit, const u8 *Page_Data, u8 *ECC_Data)
{

    u8 Data_Chunk[4][128];
    u8 ECC_Chunk[4][3];
    u8 ECC_Pad[4];

    // This is to divide the page in 128 bytes chunks
    memcpy(Data_Chunk[0], Page_Data + 0, 128);
    memcpy(Data_Chunk[1], Page_Data + 128, 128);
    memcpy(Data_Chunk[2], Page_Data + 256, 128);
    memcpy(Data_Chunk[3], Page_Data + 384, 128);

    // Ask for 128 bytes chunk ECC calculation, it returns 3 bytes per chunk
    calculateECC(ECC_Chunk[0], Data_Chunk[0]);
    calculateECC(ECC_Chunk[1], Data_Chunk[1]);
    calculateECC(ECC_Chunk[2], Data_Chunk[2]);
    calculateECC(ECC_Chunk[3], Data_Chunk[3]);

    // Prepare Padding as ECC took only 12 bytes and stand on 16 bytes
    memset(ECC_Pad, g_Vmc_Image[unit].erase_byte, sizeof(ECC_Pad));

    // "MemCopy" our four 3 bytes ECC chunks into our 16 bytes ECC data buffer
    // Finaly "MemCopy" our 4 bytes PAD chunks into last 4 bytes of our ECC data buffer
    memcpy(ECC_Data + 0, ECC_Chunk[0], 3);
    memcpy(ECC_Data + 3, ECC_Chunk[1], 3);
    memcpy(ECC_Data + 6, ECC_Chunk[2], 3);
    memcpy(ECC_Data + 9, ECC_Chunk[3], 3);
    memcpy(ECC_Data + 12, ECC_Pad, 4);
}
