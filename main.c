// #include <fcntl.h>
#include <assert.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#include "fat32.h"

t_boot_entry boot_entry;
int bytes_per_sec;	//bytes per sector

int read_sec(FILE *fd, int sec, int num, uint8_t *buf);

char *int2bin(int a, char *buffer, int buf_size);

void list_long_entry(uint8_t *buf, int size);

// void list_dir(uint8_t *buf);
void list_dir(t_dir_entry *buf);

int main(int argc, char **argv)
{
	FILE *fd;
	int num;
	unsigned char *buf;
	int sec_per_cluster;	//sectors per cluster
	long root_dir_sec;	//count of sectors occupied by the root dir
	long FATSz;		//FAT size
	long first_data_sec;	//first data sector number
	long root_dir_sec_num;
	// long tot_sec;		//total count of sectors on the volume
	// long num_data_sec;	//count of sectors in the volume's data region
	// long num_clusters;	//count of clusters in the volume
	// int i;
	// int fat_offset;
	// int this_fat_sec_num;
	// int this_fat_ent_offset;
	// uint32_t fat32_cluster_entry_val;
	// t_dir_entry dir_entry;
	// t_long_dir_entry long_dir_entry;

	setlocale(LC_ALL, "C.UTF-8");
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
	// root_dir_sec is always 0 on FAT32 volumes
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
	// root_dir_sec_num is equal to first_data_sec
	root_dir_sec_num = first_data_sec + (boot_entry.BPB_RootClus - 2) * sec_per_cluster;
	printf("And root_dir_sec_num is %ld.\n", root_dir_sec_num);
	buf = (uint8_t *)malloc(sizeof(uint8_t) * MAX_BUF);
	if (!buf) {
		perror("Memory allocation error");
		exit(1);
	}
	memset(buf, '\0', MAX_BUF);
	printf("We're going to read %ld bytes into the buf...\n", FATSz * boot_entry.BPB_NumFATs * boot_entry.BPB_BytsPerSec);
	printf("And MAX_BUF is: %d.\n", MAX_BUF);
	assert((FATSz * boot_entry.BPB_NumFATs * boot_entry.BPB_BytsPerSec) < MAX_BUF);
	// read_sec(fd, root_dir_sec_num, sec_per_cluster, buf);
	read_sec(fd, root_dir_sec_num, FATSz * boot_entry.BPB_NumFATs * boot_entry.BPB_BytsPerSec, buf);

	//reading clusters sequentally
	list_dir((t_dir_entry *)buf);

	fclose(fd);

	return 0;
}

int read_sec(FILE *fd, int sec, int num, uint8_t *buf)
{
	int pos;
	int len;

	pos = fseek(fd, sec * bytes_per_sec , SEEK_SET);
	if (pos) {
		perror("Can't find position inside the file");
		exit(1);
	}
	len = fread(buf, 1, num, fd);
	if (len != num) {
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

void list_long_entry(uint8_t *buf, int size)
{
	char *filename;
	uint8_t flen;
	int i;
	int j;
	int k;
	t_long_dir_entry *lde;

	flen = sizeof(char) * WCHAR_W * LDIR_ENT_FILENAME_LEN * size + 4;
	printf("%s@%d: flen = %d.\n", __func__, __LINE__, flen);
	filename = (char *)malloc(flen);
	memset(filename, '\0', flen);

	printf("Initially, filename in hex:\n");
	for (k = 0; k < flen; k++)
		printf("0x%02X ", filename[k]);
	printf("\n");
	j = 0;
	for (i = 0; i < size; i++) {
		lde = (t_long_dir_entry *)(buf + i * sizeof(t_long_dir_entry));
		printf("sizeof(lde->LDIR_Name1): %ld.\n", sizeof(lde->LDIR_Name1));
		printf("lde->LDIR_Name1 in hex:\n");
		for (k = 0; k < sizeof(lde->LDIR_Name1); k++)
			printf("0x%02X ", lde->LDIR_Name1[k]);
		printf("\n");
		k = 0;
		while ((k < sizeof(lde->LDIR_Name1)) && (lde->LDIR_Name1[k] != 0xFF) && (lde->LDIR_Name1[k + 1] != 0xFF)) {
			filename[j] = lde->LDIR_Name1[k];
			filename[j + 1] = lde->LDIR_Name1[k + 1];
			filename[j + 2] = 0x00;
			filename[j + 3] = 0x00;
			j += 4;
			k += 2;
		}
		printf("sizeof(lde->LDIR_Name2): %ld.\n", sizeof(lde->LDIR_Name2));
		printf("lde->LDIR_Name2 in hex:\n");
		for (k = 0; k < sizeof(lde->LDIR_Name2); k++)
			printf("0x%02X ", lde->LDIR_Name2[k]);
		printf("\n");
		k = 0;
		while ((k < sizeof(lde->LDIR_Name2)) && (lde->LDIR_Name2[k] != 0xFF) && (lde->LDIR_Name2[k + 1] != 0xFF)) {
			filename[j] = lde->LDIR_Name2[k];
			filename[j + 1] = lde->LDIR_Name2[k + 1];
			filename[j + 2] = 0x00;
			filename[j + 3] = 0x00;
			j += 4;
			k += 2;
		}
		printf("sizeof(lde->LDIR_Name3): %ld.\n", sizeof(lde->LDIR_Name3));
		printf("lde->LDIR_Name3 in hex:\n");
		for (k = 0; k < sizeof(lde->LDIR_Name3); k++)
			printf("0x%02X ", lde->LDIR_Name3[k]);
		printf("\n");
		k = 0;
		while ((k < sizeof(lde->LDIR_Name3)) && (lde->LDIR_Name3[k] != 0xFF) && (lde->LDIR_Name3[k + 1] != 0xFF)) {
			filename[j] = lde->LDIR_Name3[k];
			filename[j + 1] = lde->LDIR_Name3[k + 1];
			filename[j + 2] = 0x00;
			filename[j + 3] = 0x00;
			j += 4;
			k += 2;
		}
	}
	printf("After all, filename in hex:\n");
	for (k = 0; k < flen; k++)
		printf("%d|0x%02X ", k, filename[k]);
	printf("\n");
	printf("After all, filename is: |%ls|.\n", (wchar_t *)filename);
#if 0
	// printf("Last long entry number: %d.\n", lde_last_num);
	printf("lde->LDIR_Ord: 0x%01X.\n", lde->LDIR_Ord);
	printf("lde->LDIR_Name1 (in hex): ");
	for (j = 0; j < 10; j++) {
		if (lde->LDIR_Name1[j] == 0xFF)
			break;
		printf("%02X", lde->LDIR_Name1[j]);
	}
	printf("\n");
	printf("lde->LDIR_Name1 (as chars): |");
	for (j = 0; j < 10; j++) {
		if (lde->LDIR_Name1[j] == 0xFF)
			break;
		printf("%lc", lde->LDIR_Name1[j]);
	}
	printf("|\n");
	i2b[8] = '\0';
	int2bin(lde->LDIR_Attr, i2b, 8);
	printf("lde->LDIR_Attr: |%s|.\n", i2b);
	printf("lde->LDIR_Type: 0x%01X.\n", lde->LDIR_Type);
	printf("lde->LDIR_Chksum: 0x%01X.\n", lde->LDIR_Chksum);
	// printf("lde->LDIR_Name2: |%s|.\n", lde->LDIR_Name2);
	printf("lde->LDIR_Name2 (in hex): ");
	for (j = 0; j < 10; j++) {
		if (lde->LDIR_Name2[j] == 0xFF)
			break;
		printf("%02X", lde->LDIR_Name2[j]);
	}
	printf("\n");
	printf("lde->LDIR_Name2 (as chars): |");
	for (j = 0; j < 10; j++) {
		if (lde->LDIR_Name2[j] == 0xFF)
			break;
		printf("%lc", lde->LDIR_Name2[j]);
	}
	printf("|\n");
	printf("lde->LDIR_FstClusLO: 0x%02X.\n", lde->LDIR_FstClusLO);
	// printf("lde->LDIR_Name3: |%s|.\n", lde->LDIR_Name3);
	printf("lde->LDIR_Name3 (in hex): ");
	for (j = 0; j < 10; j++) {
		if (lde->LDIR_Name3[j] == 0xFF)
			break;
		printf("%02X", lde->LDIR_Name3[j]);
	}
	printf("\n");
	printf("lde->LDIR_Name3 (as chars): |");
	for (j = 0; j < 10; j++) {
		if (lde->LDIR_Name3[j] == 0xFF)
			break;
		printf("%lc", lde->LDIR_Name3[j]);
	}
	printf("|\n");
#endif

	return;
}

// void list_dir(uint8_t *buf)
void list_dir(t_dir_entry *buf)
{
	int i;

	i = 0;
	while (1) {
		// uint8_t *dir_entry_p;

		// memcpy(&dir_entry, buf + i * sizeof(t_dir_entry), sizeof(t_dir_entry));
		printf("==================================================\n");
		printf("Iteration #%d.\n", i);
		printf("buf->DIR_Attr: %02x.\n", buf->DIR_Attr);
		if (buf->DIR_Attr == ATTR_VOLUME_ID) {
			printf("Only ATTR_VOLUME_ID is set, skipping...\n");
			// continue;
		}
		if (buf->DIR_Name[0] == 0x00) {
			printf("%s@%d: #1: alas, we have missed!\n", __func__, __LINE__);
			break;
		};
		if (buf->DIR_Attr == ATTR_LONG_NAME) {
			int lde_last_num;
			// uint8_t *lde_buf;
			t_long_dir_entry *lde_p;
			// TODO:
			// in order to find FAT entry for given cluster
			// number N (which is contained inside DIR_FstClusHI
			// and DIR_FstClusLO) we need:
			// - calculate:
			// FATOffset = N * 4
			// ThisFATSecNum = BPB_RsvdSecCnt + (FATOffset / BPB_BytsPerSec)
			// ThisFATEntOffset FATOffset % BPB_BytsPerSec
			// - pass these values to list_dir() recursively

			// i++; // skipping
			printf("ATTR_LONG_NAME bit is set, have to do something with that....\n");
			// memcpy(&long_dir_entry, buf + i * sizeof(t_long_dir_entry), sizeof(t_long_dir_entry));
			// Zzz
			lde_p = (t_long_dir_entry *)buf;
			printf("buf->LDIR_Ord = %X.\n", lde_p->LDIR_Ord);
			lde_last_num = lde_p->LDIR_Ord & ~LAST_LONG_ENTRY;
			printf("lde_last_num = %d.\n", lde_last_num);
			// lde_buf = (uint8_t *)malloc(sizeof(uint8_t) * lde_last_num * 32);
			// memcpy(lde_buf, &long_dir_entry, sizeof(t_long_dir_entry) * lde_last_num);
			printf("%s@%d: trying to print long filename entry, #0.\n", __func__, __LINE__);
			list_long_entry((uint8_t *)lde_p, lde_last_num);
			printf("%s@%d: trying to print long filename entry, #1.\n", __func__, __LINE__);
		}
		if (buf->DIR_Attr == ATTR_DIRECTORY) {
			printf("ATTR_DIRECTORY bit is set, have to pass the entry to directory listing fn().\n");
			uint8_t *sec_val;
			uint32_t fat_offset;
			uint32_t this_fat_sec_num;
			uint32_t this_fat_end_offset;

			sec_val = (uint8_t *)malloc(sizeof(uint32_t));
			memcpy(sec_val, &buf->DIR_FstClusLO, sizeof(uint16_t));
			memcpy(sec_val + 2, &buf->DIR_FstClusHI, sizeof(uint16_t));
			fat_offset = (*sec_val) * 4;
			printf("Finally, fat_offset: %d.\n", fat_offset);
			this_fat_sec_num = boot_entry.BPB_RsvdSecCnt + (fat_offset / boot_entry.BPB_BytsPerSec);
			this_fat_end_offset = fat_offset % boot_entry.BPB_BytsPerSec;
			printf("Finally, this_fat_sec_num: %d.\n", this_fat_sec_num);
			printf("Finally, this_fat_end_offset: %d.\n", this_fat_end_offset);

			list_dir(buf + this_fat_end_offset);
		}
		printf("%s@%d: incrementing i...\n", __func__, __LINE__);
		i++;
		printf("%s@%d: shifting buf...\n", __func__, __LINE__);
		// buf = buf + sizeof(t_dir_entry);
		buf++;
		// Zzz
	}
#if 0
	for (i = 0; i < (FATSz * boot_entry.BPB_NumFATs * boot_entry.BPB_BytsPerSec) / 32; i++) {
		char i2b[9];
		memcpy(&dir_entry, buf + i * sizeof(t_dir_entry), sizeof(t_dir_entry));
		if (dir_entry.DIR_Name[0] == 0x00) {
			printf("%s@%d: #1: alas, we have missed!\n", __func__, __LINE__);
			break;
		}
		printf("--------------------------------------------------\n");
		// printf("> Entry #%d, hex: 0x%08X.\n", i, (uint32_t )dir_entry);
		printf("> Entry #%d.\n", i);
		dir_entry_p = buf + i * sizeof(t_dir_entry);
		printf("As hex: 0x%08X.\n", (unsigned int )*dir_entry_p);
		printf("dir_entry.DIR_FileSize: %u.\n", dir_entry.DIR_FileSize);
		printf("dir_entry.DIR_FstClusHI: %u.\n", dir_entry.DIR_FstClusHI);
		printf("dir_entry.DIR_FstClusLO: %u.\n", dir_entry.DIR_FstClusLO);
		uint8_t *p;
		p = (uint8_t *)&dir_entry.DIR_WrtDate;
		printf("dir_entry.DIR_WrtDate: 0x%02x, 0x%02x.\n", *p, *(p + 1));
		printf("dir_entry.DIR_WrtDate: 0x%04x.\n", dir_entry.DIR_WrtDate);
		i2b[8] = '\0';
		int2bin(dir_entry.DIR_Attr, i2b, 8);
		printf("dir_entry.DIR_Attr: |%s|.\n", i2b);
		if (dir_entry.DIR_Attr == ATTR_LONG_NAME) {
			int lde_last_num;
			uint8_t *lde_buf;
			// TODO:
			// in order to find FAT entry for given cluster
			// number N (which is contained inside DIR_FstClusHI
			// and DIR_FstClusLO) we need:
			// - calculate:
			// FATOffset = N * 4
			// ThisFATSecNum = BPB_RsvdSecCnt + (FATOffset / BPB_BytsPerSec)
			// ThisFATEntOffset FATOffset % BPB_BytsPerSec
			// - pass these values to list_dir() recursively

			// i++; // skipping
			printf("ATTR_LONG_NAME bit is set, have to do something with that....\n");
			memcpy(&long_dir_entry, buf + i * sizeof(t_long_dir_entry), sizeof(t_long_dir_entry));
			// Zzz
			printf("long_dir_entry.LDIR_Ord = %X.\n", long_dir_entry.LDIR_Ord);
			lde_last_num = long_dir_entry.LDIR_Ord & ~LAST_LONG_ENTRY;
			printf("lde_last_num = %d.\n", lde_last_num);
			lde_buf = (uint8_t *)malloc(sizeof(uint8_t) * lde_last_num * 32);
			memcpy(lde_buf, &long_dir_entry, sizeof(t_long_dir_entry) * lde_last_num);
			printf("%s@%d: trying to print long filename entry, #0.\n", __func__, __LINE__);
			list_long_entry(lde_buf, lde_last_num);
			printf("%s@%d: trying to print long filename entry, #1.\n", __func__, __LINE__);
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
		if (dir_entry.DIR_Attr & ATTR_ARCHIVE) {
			printf("ATTR_ARCHIVE bit is set.\n");
		}
		printf("%d: filename: %s, first char: %c.\n", i, dir_entry.DIR_Name, dir_entry.DIR_Name[0]);
	}
#endif
	printf("%s@%d: called.\n", __func__, __LINE__);
}
