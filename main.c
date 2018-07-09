// #include <fcntl.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SECT_SIZE 512
#define MAX_BUF 1048576

#define ATTR_READ_ONLY	0x01
#define ATTR_HIDDEN	0x02
#define ATTR_SYSTEM	0x04
#define ATTR_VOLUME_ID	0x08
#define ATTR_DIRECTORY	0x10
#define ATTR_ARCHIVE	0x20
#define ATTR_LONG_NAME	( ATTR_READ_ONLY & ATTR_HIDDEN & ATTR_SYSTEM & ATTR_VOLUME_ID )

typedef struct boot_entry { 
	//'__packed__' attribute ignored for field of type 'uint8_t'
	uint8_t BS_jmpBoot[3]; 
	uint8_t BS_OEMName[8]; 
	uint16_t BPB_BytsPerSec __attribute__ ((__packed__)); 
	uint8_t BPB_SecPerClus; 
	uint16_t BPB_RsvdSecCnt __attribute__ ((__packed__)); 
	uint8_t BPB_NumFATs; 
	uint16_t BPB_RootEntCnt __attribute__ ((__packed__)); 
	uint16_t BPB_TotSec16 __attribute__ ((__packed__)); 
	uint8_t BPB_Media; 
	uint16_t BPB_FATSz16 __attribute__ ((__packed__)); 
	uint16_t BPB_SecPerTrk __attribute__ ((__packed__)); 
	uint16_t BPB_NumHeads __attribute__ ((__packed__)); 
	uint32_t BPB_HiddSec __attribute__ ((__packed__)); 
	uint32_t BPB_TotSec32 __attribute__ ((__packed__));
	// assume that partition is FAT32, not FAT12 or FAT16,
	// so consequent fields are corresponding to this:
	uint32_t BPB_FATSz32 __attribute__ ((__packed__));
	uint16_t BPB_ExtFlags __attribute__ ((__packed__));
	uint16_t BPB_FSVer __attribute__ ((__packed__));
	uint32_t BPB_RootClus __attribute__ ((__packed__));
	uint16_t BPB_FSInfo __attribute__ ((__packed__));
	uint16_t BPB_BkBootSec __attribute__ ((__packed__));
	uint8_t BPB_Reserved[12];
	uint8_t BS_DrvNum;
	uint8_t BS_Reserved1;
	uint8_t BS_BootSig;
	uint32_t BS_VolID __attribute__ ((__packed__));
	uint8_t BS_VolLab[11];
	uint8_t BS_FilSysType[8];
} t_boot_entry;

typedef struct dir_entry {
	uint8_t DIR_Name[11];
	uint8_t DIR_Attr;
	uint8_t DIR_NTRes;
	uint8_t DIR_CrtTimeTenth;
	uint16_t DIR_CrtTime __attribute__((packed));
	uint16_t DIR_CrtDate __attribute__((packed));
	uint16_t DIR_LstAccDate __attribute__((packed));
	uint16_t DIR_FstClusHI __attribute__((packed));
	uint16_t DIR_WrtTime __attribute__((packed));
	uint16_t DIR_WrtDate __attribute__((packed));
	uint16_t DIR_FstClusLO __attribute__((packed));
	uint32_t DIR_FileSize __attribute__((packed));
} t_dir_entry;

int bytes_per_sec;	//bytes per sector

int read_sec(FILE *fd, int sec, int num, uint8_t *buf);

char *int2bin(int a, char *buffer, int buf_size);

int main(int argc, char **argv)
{
	FILE *fd;
	int num;
	unsigned char *buf;
	t_boot_entry boot_entry;
	int sec_per_cluster;	//sectors per cluster
	long root_dir_sec;	//count of sectors occupied by the root dir
	long FATSz;		//FAT size
	long first_data_sec;	//first data sector number
	long root_dir_sec_num;
	// long tot_sec;		//total count of sectors on the volume
	// long num_data_sec;	//count of sectors in the volume's data region
	// long num_clusters;	//count of clusters in the volume
	int i;
	// int fat_offset;
	// int this_fat_sec_num;
	// int this_fat_ent_offset;
	// uint32_t fat32_cluster_entry_val;
	t_dir_entry dir_entry;

	fd = fopen("./disk.img", "r");
	if (!fd) {
		perror("Failed to open file");
		exit(1);
	}

	buf = (unsigned char *)malloc(sizeof(unsigned char) * SECT_SIZE);
	if (!buf) {
		perror("Memory allocation error");
		exit(1);
	}
	if ((num = fread(buf, 1, SECT_SIZE, fd)) != SECT_SIZE) {
		perror("Failed to read root sector");
		exit(1);
	}
	memcpy(&boot_entry, buf, sizeof(t_boot_entry));
	free(buf);

	bytes_per_sec = boot_entry.BPB_BytsPerSec;
	sec_per_cluster = boot_entry.BPB_SecPerClus;

	printf("boot_entry.BS_OEMName: |%s|\n", boot_entry.BS_OEMName);
	printf("boot_entry.BPB_BytsPerSec: %d\n", boot_entry.BPB_BytsPerSec);
	printf("boot_entry.BPB_SecPerClus: %d\n", boot_entry.BPB_SecPerClus);
	printf("boot_entry.BPB_RsvdSecCnt: %d\n", boot_entry.BPB_RsvdSecCnt);
	printf("boot_entry.BPB_NumFATs: %d\n", boot_entry.BPB_NumFATs);
	printf("boot_entry.BPB_FATSz32: %d\n", boot_entry.BPB_FATSz32);

	// determine the count of sectors occupied by the root directory
	printf("boot_entry.BPB_RootEntCnt: %d.\n", boot_entry.BPB_RootEntCnt);
	printf("boot_entry.BPB_BytsPerSec: %d.\n", boot_entry.BPB_BytsPerSec);
	root_dir_sec = ((boot_entry.BPB_RootEntCnt * 32) + (boot_entry.BPB_BytsPerSec - 1)) / boot_entry.BPB_BytsPerSec;
	printf("Number of sectors, occupied by the root directory: %ld.\n", root_dir_sec);

	// determine first data sector of the volume
	if (boot_entry.BPB_FATSz16 != 0)
		FATSz = boot_entry.BPB_FATSz16;
	else
		FATSz = boot_entry.BPB_FATSz32;
	printf("boot_entry.BPB_FATSz16: %d.\n", boot_entry.BPB_FATSz16);
	printf("boot_entry.BPB_FATSz32: %d.\n", boot_entry.BPB_FATSz32);
	printf("FATSz now: %ld.\n", FATSz);
	first_data_sec = boot_entry.BPB_RsvdSecCnt + (boot_entry.BPB_NumFATs * FATSz) + root_dir_sec;
	printf("First data region sector: %ld.\n", first_data_sec);
	// given any valid data cluster number N, the sector number of the
	// first sector of that cluster (again relative to sector 0 of the FAT
	// volume) is computed as follows:
	//
	// first_cluster_sec = ((N - 2) * boot_entry.BPB_SecPerClus) + first_data_sec

#if 0
	// determine the count of sectors in the data region of the volume:
	if (boot_entry.BPB_TotSec16 != 0)
		tot_sec = boot_entry.BPB_TotSec16;
	else
		tot_sec = boot_entry.BPB_TotSec32;
	printf("Total count of sectors on the volume: %ld.\n", tot_sec);
	num_data_sec = tot_sec - (boot_entry.BPB_RsvdSecCnt + (boot_entry.BPB_NumFATs * FATSz) + root_dir_sec);
	printf("Count of sectors in the volume's data region: %ld.\n", num_data_sec);
	num_clusters = num_data_sec / sec_per_cluster;
	printf("Count of clusters in the volume: %ld.\n", num_clusters);
#endif

	//determine root directory sector number:
	printf("Revelation: boot_entry.BPB_RootClus: %u!\n", boot_entry.BPB_RootClus);
	root_dir_sec_num = first_data_sec + (boot_entry.BPB_RootClus - 2) * sec_per_cluster;
	buf = (unsigned char *)malloc(sizeof(unsigned char) * MAX_BUF);
	if (!buf) {
		perror("Memory allocation error");
		exit(1);
	}
	memset(buf, '\0', MAX_BUF);
	printf("We're going to read %ld bytes into the buf...\n", FATSz * boot_entry.BPB_NumFATs * boot_entry.BPB_BytsPerSec);
	printf("And MAX_BUF is: %d.\n", MAX_BUF);
	assert((FATSz * boot_entry.BPB_NumFATs * boot_entry.BPB_BytsPerSec) < MAX_BUF);
	read_sec(fd, root_dir_sec_num, sec_per_cluster, buf);

	//reading clusters sequentally
	// for (i = 0; i < 32; i++) {
    // i = 0;
	for (i = 0; i < (FATSz * boot_entry.BPB_NumFATs * boot_entry.BPB_BytsPerSec) / 32; i++) {
        char i2b[8];
	// do {
		memcpy(&dir_entry, buf + i * sizeof(t_dir_entry), sizeof(t_dir_entry));
        if (dir_entry.DIR_Name[0] == 0x00)
            break;
		printf("--------------------------------------------------\n");
		printf("> Entry #%d\n", i);
		printf("dir_entry.DIR_FileSize: %u.\n", dir_entry.DIR_FileSize);
		printf("dir_entry.DIR_FstClusHI: %u.\n", dir_entry.DIR_FstClusHI);
		printf("dir_entry.DIR_FstClusLO: %u.\n", dir_entry.DIR_FstClusLO);
		uint8_t *p;
		p = (uint8_t *)&dir_entry.DIR_WrtDate;
		printf("dir_entry.DIR_WrtDate: 0x%02x, 0x%02x.\n", *p, *(p + 1));
		printf("dir_entry.DIR_WrtDate: 0x%04x.\n", dir_entry.DIR_WrtDate);
		// printf("dir_entry.DIR_Attr: |%s|.\n", int2bin(dir_entry.DIR_Attr, i2b, 8));
		int2bin(dir_entry.DIR_Attr, i2b, 8);
		printf("dir_entry.DIR_Attr: |%s|.\n", i2b);
		if (dir_entry.DIR_Attr == ATTR_VOLUME_ID) {
			printf("ATTR_VOLUME_ID is set, it seems that it's a Volume ID itself...\n");
		}
		if (dir_entry.DIR_Attr & ATTR_VOLUME_ID) {
			printf("ATTR_VOLUME_ID is set.\n");
		}
		if (dir_entry.DIR_Attr & ATTR_READ_ONLY) {
			printf("ATTR_READ_ONLY bit is set.\n");
		}
		if (dir_entry.DIR_Attr & ATTR_HIDDEN) {
			printf("ATTR_HIDDEN bit is set.\n");
		}
		if (dir_entry.DIR_Attr & ATTR_SYSTEM) {
			printf("ATTR_SYSTEM bit is set.\n");
		}
		if (dir_entry.DIR_Attr & ATTR_DIRECTORY) {
			printf("ATTR_DIRECTORY bit is set.\n");
		}
		if (dir_entry.DIR_Attr & ATTR_ARCHIVE) {
			printf("ATTR_ARCHIVE bit is set.\n");
		}
		if (dir_entry.DIR_Attr & ATTR_LONG_NAME) {
			printf("ATTR_LONG_NAME bit is set.\n");
		}
		printf("%d: filename: %s, first char: %c.\n", i, dir_entry.DIR_Name, dir_entry.DIR_Name[0]);
        // i++;
	}
	// } while (dir_entry.DIR_Name[0] != 0x00);
	fclose(fd);

	return 0;
}

int read_sec(FILE *fd, int sec, int num, uint8_t *buf)
{
	int pos;
	int len;

	pos = fseek(fd, sec * bytes_per_sec, SEEK_SET);
	if (pos) {
		perror("Can't find position inside the file");
		exit(1);
	}
	if (num * bytes_per_sec > MAX_BUF) {
		perror("Buffer size too small");
		exit(1);
	}
	len = fread(buf, 1, bytes_per_sec * num, fd);
	if (len != bytes_per_sec * num) {
		perror("Error in reading sector\n");
		exit(1);
	}

	return len;
}

char *int2bin(int a, char *buffer, int buf_size)
{
    int i;
	buffer += (buf_size - 1);

	for (i = 7; i >= 0; i--) {
		*buffer-- = (a & 1) + '0';
		a >>= 1;
	}

	return buffer;
}
