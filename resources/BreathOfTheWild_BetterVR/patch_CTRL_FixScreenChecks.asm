[BetterVR_StereoRendering_ScreenChecks_V208]
moduleMatches = 0x6267BFD0

.origin = codecave

; disable agl::fx::Cloud::drawSunOcc which uses texture readback
;0x0340425C = cmpwi r1, 0

; disable peekTexture_checksQueuedRegions to narrow down interesting readback function
;0x030F1B78 = nop

; hook GX2SetPixelUniformBlock to hook the right pixels
hook_GX2SetPixelUniformBlock:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x1C(r1)
stw r4, 0x18(r1)
stw r5, 0x14(r1)
stw r6, 0x10(r1)
stw r7, 0x0C(r1)

; check size of uniform block being set
; r3 is uniform block handle
; r4 is the size of the uniform block data in bytes
; r5 is the pointer to the uniform block data to be set
cmpwi r4, 0x20
bne skipHook_GX2SetPixelUniformBlock

; modify values in the uniform block data to be set
bla import.coreinit.hook_ModifyPixelUniformBlockData

skipHook_GX2SetPixelUniformBlock:
bla import.gx2.GX2SetPixelUniformBlock

lwz r7, 0x0C(r1)
lwz r6, 0x10(r1)
lwz r5, 0x14(r1)
lwz r4, 0x18(r1)
;lwz r3, 0x1C(r1) ; r3 should be the return value of GX2SetPixelUniformBlock
lwz r0, 0x24(r1)
addi r1, r1, 0x20
mtlr r0
blr

0x038B2B2C = bla hook_GX2SetPixelUniformBlock