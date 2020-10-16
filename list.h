#ifndef __LIST_H__
#define __LIST_H__

struct list_head {
	struct list_head *next;
	struct list_head *prev;
};

/**
 *   初始化链表 
 * @param [in]  list_head* head
 *		INIT_LIST_HEAD(&stu_list_head.list);
 */
#define INIT_LIST_HEAD(head) do {			\
		(head)->next = (head)->prev = head;	\
	} while (0)

/**
 *   头插：逆序
 * @param [in] list_head* head 链表头
 * @param [in] list_head* new  待插入的节点
 * 		list_add_tail(&new_stu_node->list, &stu_list_head.list);
 */
static inline void
list_add (struct list_head *new, struct list_head *head)
{
	new->prev = head;
	new->next = head->next;

	new->prev->next = new;
	new->next->prev = new;
}

/**
 *   尾插：顺序
 * @param [in] list_head* head 链表头
 * @param [in] list_head* new  待插入的节点
 * 		list_add_tail(&new_stu_node->list, &stu_list_head.list);
 */
static inline void
list_add_tail (struct list_head *new, struct list_head *head)
{
	new->next = head;
	new->prev = head->prev;

	new->prev->next = new;
	new->next->prev = new;
}

/**
 * 删除链表中的节点
 * @param [in] list_head* old  被删除的链表节点 list_head*
 * 		list_del(&cur_node->list);
 */
static inline void
list_del (struct list_head *old)
{
	old->prev->next = old->next;
	old->next->prev = old->prev;

	old->next = (void *)0xbabebabe;
	old->prev = (void *)0xcafecafe;
}


static inline void
list_del_init (struct list_head *old)
{
	old->prev->next = old->next;
	old->next->prev = old->prev;

	old->next = old;
	old->prev = old;
}


static inline void
list_move (struct list_head *list, struct list_head *head)
{
	list_del (list);
	list_add (list, head);
}


static inline void
list_move_tail (struct list_head *list, struct list_head *head)
{
	list_del (list);
	list_add_tail (list, head);
}


/**
 * 判断链表是否为空
 * @return 空，1；非空，0
 */
static inline int
list_empty (struct list_head *head)
{
	return (head->next == head);
}


static inline void
__list_splice (struct list_head *list, struct list_head *head)
{
	(list->prev)->next = (head->next);
	(head->next)->prev = (list->prev);

	(head)->next = (list->next);
	(list->next)->prev = (head);
}


static inline void
list_splice (struct list_head *list, struct list_head *head)
{
	if (list_empty (list))
		return;

	__list_splice (list, head);
}


/* Splice moves @list to the head of the list at @head. */
static inline void
list_splice_init (struct list_head *list, struct list_head *head)
{
	if (list_empty (list))
		return;

	__list_splice (list, head);
	INIT_LIST_HEAD (list);
}

/* Splice moves @list to the tail of the list at @head. */
static inline void
list_splice_tail_init (struct list_head *list, struct list_head *head)
{
	if (list_empty (list))
		return;

	__list_splice (list, head->prev);
	INIT_LIST_HEAD (list);
}

static inline void
__list_append (struct list_head *list, struct list_head *head)
{
	(head->prev)->next = (list->next);
        (list->next)->prev = (head->prev);
        (head->prev) = (list->prev);
        (list->prev)->next = head;
}


static inline void
list_append (struct list_head *list, struct list_head *head)
{
	if (list_empty (list))
		return;

	__list_append (list, head);
}


/* Append moves @list to the end of @head */
static inline void
list_append_init (struct list_head *list, struct list_head *head)
{
	if (list_empty (list))
		return;

	__list_append (list, head);
	INIT_LIST_HEAD (list);
}

#define list_next(ptr, head, type, member)      \
        (((ptr)->member.next == head) ? NULL    \
                                 : list_entry((ptr)->member.next, type, member))

/**
 *    根据member和type，得到member所在的stu结构体
 * @param [in] list_head*  ptr  
 * @param [in] type    链表结构体类型，如: stu_list_t
 * @param [in] member  stu_list_t结构体中list成员名   
 * @return   stu_list_t* cur_node
 * @sample: 遍历
		struct list_head *iter;
		stu_list_t *stu_node, *list;

		list_for_each(iter, &stu_list_head->list) {
			stu_node = list_entry(iter, stu_list_t, list);  
			printf("id = %d, name = %s\n", stu_node->student->id, stu_node->student->name);
		}
 */
#define list_entry(ptr, type, member)					\
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

/*----------- 遍历链表 -----------*/

#define list_for_each(pos, head)                                        \
	for (pos = (head)->next; pos != (head); pos = pos->next)


#define list_for_each_safe(pos, n, head) \
        for (pos = (head)->next, n = pos->next; pos != (head); \
            pos = n, n = pos->next)

/**
 *    遍历(打印)链表
 * @example
 *     stu_list_t* entry;  // 必须是结构体类型
		// lhead类型：struct list_head*
		// list：是stu_list_t结构体中的struct list_head的member名字
		list_for_each_entry(entry, lhead, list)
		{
			printf("id=%d, name=%s\n",entry->student->id, entry->student->name);
		}
 */
#define list_for_each_entry(pos, head, member)				\
	for (pos = list_entry((head)->next, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = list_entry(pos->member.next, typeof(*pos), member))

/**
 *    遍历(删除)链表
 * @param [in] stu_list_t *  pos    当前元素
 * @param [in] stu_list_t *  n
 * @param [in] list_head*    head  	链表的头结点
 * @param [in] member   stu_list_t结构体中list成员名
 * @example 
			stu_list_t* entry, * tmp;  // 必须是结构体类型
			// lhead类型：struct list_head*
			// list：是stu_list_t结构体中的struct list_head的member名字
			list_for_each_entry_safe(entry, tmp, lhead, list)
			{
				// 从链表中删除节点
				list_del(&entry->list);
				
				// 释放
				free(entry->student);
				free(entry);
			}
 */
#define list_for_each_entry_safe(pos, n, head, member)			\
	for (pos = list_entry((head)->next, typeof(*pos), member),	\
		n = list_entry(pos->member.next, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = n, n = list_entry(n->member.next, typeof(*n), member))


#define list_for_each_entry_reverse(pos, head, member)                  \
	for (pos = list_entry((head)->prev, typeof(*pos), member);      \
	     &pos->member != (head);                                    \
	     pos = list_entry(pos->member.prev, typeof(*pos), member))


#define list_for_each_entry_safe_reverse(pos, n, head, member)          \
	for (pos = list_entry((head)->prev, typeof(*pos), member),      \
	        n = list_entry(pos->member.prev, typeof(*pos), member); \
	     &pos->member != (head);                                    \
	     pos = n, n = list_entry(n->member.prev, typeof(*n), member))

#endif /* _LLIST_H */
