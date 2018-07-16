#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#include "debug.h"
#include "fat32.h"

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

	if (!(buf = (uint8_t *)malloc(SECT_SIZE * 8))) {
		perror("Memory allocation error");
		exit(1);
	}

	memset(buf, '\0', SECT_SIZE * 8);
	if ((num = fread(buf, SECT_SIZE, 1, fd)) != 1) {
		perror("Failed to read root sector");
		exit(1);
	}
	memcpy(&boot_entry, buf, sizeof(t_boot_entry));

#if DEBUG
	list_info(boot_entry);
#endif

	bytes_per_sec = boot_entry.BPB_BytsPerSec;

	first_data_sec = boot_entry.BPB_RsvdSecCnt +
			(boot_entry.BPB_NumFATs * boot_entry.BPB_FATSz32);
	read_cluster(fd, buf, first_data_sec);

	list_dir(buf, 0, first_data_sec, lev, boot_entry.BPB_RootClus);

	free(buf);
	fclose(fd);

	return 0;
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
	printf("boot_entry.BPB_RootClus: %d.\n", boot_entry.BPB_RootClus);
	printf("boot_entry.BPB_BytsPerSec: %d.\n", boot_entry.BPB_BytsPerSec);
	printf("boot_entry.BPB_Media: %d (0x%x).\n\n", boot_entry.BPB_Media, boot_entry.BPB_Media);
}

int read_cluster(FILE *fd, uint8_t *buf, int sec)
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

void list_dir(uint8_t *recv_buf, int offset, int sec, int level, uint32_t cluster)
{
	uint8_t *buf;
	t_dir_entry *de_p;
	int local_offset = offset;
	uint32_t local_cluster;
	int num_entries_to_iterate;

	buf = (uint8_t *)malloc(SECT_SIZE * 8);
	memcpy(buf, recv_buf, SECT_SIZE * 8);

	DEBUG_LOG("=========================================================");
	DEBUG_LOG("received cluster: %u (0x%x).", cluster, cluster);
	local_cluster = cluster;
	DEBUG_LOG("assigned local cluster: %u.", local_cluster);
	while (1) {
		int i;

		// There are 128 FAT entries per 8-sector cluster
		num_entries_to_iterate = 128 - local_offset;
		for (i = 0; i < num_entries_to_iterate; i++) {
			DEBUG_LOG("===== Level [%04d] = Iteration [%04d], stop before [%04d] =====", level, i, num_entries_to_iterate);
			de_p = ((t_dir_entry *)buf) + i + local_offset;

#ifdef DEBUG
			int j;
			uint8_t *bufp = buf + (i + local_offset) * 32;
			DEBUG_LOG("FAT entry in hex:");
			for (j = 0; j < 32; j++) {
				printf("%02X|", bufp[j]);
				if (!((j + 1) % 8))
					printf(" |");
				if (!((j + 1) % 16))
					printf("\n");
			}
#endif

			DEBUG_LOG("de_p->DIR_Name[0] = %#02X.\n", de_p->DIR_Name[0]);

			if (de_p->DIR_Name[0] == 0x00) {
				DEBUG_LOG("no more entries, exiting...");
				break;
			} else if (de_p->DIR_Name[0] == 0xE5) {
				DEBUG_LOG("current entry is free, skipping...");
				continue;
			} else {
				DEBUG_LOG("[ATTN] directory 1st char unhandled case!");
			}

			if (de_p->DIR_Attr == ATTR_VOLUME_ID) {
				DEBUG_LOG("Only ATTR_VOLUME_ID is set, skipping...");
				continue;
			} else if (de_p->DIR_Attr == ATTR_LONG_NAME) {
				int long_entries_num;
				t_long_dir_entry *lde_p = (t_long_dir_entry *)de_p;

				DEBUG_LOG("ATTR_LONG_NAME bit is set.");

				DEBUG_LOG("lde_p->LDIR_Ord = %X.", lde_p->LDIR_Ord);

				if ((lde_p->LDIR_Ord & LAST_LONG_ENTRY) == LAST_LONG_ENTRY) {
					DEBUG_LOG("Last record of the long name entry sequence.");
					long_entries_num = lde_p->LDIR_Ord & ~LAST_LONG_ENTRY;
					DEBUG_LOG("long_entries_num = %d.", long_entries_num);
					list_long_entry((uint8_t *)lde_p, long_entries_num, level);
				} else {
					DEBUG_LOG("Not last record of the long name entry sequence.");
				}
			} else if (de_p->DIR_Attr == ATTR_DIRECTORY) {
				uint32_t first_sector_of_cluster;
				uint8_t *buf;
				uint8_t *first_cluster;
				uint32_t clus_fat_entry_val;

				DEBUG_LOG("ATTR_DIRECTORY bit is set.");

				first_cluster = (uint8_t *)malloc(sizeof(uint32_t));
				memset(first_cluster, '\0', sizeof(uint32_t));
				memcpy(first_cluster, &de_p->DIR_FstClusLO, sizeof(uint16_t));
				memcpy(first_cluster + 2, &de_p->DIR_FstClusHI, sizeof(uint16_t));
				DEBUG_LOG("Directory's entry first_cluster: %u (0x%x).", *(uint32_t *)first_cluster, *(uint32_t *)first_cluster);

				clus_fat_entry_val = get_clus_fat_entry_val((uint32_t *)first_cluster);
				DEBUG_LOG("clus_fat_entry_val: %d (0x%x).", clus_fat_entry_val, clus_fat_entry_val);
				if (clus_fat_entry_val >= 0x0FFFFFF8) {
					DEBUG_LOG("Last cluster in the chain");
				} else {
					DEBUG_LOG("Not last cluster in the chain");
				}

				first_sector_of_cluster =
						((*(uint32_t *)first_cluster) - 2) *
						boot_entry.BPB_SecPerClus;
				DEBUG_LOG("first_sector_of_cluster: %u.", first_sector_of_cluster);

				buf = (uint8_t *)malloc(bytes_per_sec * 8);
				read_cluster(fd, buf, first_data_sec + first_sector_of_cluster);
				list_dir(buf, 2, first_data_sec + first_sector_of_cluster, level + 1, *(uint32_t *)first_cluster);

				free(buf);
				free(first_cluster);
			} else if (de_p->DIR_Attr == ATTR_ARCHIVE) {
				DEBUG_LOG("Archive attr. is set.");
				continue;
			} else {
				DEBUG_LOG("directory attributes unhandled case.");
			}
		}

		if (local_offset) {
			DEBUG_LOG("Clearing offset to skip '.' and '..' entries.");
			local_offset -= offset;
		}

		DEBUG_LOG("Current cluster has been read.");

		// Query what value in FAT is stored for 'local_cluster' value
		uint32_t clus_fat_entry_val = get_clus_fat_entry_val(&local_cluster);
		if (clus_fat_entry_val >= 0x0FFFFFF8) {
			DEBUG_LOG("Last cluster in the chain.");
			break;
		} else {
			uint32_t first_sector_of_cluster;
			local_cluster = clus_fat_entry_val;

			first_sector_of_cluster = (clus_fat_entry_val - 2) *
				boot_entry.BPB_SecPerClus;
			read_cluster(fd, buf, first_data_sec +
					first_sector_of_cluster);
		}
	}
	free(buf);
}

void list_long_entry(uint8_t *buf, int size, int level)
{
	int8_t i;
	uint8_t j;
	uint8_t k;
	t_long_dir_entry *lde;
	uint8_t filename[256];

	memset(filename, '\0', sizeof(filename));
	j = 0;
	for (i = (size - 1); i >= 0; i--) {
		DEBUG_LOG("Records in long entry: %d, iteration: %d.", size, i);
		lde = (t_long_dir_entry *)(buf + i * sizeof(t_long_dir_entry));

		DEBUG_LOG("sizeof(lde->LDIR_Name1): %ld.", sizeof(lde->LDIR_Name1));
		k = 0;
		while ((k < sizeof(lde->LDIR_Name1)) && (lde->LDIR_Name1[k] != 0xFF) && (lde->LDIR_Name1[k + 1] != 0xFF)) {
			filename[j] = lde->LDIR_Name1[k];
			filename[j + 1] = lde->LDIR_Name1[k + 1];
			filename[j + 2] = 0x00;
			filename[j + 3] = 0x00;
			j += 4;
			k += 2;
		}
		DEBUG_LOG("sizeof(lde->LDIR_Name2): %ld.", sizeof(lde->LDIR_Name2));
		k = 0;
		while ((k < sizeof(lde->LDIR_Name2)) && (lde->LDIR_Name2[k] != 0xFF) && (lde->LDIR_Name2[k + 1] != 0xFF)) {
			filename[j] = lde->LDIR_Name2[k];
			filename[j + 1] = lde->LDIR_Name2[k + 1];
			filename[j + 2] = 0x00;
			filename[j + 3] = 0x00;
			j += 4;
			k += 2;
		}
		DEBUG_LOG("sizeof(lde->LDIR_Name3): %ld.", sizeof(lde->LDIR_Name3));
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

	if (level > 2) {
		for (i = 0; i < level - 2; i++)
			printf("\t");
	}
	if (level > 1)
		printf("+-------");
	printf("%ls\n", (wchar_t *)filename);

	return;
}

uint32_t get_clus_fat_entry_val(uint32_t *clus_val)
{
	uint8_t *buf;
	int fat_entry_sec_num;
	int fat_entry_offset;
	int clus_fat_entry_val;

	DEBUG_LOG("clus_val received = %u (0x%x).", *clus_val, *clus_val);

	buf = (uint8_t *)malloc(SECT_SIZE * 8);
	memset (buf, '\0', SECT_SIZE);

	fat_entry_sec_num = boot_entry.BPB_RsvdSecCnt +
			(*clus_val * 4) / boot_entry.BPB_BytsPerSec;
	DEBUG_LOG("fat_entry_sec_num = %u.", fat_entry_sec_num);

	fat_entry_offset = (*clus_val * 4) % boot_entry.BPB_BytsPerSec;
	DEBUG_LOG("fat_entry_offset = %u.", fat_entry_offset);

	read_cluster(fd, buf, fat_entry_sec_num);

	clus_fat_entry_val = (*((uint32_t *)&buf[fat_entry_offset])) & 0x0FFFFFFF;
	DEBUG_LOG("clus_fat_entry_val = %08x.", clus_fat_entry_val);

	free(buf);

	return clus_fat_entry_val;
}
