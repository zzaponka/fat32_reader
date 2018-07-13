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
FILE *fd;
long first_data_sec;	//first data sector number

int read_sec(FILE *fd, uint8_t *buf, int sec);

void get_filename(uint8_t *buf, uint8_t len);

char *int2bin(int a, char *buffer, int buf_size);

void list_info(t_boot_entry be);

long locate_fat(t_boot_entry be);

void list_long_entry(uint8_t *buf, int size);

void list_dir(uint8_t *recv_buf, int offset, int sec, int level);

// TODO:
// we can't get around reading FAT itself (not data region):
// - first data sector:
// first_data_sector = BPB_RsvdSecCnt + (BPB_NumFATs * BPB_FATSz32)
// - given any valid data cluster number N (first is 2), the sector number of
// the first sector of that cluster (again relative to the sector 0 of the FAT
// volume):
// first_sector_of_cluster = ((N - 2) * BPB_SecPerClus) + first_data_sector
// - number of data sectors:
// data_sec = BPB_TotSec32 - (BPB_ResvdSecCnt + (BPB_NumFATs * BPB_FATSz32))
// - count of clusters = data_sec / BPB_SecPerClus

int main(int argc, char **argv)
{
	uint8_t *buf;
	uint16_t num;
	int lev = 1;

	setlocale(LC_ALL, "C.UTF-8");

	fd = fopen("./disk.img", "r");
	if (!fd) {
		perror("Failed to open file");
		exit(1);
	}

	buf = (uint8_t *)malloc(SECT_SIZE * 8);
	if (!buf) {
		perror("Memory allocation error");
		exit(1);
	}

	memset(buf, '\0', SECT_SIZE);
	if ((num = fread(buf, SECT_SIZE, 1, fd)) != 1) {
		perror("Failed to read root sector");
		exit(1);
	}
	memcpy(&boot_entry, buf, sizeof(t_boot_entry));

	list_info(boot_entry);
	printf("%s@%d: buf->BPB_BytsPerSec = %d.\n", __func__, __LINE__, ((t_boot_entry *)buf)->BPB_BytsPerSec);
	bytes_per_sec = ((t_boot_entry *)buf)->BPB_BytsPerSec;
	printf("%s@%d: bytes_per_sec = %d.\n", __func__, __LINE__, bytes_per_sec);

	first_data_sec = boot_entry.BPB_RsvdSecCnt + (boot_entry.BPB_NumFATs * boot_entry.BPB_FATSz32);
	printf("First data region sector: %ld.\n", first_data_sec);
	read_sec(fd, buf, first_data_sec);

	list_dir(buf, 0, first_data_sec, lev);

	free(buf);
	fclose(fd);

	return 0;
}

int read_sec(FILE *fd, uint8_t *buf, int sec)
{
	int pos;
	int len;

	if ((pos = fseek(fd, sec * bytes_per_sec , SEEK_SET))) {
		perror("Can't find position inside the file");
		exit(1);
	}
	if ((len = fread(buf, bytes_per_sec, 8, fd)) != 8) {
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
	int8_t i;
	uint8_t j;
	uint8_t k;
	t_long_dir_entry *lde;
	uint8_t filename[256];

	printf(">>> %s@%d: called.\n", __func__, __LINE__);
	memset(filename, 0x00, sizeof(filename));
	j = 0;
	for (i = (len - 1); i >= 0; i--) {
		printf(">>> %s@%d: getting filename with len = %d, i = %d.\n", __func__, __LINE__, len, i);
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

void list_dir(uint8_t *recv_buf, int offset, int sec, int level)
{
	// We have to preserve received buffer to continue at the
	// current level of file tree hierarchy when returning after
	// reading underlying directory entry
	uint8_t *buf;
	t_dir_entry *de_p;

	buf = (uint8_t *)malloc(SECT_SIZE * 8);
	memcpy(buf, recv_buf, SECT_SIZE * 8);
	while (1) {
		int i;

		// There are 128 FAT entries per 8-sector cluster
		for (i = 0; i < 128; i++) {
			int j;
			// t_dir_entry *de_p = ((t_dir_entry *)buf) + (offset + i);
			de_p = ((t_dir_entry *)buf) + i + offset;
			uint8_t *bufp = buf + (i + offset) * 32;

			printf("%s@%d: [%04d]=======================\n", __func__, __LINE__, level);
			printf("%s@%d: Iteration #%d.\n", __func__, __LINE__, i);
			printf("%s@%d: FAT entry in hex:\n", __func__, __LINE__);

			for (j = 0; j < 32; j++) {
				printf("%02X|", bufp[j]);
				if (!((j + 1) % 8))
					printf(" |");
				if (!((j + 1) % 16))
					printf("\n");
			}

			if (de_p->DIR_Name[0] == 0x00) {
				printf("%s@%d: no more entries, exiting...\n", __func__, __LINE__);
				break;
			};

			if (de_p->DIR_Attr == ATTR_VOLUME_ID) {
				printf("%s@%d: Only ATTR_VOLUME_ID is set, skipping...\n", __func__, __LINE__);
				continue;
			}

			if (de_p->DIR_Attr == ATTR_LONG_NAME) {
				int long_entries_num;
				t_long_dir_entry *lde_p = (t_long_dir_entry *)de_p;

				printf("%s@%d: ATTR_LONG_NAME bit is set, have to do something with that....\n", __func__, __LINE__);
				printf("%s@%d: lde_p->LDIR_Ord = %X.\n", __func__, __LINE__, lde_p->LDIR_Ord);
				if ((lde_p->LDIR_Ord & LAST_LONG_ENTRY) == LAST_LONG_ENTRY) {
					printf(">>> %s@%d: ATTN! Last member of the long name entry sequence! <<<\n", __func__, __LINE__);
					long_entries_num = lde_p->LDIR_Ord & ~LAST_LONG_ENTRY;
					printf("%s@%d: long_entries_num = %d.\n", __func__, __LINE__, long_entries_num);
					list_long_entry((uint8_t *)lde_p, long_entries_num);
				} else {
					printf(">>> %s@%d: Okay, it's not last member of the long name entry sequence, skipping...<<<\n", __func__, __LINE__);
				}
			}

			if (de_p->DIR_Attr == ATTR_DIRECTORY) {
				printf("ATTR_DIRECTORY bit is set, have to pass the entry to directory listing fn().\n");
				uint32_t *sec_val;
				uint32_t first_sector_of_cluster;
				uint8_t *buf;

				sec_val = malloc(sizeof(uint32_t));
				memset(sec_val, '\0', sizeof(uint32_t));
				memcpy(sec_val, &de_p->DIR_FstClusLO, sizeof(uint16_t));
				memcpy(sec_val + 2, &de_p->DIR_FstClusHI, sizeof(uint16_t));
				printf("%s@%d: Finally, sec_val: %d (0x%x).\n", __func__, __LINE__, *(uint32_t *)sec_val, *(uint32_t *)sec_val);
				first_sector_of_cluster = ((*sec_val) - 2) * boot_entry.BPB_SecPerClus;
				printf("%s@%d: Finally, first_sector_of_cluster: %d.\n", __func__, __LINE__, first_sector_of_cluster);
				printf("%s@%d: buf address, passed to list_dir(): %p.\n", __func__, __LINE__, buf);
				buf = (uint8_t *)malloc(bytes_per_sec * 8);
				read_sec(fd, buf, first_data_sec + first_sector_of_cluster);
				list_dir(buf, 2, first_data_sec + first_sector_of_cluster, level + 1);
				free(buf);
			}
		}
		if (de_p->DIR_Name[0] == 0x00) {
			printf("%s@%d: REALLY, REALLY, REALLY no more entries, exiting...\n", __func__, __LINE__);
			break;
		};
		// read next cluster
		read_sec(fd, buf, sec + 8);
	}
	free(buf);
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
