#include "vmc.h"


// Fat table functions

unsigned int getFatEntry(int fd, unsigned int cluster, const unsigned int *indir_fat_clusters, GetFat_Mode Mode)
{

    int unit = (fd == g_Vmc_Image[0].fd) ? 0 : 1;

    // The fat index, aka which cluster we want to know about
    unsigned int fat_index = cluster;

    // The offset in the fat cluster where we can find information about the sector
    unsigned int fat_offset = fat_index % (g_Vmc_Image[unit].cluster_size / 4);

    // Which fat cluster that the information would be in
    unsigned int indirect_index = fat_index / (g_Vmc_Image[unit].cluster_size / 4);

    // The offset in the indirect fat cluster that points to the fat table we are interested in
    unsigned int indirect_offset = indirect_index % (g_Vmc_Image[unit].cluster_size / 4);

    // This gives the location of the fat_offset inside of the indirect fat cluster list.  Each indirect fat cluster can point to
    // 256 regular fat clusters, and each fat cluster holds information on 256 sectors.
    unsigned int dbl_indirect_index = fat_index / ((g_Vmc_Image[unit].cluster_size / 4) * (g_Vmc_Image[unit].cluster_size / 4));

    // Cache indirect file table? ( Generally we will be accessing the first part of it for most virtual memory cards )
    // Get the data for the indirect fat cluster ( as indicated by dbl_indirect_index )
    unsigned int indirect_cluster_num = indir_fat_clusters[dbl_indirect_index];

    if (g_Vmc_Image[unit].last_idc != indirect_cluster_num)
        readCluster(fd, (unsigned char *)g_Vmc_Image[unit].indirect_cluster, indirect_cluster_num);

    g_Vmc_Image[unit].last_idc = indirect_cluster_num;

    // Get the data from the fat cluster ( pointed to by the indirect fat cluster )
    unsigned int fat_cluster_num = g_Vmc_Image[unit].indirect_cluster[indirect_offset];

    if (g_Vmc_Image[unit].last_cluster != fat_cluster_num)
        readCluster(fd, (unsigned char *)g_Vmc_Image[unit].fat_cluster, fat_cluster_num);

    g_Vmc_Image[unit].last_cluster = fat_cluster_num;

    // Check if the entry in the fat table corresponds to a free cluster or a eof cluster
    if (g_Vmc_Image[unit].fat_cluster[fat_offset] == FREE_CLUSTER || g_Vmc_Image[unit].fat_cluster[fat_offset] == EOF_CLUSTER)
        return g_Vmc_Image[unit].fat_cluster[fat_offset];

    if (Mode == FAT_MASK) {

        if (g_Vmc_Image[unit].fat_cluster[fat_offset] & MASK_CLUSTER) {

            return MASK_CLUSTER;

        } else {

            return 0;
        }
    }

    // Return the fat entry, but remove the most significant bit.
    return g_Vmc_Image[unit].fat_cluster[fat_offset] & FREE_CLUSTER;
}

unsigned int setFatEntry(int fd, unsigned int cluster, unsigned int value, const unsigned int *indir_fat_clusters, SetFat_Mode Mode)
{

    int unit = (fd == g_Vmc_Image[0].fd) ? 0 : 1;

    // The fat index, aka which cluster we want to know about
    unsigned int fat_index = cluster;

    // The offset in the fat cluster where we can find information about the sector
    unsigned int fat_offset = fat_index % (g_Vmc_Image[unit].cluster_size / 4);

    // Which fat cluster that the information would be in
    unsigned int indirect_index = fat_index / (g_Vmc_Image[unit].cluster_size / 4);

    // The offset in the indirect fat cluster that points to the fat table we are interested in
    unsigned int indirect_offset = indirect_index % (g_Vmc_Image[unit].cluster_size / 4);

    // This gives the location of the fat_offset inside of the indirect fat cluster list.  Each indirect fat cluster can point to
    // 256 regular fat clusters, and each fat cluster holds information on 256 sectors.
    unsigned int dbl_indirect_index = fat_index / ((g_Vmc_Image[unit].cluster_size / 4) * (g_Vmc_Image[unit].cluster_size / 4));

    // Cache indirect file table? ( Generally we will be accessing the first part of it for most virtual memory cards
    // Get the data for the indirect fat cluster ( as indicated by dbl_indirect_index )
    unsigned int indirect_cluster_num = indir_fat_clusters[dbl_indirect_index];

    if (g_Vmc_Image[unit].last_idc != indirect_cluster_num)
        readCluster(fd, (unsigned char *)g_Vmc_Image[unit].indirect_cluster, indirect_cluster_num);

    g_Vmc_Image[unit].last_idc = indirect_cluster_num;

    // Get the data from the fat cluster ( pointed to by the indirect fat cluster )
    unsigned int fat_cluster_num = g_Vmc_Image[unit].indirect_cluster[indirect_offset];

    if (g_Vmc_Image[unit].last_cluster != fat_cluster_num)
        readCluster(fd, (unsigned char *)g_Vmc_Image[unit].fat_cluster, fat_cluster_num);

    g_Vmc_Image[unit].last_cluster = fat_cluster_num;

    if (value == FREE_CLUSTER || value == EOF_CLUSTER || Mode == FAT_RESET) {

        g_Vmc_Image[unit].fat_cluster[fat_offset] = value;

    } else {

        g_Vmc_Image[unit].fat_cluster[fat_offset] = value | MASK_CLUSTER;
    }

    writeCluster(fd, (unsigned char *)g_Vmc_Image[unit].fat_cluster, fat_cluster_num);

    // Update last free cluster
    if ((value == FREE_CLUSTER && Mode == FAT_SET) || (Mode == FAT_RESET)) {

        if (cluster < g_Vmc_Image[unit].last_free_cluster) {

            g_Vmc_Image[unit].last_free_cluster = cluster;
        }
    }

    return 0;
}
