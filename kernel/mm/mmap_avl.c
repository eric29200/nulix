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
	struct vm_area *node_left = node->vm_avl_left;
	struct vm_area *node_left_left = node_left->vm_avl_left;
	struct vm_area *node_left_right = node_left->vm_avl_right;
	int height_left_right = heightof(node_left_right);
	int height_left = heightof(node_left);

	if (heightof(node_left_left) >= height_left_right) {
		node->vm_avl_left = node_left_right;
		node_left->vm_avl_right = node;
		node_left->vm_avl_height = 1 + (node->vm_avl_height = 1 + height_left_right);
		*nodeplace = node_left;
	} else {
		node_left->vm_avl_right = node_left_right->vm_avl_left;
		node->vm_avl_left = node_left_right->vm_avl_right;
		node_left_right->vm_avl_left = node_left;
		node_left_right->vm_avl_right = node;
		node_left->vm_avl_height = height_left_right;
		node->vm_avl_height = height_left_right;
		node_left_right->vm_avl_height = height_left;
		*nodeplace = node_left_right;
	}
}

/*
 * Rebalance an AVL node on right.
 */
static void avl_rebalance_right(struct vm_area *node, struct vm_area **nodeplace)
{
	struct vm_area *node_right = node->vm_avl_right;
	struct vm_area *node_right_right = node_right->vm_avl_right;
	struct vm_area *node_right_left = node_right->vm_avl_left;
	int height_right = heightof(node_right);
	int height_right_left = heightof(node_right_left);

	if (heightof(node_right_right) >= height_right_left) {
		node->vm_avl_right = node_right_left;
		node_right->vm_avl_left = node;
		node_right->vm_avl_height = 1 + (node->vm_avl_height = 1 + height_right_left);
		*nodeplace = node_right;
	} else {
		node_right->vm_avl_left = node_right_left->vm_avl_right;
		node->vm_avl_right = node_right_left->vm_avl_left;
		node_right_left->vm_avl_right = node_right;
		node_right_left->vm_avl_left = node;
		node_right->vm_avl_height = height_right_left;
		node->vm_avl_height = height_right_left;
		node_right_left->vm_avl_height = height_right;
		*nodeplace = node_right_left;
	}
}

/*
 * Rebalance an AVL tree.
 */
static void avl_rebalance(struct vm_area ***nodeplaces_ptr, int count)
{
	int height, height_left, height_right;
	struct vm_area **nodeplace, *node;

	for ( ; count > 0 ; count--) {
		nodeplace = *--nodeplaces_ptr;
		node = *nodeplace;
		height_left = heightof(node->vm_avl_left);
		height_right = heightof(node->vm_avl_right);

		if (height_right + 1 < height_left) {
			avl_rebalance_left(node, nodeplace);
		} else if (height_left + 1 < height_right) {
			avl_rebalance_right(node, nodeplace);
		} else {
			height = (height_left < height_right ? height_right : height_left) + 1;
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