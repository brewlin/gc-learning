当前所有gc算法的实现都是基于保守式gc:

在gc过程中重要的一个步骤就是要辨别该内存是 指针 or 非指针:
```
long  p1 = 0x55555555;

//p2 = 0x5555555;
void* p2 = gc_malloc();
obj->value = p1;
//上面的情况就是p2指针实际是没有被引用的，但是在gc过程中
for(child: obj){
    //这个时候遍历到了value = 0x55555555;的时候误以为是p2指针
    //结果p2指针本来应该被回收的（因为没有被任何根引用）
    //但是因为保守式gc 导致被误判了，p2得不到回收
    mark(child);
}
```

