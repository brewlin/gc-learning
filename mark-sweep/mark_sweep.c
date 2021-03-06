#include "../gc.h"


/**
 * 对该对象进行标记
 * 并进行子对象标记
 * @param ptr
 */
void gc_mark(void * ptr)
{
    GC_Heap *gh;
    Header *hdr;

    /* mark check */
    if (!(gh = is_pointer_to_heap(ptr))){
//      printf("not pointer\n");
      return;
    } 
    if (!(hdr = get_header(gh, ptr))) {
      printf("not find header\n");
      return;
    }
    if (!FL_TEST(hdr, FL_ALLOC)) {
      printf("flag not set alloc\n");
      return;
    }
    if (FL_TEST(hdr, FL_MARK)) {
      //printf("flag not set mark\n");
      return;
    }

    /* marking */
    FL_SET(hdr, FL_MARK);

    //进行child 节点递归 标记
    for (void* p = ptr; p < (void*)NEXT_HEADER(hdr); p++) {
        //对内存解引用，因为内存里面可能存放了内存的地址 也就是引用，需要进行引用的递归标记
        gc_mark(*(void **)p);
    }
}
/**
 * 清除 未标记内存 进行回收利用
 */
void     gc_sweep(void)
{
    size_t i;
    Header *p, *pend, *pnext;

    //遍历所有的堆内存
    //因为所有的内存都从堆里申请，所以需要遍历堆找出待回收的内存
    for (i = 0; i < gc_heaps_used; i++) {
        //pend 堆内存结束为止
        pend = (Header *)(((size_t)gc_heaps[i].slot) + gc_heaps[i].size);
        //堆的起始为止 因为堆的内存可能被分成了很多份，所以需要遍历该堆的内存
        for (p = gc_heaps[i].slot; p < pend; p = NEXT_HEADER(p)) {
            //查看该堆是否已经被使用
            if (FL_TEST(p, FL_ALLOC)) {
                //查看该堆是否被标记过
                if (FL_TEST(p, FL_MARK)) {
                    DEBUG(printf("解除标记 : %p\n", p));
                    //取消标记，等待下次来回收，如果在下次回收前
                    //1. 下次回收前发现该内存又被重新访问了，则不需要清除
                    //2. 下次回收前发现该内存没有被访问过，所以需要清除
                    FL_UNSET(p, FL_MARK);
                }else {
                    DEBUG(printf("清除回收 :\n"));
                    gc_free(p+1);
                }
            }
        }
    }
}
/**
 * 标记清除算法的gc实现
 */
void  gc(void)
{
    //垃圾回收前 先从 root 开始 进行递归标记
    for(int i = 0;i < root_used;i++)
        gc_mark(roots[i].ptr);
    //标记完成后 在进行 清除 对于没有标记过的进行回收
    gc_sweep();
}

