[BetterVR_Find2DFrameBuffer_V208]
moduleMatches = 0x6267BFD0

.origin = codecave


magic2DColorValue:
.float (2.0 / 32.0)
.float 0.123456789
.float 0.987654321
.float 0.0

str_printClear2DColorBuffer_left:
.string "[PPC] Clearing 2D color buffer with left eye"
str_printClear2DColorBuffer_right:
.string "[PPC] Clearing 2D color buffer with right eye"

clear2DColorBuffer:
mflr r5
stwu r1, -0x20(r1)
stw r5, 0x24(r1)

stfs f1, 0x20(r1)
stfs f2, 0x1C(r1)
stfs f3, 0x18(r1)
stfs f4, 0x14(r1)
stw r3, 0x10(r1)

lis r3, magic2DColorValue@ha
lfs f1, magic2DColorValue@l+0x0(r3)
lfs f2, magic2DColorValue@l+0x4(r3)
lfs f3, magic2DColorValue@l+0x8(r3)
lfs f4, magic2DColorValue@l+0xC(r3)

; ignore the right eye for now
; todo: have the GUI update for both eyes, and then capture the right eye since it'd be more responsive
lis r3, currentEyeSide@ha
lwz r3, currentEyeSide@l(r3)
cmpwi r3, 1
;beq skipClearing2DColorBuffer

; clear the GX2 texture for the right eye, which translates to a vkCmdClearColorImage call that identifies the Vulkan image
mr r3, r30 ; r3 is now the agl::RenderBuffer object
cmpwi r3, 0
beq skipClearing2DColorBuffer
addi r3, r3, 0x1C ; r3 is now the agl::RenderBuffer::mColorBuffer array
lwz r3, 0(r3) ; r3 is now the agl::RenderBuffer::mColorBuffer[0] object
cmpwi r3, 0
beq skipClearing2DColorBuffer
addi r3, r3, 0xBC ; r3 is now the agl::RenderBuffer::mColorBuffer[0]::mGX2FrameBuffer object

bl import.gx2.GX2ClearColor
bla import.gx2.GX2DrawDone

stwu r1, -0x20(r1)
stw r3, 0x1C(r1)
stw r4, 0x18(r1)
stw r5, 0x14(r1)
stw r6, 0x10(r1)

lis r3, currentEyeSide@ha
lwz r3, currentEyeSide@l(r3)
cmpwi r3, 0
lis r3, str_printClear2DColorBuffer_left@ha
addi r3, r3, str_printClear2DColorBuffer_left@l
beq actualPrint_3

lis r3, str_printClear2DColorBuffer_right@ha
addi r3, r3, str_printClear2DColorBuffer_right@l
actualPrint_3:
li r4, 10
crxor 4*cr1+eq, 4*cr1+eq, 4*cr1+eq
bl import.coreinit.hook_OSReportToConsole

lwz r3, 0x1C(r1)
lwz r4, 0x18(r1)
lwz r5, 0x14(r1)
lwz r6, 0x10(r1)
addi r1, r1, 0x20

skipClearing2DColorBuffer:
lfs f1, 0x20(r1)
lfs f2, 0x1C(r1)
lfs f3, 0x18(r1)
lfs f4, 0x14(r1)
lwz r3, 0x10(r1)

lwz r5, 0x24(r1)
mtlr r5
addi r1, r1, 0x20

mr r5, r30 ; original instruction
blr

0x03A147B4 = bla clear2DColorBuffer
