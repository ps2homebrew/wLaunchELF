#include "vmc.h"


//  IO fonctions


//----------------------------------------------------------------------------
// Format a vmc file previously mounted with fileXioMount(.
// mode can be FORMAT_FULL which mean erase all pages before formatting.
// or FORMAT_FAST which skip erasing all pages before formatting.
//----------------------------------------------------------------------------
int Vmc_Format(iop_file_t *f, const char *dev, const char *blockdev, void *arg, int arglen)
{

    if (!g_Vmc_Initialized)
        return VMCFS_ERR_INITIALIZED;

    DEBUGPRINT(1, "vmc_fs: format %d\n", f->unit);

    int mode = FORMAT_FULL;

    struct direntry dirent;
    int all_sector;
    u8 *mcbuffer, *mcbuffer2;
    unsigned int i, j, k, x, y, last_blk_sector, Page_Num, Page_Num2;

    Page_Num = 0;
    Page_Num2 = 0;

    if (g_Vmc_Image[f->unit].fd < 0)
        return VMCFS_ERR_NOT_MOUNT;

    PROF_START(vmc_formatProf)

    if (mode == FORMAT_FULL) {

        for (i = 0; i < g_Vmc_Image[f->unit].total_pages / g_Vmc_Image[f->unit].header.pages_per_block; i++) {

            DEBUGPRINT(7, "vmc_fs: format erasing block %d / %d\r", i, g_Vmc_Image[f->unit].total_pages / g_Vmc_Image[f->unit].header.pages_per_block);
            eraseBlock(g_Vmc_Image[f->unit].fd, i);
        }
    }

    mcbuffer = (u8 *)malloc((g_Vmc_Image[f->unit].header.page_size + 0xFF) & ~(unsigned int)0xFF);
    mcbuffer2 = (u8 *)malloc((g_Vmc_Image[f->unit].header.page_size + 0xFF) & ~(unsigned int)0xFF);

    DEBUGPRINT(3, "vmc_fs: format start\n");

    memset(mcbuffer, g_Vmc_Image[f->unit].erase_byte, g_Vmc_Image[f->unit].header.page_size);
    memcpy(mcbuffer, &g_Vmc_Image[f->unit].header, sizeof(struct superblock));

    //  Write header page
    writePage(g_Vmc_Image[f->unit].fd, mcbuffer, Page_Num);

    Page_Num++;

    DEBUGPRINT(3, "vmc_fs: format indirect fat cluster\n");

    //  goto fat table
    for (i = 1; i < (g_Vmc_Image[f->unit].header.indir_fat_clusters[0] * g_Vmc_Image[f->unit].header.pages_per_cluster); i++, Page_Num++) {
    }

    all_sector = 0;

    DEBUGPRINT(3, "vmc_fs: format fat table\n");

    //  Write fat table pages
    for (x = 0; g_Vmc_Image[f->unit].header.indir_fat_clusters[x] != 0; x++) {

        last_blk_sector = g_Vmc_Image[f->unit].header.indir_fat_clusters[0] + x;
        Page_Num = last_blk_sector * g_Vmc_Image[f->unit].header.pages_per_cluster;

        for (i = x * g_Vmc_Image[f->unit].header.pages_per_cluster; (i < ((x + 1) * g_Vmc_Image[f->unit].header.pages_per_cluster)) && (i <= ((g_Vmc_Image[f->unit].total_pages - 1) / 65536)); i++, Page_Num++) {

            memset(mcbuffer, g_Vmc_Image[f->unit].erase_byte, g_Vmc_Image[f->unit].header.page_size);

            for (j = i * ((g_Vmc_Image[f->unit].header.page_size / 2) / g_Vmc_Image[f->unit].header.pages_per_cluster), k = 0; (j < i * ((g_Vmc_Image[f->unit].header.page_size / 2) / g_Vmc_Image[f->unit].header.pages_per_cluster) + ((g_Vmc_Image[f->unit].header.page_size / 2) / g_Vmc_Image[f->unit].header.pages_per_cluster)) && (j < ((g_Vmc_Image[f->unit].header.last_allocatable / (g_Vmc_Image[f->unit].header.page_size / 2)) + 1)); j++, k++) {

                ((unsigned int *)mcbuffer)[k] = ((g_Vmc_Image[f->unit].total_pages - 1) / 65536) + last_blk_sector + 1 + j;

                memset(mcbuffer2, g_Vmc_Image[f->unit].erase_byte, g_Vmc_Image[f->unit].header.page_size);
                Page_Num2 = ((unsigned int *)mcbuffer)[k] * g_Vmc_Image[f->unit].header.pages_per_cluster;

                for (y = 0; (y < ((g_Vmc_Image[f->unit].header.page_size / 2) / g_Vmc_Image[f->unit].header.pages_per_cluster)) && (all_sector < g_Vmc_Image[f->unit].header.last_allocatable); y++, all_sector++) {

                    if (y == 0 && j == 0 && i == 0) {

                        ((unsigned int *)mcbuffer2)[y] = EOF_CLUSTER;

                    } else {

                        ((unsigned int *)mcbuffer2)[y] = FREE_CLUSTER;
                    }
                }

                writePage(g_Vmc_Image[f->unit].fd, mcbuffer2, Page_Num2);

                Page_Num2++;
                memset(mcbuffer2, g_Vmc_Image[f->unit].erase_byte, g_Vmc_Image[f->unit].header.page_size);

                for (y = 0; (y < ((g_Vmc_Image[f->unit].header.page_size / 2) / g_Vmc_Image[f->unit].header.pages_per_cluster)) && (all_sector < g_Vmc_Image[f->unit].header.last_allocatable); y++, all_sector++) {

                    ((unsigned int *)mcbuffer2)[y] = FREE_CLUSTER;
                }

                writePage(g_Vmc_Image[f->unit].fd, mcbuffer2, Page_Num2);
            }

            writePage(g_Vmc_Image[f->unit].fd, mcbuffer, Page_Num);
        }
    }

    DEBUGPRINT(3, "vmc_fs: format root directory\n");

    Page_Num = g_Vmc_Image[f->unit].header.first_allocatable * g_Vmc_Image[f->unit].header.pages_per_cluster;

    memset(&dirent, 0, sizeof(dirent));

    dirent.mode = DF_EXISTS | DF_0400 | DF_DIRECTORY | DF_READ | DF_WRITE | DF_EXECUTE;  // 0x8427
    dirent.length = 2;
    dirent.name[0] = '.';
    getPs2Time(&dirent.created);
    getPs2Time(&dirent.modified);

    writePage(g_Vmc_Image[f->unit].fd, (unsigned char *)&dirent, Page_Num);
    Page_Num++;

    memset(&dirent, 0, sizeof(dirent));

    dirent.mode = DF_EXISTS | DF_HIDDEN | DF_0400 | DF_DIRECTORY | DF_WRITE | DF_EXECUTE;  // 0xa426;
    dirent.length = 0;
    dirent.name[0] = '.';
    dirent.name[1] = '.';
    getPs2Time(&dirent.created);
    getPs2Time(&dirent.modified);

    writePage(g_Vmc_Image[f->unit].fd, (unsigned char *)&dirent, Page_Num);

    free(mcbuffer);
    free(mcbuffer2);

    DEBUGPRINT(3, "vmc_fs: format finished\n");

    PROF_END(vmc_formatProf)

    return VMCFS_ERR_NO;
}


//----------------------------------------------------------------------------
// Open a file from vmc image. open("vmc...
//----------------------------------------------------------------------------
int Vmc_Open(iop_file_t *f, const char *path1, int flags, int mode)
{

    if (!g_Vmc_Initialized)
        return VMCFS_ERR_INITIALIZED;

    DEBUGPRINT(1, "vmc_fs: Open %i %s\n", f->unit, path1);

    if (g_Vmc_Image[f->unit].fd < 0)
        return VMCFS_ERR_NOT_MOUNT;

    if (g_Vmc_Image[f->unit].formated == FALSE)
        return VMCFS_ERR_NOT_FORMATED;

    PROF_START(vmc_openProf)

    struct direntry dirent;
    struct file_privdata *fprivdata = malloc(sizeof(struct file_privdata));

    if (fprivdata == NULL)
        return -1;

    f->privdata = fprivdata;

    fprivdata->gendata.fd = g_Vmc_Image[f->unit].fd;
    fprivdata->gendata.first_allocatable = g_Vmc_Image[f->unit].header.first_allocatable;
    fprivdata->gendata.last_allocatable = g_Vmc_Image[f->unit].header.last_allocatable;

    memcpy(fprivdata->gendata.indir_fat_clusters, g_Vmc_Image[f->unit].header.indir_fat_clusters, sizeof(unsigned int) * 32);

    unsigned int dirent_cluster = getDirentryFromPath(&dirent, path1, &(fprivdata->gendata), f->unit);

    if (dirent_cluster == ROOT_CLUSTER) {

        free(fprivdata);  //  Release the allocated memory

        PROF_END(vmc_openProf)

        return -1;  //  File not found, could not be opened.

    } else if (dirent_cluster == NOFOUND_CLUSTER) {

        if (flags & O_CREAT) {

            DEBUGPRINT(2, "vmc_fs: Open with O_CREAT.\n");

            struct direntry parent;
            unsigned int parent_cluster = 0;

            char *path = malloc(strlen(path1) + 1);  //  + 1 for null terminator
            memcpy(path, path1, strlen(path1) + 1);  //  create a local copy to work with

            char *filename = strrchr(path, '/');  //  last occurance of  / , which should split the file name from the folders

            //  essentially splits path into 2 strings
            filename[0] = '\0';
            filename++;

            DEBUGPRINT(2, "vmc_fs: Creating file %s in parent folder %s\n", filename, path);

            memset(&dirent, 0, sizeof(dirent));

            // fill direntry file information
            strcpy(dirent.name, filename);
            dirent.length = 0;
            dirent.cluster = EOF_CLUSTER;
            dirent.mode = DF_EXISTS | DF_0400 | DF_FILE | DF_READ | DF_WRITE | DF_EXECUTE;  //  0x8417
            getPs2Time(&dirent.created);
            getPs2Time(&dirent.modified);

            //  we need to know this directories entry in its parent...
            if (path[0] == '\0') {

                //  get the root directories entry
                parent_cluster = getDirentryFromPath(&parent, ".", &(fprivdata->gendata), f->unit);

            } else {

                //  get the folder entry for the parent
                parent_cluster = getDirentryFromPath(&parent, path, &(fprivdata->gendata), f->unit);
            }

            if (parent_cluster == NOFOUND_CLUSTER) {

                DEBUGPRINT(3, "vmc_fs: Unable to find parent directory.\n");

                free(path);
                free(fprivdata);  //  Release the allocated memory

                PROF_END(vmc_openProf)

                return -1;
            }

            dirent_cluster = addObject(&(fprivdata->gendata), parent_cluster, &parent, &dirent, f->unit);

            if (dirent_cluster == ERROR_CLUSTER) {

                DEBUGPRINT(2, "vmc_fs: open failed on %s\n", path1);

                free(path);
                free(fprivdata);  //  Release the allocated memory

                PROF_END(vmc_openProf)

                return -1;  //  File not found, could not be opened.
            }

            free(path);

        } else {

            free(fprivdata);  //  Release the allocated memory

            PROF_END(vmc_openProf)

            return -1;  //  File not found, could not be opened.
        }

    } else if ((!(dirent.mode & DF_EXISTS)) || (flags & O_TRUNC))  // File found but is hidden, or trunc the file
    {

        if (flags & O_TRUNC)
            DEBUGPRINT(3, "vmc_fs: Open with O_TRUNC.\n");

        int first_cluster = 1;
        unsigned int current_cluster;
        unsigned int last_cluster = dirent.cluster;

        DEBUGPRINT(4, "vmc_fs: Searching last cluster of file ...\n");

        while (1) {

            current_cluster = getFatEntry(fprivdata->gendata.fd, last_cluster, fprivdata->gendata.indir_fat_clusters, FAT_VALUE);

            if (current_cluster == FREE_CLUSTER) {

                // FREE_CLUSTER mean last cluster of the hidden file is found
                DEBUGPRINT(8, "vmc_fs: Last cluster of file at %u\n", last_cluster);

                break;

            } else if (current_cluster == EOF_CLUSTER) {

                // EOF_CLUSTER mean last cluster of the file is found
                DEBUGPRINT(8, "vmc_fs: EOF_CLUSTER found.\n");

                setFatEntry(fprivdata->gendata.fd, last_cluster, FREE_CLUSTER, fprivdata->gendata.indir_fat_clusters, FAT_SET);

                break;

            } else {

                DEBUGPRINT(8, "vmc_fs: Testing cluster %u ... value is %u\n", last_cluster, current_cluster);

                // Otherwise set first cluster as eof and remaining clusters as free
                if (first_cluster == 1) {

                    setFatEntry(fprivdata->gendata.fd, last_cluster, EOF_CLUSTER, fprivdata->gendata.indir_fat_clusters, FAT_SET);

                    dirent.length = 0;
                    dirent.cluster = EOF_CLUSTER;
                    dirent.mode = DF_EXISTS | DF_0400 | DF_FILE | DF_READ | DF_WRITE | DF_EXECUTE;  //  0x8417
                    getPs2Time(&dirent.created);
                    getPs2Time(&dirent.modified);
                    writePage(fprivdata->gendata.fd, (unsigned char *)&dirent, (dirent_cluster + fprivdata->gendata.first_allocatable) * g_Vmc_Image[f->unit].header.pages_per_cluster + fprivdata->gendata.dirent_page);

                    first_cluster = 0;

                } else {

                    setFatEntry(fprivdata->gendata.fd, last_cluster, FREE_CLUSTER, fprivdata->gendata.indir_fat_clusters, FAT_SET);
                }
            }

            last_cluster = current_cluster;
        }
    }

    // fill opened file informations
    fprivdata->file_position = 0;
    fprivdata->file_dirpage = (dirent_cluster + fprivdata->gendata.first_allocatable) * g_Vmc_Image[f->unit].header.pages_per_cluster + fprivdata->gendata.dirent_page;
    fprivdata->file_cluster = dirent.cluster;
    fprivdata->cluster_offset = 0;
    fprivdata->file_startcluster = dirent.cluster;
    fprivdata->file_length = dirent.length;  //  set the length to what it should be, and go from there

    DEBUGPRINT(2, "vmc_fs: File %s opened with length %u\n", path1, fprivdata->file_length);

    PROF_END(vmc_openProf)

    return 1;
}


//----------------------------------------------------------------------------
// Close a file previously open with open("vmc...
//----------------------------------------------------------------------------
int Vmc_Close(iop_file_t *f)
{

    if (!g_Vmc_Initialized)
        return VMCFS_ERR_INITIALIZED;

    DEBUGPRINT(1, "vmc_fs: Close %i\n", f->unit);

    if (g_Vmc_Image[f->unit].fd < 0)
        return VMCFS_ERR_NOT_MOUNT;

    if (g_Vmc_Image[f->unit].formated == FALSE)
        return VMCFS_ERR_NOT_FORMATED;

    PROF_START(vmc_closeProf)

    struct file_privdata *fprivdata = f->privdata;

    free(fprivdata);

    PROF_END(vmc_closeProf)

    return 0;
}


//----------------------------------------------------------------------------
// Read a file previously open with open("vmc...
//----------------------------------------------------------------------------
int Vmc_Read(iop_file_t *f, void *buffer, int size)
{

    if (!g_Vmc_Initialized)
        return VMCFS_ERR_INITIALIZED;

    DEBUGPRINT(1, "vmc_fs: Read %d bytes\n", size);

    if (g_Vmc_Image[f->unit].fd < 0)
        return VMCFS_ERR_NOT_MOUNT;

    if (g_Vmc_Image[f->unit].formated == FALSE)
        return VMCFS_ERR_NOT_FORMATED;

    PROF_START(vmc_readProf)

    if (!(f->mode & O_RDONLY)) {

        PROF_END(vmc_readProf)

        return -1;  //  if the file isn't opened for reading, return -1. No return eroor code from errno
    }

    struct file_privdata *fprivdata = f->privdata;

    fprivdata->gendata.first_allocatable = g_Vmc_Image[f->unit].header.first_allocatable;
    fprivdata->gendata.last_allocatable = g_Vmc_Image[f->unit].header.last_allocatable;

    memcpy(fprivdata->gendata.indir_fat_clusters, g_Vmc_Image[f->unit].header.indir_fat_clusters, sizeof(unsigned int) * 32);

    //  we are at eof
    if ((fprivdata->file_length - fprivdata->file_position) == 0) {

        PROF_END(vmc_readProf)

        return 0;
    }

    DEBUGPRINT(3, "vmc_fs: Reading in %i bytes\n", size);

    //  we need to read in size, unless size is larger then length
    if (size > fprivdata->file_length - fprivdata->file_position) {

        DEBUGPRINT(3, "vmc_fs: Adjusting size to read in to be %u\n", fprivdata->file_length - fprivdata->file_position);

        size = fprivdata->file_length - fprivdata->file_position;
    }

    //  Ok so we have been requested to read size bytes from the open file, which starts with fprivdata->file_cluster
    //  and our current position in the cluster is cluster_offset, while our overall position is file_position.
    //  We can determine what cluster in the link we should be in, by taking our overall position and dividing by 1024
    //  the size of a cluster.

    int data_read = 0;  //  How much data we have read in so far
    u8 *cluster_data;   //  Temporary storage for a cluster of data

    cluster_data = (u8 *)malloc((g_Vmc_Image[f->unit].cluster_size + 0xFF) & ~(unsigned int)0xFF);
    memset(cluster_data, 0, g_Vmc_Image[f->unit].cluster_size);

    //  While we still have data to read in
    while (data_read < size) {

        //  Read in the file_cluster
        readCluster(fprivdata->gendata.fd, cluster_data, fprivdata->file_cluster + fprivdata->gendata.first_allocatable);

        //  We have 1024 bytes in cluster_data now, but we already read in cluster_offset bytes
        //  So we now need to copy whats left into the buffer that was passed to us

        DEBUGPRINT(3, "vmc_fs: There are %i bytes remaining\n", size - data_read);
        DEBUGPRINT(3, "vmc_fs: There are %i bytes left in cluster %u\n", (fprivdata->cluster_offset == 0) ? 0 : g_Vmc_Image[f->unit].cluster_size - fprivdata->cluster_offset, fprivdata->file_cluster + fprivdata->gendata.first_allocatable);

        //  If the data remaining in a cluster is larger then the buffer passed to us, we can only read in size data
        if (size - data_read < g_Vmc_Image[f->unit].cluster_size - fprivdata->cluster_offset) {

            DEBUGPRINT(3, "vmc_fs: Read in %i bytes\n", size);

            memcpy(buffer + data_read, cluster_data + fprivdata->cluster_offset, size - data_read);
            fprivdata->cluster_offset += (size - data_read);
            data_read += (size - data_read);

        } else  //  We can copy in the rest of the cluster, since there is space in the buffer passed to us
        {

            DEBUGPRINT(3, "vmc_fs: Read in %i bytes\n", g_Vmc_Image[f->unit].cluster_size - fprivdata->cluster_offset);

            memcpy(buffer + data_read, cluster_data + fprivdata->cluster_offset, g_Vmc_Image[f->unit].cluster_size - fprivdata->cluster_offset);
            data_read += (g_Vmc_Image[f->unit].cluster_size - fprivdata->cluster_offset);
            fprivdata->cluster_offset += (g_Vmc_Image[f->unit].cluster_size - fprivdata->cluster_offset);
        }

        //  If we have used up all of the data in a cluster, we need to tell it that the current cluster is the next one in the list
        if (fprivdata->cluster_offset == g_Vmc_Image[f->unit].cluster_size) {

            DEBUGPRINT(6, "vmc_fs: Moving onto next cluster\n");

            fprivdata->file_cluster = getFatEntry(fprivdata->gendata.fd, fprivdata->file_cluster, fprivdata->gendata.indir_fat_clusters, FAT_VALUE);
            fprivdata->cluster_offset = 0;
        }
    }

    free(cluster_data);

    //  Increase the file position by read
    fprivdata->file_position += data_read;

    PROF_END(vmc_readProf)

    //  Return the amount read in.
    return data_read;
}


//----------------------------------------------------------------------------
// Write a file previously open with open("vmc...
//----------------------------------------------------------------------------
int Vmc_Write(iop_file_t *f, void *buffer, int size)
{

    if (!g_Vmc_Initialized)
        return VMCFS_ERR_INITIALIZED;

    DEBUGPRINT(1, "vmc_fs: Write %i bytes\n", size);

    if (g_Vmc_Image[f->unit].fd < 0)
        return VMCFS_ERR_NOT_MOUNT;

    if (g_Vmc_Image[f->unit].formated == FALSE)
        return VMCFS_ERR_NOT_FORMATED;

    PROF_START(vmc_writeProf)

    if (!(f->mode & O_WRONLY)) {

        PROF_END(vmc_writeProf)

        return -1;  //  if the file isn't opened for writing, return -1
    }

    struct file_privdata *fprivdata = f->privdata;

    fprivdata->gendata.first_allocatable = g_Vmc_Image[f->unit].header.first_allocatable;
    fprivdata->gendata.last_allocatable = g_Vmc_Image[f->unit].header.last_allocatable;

    memcpy(fprivdata->gendata.indir_fat_clusters, g_Vmc_Image[f->unit].header.indir_fat_clusters, sizeof(unsigned int) * 32);

    if (f->mode & O_APPEND) {

        DEBUGPRINT(8, "vmc_fs: Write with O_APPEND.\n");

        fprivdata->file_position = fprivdata->file_length;
    }

    unsigned int current_cluster;

    //  if we are going to go past the eof with this write
    //  we need to allocate some more free cluster space
    int num_clusters = fprivdata->file_length / g_Vmc_Image[f->unit].cluster_size;

    if ((fprivdata->file_length % g_Vmc_Image[f->unit].cluster_size) > 0)
        num_clusters++;

    DEBUGPRINT(5, "vmc_fs: File lenght in cluster before this write: %u\n", num_clusters);

    int num_clusters_required = (fprivdata->file_position + size) / g_Vmc_Image[f->unit].cluster_size;

    if (((fprivdata->file_position + size) % g_Vmc_Image[f->unit].cluster_size) > 0)
        num_clusters_required++;

    DEBUGPRINT(5, "vmc_fs: File lenght in cluster after this write : %u\n", num_clusters_required);

    if (num_clusters_required > num_clusters) {

        fprivdata->file_length = fprivdata->file_position + size;

        DEBUGPRINT(3, "vmc_fs: File position:  %u\n", fprivdata->file_position);
        DEBUGPRINT(3, "vmc_fs: Size to write:  %u\n", size);
        DEBUGPRINT(3, "vmc_fs: File length  :  %u\n", fprivdata->file_length);

        //  now determine how many clusters we need forthis write
        unsigned int clusters_needed = num_clusters_required - num_clusters;

        DEBUGPRINT(3, "vmc_fs: We need to allocate %u more clusters for storage\n", clusters_needed);

        int i = 0;
        unsigned int free_cluster = 0;
        unsigned int last_cluster = 0;

        if (fprivdata->file_startcluster == EOF_CLUSTER) {

            free_cluster = getFreeCluster(&(fprivdata->gendata), f->unit);

            if (free_cluster == ERROR_CLUSTER) {

                DEBUGPRINT(2, "Not enough free space to add initial cluster.  Aborting.\n");

                PROF_END(vmc_writeProf)

                return -1;
            }

            DEBUGPRINT(3, "vmc_fs: Initial cluster at %u\n", free_cluster);

            //  mark the free cluster as eof
            setFatEntry(fprivdata->gendata.fd, free_cluster, EOF_CLUSTER, fprivdata->gendata.indir_fat_clusters, FAT_SET);

            struct direntry dirent;

            readPage(fprivdata->gendata.fd, (unsigned char *)&dirent, fprivdata->file_dirpage);
            dirent.cluster = free_cluster;
            writePage(fprivdata->gendata.fd, (unsigned char *)&dirent, fprivdata->file_dirpage);

            fprivdata->file_startcluster = free_cluster;
            fprivdata->file_cluster = free_cluster;

        } else {

            //  loop to find the last cluster of the file.
            last_cluster = fprivdata->file_startcluster;

            DEBUGPRINT(3, "vmc_fs: Searching last cluster of file\n");

            while (1) {

                current_cluster = getFatEntry(fprivdata->gendata.fd, last_cluster, fprivdata->gendata.indir_fat_clusters, FAT_VALUE);

                if (current_cluster == FREE_CLUSTER) {

                    DEBUGPRINT(10, "vmc_fs: Testing cluster %d ... value is FREE_CLUSTER\n", last_cluster);

                } else if (current_cluster == EOF_CLUSTER) {

                    DEBUGPRINT(10, "vmc_fs: Testing cluster %d ... value is EOF_CLUSTER\n", i);
                    break;

                } else {

                    DEBUGPRINT(10, "vmc_fs: Testing cluster %d ... value is %d\n", last_cluster, current_cluster);
                }

                last_cluster = current_cluster;
                i++;
            }

            DEBUGPRINT(3, "vmc_fs: Last cluster of file at %u\n", last_cluster);

            free_cluster = getFreeCluster(&(fprivdata->gendata), f->unit);

            if (free_cluster == ERROR_CLUSTER) {

                DEBUGPRINT(2, "Not enough free space to add restart cluster.  Aborting.\n");

                PROF_END(vmc_writeProf)

                return -1;
            }

            DEBUGPRINT(3, "vmc_fs: Restart cluster for the file at %u\n", free_cluster);

            //  mark the last cluster as restart of our file
            setFatEntry(fprivdata->gendata.fd, last_cluster, free_cluster, fprivdata->gendata.indir_fat_clusters, FAT_SET);

            //  mark the free cluster as eof
            setFatEntry(fprivdata->gendata.fd, free_cluster, EOF_CLUSTER, fprivdata->gendata.indir_fat_clusters, FAT_SET);

            // check if last write operation finished at cluster end
            if (fprivdata->cluster_offset == 0)
                fprivdata->file_cluster = free_cluster;
            else
                fprivdata->file_cluster = last_cluster;
        }

        clusters_needed--;
        last_cluster = free_cluster;

        //  allocate free clusters for the space required.
        for (i = 0; i < clusters_needed; i++) {

            free_cluster = getFreeCluster(&(fprivdata->gendata), f->unit);

            if (free_cluster == ERROR_CLUSTER) {

                DEBUGPRINT(2, "Not enough free space to add cluster to the chain.  Aborting.\n");

                PROF_END(vmc_writeProf)

                return -1;
            }

            DEBUGPRINT(3, "vmc_fs: Adding cluster %u to the chain for the file\n", free_cluster);

            setFatEntry(fprivdata->gendata.fd, last_cluster, free_cluster, fprivdata->gendata.indir_fat_clusters, FAT_SET);
            last_cluster = free_cluster;

            //  mark the last cluster as eof
            setFatEntry(fprivdata->gendata.fd, last_cluster, EOF_CLUSTER, fprivdata->gendata.indir_fat_clusters, FAT_SET);
        }
    }

    //  ok space is definitly ready for us to write to...
    current_cluster = fprivdata->file_cluster;

    unsigned int data_written = 0;

    while (data_written < size) {

        //  if we have more then a clusters worth of data to write...
        int sizewritten = 0;

        if ((size - data_written) >= (g_Vmc_Image[f->unit].cluster_size - fprivdata->cluster_offset)) {

            if (fprivdata->cluster_offset == 0) {

                DEBUGPRINT(4, "vmc_fs: Writing to cluster %u\n", current_cluster);

                writeCluster(fprivdata->gendata.fd, ((unsigned char *)buffer) + data_written, current_cluster + fprivdata->gendata.first_allocatable);
                sizewritten = g_Vmc_Image[f->unit].cluster_size;

            } else {

                DEBUGPRINT(4, "vmc_fs: Writing to cluster %u at offset %u for length %u\n", current_cluster, fprivdata->cluster_offset, g_Vmc_Image[f->unit].cluster_size - fprivdata->cluster_offset);

                writeClusterPart(fprivdata->gendata.fd, ((unsigned char *)buffer) + data_written, current_cluster + fprivdata->gendata.first_allocatable, fprivdata->cluster_offset, g_Vmc_Image[f->unit].cluster_size - fprivdata->cluster_offset);
                sizewritten = g_Vmc_Image[f->unit].cluster_size - fprivdata->cluster_offset;
            }

        } else {

            DEBUGPRINT(4, "vmc_fs: Writing to cluster %u at offset %u for length %u\n", current_cluster, fprivdata->cluster_offset, size - data_written);

            writeClusterPart(fprivdata->gendata.fd, ((unsigned char *)buffer) + data_written, current_cluster + fprivdata->gendata.first_allocatable, fprivdata->cluster_offset, size - data_written);
            sizewritten = size - data_written;
        }

        DEBUGPRINT(5, "vmc_fs: Wrote %i bytes\n", sizewritten);

        data_written += sizewritten;
        fprivdata->file_position += sizewritten;
        fprivdata->cluster_offset += sizewritten;

        if (fprivdata->cluster_offset == g_Vmc_Image[f->unit].cluster_size) {

            DEBUGPRINT(7, "vmc_fs: Moving onto next cluster\n");

            current_cluster = getFatEntry(fprivdata->gendata.fd, current_cluster, fprivdata->gendata.indir_fat_clusters, FAT_VALUE);
            fprivdata->cluster_offset = 0;
        }
    }

    struct direntry dirent;

    if (f->mode & O_TRUNC || f->mode & O_APPEND) {

        DEBUGPRINT(8, "vmc_fs: Write with O_TRUNC.\n");

        fprivdata->file_length = fprivdata->file_position;
    }

    readPage(fprivdata->gendata.fd, (unsigned char *)&dirent, fprivdata->file_dirpage);

    // Update file length
    dirent.length = fprivdata->file_length;

    // Update timestamp
    getPs2Time(&dirent.modified);

    // Write this changes
    writePage(fprivdata->gendata.fd, (unsigned char *)&dirent, fprivdata->file_dirpage);

    fprivdata->file_cluster = current_cluster;

    PROF_END(vmc_writeProf)

    return data_written;
}


//----------------------------------------------------------------------------
// Seek a file previously open with open("vmc...
//----------------------------------------------------------------------------
int Vmc_Lseek(iop_file_t *f, int offset, int whence)
{

    if (!g_Vmc_Initialized)
        return VMCFS_ERR_INITIALIZED;

    DEBUGPRINT(1, "vmc_fs: Seek\n");

    if (g_Vmc_Image[f->unit].fd < 0)
        return VMCFS_ERR_NOT_MOUNT;

    if (g_Vmc_Image[f->unit].formated == FALSE)
        return VMCFS_ERR_NOT_FORMATED;

    PROF_START(vmc_lseekProf)

    struct file_privdata *fprivdata = f->privdata;

    switch (whence) {

        case SEEK_SET:
            fprivdata->file_position = offset;
            break;

        case SEEK_CUR:
            fprivdata->file_position += offset;
            break;

        case SEEK_END:
            fprivdata->file_position = fprivdata->file_length + offset;
            break;

        default:
            return -1;
    }

    //  Return an error if we are past the end of the file
    if (fprivdata->file_position > fprivdata->file_length)
        return -1;

    //  Now we need to position cluster offsets to be correct
    fprivdata->file_cluster = fprivdata->file_startcluster;

    int i = 0;
    int chainclusternum = fprivdata->file_position / g_Vmc_Image[f->unit].cluster_size;

    for (i = 0; i < chainclusternum; i++)
        fprivdata->file_cluster = getFatEntry(fprivdata->gendata.fd, fprivdata->file_cluster, fprivdata->gendata.indir_fat_clusters, FAT_VALUE);

    fprivdata->cluster_offset = fprivdata->file_position % g_Vmc_Image[f->unit].cluster_size;

    PROF_END(vmc_lseekProf)

    return fprivdata->file_position;
}


//----------------------------------------------------------------------------
// Control command.
// IOCTL_VMCFS_RECOVER :  Recover an object marked as none existing. ( data must be a valid path to an object in vmc file )
//----------------------------------------------------------------------------
int Vmc_Ioctl(iop_file_t *f, int cmd, void *data)
{

    if (!g_Vmc_Initialized)
        return VMCFS_ERR_INITIALIZED;

    switch (cmd) {

        case IOCTL_VMCFS_RECOVER: {

            Vmc_Recover(f->unit, (char *)data);

        } break;

        default:

            DEBUGPRINT(1, "vmc_fs: Unrecognized ioctl command %d\n", cmd);
            break;
    }

    return VMCFS_ERR_NO;
}


//----------------------------------------------------------------------------
// Remove an object from vmc image. Object can be a file or a folder.
//----------------------------------------------------------------------------
int Vmc_Remove(iop_file_t *f, const char *path)
{

    if (!g_Vmc_Initialized)
        return VMCFS_ERR_INITIALIZED;

    DEBUGPRINT(1, "vmc_fs: remove %s\n", path);

    if (g_Vmc_Image[f->unit].fd < 0)
        return VMCFS_ERR_NOT_MOUNT;

    if (g_Vmc_Image[f->unit].formated == FALSE)
        return VMCFS_ERR_NOT_FORMATED;

    PROF_START(vmc_removeProf)

    struct direntry dirent;
    struct gen_privdata gendata;

    gendata.fd = g_Vmc_Image[f->unit].fd;
    gendata.first_allocatable = g_Vmc_Image[f->unit].header.first_allocatable;
    gendata.last_allocatable = g_Vmc_Image[f->unit].header.last_allocatable;

    memcpy(gendata.indir_fat_clusters, g_Vmc_Image[f->unit].header.indir_fat_clusters, sizeof(unsigned int) * 32);

    unsigned int dirent_cluster = getDirentryFromPath(&dirent, path, &gendata, f->unit);

    if (dirent_cluster == ROOT_CLUSTER) {

        DEBUGPRINT(2, "vmc_fs: remove failed. Root directory is protected.\n");

        PROF_END(vmc_removeProf)

        return -1;

    } else if (dirent_cluster == NOFOUND_CLUSTER) {

        DEBUGPRINT(2, "vmc_fs: remove failed. %s not found.\n", path);

        PROF_END(vmc_removeProf)

        return -1;
    }

    if (dirent.mode & DF_FILE) {

        removeObject(&gendata, dirent_cluster, &dirent, f->unit);

        DEBUGPRINT(2, "vmc_fs: file %s removed.\n", path);

    } else {

        DEBUGPRINT(2, "vmc_fs: remove failed. %s is not a valid file.\n", path);

        PROF_END(vmc_removeProf)

        return -1;
    }

    PROF_END(vmc_removeProf)

    return 0;
}


//----------------------------------------------------------------------------
// Create a new folder into vmc image.
//----------------------------------------------------------------------------
int Vmc_Mkdir(iop_file_t *f, const char *path1, int mode)
{

    if (!g_Vmc_Initialized)
        return VMCFS_ERR_INITIALIZED;

    DEBUGPRINT(1, "vmc_fs: mkdir %s\n", path1);

    if (g_Vmc_Image[f->unit].fd < 0)
        return VMCFS_ERR_NOT_MOUNT;

    if (g_Vmc_Image[f->unit].formated == FALSE)
        return VMCFS_ERR_NOT_FORMATED;

    PROF_START(vmc_mkdirProf)

    struct direntry dirent;
    struct gen_privdata gendata;

    gendata.fd = g_Vmc_Image[f->unit].fd;
    gendata.first_allocatable = g_Vmc_Image[f->unit].header.first_allocatable;
    gendata.last_allocatable = g_Vmc_Image[f->unit].header.last_allocatable;

    memcpy(gendata.indir_fat_clusters, g_Vmc_Image[f->unit].header.indir_fat_clusters, sizeof(unsigned int) * 32);

    memset(&dirent, 0, sizeof(dirent));

    // Make a local copy of path.
    char *path = malloc(strlen(path1) + 1);
    memcpy(path, path1, strlen(path1) + 1);

    if (path[strlen(path) - 1] == '/')
        path[strlen(path) - 1] = '\0';

    unsigned int dirent_cluster = getDirentryFromPath(&dirent, path, &gendata, f->unit);

    if (dirent_cluster == ROOT_CLUSTER) {

        DEBUGPRINT(2, "vmc_fs: mkdir failed. Root directory is protected.\n");

        free(path);

        PROF_END(vmc_mkdirProf)

        return -1;  //  File not found, could not be opened.

    } else if (dirent_cluster == NOFOUND_CLUSTER) {

        struct direntry new_dirent;
        unsigned int new_dirent_cluster = 0;
        struct direntry parent_dirent;
        unsigned int parent_cluster = 0;
        struct gen_privdata parent_gendata;

        parent_gendata.fd = g_Vmc_Image[f->unit].fd;
        parent_gendata.first_allocatable = g_Vmc_Image[f->unit].header.first_allocatable;
        parent_gendata.last_allocatable = g_Vmc_Image[f->unit].header.last_allocatable;

        memcpy(parent_gendata.indir_fat_clusters, g_Vmc_Image[f->unit].header.indir_fat_clusters, sizeof(unsigned int) * 32);

        // Find name of directory, and name of parent
        char *newdir = strrchr(path, '/');

        newdir[0] = '\0';
        newdir++;

        DEBUGPRINT(2, "vmc_fs: Creating folder %s in parent folder %s\n", newdir, (path[0] == '\0') ? "Root" : path);

        memset(&new_dirent, 0, sizeof(new_dirent));

        //  fill new direntry
        new_dirent.mode = DF_EXISTS | DF_0400 | DF_DIRECTORY | DF_READ | DF_WRITE | DF_EXECUTE;  //  0x8427
        strcpy(new_dirent.name, newdir);
        getPs2Time(&new_dirent.created);
        getPs2Time(&new_dirent.modified);

        if (path[0] == '\0') {

            //  get the root directories entry
            parent_cluster = getDirentryFromPath(&parent_dirent, ".", &parent_gendata, f->unit);

        } else {

            //  get the folder entry for the parent
            parent_cluster = getDirentryFromPath(&parent_dirent, path, &parent_gendata, f->unit);
        }

        if (parent_cluster == NOFOUND_CLUSTER) {

            DEBUGPRINT(3, "vmc_fs: Unable to find parent directory\n");

            free(path);

            PROF_END(vmc_mkdirProf)

            return -1;
        }

        DEBUGPRINT(3, "vmc_fs: Parent Information.\n");
        DEBUGPRINT(3, "vmc_fs: parent_cluster  = %u\n", parent_cluster);
        DEBUGPRINT(3, "vmc_fs: dir_cluster    = %u\n", parent_dirent.cluster);
        DEBUGPRINT(3, "vmc_fs: dirent.name    = %s\n", parent_dirent.name);
        DEBUGPRINT(3, "vmc_fs: dirent.length  = %u\n", parent_dirent.length);
        DEBUGPRINT(3, "vmc_fs: dirent.mode    = %X\n", parent_dirent.mode);
        DEBUGPRINT(3, "vmc_fs: dirent_page    = %i\n", parent_gendata.dirent_page);

        new_dirent_cluster = addObject(&parent_gendata, parent_cluster, &parent_dirent, &new_dirent, f->unit);

        if (new_dirent_cluster == ERROR_CLUSTER) {

            DEBUGPRINT(2, "vmc_fs: mkdir failed on %s\n", path);

            free(path);

            PROF_END(vmc_mkdirProf)

            return -1;
        }

    } else  // directory allready exist, so test DF_EXISTS flag
    {

        if (dirent.mode & DF_EXISTS) {

            DEBUGPRINT(2, "vmc_fs: mkdir failed on %s. Directory allready exist.\n", path1);

            free(path);

            PROF_END(vmc_mkdirProf)

            return -1;

        } else {

            DEBUGPRINT(8, "vmc_fs: mkdir directory %s allready exist but is hidden. Changing attributs.\n", path1);

            DEBUGPRINT(8, "vmc_fs: Following fat table cluster %u\n", dirent.cluster);

            unsigned int pseudo_entry_cluster = getFatEntry(gendata.fd, dirent.cluster, gendata.indir_fat_clusters, FAT_VALUE);

            DEBUGPRINT(8, "vmc_fs: Changing cluster mask of fat table cluster %u.\n", pseudo_entry_cluster);

            // change cluster mask of the direntry
            setFatEntry(gendata.fd, dirent.cluster, pseudo_entry_cluster, gendata.indir_fat_clusters, FAT_SET);

            DEBUGPRINT(8, "vmc_fs: Changing direntry %s attributs.\n", path1);

            //  Update time stamp, and set dirent.mode to exist flag
            dirent.mode = dirent.mode | DF_EXISTS;
            getPs2Time(&dirent.created);
            getPs2Time(&dirent.modified);
            writePage(gendata.fd, (unsigned char *)&dirent, (dirent_cluster + gendata.first_allocatable) * g_Vmc_Image[f->unit].header.pages_per_cluster + gendata.dirent_page);

            DEBUGPRINT(8, "vmc_fs: Restoring EOF cluster at %u.\n", pseudo_entry_cluster);

            setFatEntry(gendata.fd, pseudo_entry_cluster, EOF_CLUSTER, gendata.indir_fat_clusters, FAT_SET);

            struct direntry pseudo_entries;

            DEBUGPRINT(8, "vmc_fs: Updating pseudo entries time stamps at cluster %u / page %u.\n", dirent.cluster + gendata.first_allocatable, (dirent.cluster + gendata.first_allocatable) * g_Vmc_Image[f->unit].header.pages_per_cluster);

            //  Update time stamp of '.' entry
            readPage(gendata.fd, (unsigned char *)&pseudo_entries, (dirent.cluster + gendata.first_allocatable) * g_Vmc_Image[f->unit].header.pages_per_cluster);
            getPs2Time(&pseudo_entries.created);
            getPs2Time(&pseudo_entries.modified);
            writePage(gendata.fd, (unsigned char *)&pseudo_entries, (dirent.cluster + gendata.first_allocatable) * g_Vmc_Image[f->unit].header.pages_per_cluster);

            DEBUGPRINT(8, "vmc_fs: Updating pseudo entries time stamps at cluster %u / page %u.\n", dirent.cluster + gendata.first_allocatable, (dirent.cluster + gendata.first_allocatable) * g_Vmc_Image[f->unit].header.pages_per_cluster + 1);

            //  Update time stamp of '..' entry
            readPage(gendata.fd, (unsigned char *)&pseudo_entries, (dirent.cluster + gendata.first_allocatable) * g_Vmc_Image[f->unit].header.pages_per_cluster + 1);
            getPs2Time(&pseudo_entries.created);
            getPs2Time(&pseudo_entries.modified);
            writePage(gendata.fd, (unsigned char *)&pseudo_entries, (dirent.cluster + gendata.first_allocatable) * g_Vmc_Image[f->unit].header.pages_per_cluster + 1);

            free(path);
        }
    }

    DEBUGPRINT(3, "vmc_fs: Directory %s created.\n", path1);

    PROF_END(vmc_mkdirProf)

    return 0;
}


//----------------------------------------------------------------------------
// Remove a folder from vmc image.
//----------------------------------------------------------------------------
int Vmc_Rmdir(iop_file_t *f, const char *path1)
{

    if (!g_Vmc_Initialized)
        return VMCFS_ERR_INITIALIZED;

    DEBUGPRINT(1, "vmc_fs: rmdir %s\n", path1);

    if (g_Vmc_Image[f->unit].fd < 0)
        return VMCFS_ERR_NOT_MOUNT;

    if (g_Vmc_Image[f->unit].formated == FALSE)
        return VMCFS_ERR_NOT_FORMATED;

    PROF_START(vmc_rmdirProf)

    struct direntry dirent;
    struct gen_privdata folder_gendata;

    folder_gendata.fd = g_Vmc_Image[f->unit].fd;
    folder_gendata.first_allocatable = g_Vmc_Image[f->unit].header.first_allocatable;
    folder_gendata.last_allocatable = g_Vmc_Image[f->unit].header.last_allocatable;

    memcpy(folder_gendata.indir_fat_clusters, g_Vmc_Image[f->unit].header.indir_fat_clusters, sizeof(unsigned int) * 32);

    // Make a local copy of path
    char *path = malloc(strlen(path1) + 1);
    memcpy(path, path1, strlen(path1) + 1);

    if (path[strlen(path) - 1] == '/')
        path[strlen(path) - 1] = '\0';

    unsigned int dirent_cluster = getDirentryFromPath(&dirent, path, &folder_gendata, f->unit);

    if (dirent_cluster == ROOT_CLUSTER) {

        DEBUGPRINT(2, "vmc_fs: rmdir failed. Root directory is protected.\n");

        free(path);

        PROF_END(vmc_rmdirProf)

        return -1;

    } else if (dirent_cluster == NOFOUND_CLUSTER) {

        DEBUGPRINT(2, "vmc_fs: rmdir failed. %s not found.\n", path1);

        free(path);

        PROF_END(vmc_rmdirProf)

        return -1;
    }

    if (!(dirent.mode & DF_EXISTS)) {

        DEBUGPRINT(2, "vmc_fs: rmdir failed. %s is allready removed.\n", path1);

        free(path);

        PROF_END(vmc_rmdirProf)

        return -1;
    }

    if (dirent.mode & DF_DIRECTORY) {

        // Find name of directory, and name of parent
        char *foldername = strrchr(path, '/');

        foldername[0] = '\0';
        foldername++;

        struct direntry child;
        unsigned int child_cluster;
        struct gen_privdata child_gendata;
        char *child_path;

        child_path = (char *)malloc(MAX_PATH);
        memset(child_path, 0, MAX_PATH);

        int i = 0;
        int dir_number = 0;
        unsigned int search_cluster = dirent.cluster;

        child_gendata.fd = g_Vmc_Image[f->unit].fd;
        child_gendata.first_allocatable = g_Vmc_Image[f->unit].header.first_allocatable;
        child_gendata.last_allocatable = g_Vmc_Image[f->unit].header.last_allocatable;

        memcpy(child_gendata.indir_fat_clusters, g_Vmc_Image[f->unit].header.indir_fat_clusters, sizeof(unsigned int) * 32);

        // remove file contained into the directory
        for (i = 0; i < dirent.length; i++) {

            // read in the next directory entry
            readPage(folder_gendata.fd, (unsigned char *)&child, (search_cluster + folder_gendata.first_allocatable) * g_Vmc_Image[f->unit].header.pages_per_cluster + dir_number);

            // if child exist, remove it. But don't touch to pseudo entries.
            if (child.mode & DF_EXISTS && i >= 2) {

                sprintf(child_path, "%s/%s/%s", path, dirent.name, child.name);

                child_cluster = getDirentryFromPath(&child, child_path, &child_gendata, f->unit);

                if (child_cluster != NOFOUND_CLUSTER) {

                    removeObject(&child_gendata, child_cluster, &child, f->unit);
                }
            }

            if (dir_number == 1) {

                dir_number = 0;
                search_cluster = getFatEntry(folder_gendata.fd, search_cluster, folder_gendata.indir_fat_clusters, FAT_VALUE);

            } else {

                dir_number = 1;
            }
        }

        // finaly, remove directory
        removeObject(&folder_gendata, dirent_cluster, &dirent, f->unit);

        free(path);
        free(child_path);

    } else {

        DEBUGPRINT(2, "vmc_fs: rmdir failed. %s is not a valid directory.\n", path1);

        free(path);

        PROF_END(vmc_rmdirProf)

        return -1;
    }

    DEBUGPRINT(3, "vmc_fs: Directory %s removed.\n", path1);

    PROF_END(vmc_removeProf)

    return 0;
}


//----------------------------------------------------------------------------
// Open a folder in vmc image.
//----------------------------------------------------------------------------
int Vmc_Dopen(iop_file_t *f, const char *path)
{

    if (!g_Vmc_Initialized)
        return VMCFS_ERR_INITIALIZED;

    DEBUGPRINT(1, "vmc_fs: dopen %i %s\n", f->unit, path);

    if (g_Vmc_Image[f->unit].fd < 0)
        return VMCFS_ERR_NOT_MOUNT;

    if (g_Vmc_Image[f->unit].formated == FALSE)
        return VMCFS_ERR_NOT_FORMATED;

    PROF_START(vmc_dopenProf)

    struct direntry dirent;
    struct dir_privdata *fprivdata = malloc(sizeof(struct dir_privdata));

    if (fprivdata == NULL)
        return -1;

    f->privdata = fprivdata;

    fprivdata->gendata.fd = g_Vmc_Image[f->unit].fd;
    fprivdata->gendata.first_allocatable = g_Vmc_Image[f->unit].header.first_allocatable;

    memcpy(fprivdata->gendata.indir_fat_clusters, g_Vmc_Image[f->unit].header.indir_fat_clusters, sizeof(unsigned int) * 32);

    unsigned int dirent_cluster = getDirentryFromPath(&dirent, path, &(fprivdata->gendata), f->unit);

    if (dirent_cluster == NOFOUND_CLUSTER) {

        DEBUGPRINT(2, "vmc_fs: dopen failed. %s not found.\n", path);

        free(fprivdata);  //  Release the allocated memory

        PROF_END(vmc_dopenProf)

        return -1;  //  Folder not found, could not be opened.
    }

    if (!(dirent.mode & DF_EXISTS)) {

        DEBUGPRINT(2, "vmc_fs: dopen failed. %s is hidden.\n", path);

        free(fprivdata);  //  Release the allocated memory

        PROF_END(vmc_dopenProf)

        return -1;  //  Folder not found, could not be opened.
    }

    fprivdata->dir_cluster = dirent.cluster;
    fprivdata->dir_length = dirent.length;
    fprivdata->dir_number = 0;

    DEBUGPRINT(2, "vmc_fs: Directory %s opened with length %u\n", path, fprivdata->dir_length);

    PROF_END(vmc_dopenProf)

    return 1;
}


//----------------------------------------------------------------------------
// Close a folder in vmc image.
//----------------------------------------------------------------------------
int Vmc_Dclose(iop_file_t *f)
{

    if (!g_Vmc_Initialized)
        return VMCFS_ERR_INITIALIZED;

    DEBUGPRINT(1, "vmc_fs: dclose %i\n", f->unit);

    if ((f->unit == 2) || (f->unit == 3))
        return 0;  //  Close our fake device used only for ioctl commands

    PROF_START(vmc_dcloseProf)

    struct dir_privdata *fprivdata = f->privdata;

    free(fprivdata);

    PROF_END(vmc_dcloseProf)

    return 0;
}


//----------------------------------------------------------------------------
// Read the content of a folder in vmc image.
//----------------------------------------------------------------------------
int Vmc_Dread(iop_file_t *f, iox_dirent_t *buffer)
{

    if (!g_Vmc_Initialized)
        return VMCFS_ERR_INITIALIZED;

    DEBUGPRINT(1, "vmc_fs: dread %i\n", f->unit);

    if (g_Vmc_Image[f->unit].fd < 0)
        return VMCFS_ERR_NOT_MOUNT;

    if (g_Vmc_Image[f->unit].formated == FALSE)
        return VMCFS_ERR_NOT_FORMATED;

    PROF_START(vmc_dreadProf)

    int next;
    vmc_datetime access_time;
    struct direntry dirent;
    struct dir_privdata *fprivdata = f->privdata;
    iox_dirent_t *buf = buffer;

    memset(buf, 0, sizeof(iox_dirent_t));

next_entry:

    next = 0;

    if (fprivdata->dir_length == 0)
        return -1;

    //  read in the next directory entry
    readPage(fprivdata->gendata.fd, (unsigned char *)&dirent, (fprivdata->dir_cluster + fprivdata->gendata.first_allocatable) * g_Vmc_Image[f->unit].header.pages_per_cluster + fprivdata->dir_number);

    DEBUGPRINT(4, "vmc_fs: name: %s; cluster %u; length %u; mode 0x%02x\n", dirent.name, dirent.cluster, dirent.length, dirent.mode);

    if (dirent.mode & DF_EXISTS) {

        buf->stat.mode = dirent.mode;
        buf->stat.attr = dirent.attr;
        buf->stat.size = dirent.length;

        //  File created Time  /  Date
        memcpy(buf->stat.ctime, &dirent.created, sizeof(vmc_datetime));

        //  File Modification Time  /  Date
        memcpy(buf->stat.mtime, &dirent.modified, sizeof(vmc_datetime));

        //  Last File Access Time : now
        getPs2Time(&access_time);
        memcpy(buf->stat.atime, &access_time, sizeof(vmc_datetime));

        buf->stat.hisize = 0;  //  No idea what hisize is?

        strcpy(buf->name, dirent.name);

    } else {

        next = 1;
    }

    fprivdata->dir_length--;
    //  return 1 if there are more entries to read, otherwise return -1

    if (fprivdata->dir_number) {

        fprivdata->dir_number = 0;
        fprivdata->dir_cluster = getFatEntry(fprivdata->gendata.fd, fprivdata->dir_cluster, fprivdata->gendata.indir_fat_clusters, FAT_VALUE);

    } else {

        fprivdata->dir_number = 1;
    }

    if (next == 1)
        goto next_entry;

    PROF_END(vmc_dreadProf)

    return 1;
}


//----------------------------------------------------------------------------
// Get stats from an object in vmc image.
//----------------------------------------------------------------------------
int Vmc_Getstat(iop_file_t *f, const char *path, iox_stat_t *stat)
{

    if (!g_Vmc_Initialized)
        return VMCFS_ERR_INITIALIZED;

    DEBUGPRINT(1, "vmc_fs: getstat %i %s\n", f->unit, path);

    if (g_Vmc_Image[f->unit].fd < 0)
        return VMCFS_ERR_NOT_MOUNT;

    if (g_Vmc_Image[f->unit].formated == FALSE)
        return VMCFS_ERR_NOT_FORMATED;

    PROF_START(vmc_getstatProf)

    struct direntry dirent;
    struct gen_privdata gendata;
    vmc_datetime access_time;

    gendata.fd = g_Vmc_Image[f->unit].fd;
    gendata.first_allocatable = g_Vmc_Image[f->unit].header.first_allocatable;
    gendata.last_allocatable = g_Vmc_Image[f->unit].header.last_allocatable;

    memcpy(gendata.indir_fat_clusters, g_Vmc_Image[f->unit].header.indir_fat_clusters, sizeof(unsigned int) * 32);

    unsigned int dirent_cluster = getDirentryFromPath(&dirent, path, &gendata, f->unit);

    if (dirent_cluster == NOFOUND_CLUSTER) {

        DEBUGPRINT(2, "vmc_fs: getstat failed. %s not found.\n", path);

        PROF_END(vmc_getstatProf)

        return -1;
    }

    stat->mode = dirent.mode;
    stat->attr = dirent.attr;
    stat->size = dirent.length;

    //  File created Time  /  Date
    memcpy(stat->ctime, &dirent.created, sizeof(vmc_datetime));

    //  File Modification Time  /  Date
    memcpy(stat->mtime, &dirent.modified, sizeof(vmc_datetime));

    //  Last File Access Time : now
    getPs2Time(&access_time);
    memcpy(stat->atime, &access_time, sizeof(vmc_datetime));

    stat->hisize = 0;  //  No idea what hisize is?

    PROF_END(vmc_getstatProf)

    return 0;
}


//----------------------------------------------------------------------------
// Put stats to an object in vmc image.
//----------------------------------------------------------------------------
int Vmc_Chstat(iop_file_t *f, const char *path, iox_stat_t *stat, unsigned int statmask)
{

    if (!g_Vmc_Initialized)
        return VMCFS_ERR_INITIALIZED;

    DEBUGPRINT(1, "vmc_fs: chstat %i %s\n", f->unit, path);

    if (g_Vmc_Image[f->unit].fd < 0)
        return VMCFS_ERR_NOT_MOUNT;

    if (g_Vmc_Image[f->unit].formated == FALSE)
        return VMCFS_ERR_NOT_FORMATED;

    PROF_START(vmc_chstatProf)

    struct direntry dirent;
    struct gen_privdata gendata;

    gendata.fd = g_Vmc_Image[f->unit].fd;
    gendata.first_allocatable = g_Vmc_Image[f->unit].header.first_allocatable;
    gendata.last_allocatable = g_Vmc_Image[f->unit].header.last_allocatable;

    memcpy(gendata.indir_fat_clusters, g_Vmc_Image[f->unit].header.indir_fat_clusters, sizeof(unsigned int) * 32);

    unsigned int dirent_cluster = getDirentryFromPath(&dirent, path, &gendata, f->unit);

    if (dirent_cluster == NOFOUND_CLUSTER) {

        DEBUGPRINT(2, "vmc_fs: chstat failed. %s not found.\n", path);

        PROF_END(vmc_chstatProf)

        return -1;
    }

    if (statmask & FIO_CST_MODE) {
        //  File mode bits. Only Read, Write, Execute and Protected can be changed.
        dirent.mode = (dirent.mode & ~(DF_READ | DF_WRITE | DF_EXECUTE | DF_PROTECTED)) | (stat->mode & (DF_READ | DF_WRITE | DF_EXECUTE | DF_PROTECTED));
    }

    if (statmask & FIO_CST_CT) {
        //  File created Time  /  Date
        memcpy(&dirent.created, stat->ctime, sizeof(vmc_datetime));
    }

    if (statmask & FIO_CST_MT) {
        //  File Modification Time  /  Date
        memcpy(&dirent.created, stat->mtime, sizeof(vmc_datetime));
    }

    //  Write this change
    writePage(gendata.fd, (unsigned char *)&dirent, (dirent_cluster + gendata.first_allocatable) * g_Vmc_Image[f->unit].header.pages_per_cluster + gendata.dirent_page);

    PROF_END(vmc_chstatProf)

    return 0;
}


// Start of extended io fonctions.


//----------------------------------------------------------------------------
// Rename an object into a vmc file.
//----------------------------------------------------------------------------
int Vmc_Rename(iop_file_t *f, const char *path, const char *new_name)
{

    if (!g_Vmc_Initialized)
        return VMCFS_ERR_INITIALIZED;

    DEBUGPRINT(1, "vmc_fs: rename %s\n", path);

    if (g_Vmc_Image[f->unit].fd < 0)
        return VMCFS_ERR_NOT_MOUNT;

    if (g_Vmc_Image[f->unit].formated == FALSE)
        return VMCFS_ERR_NOT_FORMATED;

    PROF_START(vmc_renameProf)

    struct direntry dirent;
    struct gen_privdata gendata;

    gendata.fd = g_Vmc_Image[f->unit].fd;
    gendata.first_allocatable = g_Vmc_Image[f->unit].header.first_allocatable;
    gendata.last_allocatable = g_Vmc_Image[f->unit].header.last_allocatable;

    memcpy(gendata.indir_fat_clusters, g_Vmc_Image[f->unit].header.indir_fat_clusters, sizeof(unsigned int) * 32);

    unsigned int dirent_cluster = getDirentryFromPath(&dirent, path, &gendata, f->unit);

    if (dirent_cluster == ROOT_CLUSTER) {

        DEBUGPRINT(2, "vmc_fs: rename failed. Root directory is protected.\n");

        PROF_END(vmc_renameProf)

        return -1;

    } else if (dirent_cluster == NOFOUND_CLUSTER) {

        DEBUGPRINT(2, "vmc_fs: rename failed. %s not found.\n", path);

        PROF_END(vmc_renameProf)

        return -1;
    }

    //  Change the name of the object
    strcpy(dirent.name, new_name);

    // Update timestamp
    getPs2Time(&dirent.modified);

    //  Write this change
    writePage(gendata.fd, (unsigned char *)&dirent, (dirent_cluster + gendata.first_allocatable) * g_Vmc_Image[f->unit].header.pages_per_cluster + gendata.dirent_page);

    DEBUGPRINT(2, "vmc_fs: Object %s renamed to %s\n", path, new_name);

    PROF_END(vmc_renameProf)

    return 0;
}


//----------------------------------------------------------------------------
// Not Implemented for the moment.
//----------------------------------------------------------------------------
int Vmc_Chdir(iop_file_t *f, const char *path)
{

    return VMCFS_ERR_IMPLEMENTED;
}


//----------------------------------------------------------------------------
// Not Implemented for the moment.
//----------------------------------------------------------------------------
int Vmc_Sync(iop_file_t *f, const char *device, int flag)
{

    return VMCFS_ERR_IMPLEMENTED;
}


//----------------------------------------------------------------------------
// Mount a vmc file with fileXioMount(... call.
//----------------------------------------------------------------------------
int Vmc_Mount(iop_file_t *f, const char *fsname, const char *devname, int flag, void *arg, int arg_len)
{
    int errcode;

    DEBUGPRINT(2, "vmc_fs: Mount %s at mount point: %d\n", devname, f->unit);

    if (g_Vmc_Image[f->unit].fd >= 0)
        return VMCFS_ERR_MOUNT_BUSY;

    g_Vmc_Image[f->unit].fd = open(devname, O_RDWR, 0x666);

    if (g_Vmc_Image[f->unit].fd < 0) {
        DEBUGPRINT(1, "vmc_fs: Error opening vmc file %s\n", devname);
        DEBUGPRINT(1, "vmc_fs: open error code: %d\n", g_Vmc_Image[f->unit].fd);

        return VMCFS_ERR_VMC_OPEN;
    }

    //  read informations from the superblock
    int r = read(g_Vmc_Image[f->unit].fd, &g_Vmc_Image[f->unit].header, sizeof(struct superblock));

    if (r != sizeof(struct superblock)) {
        DEBUGPRINT(1, "vmc_fs: Error reading vmc file %s\n", devname);
        DEBUGPRINT(1, "vmc_fs: fd: %d, error code:%d\n", g_Vmc_Image[f->unit].fd, r);

        errcode = VMCFS_ERR_VMC_READ;

    mountAbort:
        close(g_Vmc_Image[f->unit].fd);
        g_Vmc_Image[f->unit].fd = VMCFS_ERR_NOT_MOUNT;
        return errcode;
    }

    g_Vmc_Image[f->unit].card_size = lseek(g_Vmc_Image[f->unit].fd, 0, SEEK_END);
    lseek(g_Vmc_Image[f->unit].fd, 0, SEEK_SET);

    if (g_Vmc_Image[f->unit].header.magic[0] != 'S' || g_Vmc_Image[f->unit].header.magic[1] != 'o' || g_Vmc_Image[f->unit].header.magic[2] != 'n' || g_Vmc_Image[f->unit].header.magic[3] != 'y' || g_Vmc_Image[f->unit].header.magic[4] != ' ' || g_Vmc_Image[f->unit].header.magic[5] != 'P' || g_Vmc_Image[f->unit].header.magic[6] != 'S' || g_Vmc_Image[f->unit].header.magic[7] != '2' || g_Vmc_Image[f->unit].header.magic[8] != ' ' || g_Vmc_Image[f->unit].header.magic[9] != 'M' || g_Vmc_Image[f->unit].header.magic[10] != 'e' || g_Vmc_Image[f->unit].header.magic[11] != 'm' || g_Vmc_Image[f->unit].header.magic[12] != 'o' || g_Vmc_Image[f->unit].header.magic[13] != 'r' || g_Vmc_Image[f->unit].header.magic[14] != 'y' || g_Vmc_Image[f->unit].header.magic[15] != ' ' || g_Vmc_Image[f->unit].header.magic[16] != 'C' || g_Vmc_Image[f->unit].header.magic[17] != 'a' || g_Vmc_Image[f->unit].header.magic[18] != 'r' || g_Vmc_Image[f->unit].header.magic[19] != 'd' || g_Vmc_Image[f->unit].header.magic[20] != ' ' || g_Vmc_Image[f->unit].header.magic[21] != 'F' || g_Vmc_Image[f->unit].header.magic[22] != 'o' || g_Vmc_Image[f->unit].header.magic[23] != 'r' || g_Vmc_Image[f->unit].header.magic[24] != 'm' || g_Vmc_Image[f->unit].header.magic[25] != 'a' || g_Vmc_Image[f->unit].header.magic[26] != 't') {
        //  Card is not formated
        DEBUGPRINT(1, "vmc_fs: Warning vmc file %s is not formated\n", devname);
        if (!setDefaultSpec(f->unit)) {
            //  Card size error
            DEBUGPRINT(1, "vmc_fs: Error size of vmc file %s is incompatible\n", devname);

            errcode = VMCFS_ERR_VMC_SIZE;
            goto mountAbort;
        }
        g_Vmc_Image[f->unit].formated = FALSE;
    } else {
        g_Vmc_Image[f->unit].formated = TRUE;

        DEBUGPRINT(4, "vmc_fs: SuperBlock readed from vmc file %s.\n", devname);

        DEBUGPRINT(4, "vmc_fs: SuperBlock Info: magic[40]             : %s\n", g_Vmc_Image[f->unit].header.magic);
        DEBUGPRINT(4, "vmc_fs: SuperBlock Info: page_size             : 0x%02x\n", g_Vmc_Image[f->unit].header.page_size);
        DEBUGPRINT(4, "vmc_fs: SuperBlock Info: pages_per_cluster     : 0x%02x\n", g_Vmc_Image[f->unit].header.pages_per_cluster);
        DEBUGPRINT(4, "vmc_fs: SuperBlock Info: pages_per_block       : 0x%02x\n", g_Vmc_Image[f->unit].header.pages_per_block);
        DEBUGPRINT(4, "vmc_fs: SuperBlock Info: clusters_per_card     : 0x%02x\n", g_Vmc_Image[f->unit].header.clusters_per_card);
        DEBUGPRINT(4, "vmc_fs: SuperBlock Info: first_allocatable     : 0x%02x\n", g_Vmc_Image[f->unit].header.first_allocatable);
        DEBUGPRINT(4, "vmc_fs: SuperBlock Info: last_allocatable      : 0x%02x\n", g_Vmc_Image[f->unit].header.last_allocatable);
        DEBUGPRINT(4, "vmc_fs: SuperBlock Info: root_cluster          : 0x%02x\n", g_Vmc_Image[f->unit].header.root_cluster);
        DEBUGPRINT(4, "vmc_fs: SuperBlock Info: backup_block1         : 0x%02x\n", g_Vmc_Image[f->unit].header.backup_block1);
        DEBUGPRINT(4, "vmc_fs: SuperBlock Info: backup_block2         : 0x%02x\n", g_Vmc_Image[f->unit].header.backup_block2);
        for (r = 0; g_Vmc_Image[f->unit].header.indir_fat_clusters[r] != 0; r++)
            DEBUGPRINT(4, "vmc_fs: SuperBlock Info: indir_fat_clusters[%d] : 0x%02x\n", r, g_Vmc_Image[f->unit].header.indir_fat_clusters[r]);
        DEBUGPRINT(4, "vmc_fs: SuperBlock Info: mc_type               : 0x%02x\n", g_Vmc_Image[f->unit].header.mc_type);
        DEBUGPRINT(4, "vmc_fs: SuperBlock Info: mc_flag               : 0x%02x\n", g_Vmc_Image[f->unit].header.mc_flag);

        if (g_Vmc_Image[f->unit].header.mc_type != PS2_MEMORYCARD) {
            //  Card is not a PS2 one
            DEBUGPRINT(1, "vmc_fs: Error vmc file %s is not a valid PS2 image\n", devname);

            errcode = VMCFS_ERR_CARD_TYPE;
            goto mountAbort;
        }

        //Reaching this point means we have a valid PS2 image
        DEBUGPRINT(4, "vmc_fs: Image file Info: Vmc card type         : %s MemoryCard.\n", (g_Vmc_Image[f->unit].header.mc_type == PSX_MEMORYCARD ? "PSX" : (g_Vmc_Image[f->unit].header.mc_type == PS2_MEMORYCARD ? "PS2" : "PDA")));

        g_Vmc_Image[f->unit].total_pages = g_Vmc_Image[f->unit].header.pages_per_cluster * g_Vmc_Image[f->unit].header.clusters_per_card;
        g_Vmc_Image[f->unit].cluster_size = g_Vmc_Image[f->unit].header.page_size * g_Vmc_Image[f->unit].header.pages_per_cluster;
        g_Vmc_Image[f->unit].erase_byte = (g_Vmc_Image[f->unit].header.mc_flag & 0x10) ? 0x0 : 0xFF;
        g_Vmc_Image[f->unit].last_idc = EOF_CLUSTER;
        g_Vmc_Image[f->unit].last_cluster = EOF_CLUSTER;
        g_Vmc_Image[f->unit].last_free_cluster = g_Vmc_Image[f->unit].header.first_allocatable;

        memset(&g_Vmc_Image[f->unit].indirect_cluster, g_Vmc_Image[f->unit].erase_byte, MAX_CLUSTER_SIZE);
        memset(&g_Vmc_Image[f->unit].fat_cluster, g_Vmc_Image[f->unit].erase_byte, MAX_CLUSTER_SIZE);

        if (g_Vmc_Image[f->unit].card_size == ((g_Vmc_Image[f->unit].header.page_size + 0x10) * g_Vmc_Image[f->unit].total_pages)) {
            g_Vmc_Image[f->unit].ecc_flag = TRUE;
        } else if (g_Vmc_Image[f->unit].card_size == (g_Vmc_Image[f->unit].header.page_size * g_Vmc_Image[f->unit].total_pages)) {
            g_Vmc_Image[f->unit].ecc_flag = FALSE;
        } else {
            //  Card size error
            DEBUGPRINT(1, "vmc_fs: Error size of vmc file %s is incompatible\n", devname);
            errcode = VMCFS_ERR_VMC_SIZE;
            goto mountAbort;
        }

        DEBUGPRINT(4, "vmc_fs: Image file Info: Number of pages       : %d\n", g_Vmc_Image[f->unit].total_pages);
        DEBUGPRINT(4, "vmc_fs: Image file Info: Size of a cluster     : %d bytes\n", g_Vmc_Image[f->unit].cluster_size);
        DEBUGPRINT(4, "vmc_fs: Image file Info: ECC shunk found       : %s\n", g_Vmc_Image[f->unit].ecc_flag ? "YES" : "NO");
    }

    if (g_Vmc_Image[f->unit].formated == FALSE) {
        errcode = VMCFS_ERR_NOT_FORMATED;
        goto mountAbort;
    }

    return 0;
}

//----------------------------------------------------------------------------
// Unmount a vmc file previously mounted with fileXioMount(
//----------------------------------------------------------------------------
int Vmc_Umount(iop_file_t *f, const char *fsname)
{

    DEBUGPRINT(2, "vmc_fs: UnMount %s at mount point: %d\n", fsname, f->unit);

    close(g_Vmc_Image[f->unit].fd);
    g_Vmc_Image[f->unit].fd = VMCFS_ERR_NOT_MOUNT;

    return 0;
}

//----------------------------------------------------------------------------
// Not Implemented for the moment.
//----------------------------------------------------------------------------
s64 Vmc_Lseek64(iop_file_t *f, s64 offset, int whence)
{

    return VMCFS_ERR_IMPLEMENTED;
}


//----------------------------------------------------------------------------
// Control command.
// DEVCTL_VMCFS_CLEAN   :  Set as free all fat cluster corresponding to a none existing object. ( Object are just marked as none existing but not removed from fat table when rmdir or remove fonctions are call. This allow to recover a deleted file. )
// DEVCTL_VMCFS_CKFREE  :  Check free space available on vmc file.
//----------------------------------------------------------------------------
int Vmc_Devctl(iop_file_t *f, const char *path, int cmd, void *arg, unsigned int arglen, void *buf, unsigned int buflen)
{

    if (!g_Vmc_Initialized)
        return VMCFS_ERR_INITIALIZED;

    switch (cmd) {

        case DEVCTL_VMCFS_CLEAN: {

            Vmc_Clean(f->unit);

        } break;

        case DEVCTL_VMCFS_CKFREE: {

            unsigned int free_space = Vmc_Checkfree(f->unit);

            return free_space;

        } break;

        default:

            DEBUGPRINT(1, "vmc_fs: Unrecognized devctl command %d\n", cmd);
            break;
    }

    return VMCFS_ERR_NO;
}


//----------------------------------------------------------------------------
// Not Implemented for the moment.
//----------------------------------------------------------------------------
int Vmc_Symlink(iop_file_t *f, const char *old, const char *new)
{

    return VMCFS_ERR_IMPLEMENTED;
}


//----------------------------------------------------------------------------
// Not Implemented for the moment.
//----------------------------------------------------------------------------
int Vmc_Readlink(iop_file_t *f, const char *path, char *buf, unsigned int buf_len)
{

    return VMCFS_ERR_IMPLEMENTED;
}


//----------------------------------------------------------------------------
// Not Implemented for the moment.
//----------------------------------------------------------------------------
int Vmc_Ioctl2(iop_file_t *f, int cmd, void *arg, unsigned int arglen, void *buf, unsigned int buflen)
{

    return VMCFS_ERR_IMPLEMENTED;
}


// Extanded ioctl fonctions.


//----------------------------------------------------------------------------
// Recover an object in vmc image after removing it.
// Not working properly when object have been overwrited
//----------------------------------------------------------------------------
int Vmc_Recover(int unit, const char *path1)
{

    if (!g_Vmc_Initialized)
        return VMCFS_ERR_INITIALIZED;

    DEBUGPRINT(1, "vmc_fs: recover %s\n", path1);

    if (g_Vmc_Image[unit].fd < 0)
        return VMCFS_ERR_NOT_MOUNT;

    if (g_Vmc_Image[unit].formated == FALSE)
        return VMCFS_ERR_NOT_FORMATED;

    PROF_START(vmc_recoverProf)

    struct direntry dirent;
    struct gen_privdata gendata;

    gendata.fd = g_Vmc_Image[unit].fd;
    gendata.first_allocatable = g_Vmc_Image[unit].header.first_allocatable;
    gendata.last_allocatable = g_Vmc_Image[unit].header.last_allocatable;

    memcpy(gendata.indir_fat_clusters, g_Vmc_Image[unit].header.indir_fat_clusters, sizeof(unsigned int) * 32);

    // Make a local copy of path.
    char *path = malloc(strlen(path1) - strlen("vmc0:") + 1);
    memcpy(path, path1 + strlen("vmc0:"), strlen(path1) - strlen("vmc0:") + 1);

    if (path[strlen(path) - 1] == '/')
        path[strlen(path) - 1] = '\0';

    unsigned int dirent_cluster = getDirentryFromPath(&dirent, path, &gendata, unit);

    if (dirent_cluster == ROOT_CLUSTER) {

        DEBUGPRINT(2, "vmc_fs: recover failed. Root directory is protected.\n");

        free(path);

        PROF_END(vmc_recoverProf)

        return -1;

    } else if (dirent_cluster == NOFOUND_CLUSTER) {

        DEBUGPRINT(2, "vmc_fs: recover failed. %s not found.\n", path1);

        free(path);

        PROF_END(vmc_recoverProf)

        return -1;
    }

    struct direntry parent;
    struct gen_privdata parent_gendata;
    unsigned int parent_cluster = 0;

    parent_gendata.fd = g_Vmc_Image[unit].fd;
    parent_gendata.first_allocatable = g_Vmc_Image[unit].header.first_allocatable;
    parent_gendata.last_allocatable = g_Vmc_Image[unit].header.last_allocatable;

    memcpy(parent_gendata.indir_fat_clusters, g_Vmc_Image[unit].header.indir_fat_clusters, sizeof(unsigned int) * 32);

    // Find name of file, and name of parent
    char *filename = strrchr(path, '/');

    filename[0] = '\0';
    filename++;

    DEBUGPRINT(4, "vmc_fs: Checking attributs parent directory %s\n", path);

    if (path[0] == '\0') {

        //  get the root directories entry
        parent_cluster = getDirentryFromPath(&parent, ".", &parent_gendata, unit);

    } else {

        //  get the folder entry for the parent
        parent_cluster = getDirentryFromPath(&parent, path, &parent_gendata, unit);
    }

    if (parent_cluster == NOFOUND_CLUSTER) {

        DEBUGPRINT(3, "vmc_fs: Unable to recover %s. Parent directory not found.\n", path1);

        free(path);

        PROF_END(vmc_recoverProf)

        return -1;
    }

    DEBUGPRINT(6, "vmc_fs: Parent Information.\n");
    DEBUGPRINT(6, "vmc_fs: parent_cluster  = %u\n", parent_cluster);
    DEBUGPRINT(6, "vmc_fs: dir_cluster    = %u\n", parent.cluster);
    DEBUGPRINT(6, "vmc_fs: dirent.name    = %s\n", parent.name);
    DEBUGPRINT(6, "vmc_fs: dirent.length  = %u\n", parent.length);
    DEBUGPRINT(6, "vmc_fs: dirent.mode    = %X\n", parent.mode);
    DEBUGPRINT(6, "vmc_fs: dirent_page    = %i\n", parent_gendata.dirent_page);

    if (!(parent.mode & DF_EXISTS)) {

        DEBUGPRINT(3, "vmc_fs: Unable to restore %s. Parent directory %s is hidden.\n", path1, path);

        free(path);

        PROF_END(vmc_recoverProf)

        return -1;
    }

    if (dirent.mode & DF_DIRECTORY)  // directory case
    {

        if (dirent.mode & DF_EXISTS) {

            DEBUGPRINT(2, "vmc_fs: recover failed on %s. Directory allready exist.\n", path1);

            free(path);

            PROF_END(vmc_recoverProf)

            return -1;

        } else {

            DEBUGPRINT(8, "vmc_fs: recover directory %s allready exist but is hidden. Changing attributs.\n", path1);

            DEBUGPRINT(8, "vmc_fs: Following fat table cluster %u\n", dirent.cluster);

            unsigned int pseudo_entry_cluster = getFatEntry(gendata.fd, dirent.cluster, gendata.indir_fat_clusters, FAT_VALUE);

            DEBUGPRINT(8, "vmc_fs: Changing cluster mask of fat table cluster %u.\n", pseudo_entry_cluster);

            // change cluster mask of the direntry
            setFatEntry(gendata.fd, dirent.cluster, pseudo_entry_cluster, gendata.indir_fat_clusters, FAT_SET);

            DEBUGPRINT(8, "vmc_fs: Changing direntry %s attributs.\n", path1);

            //  Update time stamp, and set dirent.mode to exist flag
            dirent.mode = dirent.mode | DF_EXISTS;
            getPs2Time(&dirent.created);
            getPs2Time(&dirent.modified);
            writePage(gendata.fd, (unsigned char *)&dirent, (dirent_cluster + gendata.first_allocatable) * g_Vmc_Image[unit].header.pages_per_cluster + gendata.dirent_page);

            DEBUGPRINT(8, "vmc_fs: Restoring EOF cluster at %u.\n", pseudo_entry_cluster);

            setFatEntry(gendata.fd, pseudo_entry_cluster, EOF_CLUSTER, gendata.indir_fat_clusters, FAT_SET);

            struct direntry pseudo_entries;

            DEBUGPRINT(8, "vmc_fs: Updating pseudo entries time stamps at cluster %u / page %u.\n", dirent.cluster + gendata.first_allocatable, (dirent.cluster + gendata.first_allocatable) * g_Vmc_Image[unit].header.pages_per_cluster);

            //  Update time stamp of '.' entry
            readPage(gendata.fd, (unsigned char *)&pseudo_entries, (dirent.cluster + gendata.first_allocatable) * g_Vmc_Image[unit].header.pages_per_cluster);
            getPs2Time(&pseudo_entries.created);
            getPs2Time(&pseudo_entries.modified);
            writePage(gendata.fd, (unsigned char *)&pseudo_entries, (dirent.cluster + gendata.first_allocatable) * g_Vmc_Image[unit].header.pages_per_cluster);

            DEBUGPRINT(8, "vmc_fs: Updating pseudo entries time stamps at cluster %u / page %u.\n", dirent.cluster + gendata.first_allocatable, (dirent.cluster + gendata.first_allocatable) * g_Vmc_Image[unit].header.pages_per_cluster + 1);

            //  Update time stamp of '..' entry
            readPage(gendata.fd, (unsigned char *)&pseudo_entries, (dirent.cluster + gendata.first_allocatable) * g_Vmc_Image[unit].header.pages_per_cluster + 1);
            getPs2Time(&pseudo_entries.created);
            getPs2Time(&pseudo_entries.modified);
            writePage(gendata.fd, (unsigned char *)&pseudo_entries, (dirent.cluster + gendata.first_allocatable) * g_Vmc_Image[unit].header.pages_per_cluster + 1);

            DEBUGPRINT(2, "vmc_fs: Directory %s recovered.\n", path1);

            free(path);

            goto end;
        }

    } else  // file case
    {

        if (dirent.mode & DF_EXISTS) {

            DEBUGPRINT(2, "vmc_fs: recover failed on %s. File allready exist.\n", path);

            free(path);

            PROF_END(vmc_recoverProf)

            return -1;

        } else {

            unsigned int current_cluster;
            unsigned int last_cluster = dirent.cluster;

            DEBUGPRINT(8, "vmc_fs: Restoring fat table clusters of file %s\n", filename);

            while (1) {

                current_cluster = getFatEntry(gendata.fd, last_cluster, gendata.indir_fat_clusters, FAT_VALUE);

                if (current_cluster == FREE_CLUSTER) {

                    // FREE_CLUSTER mean last cluster of the direntry is found
                    DEBUGPRINT(8, "vmc_fs: Last cluster of file at %u\n", last_cluster);

                    DEBUGPRINT(8, "vmc_fs: Restoring End Of File at fat table cluster %u\n", last_cluster);

                    setFatEntry(gendata.fd, last_cluster, EOF_CLUSTER, gendata.indir_fat_clusters, FAT_SET);

                    break;

                } else if (current_cluster == EOF_CLUSTER) {

                    // EOF_CLUSTER mean nothing to create or error, so goto end
                    DEBUGPRINT(3, "vmc_fs: Error. EOF_CLUSTER found !!!\n");

                    free(path);

                    goto end;

                } else {

                    // Otherwise set cluster as free
                    DEBUGPRINT(10, "vmc_fs: Testing cluster %u ... value is %u\n", last_cluster, current_cluster);

                    DEBUGPRINT(8, "vmc_fs: Restoring cluster mask at fat table cluster %u\n", last_cluster);

                    setFatEntry(gendata.fd, last_cluster, current_cluster, gendata.indir_fat_clusters, FAT_SET);
                }

                last_cluster = current_cluster;
            }

            DEBUGPRINT(8, "vmc_fs: Restoring direntry at cluster %u / page %u\n", dirent_cluster + gendata.first_allocatable, (dirent.cluster + gendata.first_allocatable) * g_Vmc_Image[unit].header.pages_per_cluster + gendata.dirent_page);

            dirent.mode = dirent.mode | DF_EXISTS;
            getPs2Time(&dirent.created);
            getPs2Time(&dirent.modified);
            writePage(gendata.fd, (unsigned char *)&dirent, (dirent_cluster + gendata.first_allocatable) * g_Vmc_Image[unit].header.pages_per_cluster + gendata.dirent_page);

            free(path);

            DEBUGPRINT(3, "vmc_fs: File %s restored.\n", path1);
        }
    }

end:

    PROF_END(vmc_recoverProf)

    return 0;
}


//----------------------------------------------------------------------------
// Check actual free space of the vmc file.
//----------------------------------------------------------------------------
unsigned int Vmc_Checkfree(int unit)
{

    if (!g_Vmc_Initialized)
        return VMCFS_ERR_INITIALIZED;

    DEBUGPRINT(1, "vmc_fs: check free %d\n", unit);

    if (g_Vmc_Image[unit].fd < 0)
        return VMCFS_ERR_NOT_MOUNT;

    if (g_Vmc_Image[unit].formated == FALSE)
        return VMCFS_ERR_NOT_FORMATED;

    PROF_START(vmc_checkfreeProf)

    int i = 0;
    unsigned int cluster_value;
    unsigned int cluster_mask;
    unsigned int free_space = 0;
    unsigned int free_cluster_num = 0;
    struct gen_privdata gendata;

    gendata.fd = g_Vmc_Image[unit].fd;
    gendata.first_allocatable = g_Vmc_Image[unit].header.first_allocatable;
    gendata.last_allocatable = g_Vmc_Image[unit].header.last_allocatable;

    memcpy(gendata.indir_fat_clusters, g_Vmc_Image[unit].header.indir_fat_clusters, sizeof(unsigned int) * 32);

    for (i = gendata.first_allocatable; i < gendata.last_allocatable; i++) {

        cluster_value = getFatEntry(gendata.fd, i - gendata.first_allocatable, gendata.indir_fat_clusters, FAT_VALUE);

        if (cluster_value == FREE_CLUSTER) {

            DEBUGPRINT(10, "vmc_fs: Testing cluster %d ... value is FREE_CLUSTER\n", i);

            DEBUGPRINT(6, "vmc_fs: Free cluster found at %d\n", i);

            free_cluster_num++;

        } else if (cluster_value == EOF_CLUSTER) {

            DEBUGPRINT(10, "vmc_fs: Testing cluster %d ... value is EOF_CLUSTER\n", i);

        } else {

            DEBUGPRINT(10, "vmc_fs: Testing cluster %d ... value is %u\n", i, cluster_value);

            cluster_mask = getFatEntry(gendata.fd, i - gendata.first_allocatable, gendata.indir_fat_clusters, FAT_MASK);

            if (cluster_mask != MASK_CLUSTER) {

                DEBUGPRINT(6, "vmc_fs: Free cluster found at %d\n", i);

                free_cluster_num++;
            }
        }
    }

    free_space = free_cluster_num * g_Vmc_Image[unit].cluster_size;

    PROF_END(vmc_checkfreeProf)

    DEBUGPRINT(3, "vmc_fs: Total free space: %u\n", free_space);

    return free_space;
}


//----------------------------------------------------------------------------
// Clean unused cluster of the vmc file.
//----------------------------------------------------------------------------
int Vmc_Clean(int unit)
{

    if (!g_Vmc_Initialized)
        return VMCFS_ERR_INITIALIZED;

    DEBUGPRINT(1, "vmc_fs: clean %d\n", unit);

    if (g_Vmc_Image[unit].fd < 0)
        return VMCFS_ERR_NOT_MOUNT;

    if (g_Vmc_Image[unit].formated == FALSE)
        return VMCFS_ERR_NOT_FORMATED;

    PROF_START(vmc_cleanProf)

    int i = 0;
    unsigned int cluster_value;
    unsigned int cluster_mask;
    struct gen_privdata gendata;

    gendata.fd = g_Vmc_Image[unit].fd;
    gendata.first_allocatable = g_Vmc_Image[unit].header.first_allocatable;
    gendata.last_allocatable = g_Vmc_Image[unit].header.last_allocatable;

    memcpy(gendata.indir_fat_clusters, g_Vmc_Image[unit].header.indir_fat_clusters, sizeof(unsigned int) * 32);

    for (i = gendata.first_allocatable; i < gendata.last_allocatable; i++) {

        cluster_value = getFatEntry(gendata.fd, i - gendata.first_allocatable, gendata.indir_fat_clusters, FAT_VALUE);

        if (cluster_value == FREE_CLUSTER) {

            DEBUGPRINT(10, "vmc_fs: Testing cluster %d ... value is FREE_CLUSTER\n", i);

        } else if (cluster_value == EOF_CLUSTER) {

            DEBUGPRINT(10, "vmc_fs: Testing cluster %d ... value is EOF_CLUSTER\n", i);

        } else {

            DEBUGPRINT(10, "vmc_fs: Testing cluster %d ... value is %u\n", i, cluster_value);

            cluster_mask = getFatEntry(gendata.fd, i - gendata.first_allocatable, gendata.indir_fat_clusters, FAT_MASK);

            if (cluster_mask != MASK_CLUSTER) {

                DEBUGPRINT(6, "vmc_fs: Setting cluster %d as free cluster.\n", i);

                setFatEntry(gendata.fd, i - gendata.first_allocatable, FREE_CLUSTER, gendata.indir_fat_clusters, FAT_SET);
            }
        }
    }

    return 0;
}
