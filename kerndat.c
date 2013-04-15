#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>

#include "log.h"
#include "kerndat.h"
#include "asm/types.h"

dev_t kerndat_shmem_dev;

/*
 * Anonymous shared mappings are backed by hidden tmpfs
 * mount. Find out its dev to distinguish such mappings
 * from real tmpfs files maps.
 */

static int kerndat_get_shmemdev(void)
{
	void *map;
	char maps[128];
	struct stat buf;

	map = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_ANONYMOUS, 0, 0);
	if (map == MAP_FAILED) {
		pr_perror("Can't mmap piggie");
		return -1;
	}

	sprintf(maps, "/proc/self/map_files/%lx-%lx",
			(unsigned long)map, (unsigned long)map + PAGE_SIZE);
	if (stat(maps, &buf) < 0) {
		pr_perror("Can't stat piggie");
		return -1;
	}

	munmap(map, PAGE_SIZE);

	kerndat_shmem_dev = buf.st_dev;
	pr_info("Found anon-shmem piggie at %"PRIx64"\n", kerndat_shmem_dev);
	return 0;
}

/*
 * Check whether pagemap2 reports soft dirty bit. Kernel has
 * this functionality under CONFIG_MEM_SOFT_DIRTY option.
 */

#define PME_SOFT_DIRTY	(1Ull << 55)

bool kerndat_has_dirty_track = false;

int kerndat_get_dirty_track(void)
{
	char *map;
	int pm2;
	u64 pmap = 0;

	map = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	if (map == MAP_FAILED) {
		pr_perror("Can't mmap piggie2");
		return -1;
	}

	map[0] = '\0';

	pm2 = open("/proc/self/pagemap2", O_RDONLY);
	if (pm2 < 0) {
		pr_perror("Can't open pagemap2 file");
		return -1;
	}

	lseek(pm2, (unsigned long)map / PAGE_SIZE * sizeof(u64), SEEK_SET);
	read(pm2, &pmap, sizeof(pmap));
	close(pm2);
	munmap(map, PAGE_SIZE);

	if (pmap & PME_SOFT_DIRTY) {
		pr_info("Dirty track supported on kernel\n");
		kerndat_has_dirty_track = true;
	} else
		pr_err("Dirty tracking support is OFF\n");

	return 0;
}

int kerndat_init(void)
{
	int ret;

	ret = kerndat_get_shmemdev();
	if (!ret)
		ret = kerndat_get_dirty_track();

	return ret;
}
