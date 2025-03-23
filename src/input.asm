.data
select: .word 1
n: .word 13
arr: .word 5,4,3,2,5,10,23,100,1,2,1,1,2

.text
#x2 - temporary origin to data
lui x2,0x10000
#x3 - select
lw x3,0(x2)
#x4 - n
lw x4,0x4(x2)
#x5 - starting address of arr
addi x5,x2,0x8
#x6 - starting address of destination arr
addi x6,x2,0x500

# for(int i=0;i<n;i++){
#     for(int j=0;i<n-1;j++){
#         if(a[j]>a[j+1]){
#             swap(a[j],a[j+1])
#         }
#     }
# }

addi x7,x0,0
copy:
    beq x7,x4,exitcopy
    slli x30,x7,2
    add x8,x5,x30 #source
    add x9,x6,x30 #destination
    lw x10,0(x8)
    sw x10,0(x9)
    addi x7,x7,1
    beq x0,x0,copy
exitcopy:   

#select if x3 is not 0 then optimised
bgt x3,x0,optimised

#x7 - I
addi x7,x0,0
#x30 - n-1
addi x30,x4,-1
outer:
    beq x7,x4,exit
    addi x7,x7,1
    #x8 - j
    addi x8,x0,0
    inner:
        beq x8,x30,outer
        #x11 - pointer to the copied array
        slli x31,x8,2
        add x11,x6,x31
        #x9 - a[j] , x10 - a[j+1]
        lw x9,0(x11)
        lw x10,0x4(x11)
        bgt x9,x10,swap
        addi x8,x8,1
        beq x0,x0,inner
    swap:
        sw x9,0x4(x11)
        sw x10,0(x11)
        addi x8,x8,1
        beq x0,x0,inner 
        
optimised:
#x7 - I
addi x7,x0,0
#x30 - n-1
addi x30,x4,0
addi x20,x3,0
out: 
   beq x7,x4,exit
   beq x20,x0,exit
   #x20 - flag
   addi x20,x0,0
   addi x7,x7,1
   #x8 - j
   addi x8,x0,0
   #x30 - n-2-i
   sub x30,x30,x7
   in:
    beq x8,x30,outer
    #x11 - pointer to the copied array
    slli x31,x8,2
    add x11,x6,x31 
    lw x9,0(x11)
    lw x10,0x4(x11)
    bgt x9,x10,sw
    addi x8,x8,1
    beq x0,x0,inner
    sw:
        sw x9,0x4(x11)
        sw x10,0(x11)
        addi x8,x8,1
        addi x20,x0,1
        beq x0,x0,in   
exit:
    
