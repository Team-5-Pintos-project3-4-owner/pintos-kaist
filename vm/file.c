/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include "threads/mmu.h"

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	printf("file_backed_initializer, page->va: %p\n", page->va);
	page->operations = &file_ops;
	struct file_page *temp = (struct file_page *);

	struct file_page *file_page = &page->file;
	printf("file_backed_initializer, file_page: %p\n", file_page);
	file_page->file = temp->file;
	file_page->offset = temp->offset;
	file_page->length = temp->length;
	file_page->page_read_bytes = temp->page_read_bytes;
	file_page->page_zero_bytes = temp->page_zero_bytes;
	file_page->writable = temp->writable;

	printf("initializer: %d\n", temp->page_read_bytes);
	printf("initializer: %d\n", file_page->page_read_bytes);

	// printf("zz\n");
	// free(temp); // aux로 넘어 온 애였으니까 이제 필요가 없어서 바이바이
	// printf("zzzzz\n");

	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;

}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {

	void *origin_addr = addr;
	uint32_t read_bytes = length;
	uint32_t zero_bytes = (length==PGSIZE) ? 0 : PGSIZE - (length % PGSIZE);
	// printf("length: %d\n", length);
	// printf("read bytes: %d\n", read_bytes);
	// printf("zero bytes: %d\n", zero_bytes);

	while (read_bytes > 0 || zero_bytes > 0)
	{
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		// printf("read bytes: %d\n", read_bytes);

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		struct file_page *aux_file_info;
		aux_file_info = (struct file_page *)malloc(sizeof(struct file_page));
		aux_file_info->file = file;
		aux_file_info->offset = offset;
		aux_file_info->length = length;
		aux_file_info->page_read_bytes = page_read_bytes;
		aux_file_info->page_zero_bytes = page_zero_bytes;
		aux_file_info->writable = writable;

		// printf("mmap: page_read_bytes - %d\n", aux_file_info->page_read_bytes);

		if (!vm_alloc_page_with_initializer(VM_FILE, addr,
											writable, lazy_load_segment, (void *)aux_file_info))
		{
			// printf("alloc 실패!");
			return NULL;
		}
		// printf("alloc 성공!\n");
		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		addr += PGSIZE;
		offset += page_read_bytes;
	}
	// printf("성공\n");
	return origin_addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	// printf("addr in munmap = %p\n",  addr);

	/* 
	1) addr에 대한 매핑 해제 (addr은 동일 프로세스에서 mmap으로 반환된 va)
	2) 프로세스에 의해 기록된 모든 페이지가 파일에 다시 기록되며, 쓰지 않은 페이지는 기록 x
	3) 가상 페이지 목록에서 페이지 제거
	*/

	struct thread *cur = thread_current();
	struct page *p = spt_find_page(&cur->spt, addr);
	// printf("do_munmap p->va: %p\n", p->va);

	// addr이 현재 프로세스의 spt에 존재하는지 확인
	uint32_t read_bytes = p->file.length;
	uint32_t zero_bytes = (read_bytes == PGSIZE) ? 0 : PGSIZE - (read_bytes % PGSIZE);
	while (read_bytes > 0 || zero_bytes > 0)
	{

		struct file_page *fp = (struct file_page *)&p->file;
		// printf("[0.5] fp: %p\n", fp);
		uint32_t page_read_bytes = fp->page_read_bytes;
		// printf("[0.7]\n");
		uint32_t page_zero_bytes = fp->page_zero_bytes;
		// printf("[1]\n");
		// addr에 대한 매핑을 해제
		p->frame->page = NULL;
		p->frame = NULL;
		// printf("[2]\n");

		if (pml4_is_dirty(cur->pml4, addr))
		{
			// printf("[3]\n");
			file_write_at(fp->file, addr, page_read_bytes, fp->offset);
			// printf("[4]\n");
			pml4_set_dirty(cur->pml4, addr, 0);
			// printf("[5]\n");
		}

		pml4_clear_page(cur->pml4, addr);
		// printf("[6]\n");
		spt_remove_page(&cur->spt, p);
		// printf("[7]\n");

		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		addr += PGSIZE;

		p = spt_find_page(&cur->spt, addr);
		if (p==NULL){
			return;
		}
	}
	// printf("hi!\n");
}

// void do_munmap(void *addr)
// {
// 	while (true)
// 	{
// 		struct thread *curr = thread_current();
// 		struct page *find_page = spt_find_page(&curr->spt, addr);
// 		struct frame *find_frame = find_page->frame;
// 		printf("addr in munmap = %p\n", addr);
// 		if (find_page == NULL)
// 		{
// 			return NULL;
// 		}

// 		// // 연결 해제
// 		// find_page->frame = NULL;
// 		// find_frame->page = NULL;

// 		struct file_info *container = (struct file_info *)find_page->uninit.aux;
// 		// 페이지의 dirty bit이 1이면 true를, 0이면 false를 리턴한다.
// 		if (pml4_is_dirty(&curr->pml4, find_page->va) == true)
// 		{
// 			// 물리 프레임에 변경된 데이터를 다시 디스크 파일에 업데이트 buffer에 있는 데이터를 size만큼, file의 file_ofs부터 써준다.
// 			file_write_at(container->file, addr, container->read_bytes, container->offset);
// 			// dirty bit = 0
// 			// 인자로 받은 dirty의 값이 1이면 page의 dirty bit을 1로, 0이면 0으로 변경해준다.
// 			pml4_set_dirty(curr->pml4, find_page->va, 0);
// 		}
// 		// dirty bit = 0
// 		// 인자로 받은 dirty의 값이 1이면 page의 dirty bit을 1로, 0이면 0으로 변경해준다.

// 		// present bit = 0
// 		// 페이지의 present bit 값을 0으로 만들어주는 함수
// 		pml4_clear_page(curr->pml4, find_page->va);
// 		addr += PGSIZE;
// 	}
// }