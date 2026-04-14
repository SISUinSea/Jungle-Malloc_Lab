# Explicit Memory Allocation을 어떻게 구현하지?
- Free block은 어짜피 안 쓴다. 
- 그럼 그 안에 pred, succ 포인터를 배치해서 Free block들을 이중 연결 리스트 처럼 사용한다.
- 이 특징을 제외하면 나머지 부분은 implicit에서 구현한 것과 비슷하다.

## Allocation 을 위한 탐색
- Free list를 순회하며 맞는 block을 찾는다. 이를 위해서는 Free entry로 사용할 포인터가 필요하다.
- 찾았다면, 해당 block을 사용 처리한다.
    - 세부적인 처리도 필요하다. 예를 들면 split을 했다면 그걸 또 새로운 Free List에 연결해야 한다.

## 어떻게 Free block을 List에 추가할 것인가?(LIFO + First fit 정책 사용)
- LIFO 스택 구조로 LinkedList를 사용하는 것은 쉽다.
- 이번에는 그렇게 구현해보자.

# 어디서 수정이 필요한가?
- [ ] 시작할 때, free list ptr를 들고 있어야 한다. 이름을 정하고 선언 필요.
- [ ] `mm_init`에서 free list ptr을 초기화 필요.
- [ ] `find_fit`에서 free list 를 순회하며 적당한 블록을 찾도록 로직을 수정해야 함.
- [ ] `extend_heap`에서 만든 블록을 free list에 추가해야 한다.
- [ ] `place`에서 할당한 블록을 free list 에서 제거해야 한다.
- [ ] `place`에서 split으로 새롭게 만들어진 free block을 free list에 추가해야 한다.
- [ ] `mm_free`에서 free block을 list에 추가해야 한다.(LIFO 방식으로)
- [ ] free list 정책은 다음과 같은 상황들을 상정해서 결정해야 한다.
    1. free list를 제거하는 상황(block을 list에서 제거, coalesce로 제거)
    2. free list를 추가하는 상황(split으로 추가됨, extend_heap로 추가됨)




---

- 코드 쓰기 전에 블록 레이아웃 / free list 정책 / invariant를 네가 먼저 적는다.
- insert_free_block, remove_free_block, coalesce, place 중 적어도 핵심 함수들은 네가 설계 결정을 설명할 수 있어야 한다.
- mm_checkheap은 직접 만든다.
- 완성 후에는 “왜 이 포인터 쓰기가 필요한지”, “어디서 깨질 수 있는지”를 말로 설명할 수 있어야 한다.
- trace 하나를 잡아서 split/coalesce/free-list update가 어떻게 변하는지 손으로 따라갈 수 있어야 한다.


- [ ] insert의 책임
    새롭게 생성한 free block을 free_listp로 가리켜야 한다. 
    새롭게 생성한 free block의 SUCC는 이전 free_listp 가 가리키던 포인터로 이동
    기존의 block의 PRED는 새롭게 생성한 free block을 가리켜야 한다.

    insert는 free block이 발생하는 곳에서 호출된다. 인자로는 새로운 free block의 ptr을 받아 free block에 연결하면 된다.

    PRED, SUCC를 저장할 공간이 없는 block을 생성해서는 안된다. 잠깐,  그건 insert의 책임이 아니다. extend_heap을 부르기 전에 extend_size를 MIN_FREE_BLOCK_SIZE보다 작아지지 않도록 바꿨다.
- [ ] remove의 책임
- [ ] coalesce의 책임
- [ ] checker 가 왜 필요한지?
- [ ] invariant가 뭔지?
    여기서 invariant는 free list에 관련된 것을 결정해야 할 것 같음.
    예를 들면 이런거지.
    free list는
        1. 반환된 블록을 stack에 push 하는 LIFO 구조
        2. coalesce 되거나 allocated 되어서 free list에서 사라졌다면 free list에서 그것을 반영해야 함.
        3. stack의 top 부터 탐색
    이런건 policy와 섞여있다. 다시 써본다면..(invariant는 어떤 연산이 끝나든 지켜져야 하는 상태)
        1. heap에서의 free block과 free list에서의 element가 동일해야 함.
            동일하다는 것은 다음을 의미(두 블록을 비교했을 떄)
            - header, footer 등의 메타 데이터가 같음.
            - pred, succ가 가리키는 정보가 같음.

    ---
    힌트를 받았다. invariant를 다시 작성해보겠다.
    어떤 상황에서든 유지되어야 하는 구조는 다음과 같다.
        1. 개별 블록의 invariant: free block은 header, footer를 가져야 하고, 추가적으로 payload 안에 pred, succ를 가리키는 포인터를 위한 공간이 있어야 한다.
            1.1. header의 4byte 바로 다음, 다른 블록 주소를 저장하기 위한 공간만큼을 pred가 차지한다.
            1.2. pred 다음 다른 블록 주소를 저장하기 위한 공간만큼을 succ가 차지한다.
        2. free list의 invariant: free list의 모든 element들은 pred, succ로 문제없이 연결되어야 한다.
            2.1. free list를 순회하면 유한 번 안에 모든 free element에 접근할 수 있어야 한다. 유한 번의 제한은 heap을 직접 돌면서 몇 번 안에 모든 free element를 돌아야 하는지 확인할 수 있다.
            2.2. free list의 header, trailer를 제외한 모든 요소에 대해서 X.succ == Y 라면, Y.pred == X 이어야 한다.
        3. heap에서의 free block들의 집합과 free list에서의 block들의 집합이 일치해야 한다.
            3.1 heap에서의 free block 개수와 free list에서의 block 개수가 동일해야 한다.
            3.2 heap 에서 존재하는 free block을 free list에서 찾을 수 있어야 한다. 또 free list에 있는 free block은 heap에서 찾을 수 있어야 한다.(GET_ALLOC으로 검사했을 때 free 이어야 함)
            3.3 free list에 존재하는 block을 heap에서 봤을 때 앞뒤로 free block이 존재해서는 안 된다.(immediate coalesce 정책을 적용중이기 때문이다.)



---
위 invariant를 가지고 checker를 만들어본다면 다음과 같이 만들 수 있다.
1. 개별 블록이 header, footer와 더불어 pred, succ를 위한 공간을 가지고 있는지 점검한다. 그 이외의 것을 개별 블록 level에서 점검할 수 없다.
    !: 최소 block size / alignment / free block일 때 header-footer 일치

2.1. heap을 순회하면서 free block 개수를 계산한다. 그 만큼 free list를 순회했을 때 개수가 맞는지 확인한다. !: 그 과정에서 같은 노드를 두 번 방문하면 안 된다.
2.2. 모든 pred, succ가 정상적으로 연결되었는지 확인한다. (X.succ == Y 라면, Y.pred == X 이어야 한다.)

3.1. (2.1)에서 이미 체크했다. 중복되니 체크할 필요 없다. !: 2.1하고 겹치는 게 아니라, heap free count와 list node count가 맞는지 보는 별도 축이야.
3.2. heap을 순회하며 free block을 만났을 때 다음을 수행한다. free block의 주소를 기록해둔다. 이후 free list에서 순회를 하며 각 주소를 방문처리한다. free list 순회가 끝났을 때 모든 주소가 방문처리 되어야 한다. 두 번 방문처리 되는 경우는 실패로 처리한다.
3.3. free list를 순회하면서 각 block의 heap에 존재하는지 확인한다. 해당 block의 heap상에서의 물리적으로 인접한 블록인 prev, next가 free block이 아님을 검증한다.


---
### 3.2, 3.3을 위한 checker.. edge case에 대해서 생각해보기
heap에 있고 free list에 없을 수 있다.(split으로 만든 블록이 list에 반영 안 됨, free block이 반영 안 됨.)
heap에 없고 free list에 있을 수 있다. (이미 할당 된 블록/ coalesce 된 블록이 list에서 사라지지 않음)
heap에 있고, free list에서 중복될 수 있다.(coalesce 된 블록이 list에서 사라지지 않음)


그건 3.2, 3.3에 대한 엣지 케이스이긴 했어.



### 다른 checker에 대해서도 생각해보자.
가장 큰 단위를 검사했으니 가장 작은 단위도 검사해야 한다.
1. 개별 블록이 header, footer와 더불어 pred, succ를 위한 공간을 가지고 있는지 점검한다. 그 이외의 것을 개별 블록 level에서 점검할 수 없다.
    !: 최소 block size / alignment / free block일 때 header-footer 일치
