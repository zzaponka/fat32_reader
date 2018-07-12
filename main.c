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
int bytes_per_sec; // bytes per sector

int read_sec(FILE *fd, int sec, int num, uint8_t *buf);

void get_filename(uint8_t *buf, uint8_t len);

char *int2bin(int a, char *buffer, int buf_size);

void list_info(t_boot_entry be);

long locate_fat(t_boot_entry be);

void list_long_entry(uint8_t *buf, int size);

void list_dir(t_dir_entry *buf, long offset);

int main(int argc, char **argv)
{
	FILE *fd;
	uint8_t *buf;
	uint16_t num;

	setlocale(LC_ALL, "C.UTF-8");

	fd = fopen("./disk.img", "r");
	if (!fd) {
		perror("Failed to open file");
		exit(1);
	}

	buf = (uint8_t *)malloc(sizeof(uint8_t) * SECT_SIZE);
	if (!buf) {
		perror("Memory allocation error");
		exit(1);
	}

	// TODO: refactor to read_sect()
	if ((num = fread(buf, 1, SECT_SIZE, fd)) != SECT_SIZE) {
		perror("Failed to read root sector");
		exit(1);
	}
	// TODO: create boot entry pointer instead of structure
	memcpy(&boot_entry, buf, sizeof(t_boot_entry));
	free(buf);

	list_info(boot_entry);
	printf("%s@%d: boot_entry.BPB_BytsPerSec = %d.\n", __func__, __LINE__, boot_entry.BPB_BytsPerSec);
	bytes_per_sec = boot_entry.BPB_BytsPerSec;
	printf("%s@%d: bytes_per_sec = %d.\n", __func__, __LINE__, bytes_per_sec);

	buf = (uint8_t *)malloc(sizeof(uint8_t) * MAX_BUF);
	if (!buf) {
		perror("Memory allocation error");
		exit(1);
	}
	memset(buf, '\0', MAX_BUF);
	// TODO: calculate FAT size into separate variable
	printf("We're going to read %d bytes into the buf...\n", boot_entry.BPB_FATSz32 * boot_entry.BPB_NumFATs * boot_entry.BPB_BytsPerSec);
	printf("And MAX_BUF is: %d.\n", MAX_BUF);
	assert((boot_entry.BPB_FATSz32 * boot_entry.BPB_NumFATs * boot_entry.BPB_BytsPerSec) < MAX_BUF);
	// read_sec(fd, root_dir_sec_num, sec_per_cluster, buf);
	read_sec(fd, locate_fat(boot_entry), boot_entry.BPB_FATSz32 * boot_entry.BPB_NumFATs * boot_entry.BPB_BytsPerSec, buf);

	//reading clusters sequentally
	printf("%s@%d: buf address, passed to list_dir(): %p.\n", __func__, __LINE__, buf);
	list_dir((t_dir_entry *)buf, 0);

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
	uint8_t flen;

	printf(">>> %s@%d: called.\n", __func__, __LINE__);
	flen = sizeof(uint8_t) * WCHAR_W * LDIR_ENT_FILENAME_LEN * size + 4;
	printf("%s@%d: flen = %d.\n", __func__, __LINE__, flen);


	get_filename(buf, size);

	printf(">>> %s@%d: returning.\n", __func__, __LINE__);

	return;
}

void get_filename(uint8_t *buf, uint8_t len)
{
	uint8_t i;
	uint8_t j;
	uint8_t k;
	t_long_dir_entry *lde;
	uint8_t filename[256];

	printf(">>> %s@%d: called.\n", __func__, __LINE__);
	memset(filename, 0x00, sizeof(filename));
	j = 0;
	for (i = 0; i < len; i++) {
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

	printf("%s%d: finally, filename is |%ls|.\n", __func__, __LINE__, (wchar_t *)filename);
	printf(">>> %s@%d: returning.\n", __func__, __LINE__);

	return;
}

// void list_dir(uint8_t *buf)
void list_dir(t_dir_entry *buf, long offset)
{
	int i;
	t_dir_entry *entry_buf = buf + offset;

	printf("%s@%d: buf address received: %p.\n", __func__, __LINE__, buf);
	printf("%s@%d: entry_buf address copied: %p.\n", __func__, __LINE__, entry_buf);
	i = 0;
	while (1) {
		printf("%s@%d: =======================\n", __func__, __LINE__);
		printf("%s@%d: Iteration #%d.\n", __func__, __LINE__, i);
		printf("%s@%d: FAT entry in hex:\n", __func__, __LINE__);
		uint8_t *bufp = (uint8_t *)entry_buf;
		int j;
		for (j = 0; j < 32; j++) {
			printf("%02X|", bufp[j]);
			if (!((j + 1) % 8))
				printf(" |");
			if (!((j + 1) % 16))
				printf("\n");
		}

		printf("entry_buf->DIR_Attr: %02x.\n", entry_buf->DIR_Attr);
		if (entry_buf->DIR_Name[0] == 0x00) {
			printf("%s@%d: no more entries, exiting...\n", __func__, __LINE__);
			break;
		};
		if (entry_buf->DIR_Attr == ATTR_VOLUME_ID) {
			printf("%s@%d: Only ATTR_VOLUME_ID is set, skipping...\n", __func__, __LINE__);
		}
		if (entry_buf->DIR_Attr == ATTR_LONG_NAME) {
			int lde_last_num;
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

			printf("%s@%d: ATTR_LONG_NAME bit is set, have to do something with that....\n", __func__, __LINE__);
			lde_p = (t_long_dir_entry *)entry_buf;
			printf("%s@%d: buf->LDIR_Ord = %X.\n", __func__, __LINE__, lde_p->LDIR_Ord);
			lde_last_num = lde_p->LDIR_Ord & ~LAST_LONG_ENTRY;
			printf("%s@%d: lde_last_num = %d.\n", __func__, __LINE__, lde_last_num);
			list_long_entry((uint8_t *)lde_p, lde_last_num);
			// skipping short entry
			// i++;
		}
		if (entry_buf->DIR_Attr == ATTR_DIRECTORY) {
			// TODO: move offset calculation to separate function
			printf("ATTR_DIRECTORY bit is set, have to pass the entry to directory listing fn().\n");
			uint8_t *sec_val;
			uint32_t fat_offset;
			// uint32_t this_fat_sec_num;
			uint32_t this_fat_end_offset;
			uint32_t first_sector_of_cluster;

			sec_val = (uint8_t *)malloc(sizeof(uint32_t));
			memcpy(sec_val, &entry_buf->DIR_FstClusLO, sizeof(uint16_t));
			memcpy(sec_val + 2, &entry_buf->DIR_FstClusHI, sizeof(uint16_t));
			fat_offset = (*sec_val) * 4;
			printf("%s@%d: Finally, fat_offset: %d.\n", __func__, __LINE__, fat_offset);
			// this_fat_sec_num = boot_entry.BPB_RsvdSecCnt + (fat_offset / boot_entry.BPB_BytsPerSec);
			// this_fat_sec_num = root_dir_sec_num + (fat_offset / boot_entry.BPB_BytsPerSec);
			this_fat_end_offset = fat_offset % boot_entry.BPB_BytsPerSec;
			first_sector_of_cluster = ((*sec_val) - 2) * boot_entry.BPB_SecPerClus;
			// printf("%s@%d: Finally, this_fat_sec_num: %d.\n", __func__, __LINE__, this_fat_sec_num);
			printf("%s@%d: Finally, this_fat_end_offset: %d.\n", __func__, __LINE__, this_fat_end_offset); 
			printf("%s@%d: Finally, first_sector_of_cluster: %d.\n", __func__, __LINE__, first_sector_of_cluster);
			printf("%s@%d: buf address, passed to list_dir(): %p.\n", __func__, __LINE__, buf);
			list_dir(buf, first_sector_of_cluster * 16);
		}
		printf("%s@%d: incrementing i...\n", __func__, __LINE__);
		i++;
		printf("%s@%d: shifting buf...\n", __func__, __LINE__);
		entry_buf++;
	}
}

void list_info(t_boot_entry be)
{
	printf("boot_entry.BS_OEMName: |%s|\n", boot_entry.BS_OEMName);
	printf("boot_entry.BPB_BytsPerSec: %d\n", boot_entry.BPB_BytsPerSec);
	printf("boot_entry.BPB_SecPerClus: %d\n", boot_entry.BPB_SecPerClus);
	printf("boot_entry.BPB_RsvdSecCnt: %d\n", boot_entry.BPB_RsvdSecCnt);
	printf("boot_entry.BPB_NumFATs: %d\n", boot_entry.BPB_NumFATs);
	printf("boot_entry.BPB_FATSz32: %d\n", boot_entry.BPB_FATSz32);
	printf("boot_entry.BPB_RootEntCnt: %d.\n", boot_entry.BPB_RootEntCnt);
	printf("boot_entry.BPB_BytsPerSec: %d.\n", boot_entry.BPB_BytsPerSec);
}

long locate_fat(t_boot_entry be)
{
	long root_dir_sec;	//count of sectors occupied by the root dir
	long FATSz;		//FAT size
	long first_data_sec;	//first data sector number
	long root_dir_sec_num;
	int sec_per_cluster;	//sectors per cluster

	printf(">>> %s@%d: called.\n", __func__, __LINE__);
	printf("%s@%d: be.BPB_SecPerClus = %d.\n", __func__, __LINE__, be.BPB_SecPerClus);
	sec_per_cluster = be.BPB_SecPerClus;
	printf("%s@%d: sec_per_cluster = %d.\n", __func__, __LINE__, sec_per_cluster);
	printf("%s@%d: be.BPB_RootEntCnt = %d.\n", __func__, __LINE__, be.BPB_RootEntCnt);
	printf("%s@%d: be.BPB_BytsPerSec = %d.\n", __func__, __LINE__, be.BPB_BytsPerSec);
	root_dir_sec = ((be.BPB_RootEntCnt * 32) + (be.BPB_BytsPerSec - 1)) / be.BPB_BytsPerSec;
	printf("Number of sectors, occupied by the root directory: %ld.\n", root_dir_sec);

	// determine first data sector of the volume
	if (boot_entry.BPB_FATSz16 != 0)
		FATSz = boot_entry.BPB_FATSz16;
	else
		FATSz = boot_entry.BPB_FATSz32;
	printf("boot_entry.BPB_FATSz16: %d.\n", boot_entry.BPB_FATSz16);
	printf("boot_entry.BPB_FATSz32: %d.\n", boot_entry.BPB_FATSz32);
	printf("FATSz now: %ld.\n", FATSz);
	printf("%s@%d: be.BPB_RsvdSecCnt = %d.\n", __func__, __LINE__, be.BPB_RsvdSecCnt);
	printf("%s@%d: be.BPB_NumFATs = %d.\n", __func__, __LINE__, be.BPB_NumFATs);
	first_data_sec = boot_entry.BPB_RsvdSecCnt + (boot_entry.BPB_NumFATs * FATSz) + root_dir_sec;
	printf("First data region sector: %ld.\n", first_data_sec);

	//determine root directory sector number:
	printf("Revelation: boot_entry.BPB_RootClus: %u!\n", boot_entry.BPB_RootClus);
	// root_dir_sec_num is equal to first_data_sec
	root_dir_sec_num = first_data_sec + (boot_entry.BPB_RootClus - 2) * sec_per_cluster;
	printf("And root_dir_sec_num is %ld.\n", root_dir_sec_num);
	printf("<<< %s@%d: exit.\n", __func__, __LINE__);

	return root_dir_sec_num;
}
