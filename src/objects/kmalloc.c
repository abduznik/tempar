#include "psp_compat.h"
#include <stdio.h>
#include <string.h>
#include <pspkernel.h>
#include <pspsysmem_kernel.h>
#include "common.h"

int choose_alloc(SceSize size) {
	SceUID partitionids[] = {9, 1, 6};

	int i;
	for(i = 0; i < (int)(sizeof(partitionids) / sizeof(partitionids[0])); i++) {
		PspSysmemPartitionInfo info;
		memset(&info, 0, sizeof(info));
		info.size = sizeof(info);

		if(sceKernelQueryMemoryPartitionInfo(partitionids[i], &info) == 0) {
			if(sceKernelPartitionMaxFreeMemSize(partitionids[i]) > size + 512) {
				return partitionids[i];
			}
		}
	}

	return 0;
}

void *kmalloc_align(SceUID partitionid, int type, SceSize size, int align) {
	size = size + align + sizeof(SceUID);

    if(partitionid == 0) {
        partitionid = choose_alloc(size);
        // all named partitions exhausted — fail closed rather than
        // falling through to partition 0 (would steal game memory).
        if(partitionid == 0) {
            return NULL;
        }
    }

    SceUID mem_id = sceKernelAllocPartitionMemory(partitionid, "", type, size, NULL);

	if(mem_id >= 0) {    
		void *mem_addr = sceKernelGetBlockHeadAddr(mem_id);

		if(mem_addr != NULL) {
			mem_addr += sizeof(SceUID);

			if(align > 0) {
				mem_addr = (void*)(((u32)mem_addr & (~(align - 1))) + align);
			}

			*(u32*)(mem_addr - sizeof(SceUID)) = mem_id;
    
			return mem_addr;
		}
	}

	return NULL;
}

void *kmalloc(SceUID partitionid, int type, SceSize size) {
	return kmalloc_align(partitionid, type, size, 64);
}

void kfree(void *mem_addr) {
	if(mem_addr != NULL) {
		sceKernelFreePartitionMemory(*(u32*)(mem_addr - 4));
	}
}