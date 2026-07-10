#include <mm/mm.h>
#include <mm/mmap.h>
#include <proc/sched.h>
#include <stdio.h>

#define AVL_MAXHEIGHT		41
#define heightof(tree)		((tree) == NULL ? 0 : (tree)->vm_avl_height)

/*
 * Rebalance an AVL node on left.
 */
static void avl_rebalance_left(struct vm_area *node, struct vm_area **nodeplace)
{
	struct vm_area *nodeleft = node->vm_avl_left;
	struct vm_area *nodeleftleft = nodeleft->vm_avl_left;
	struct vm_area *nodeleftright = nodeleft->vm_avl_right;
	int heightleftright = heightof(nodeleftright);
	int heightleft = heightof(nodeleft);

	if (heightof(nodeleftleft) >= heightleftright) {
		node->vm_avl_left = nodeleftright;
		nodeleft->vm_avl_right = node;
		nodeleft->vm_avl_height = 1 + (node->vm_avl_height = 1 + heightleftright);
		*nodeplace = nodeleft;
	} else {
		nodeleft->vm_avl_right = nodeleftright->vm_avl_left;
		node->vm_avl_left = nodeleftright->vm_avl_right;
		nodeleftright->vm_avl_left = nodeleft;
		nodeleftright->vm_avl_right = node;
		nodeleft->vm_avl_height = heightleftright;
		node->vm_avl_height = heightleftright;
		nodeleftright->vm_avl_height = heightleft;
		*nodeplace = nodeleftright;
	}
}

/*
 * Rebalance an AVL node on right.
 */
static void avl_rebalance_right(struct vm_area *node, struct vm_area **nodeplace)
{
	struct vm_area *noderight = node->vm_avl_right;
	struct vm_area *noderightright = noderight->vm_avl_right;
	struct vm_area *noderightleft = noderight->vm_avl_left;
	int heightright = heightof(noderight);
	int heightrightleft = heightof(noderightleft);

	if (heightof(noderightright) >= heightrightleft) {
		node->vm_avl_right = noderightleft;
		noderight->vm_avl_left = node;
		noderight->vm_avl_height = 1 + (node->vm_avl_height = 1 + heightrightleft);
		*nodeplace = noderight;
	} else {
		noderight->vm_avl_left = noderightleft->vm_avl_right;
		node->vm_avl_right = noderightleft->vm_avl_left;
		noderightleft->vm_avl_right = noderight;
		noderightleft->vm_avl_left = node;
		noderight->vm_avl_height = heightrightleft;
		node->vm_avl_height = heightrightleft;
		noderightleft->vm_avl_height = heightright;
		*nodeplace = noderightleft;
	}
}

/*
 * Rebalance an AVL tree.
 */
static void avl_rebalance(struct vm_area ***nodeplaces_ptr, int count)
{
	int height, heightleft, heightright;
	struct vm_area **nodeplace, *node;

	for ( ; count > 0 ; count--) {
		nodeplace = *--nodeplaces_ptr;
		node = *nodeplace;
		heightleft = heightof(node->vm_avl_left);
		heightright = heightof(node->vm_avl_right);

		if (heightright + 1 < heightleft) {
			avl_rebalance_left(node, nodeplace);
		} else if (heightleft + 1 < heightright) {
			avl_rebalance_right(node, nodeplace);
		} else {
			height = (heightleft < heightright ? heightright : heightleft) + 1;
			if (height == node->vm_avl_height)
				break;
			node->vm_avl_height = height;
		}
	}
}

/*
 * Insert a memory region in AVL tree.
 */
static void avl_insert(struct vm_area *new_node, struct vm_area **ptree)
{
	struct vm_area **nodeplace = ptree, **stack[AVL_MAXHEIGHT], *node;
	struct vm_area ***stack_ptr = &stack[0];
	uint32_t key = new_node->vm_end;
	int stack_count = 0;

	/* find place to insert */
	for (;;) {
		node = *nodeplace;
		if (!node)
			break;
		*stack_ptr++ = nodeplace;
		stack_count++;
		if (key < node->vm_end)
			nodeplace = &node->vm_avl_left;
		else
			nodeplace = &node->vm_avl_right;
	}

	/* insert new node */
	new_node->vm_avl_left = NULL;
	new_node->vm_avl_right = NULL;
	new_node->vm_avl_height = 1;
	*nodeplace = new_node;

	/* rebalance tree */
	avl_rebalance(stack_ptr, stack_count);
}

/*
 * Insert a memory region in AVL tree.
 */
void avl_insert_neighbours(struct vm_area *new_node, struct vm_area **ptree, struct vm_area **to_the_left, struct vm_area **to_the_right)
{
	struct vm_area **nodeplace = ptree, **stack[AVL_MAXHEIGHT], *node;
	struct vm_area ***stack_ptr = &stack[0];
	uint32_t key = new_node->vm_end;
	int stack_count = 0;

	*to_the_left = NULL;
	*to_the_right = NULL;

	/* find place to insert */
	for (;;) {
		node = *nodeplace;
		if (!node)
			break;

		*stack_ptr++ = nodeplace;
		stack_count++;

		if (key < node->vm_end) {
			*to_the_right = node;
			nodeplace = &node->vm_avl_left;
		} else {
			*to_the_left = node;
			nodeplace = &node->vm_avl_right;
		}
	}

	/* insert new node */
	new_node->vm_avl_left = NULL;
	new_node->vm_avl_right = NULL;
	new_node->vm_avl_height = 1;
	*nodeplace = new_node;

	/* rebalance tree */
	avl_rebalance(stack_ptr, stack_count);
}

/*
 * Remove a memory region from an AVL tree.
 */
void avl_remove(struct vm_area *node_to_delete, struct vm_area **ptree)
{
	struct vm_area **nodeplace = ptree, **stack[AVL_MAXHEIGHT], **nodeplace_to_delete, *node;
	struct vm_area ***stack_ptr = &stack[0], ***stack_ptr_to_delete;
	uint32_t key = node_to_delete->vm_end;
	int stack_count = 0;

	/* find place to delete */
	for (;;) {
		node = *nodeplace;
		*stack_ptr++ = nodeplace;
		stack_count++;
		if (key == node->vm_end)
			break;
		if (key < node->vm_end)
			nodeplace = &node->vm_avl_left;
		else
			nodeplace = &node->vm_avl_right;
	}
	nodeplace_to_delete = nodeplace;

	/* have to remove node_to_delete = *nodeplace_to_delete */
	if (!node_to_delete->vm_avl_left) {
		*nodeplace_to_delete = node_to_delete->vm_avl_right;
		stack_ptr--;
		stack_count--;
	} else {
		stack_ptr_to_delete = stack_ptr;
		nodeplace = &node_to_delete->vm_avl_left;

		for (;;) {
			node = *nodeplace;
			if (!node->vm_avl_right)
				break;
			*stack_ptr++ = nodeplace;
			stack_count++;
			nodeplace = &node->vm_avl_right;
		}
		*nodeplace = node->vm_avl_left;

		/* node replaces node_to_delete */
		node->vm_avl_left = node_to_delete->vm_avl_left;
		node->vm_avl_right = node_to_delete->vm_avl_right;
		node->vm_avl_height = node_to_delete->vm_avl_height;
		*nodeplace_to_delete = node;
		*stack_ptr_to_delete = &node->vm_avl_left;
	}

	/* rebalance tree */
	avl_rebalance(stack_ptr, stack_count);
}

/*
 * Build mmap AVL tree.
 */
void build_mmap_avl(struct mm_struct *mm)
{
	struct vm_area *vma;

	/* reset tree */
	mm->mmap_avl = NULL;

	/* insert memory regions */
	for (vma = mm->mmap; vma; vma = vma->vm_next)
		avl_insert(vma, &mm->mmap_avl);
}