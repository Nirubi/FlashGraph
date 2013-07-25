#ifndef __NUMA_CACHE_H__
#define __NUMA_CACHE_H__

#include <tr1/unordered_map>
#include <vector>

#include "cache.h"
#include "cache_config.h"

/**
 * This cache divides a global cache to pieces of the same size,
 * and place them on the specified NUMA nodes.
 */
class NUMA_cache: public page_cache
{
	const cache_config *cache_conf;
	std::vector<page_cache *> caches;
public:
	NUMA_cache(const cache_config *config): caches(config->get_num_caches()) {
		cache_conf = config;
		std::vector<int> node_ids;
		cache_conf->get_node_ids(node_ids);
		for (size_t i = 0; i < node_ids.size(); i++) {
			page_cache *cache = cache_conf->create_cache_on_node(node_ids[i]);
			caches[i] = cache;
		}
	}

	virtual page *search(off_t offset, off_t &old_off) {
		int idx = cache_conf->page2cache(offset);
		return caches[idx]->search(offset, old_off);
	}

	virtual page *search(off_t offset) {
		int idx = cache_conf->page2cache(offset);
		return caches[idx]->search(offset);
	}

	virtual long size() {
		return cache_conf->get_size();
	}

	// TODO shouldn't I use a different underlying IO for cache
	// on the different nodes.
	virtual void init(io_interface *underlying) {
		for (size_t i = 0; i < caches.size(); i++) {
			// TODO this is a ugly hack. It makes sure that each flush
			// thread of a SA-cache has a reference to this NUMA cache.
			caches[i]->create_flush_thread(underlying, this);
			caches[i]->init(underlying);
		}
	}

	virtual void print_stat() const {
		for (size_t i = 0; i < caches.size(); i++)
			caches[i]->print_stat();
	}

	virtual void mark_dirty_pages(thread_safe_page *pages[], int num) {
		for (int i = 0; i < num; i++) {
			int idx = cache_conf->page2cache(pages[i]->get_offset());
			caches[idx]->mark_dirty_pages(&pages[i], 1);
		}
	}

	virtual void flush_callback(io_request &req) {
		int idx = cache_conf->page2cache(req.get_offset());
		caches[idx]->flush_callback(req);
	}
};

#endif