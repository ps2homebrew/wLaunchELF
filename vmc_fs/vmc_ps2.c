#include "vmc.h"


// Functions specific to the ps2

int readPage(int fd, u8 *page, unsigned int pagenum)
{

    int unit;
    unsigned int position;

    unit = (fd == g_Vmc_Image[0].fd) ? 0 : 1;

    if (g_Vmc_Image[unit].ecc_flag) {

        position = pagenum * (g_Vmc_Image[unit].header.page_size + 0x10);

    } else {

        position = pagenum * g_Vmc_Image[unit].header.page_size;
    }

    lseek(fd, position, SEEK_SET);
    read(fd, page, g_Vmc_Image[unit].header.page_size);

    return 0;
}

int readCluster(int fd, u8 *cluster, unsigned int clusternum)
{

    int i, unit;
    u8 *Page_Data;
    unsigned int Page_Num;

    unit = (fd == g_Vmc_Image[0].fd) ? 0 : 1;

    Page_Num = clusternum * g_Vmc_Image[unit].header.pages_per_cluster;

    Page_Data = (u8 *)malloc((g_Vmc_Image[unit].header.page_size + 0xFF) & ~(unsigned int)0xFF);

    for (i = 0; i < g_Vmc_Image[unit].header.pages_per_cluster; i++) {

        memset(Page_Data, g_Vmc_Image[unit].erase_byte, g_Vmc_Image[unit].header.page_size);

        readPage(fd, Page_Data, Page_Num + i);

        memcpy(cluster + (i * g_Vmc_Image[unit].header.page_size), Page_Data, g_Vmc_Image[unit].header.page_size);
    }

    free(Page_Data);

    return 0;
}

int writePage(int fd, u8 *page, unsigned int pagenum)
{

    int unit;
    unsigned int position;

    unit = (fd == g_Vmc_Image[0].fd) ? 0 : 1;

    if (g_Vmc_Image[unit].ecc_flag) {

        position = pagenum * (g_Vmc_Image[unit].header.page_size + 0x10);

    } else {

        position = pagenum * g_Vmc_Image[unit].header.page_size;
    }

    lseek(fd, position, SEEK_SET);
    write(fd, page, g_Vmc_Image[unit].header.page_size);

    if (g_Vmc_Image[unit].ecc_flag) {

        u8 *ECC_Data;

        ECC_Data = (u8 *)malloc((0x10 + 0xFF) & ~(unsigned int)0xFF);

        buildECC(unit, page, ECC_Data);
        write(fd, ECC_Data, 0x10);

        free(ECC_Data);
    }

    return 0;
}

int writeCluster(int fd, u8 *cluster, unsigned int clusternum)
{

    int i, unit;
    u8 *Page_Data;
    unsigned int Page_Num;

    unit = (fd == g_Vmc_Image[0].fd) ? 0 : 1;

    Page_Num = clusternum * g_Vmc_Image[unit].header.pages_per_cluster;

    Page_Data = (u8 *)malloc((g_Vmc_Image[unit].header.page_size + 0xFF) & ~(unsigned int)0xFF);

    for (i = 0; i < g_Vmc_Image[unit].header.pages_per_cluster; i++) {

        memset(Page_Data, g_Vmc_Image[unit].erase_byte, g_Vmc_Image[unit].header.page_size);
        memcpy(Page_Data, cluster + (i * g_Vmc_Image[unit].header.page_size), g_Vmc_Image[unit].header.page_size);

        writePage(fd, Page_Data, Page_Num + i);
    }

    free(Page_Data);

    return 0;
}

int writeClusterPart(int fd, u8 *cluster, unsigned int clusternum, int cluster_offset, int size)
{

    int i, unit;
    u8 *Page_Data;
    unsigned int Page_Num, Page_offset;

    unit = (fd == g_Vmc_Image[0].fd) ? 0 : 1;

    Page_Num = clusternum * g_Vmc_Image[unit].header.pages_per_cluster;
    Page_offset = cluster_offset;

    Page_Data = (u8 *)malloc((g_Vmc_Image[unit].header.page_size + 0xFF) & ~(unsigned int)0xFF);

    for (i = 0; i < g_Vmc_Image[unit].header.pages_per_cluster; i++) {

        if ((cluster_offset >= (g_Vmc_Image[unit].header.page_size * i)) && (cluster_offset < (g_Vmc_Image[unit].header.page_size * (i + 1)))) {

            Page_Num += i;
            Page_offset = cluster_offset - g_Vmc_Image[unit].header.page_size * i;

            memset(Page_Data, g_Vmc_Image[unit].erase_byte, g_Vmc_Image[unit].header.page_size);
            readPage(fd, Page_Data, Page_Num);

            if (size > g_Vmc_Image[unit].header.page_size - Page_offset) {

                memcpy(Page_Data + Page_offset, cluster, g_Vmc_Image[unit].header.page_size - Page_offset);
                writePage(fd, Page_Data, Page_Num);

                Page_Num++;

                memset(Page_Data, g_Vmc_Image[unit].erase_byte, g_Vmc_Image[unit].header.page_size);
                readPage(fd, Page_Data, Page_Num);

                memcpy(Page_Data, cluster + (g_Vmc_Image[unit].header.page_size - Page_offset), size - (g_Vmc_Image[unit].header.page_size - Page_offset));
                writePage(fd, Page_Data, Page_Num);

            } else {

                memcpy(Page_Data + Page_offset, cluster, size);
                writePage(fd, Page_Data, Page_Num);
            }
        }
    }

    free(Page_Data);

    return 0;
}

int eraseBlock(int fd, unsigned int block)
{

    int i, unit;
    u8 *Page_Data;
    unsigned int Page_Num;

    unit = (fd == g_Vmc_Image[0].fd) ? 0 : 1;

    Page_Data = (u8 *)malloc((g_Vmc_Image[unit].header.page_size + 0xFF) & ~(unsigned int)0xFF);

    Page_Num = block * g_Vmc_Image[unit].header.pages_per_block;

    memset(Page_Data, g_Vmc_Image[unit].erase_byte, g_Vmc_Image[unit].header.page_size);

    for (i = 0; i < g_Vmc_Image[unit].header.pages_per_block; i++) {

        writePage(fd, Page_Data, Page_Num + i);
    }

    free(Page_Data);

    return 0;
}

void *malloc(int size)
{
    int OldState;
    void *result;

    CpuSuspendIntr(&OldState);
    result = AllocSysMemory(ALLOC_FIRST, size, NULL);
    CpuResumeIntr(OldState);

    return result;
}

void free(void *ptr)
{
    int OldState;

    CpuSuspendIntr(&OldState);
    FreeSysMemory(ptr);
    CpuResumeIntr(OldState);
}
