#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef uint8_t  bool;
#define true 1
#define false 0

typedef struct {
    uint8_t BootJumpInstrction[3];
    uint8_t OemIdentifier[8];
    uint16_t BytesPerSector;
    uint8_t SectorsPerCluster;
    uint16_t ReservedSectorCount;
    uint8_t FatCount;
    uint16_t DirEntryCount;
    uint16_t TotalSectors;
    uint8_t MediaDescriptorType;
    uint16_t SectorsPerFat;
    uint16_t SectorsPerTrack;
    uint16_t Heads;
    uint32_t HiddenSectors;
    uint32_t LargeSectorCount;

    uint32_t DriveNumber;
    uint8_t _Reserved1;
    uint8_t Signature;
    uint32_t VolumeID;
    uint8_t VolumeLabel[11];
    uint8_t SystemId[8];
} __attribute__((packed)) BootSector;

typedef struct {
    uint8_t Name[11];
    uint8_t Attributes;
    uint8_t _Reserved;
    uint8_t CreationTimeTenth;
    uint16_t CreationTime;
    uint16_t CreationDate;
    uint16_t LastAccessDate;
    uint16_t FirstClusterHigh;
    uint16_t LastWriteTime;
    uint16_t LastWriteDate; 
    uint16_t FirstClusterLow;
    uint32_t FileSize;
} __attribute__((packed)) DirectoryEntry;

BootSector g_BootSector;
uint8_t* g_Fat = NULL;
DirectoryEntry* g_DirectoryEntries = NULL;
uint32_t g_RootDirectoryEnd;

// Load the boot sector from the disk image
// and read the FAT table and root directory entries
bool readBootSector (FILE* disk){
    return fread(&g_BootSector, sizeof(BootSector), 1, disk) == 1;
}

// Read sectors from the disk image
// lba: Logical Block Addressing
// count: Number of sectors to read
// bufferOut: Pointer to the buffer where the data will be stored
bool readSectors(FILE* disk, uint32_t lba, uint32_t count, void* bufferOut){
    bool ok = true;
    ok = ok && (fseek(disk, lba * g_BootSector.BytesPerSector, SEEK_SET) == 0);
    ok = ok && (fread(bufferOut, g_BootSector.BytesPerSector, count, disk) == count);
    return ok;
}

// Read the FAT table from the disk image
// The FAT table is stored in the reserved sectors
// after the boot sector
bool readFat(FILE* disk){
    g_Fat = (uint8_t*)malloc(g_BootSector.SectorsPerFat * g_BootSector.BytesPerSector);
    return readSectors(disk, g_BootSector.ReservedSectorCount, g_BootSector.SectorsPerFat, g_Fat);
}

// Read the root directory entries from the disk image
// The root directory is stored in the reserved sectors
// after the FAT table
// The root directory is not stored in a contiguous block
// but is scattered across the disk
bool readRootDirectory(FILE* disk){

    uint32_t lba = g_BootSector.ReservedSectorCount + (g_BootSector.FatCount * g_BootSector.SectorsPerFat);
    uint32_t size = sizeof(DirectoryEntry) * g_BootSector.DirEntryCount;
    uint32_t sectors = size / g_BootSector.BytesPerSector;
    if (size % g_BootSector.BytesPerSector > 0) {
        sectors++;
    }

    g_RootDirectoryEnd = lba + sectors;
    g_DirectoryEntries = (DirectoryEntry*)malloc(sectors * g_BootSector.BytesPerSector);
    return readSectors(disk, lba, sectors, g_DirectoryEntries);
}

// Find a file in the root directory
// The file name is stored in the first 11 bytes of the directory entry
// The file name is padded with spaces to 11 bytes
DirectoryEntry* findFile(const char* name){
    for (uint32_t i = 0; i < g_BootSector.DirEntryCount; i++) {
        if (memcmp(g_DirectoryEntries[i].Name, name, 11) == 0) {
            return &g_DirectoryEntries[i];
        }
    }
    return NULL;
}


bool readFile(DirectoryEntry* fileEntry, FILE* disk, uint8_t* outputBuffer){

    bool ok = true;
    uint16_t currentCluster = fileEntry->FirstClusterLow;

    do {
        uint32_t lba = g_RootDirectoryEnd + (currentCluster - 2) * g_BootSector.SectorsPerCluster;
        ok = ok && readSectors(disk, lba, g_BootSector.SectorsPerCluster, outputBuffer);
        outputBuffer += g_BootSector.SectorsPerCluster * g_BootSector.BytesPerSector;

        uint32_t fatIndex = currentCluster * 3 / 2;
        if (currentCluster % 2 == 0) {
            currentCluster = *(uint16_t*)&g_Fat[fatIndex] & 0x0FFF;
        } else {
            currentCluster = (*(uint16_t*)&g_Fat[fatIndex] >> 4) & 0x0FFF;
        }
    } while (ok && currentCluster < 0x0FF8);

    return ok;
}

int main(int argc, char** argv){

    if (argc < 3) {
        printf("Syntax: %s <disk image> <file name>\n", argv[0]);
        return -1;
    }

    FILE* disk = fopen(argv[1], "rb");
    if (!disk) {
        fprintf(stderr, "Cannot open disk image %s!\n", argv[1]);
        return -1;
    }

    if (!readBootSector(disk)) {
        fprintf(stderr, "Could not read boot sector!\n");
        return -2;
    }

    if (!readFat(disk)) {
        fprintf(stderr, "Could not read FAT!\n");
        free(g_Fat);
        return -3;
    }

    if (!readRootDirectory(disk)) {
        fprintf(stderr, "Could not read FAT!\n");
        free(g_Fat);
        free(g_DirectoryEntries);
        return -4;
    }

    DirectoryEntry* fileEntry = findFile(argv[2]);

    if (!fileEntry) {
        fprintf(stderr, "Could not find file %s!\n", argv[2]);
        free(g_Fat);
        free(g_DirectoryEntries);
        return -5;
    }

    uint8_t* outputBuffer = (uint8_t*)malloc(fileEntry->FileSize + g_BootSector.BytesPerSector - 1);
    if (!readFile(fileEntry, disk, outputBuffer)) {
        fprintf(stderr, "Could not read file %s!\n", argv[2]);
        free(g_Fat);
        free(g_DirectoryEntries);
        free(outputBuffer);
        return -5;
    }

    for (size_t i = 0; i < fileEntry->FileSize; i++) {
        if (isprint(outputBuffer[i])) {
            fputc(outputBuffer[i], stdout);
        } else {
            printf("<%02x>", outputBuffer[i]);
        }
    }

    printf("\n");

    free(g_Fat);
    free(g_DirectoryEntries);
    free(outputBuffer);
    fclose(disk);
    return 0;
}