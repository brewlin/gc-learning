#include "gc.h"
#include "generational.h"

typedef struct t{
    int       value;
    struct t* next;
}T;
int clear(){
    free_list = NULL;
    new_free_p = NULL;
    to_free_p = NULL;
    root_used = 0;
    gc_heaps_used = 0;
    //清空结果集
    rs_index = 0;
}
void test_auto_gc(){
    /**
     * 1. 每个堆 一个 T 空间大小
     * 2. 总共4个堆
     *          heaps[0] 作为 新生代
     *          heaps[1] 作为 幸存代 from
     *          heaps[2] 作为 幸存代 to
     *          heaps[3] 作为 老年代
     */
    gc_init(sizeof(T) + HEADER_SIZE);

    T* p1 = minor_malloc(sizeof(T));
    p1->value = 100;
    assert(p1);
    //内存从新生代申请 且已经满了
    assert(is_pointer_to_space(p1,newg));

    //内存不够触发gc p1被回收 p2 依然等于p1 从同一份内存中申请
    T* p2 = minor_malloc(sizeof(T));
    assert(p1->value == 0);
    assert(p1 == p2);
}
/**
 * 测试加入root后的gc情况
 */
void test_add_root_gc(){
    /**
     * 1. 每个堆 一个 T 空间大小
     *
     * 2. 总共4个堆
     *          heaps[0] 作为 新生代
     *          heaps[1] 作为 幸存代 from
     *          heaps[2] 作为 幸存代 to
     *          heaps[3] 作为 老年代
     */
     //每个堆可以容纳2个T
    gc_init(2*(sizeof(T) + HEADER_SIZE));

    //从 heaps[0]分配
    T* t1 = minor_malloc(sizeof(T));
    t1->value = 11;
    assert(is_pointer_to_space(t1,newg));
    //加入root
    add_roots(&t1);

    //t2也是 heaps[0] 新生代分配
    T* t2 = minor_malloc(sizeof(T));
    t2->value = 22;
    assert(is_pointer_to_space(t1,newg));

    //t1引用了t2
    t1->next = t2;

    /*1. --------------------t3 申请时会自动触发gc--------------------*/
    //t1 t2 拷贝到了 to区 因为gc后会交换 from = to 所以先保存to索引
    T* t3 = minor_malloc(sizeof(T));
    t3->value = 33;
    /*2. --------------------现在 新生代区已经空了出来 t3从新生代区分配--------------------*/
    assert(is_pointer_to_space(t1,survivorfromg));
    assert(is_pointer_to_space(t1->next,survivorfromg));
    //TODO:不要对 没有加入 root 但是却被root里的对象引用的对象进行测试
    // assert(is_pointer_to_space(t2,&to));
    //因为t2 所指向的地址不会被修改，但是t1->next 原先是执向的t2 现在已经复制后拷贝了t2 也就是t1->next != t2
    //所以如果t2 在后面要继续直接引用的话 在gc应用的时候也需要加入root
    assert(t1->next != t2);
    assert(is_pointer_to_space(t3,newg));
    //测试他们的年龄对不对
    assert(CURRENT_HEADER(t1)->age == 1);
    assert(CURRENT_HEADER(t1->next)->age == 1);

    //新生代依然还有一个空间 申请过后就满了
    T* t4 = minor_malloc(sizeof(T));
    assert(t4);
    assert(is_pointer_to_space(t4,newg));
    t4->value = 44;

    //分配t5的时候 新生代满了 需要gc
    //: t4 t3 没有加入root 被回收
    //: t1 t2 有root 年龄 + 1
    //: t1 t2 拷贝到了 to空间
    T* t5 = minor_malloc(sizeof(T));
    assert(t5);
    assert(is_pointer_to_space(t5,newg));
    //t3 t4 被回收
    assert(t3->value != 33);
    assert(t4->value != 44);
    assert(CURRENT_HEADER(t1)->age == 2);
    //不要直接测试 t2->age 因为t2没有加入root被回收了但是 t1-next没有
    assert(CURRENT_HEADER(t1->next)->age == 2);
    assert(is_pointer_to_space(t1,survivorfromg));
    assert(is_pointer_to_space(t1->next,survivorfromg));
}
/**
 * 测试晋升老年代
 */
void test_promote()
{
    /**
     * 1. 每个堆 一个 T 空间大小
     *
     * 2. 总共4个堆
     *          heaps[0] 作为 新生代
     *          heaps[1] 作为 幸存代 from
     *          heaps[2] 作为 幸存代 to
     *          heaps[3] 作为 老年代
     */
    //每个堆可以容纳2个T
    gc_init(2*(sizeof(T) + HEADER_SIZE));

    T* t1 = minor_malloc(sizeof(T));
    add_roots(&t1);
    assert(t1);

    minor_gc();
    //拷贝到了幸存to堆
    assert(CURRENT_HEADER(t1)->age == 1);
    assert(is_pointer_to_space(t1,survivorfromg));

    //拷贝到了幸存to堆
    minor_gc();
    assert(CURRENT_HEADER(t1)->age == 2);
    assert(is_pointer_to_space(t1,survivorfromg));



    minor_gc();
    assert(CURRENT_HEADER(t1)->age == 3);
    assert(is_pointer_to_space(t1,survivorfromg));

    //这个时候新申请一个内存不加入root
    T* younger = minor_malloc(sizeof(T));
    t1->next = younger;

    //开始老年代gc 现在t1 完全归老年代管理了且 只执行标记清除算法
    //yonger 还在新生代 但是root已经索引不到他了 但是依然会被老年代通过rs 阶段进行复制
    minor_gc();
    assert(rs_index == 1);
    //t1 指向了老年代区
    assert(is_pointer_to_space(t1,oldg));
    assert(CURRENT_HEADER(t1->next)->age == 1);
    assert(is_pointer_to_space(t1->next,survivorfromg));

    /****************接下来测试 yonger 作为被老年代引用的新生代对象 一起晋升为老年代后导致 t1 和 yonger都被回收 ********/
    t1->value = 11;
    t1->next->value = 22;

    minor_gc();
    assert(CURRENT_HEADER(t1->next)->age == 2);
    minor_gc();
    assert(CURRENT_HEADER(t1->next)->age == 3);

    //这里两边都成为老年代 再也没有新生代的引用 被标记清除回收
    minor_gc();
    T* old = t1->next;
    assert(is_pointer_to_space(t1->next,oldg));

    //执行老年代gc
    major_gc();

    assert(t1->value == 0);
    assert(old->value == 0);



}
int main(){
    // 测试不加入 root的时候  会自动进行gc复制
    //看看gc复制的结果是否符合预期
    printf("\n*******************测试添加root 后的gc *************");
    test_auto_gc();
    clear();
    printf("*********************test auto_gc pass *************\n\n");

    printf("\n*******************测试添加root 后的gc *************\n");
    test_add_root_gc();
    clear();
    printf("*********************test auto_gc pass *************\n\n");


    printf("\n*******************测试晋升老年代gc *************\n");
    test_promote();
    printf("*********************test promote pass *************\n\n");

    return 0;
}