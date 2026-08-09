#include <net/9p/9p.h>
#include <lib/parser.h>
#include <lib/list.h>
#include <string.h>

/* transports list */
static LIST_HEAD(v9fs_trans_list);

/*
 * Register a transport.
 */
void v9fs_register_trans(struct p9_trans_module *trans)
{
	list_add_tail(&trans->list, &v9fs_trans_list);
}

/*
 * Get transmission module by name.
 */
struct p9_trans_module *v9fs_get_trans_by_name(const struct substring *name)
{
	struct p9_trans_module *t;
	struct list_head *pos;

	list_for_each(pos, &v9fs_trans_list) {
		t = list_entry(pos, struct p9_trans_module, list);
		if (strncmp(t->name, name->from, name->to-name->from) == 0)
			return t;
	}

	return NULL;
}

/*
 * Get default transmission module.
 */
struct p9_trans_module *v9fs_get_default_trans()
{
	if (list_empty(&v9fs_trans_list))
		return NULL;

	return list_first_entry(&v9fs_trans_list, struct p9_trans_module, list);
}

/*
 * Init 9p.
 */
int init_p9()
{
	return p9_trans_fd_init();
}