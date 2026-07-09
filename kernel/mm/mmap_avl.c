#include <mm/mmap.h>

#define max(a, b)			(((a) > (b)) ? (a) : (b))
#define avl_height(vma)			((vma) != NULL ? (vma)->vm_avl_height : 0)
#define avl_balance(vma)		((vma) != NULL ? (avl_height((vma)->vm_avl_left) - avl_height((vma)->vm_avl_right)) : 0)

static struct vm_area *__rotate_right(struct vm_area *y)
{
	struct vm_area *x, *t2;

	x = y->vm_avl_left;
	t2 = x->vm_avl_right;

	x->vm_avl_right = y;
	y->vm_avl_left = t2;

	y->vm_avl_height = max(avl_height(y->vm_avl_left), avl_height(y->vm_avl_right)) + 1;
	x->vm_avl_height = max(avl_height(x->vm_avl_left), avl_height(x->vm_avl_right)) + 1;

	return x;
}

static struct vm_area *__rotate_left(struct vm_area *x)
{
	struct vm_area *y, *t2;

	y = x->vm_avl_right;
	t2 = y->vm_avl_left;

	y->vm_avl_left = x;
	x->vm_avl_right = t2;

	x->vm_avl_height = max(avl_height(x->vm_avl_left), avl_height(x->vm_avl_right)) + 1;
	y->vm_avl_height = max(avl_height(y->vm_avl_left), avl_height(y->vm_avl_right)) + 1;

	return y;
}

struct vm_area *mmap_avl_insert(struct vm_area *root, struct vm_area *new, struct vm_area **pprev, struct vm_area **pnext)
{
	int balance;

	/*if (!vma) {
		new->vm_next = *next;
		if (*prev)
			(*prev)->vm_next = new;
		else
			vm_areas_tree_list = new;
		return new;
	}*/

	if (new->vm_end < root->vm_end) {
		*pnext = root;
		root->vm_avl_left = mmap_avl_insert(root->vm_avl_left, new, pprev, pnext);
	} else if (new->vm_end > root->vm_end) {
		*pprev = root;
		root->vm_avl_right = mmap_avl_insert(root->vm_avl_right, new, pprev, pnext);
	} else {
		return root;
	}

	root->vm_avl_height = 1 + max(avl_height(root->vm_avl_left), avl_height(root->vm_avl_right));

	balance = avl_balance(root);

	if (balance > 1 && new->vm_end < root->vm_avl_left->vm_end)
		return __rotate_right(root);

	if (balance < -1 && new->vm_end > root->vm_avl_right->vm_end)
		return __rotate_left(root);

	if (balance > 1 && new->vm_end > root->vm_avl_left->vm_end) {
		root->vm_avl_left = __rotate_left(root->vm_avl_left);
		return __rotate_right(root);
	}

	if (balance < -1 && new->vm_end < root->vm_avl_right->vm_end) {
		root->vm_avl_right = __rotate_right(root->vm_avl_right);
		return __rotate_left(root);
	}

	return root;
}

static struct vm_area *detach_min(struct vm_area *root, struct vm_area **min_node)
{
	int balance;

	if (!root->vm_avl_left) {
		*min_node = root;
		return root->vm_avl_right;
	}

	root->vm_avl_left = detach_min(root->vm_avl_left, min_node);

	root->vm_avl_height = 1 + max(avl_height(root->vm_avl_left), avl_height(root->vm_avl_right));

	balance = avl_balance(root);

	if (balance > 1 && avl_balance(root->vm_avl_left) >= 0)
		return __rotate_right(root);

	if (balance > 1 && avl_balance(root->vm_avl_left) < 0) {
		root->vm_avl_left = __rotate_left(root->vm_avl_left);
		return __rotate_right(root);
	}

	if (balance < -1 && avl_balance(root->vm_avl_right) <= 0)
		return __rotate_left(root);

	if (balance < -1 && avl_balance(root->vm_avl_right) > 0) {
		root->vm_avl_right = __rotate_right(root->vm_avl_right);
		return __rotate_left(root);
	}

	return root;
}

struct vm_area *mmap_avl_remove(struct vm_area *root, struct vm_area *to_remove)
{
	struct vm_area *successor;
	int balance;

	if (!root)
		return NULL;

	if (to_remove->vm_end < root->vm_end) {
		root->vm_avl_left = mmap_avl_remove(root->vm_avl_left, to_remove);
	} else if (to_remove->vm_end > root->vm_end) {
		root->vm_avl_right = mmap_avl_remove(root->vm_avl_right, to_remove);
	} else {
		if (!root->vm_avl_left && !root->vm_avl_right)
			return NULL;

		if (!root->vm_avl_left)
			return root->vm_avl_right;

		if (!root->vm_avl_right)
			return root->vm_avl_left;

		successor = NULL;
		root->vm_avl_right = detach_min(root->vm_avl_right, &successor);

		successor->vm_avl_left = root->vm_avl_left;
		successor->vm_avl_right = root->vm_avl_right;

		root = successor;
	}

	root->vm_avl_height = 1 + max(avl_height(root->vm_avl_left), avl_height(root->vm_avl_right));

	balance = avl_balance(root);

	if (balance > 1 && avl_balance(root->vm_avl_left) >= 0)
		return __rotate_right(root);

	if (balance > 1 && avl_balance(root->vm_avl_left) < 0) {
		root->vm_avl_left = __rotate_left(root->vm_avl_left);
		return __rotate_right(root);
	}

	if (balance < -1 && avl_balance(root->vm_avl_right) <= 0)
		return __rotate_left(root);

	if (balance < -1 && avl_balance(root->vm_avl_right) > 0) {
		root->vm_avl_right = __rotate_right(root->vm_avl_right);
		return __rotate_left(root);
	}

	return root;
}