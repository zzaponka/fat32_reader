#ifndef __FAT32_H__
#define __FAT32_H__

#define SECT_SIZE 512
#define WCHAR_W 2
#define LDIR_ENT_FILENAME_LEN 13

#define ATTR_READ_ONLY		0x01
#define ATTR_HIDDEN		0x02
#define ATTR_SYSTEM		0x04
#define ATTR_VOLUME_ID		0x08
#define ATTR_DIRECTORY		0x10
#define ATTR_ARCHIVE		0x20
#define LAST_LONG_ENTRY		0x40
#define ATTR_LONG_NAME		( ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID )
#define ATTR_LONG_NAME_MASK	( ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID | ATTR_DIRECTORY | ATTR_ARCHIVE )

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

typedef struct long_dir_entry {
	uint8_t LDIR_Ord;
	uint8_t LDIR_Name1[10];
	uint8_t LDIR_Attr;
	uint8_t LDIR_Type;
	uint8_t LDIR_Chksum;
	uint8_t LDIR_Name2[12];
	uint16_t LDIR_FstClusLO;
	uint8_t LDIR_Name3[4];
} t_long_dir_entry;

// Global variables
FILE *fd;
t_boot_entry boot_entry;
int bytes_per_sec;
uint32_t first_data_sec;

// Functions prototypes
void list_info(t_boot_entry be);

int read_cluster(FILE *fd, uint8_t *buf, int sec);

void list_dir(uint8_t *recv_buf, int offset, int sec, int level, uint32_t cluster);

void list_long_entry(uint8_t *buf, int size, int level);

uint32_t get_clus_fat_entry_val(uint32_t *clus_fat_entry_val);

#endif // __FAT32_H__
