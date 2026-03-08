[BetterVR_StereoRendering_CameraAndProjection_V208]
moduleMatches = 0x6267BFD0

.origin = codecave

0x1046CF88 = Camera__sInstance:
0x03191B70 = Camera__getLookAtCamera:

const_0:
.float 0.0
0x10323118 = const_epsilon:

0x105978C4 = sDefaultCameraMatrix:

modifiedCopy_seadLookAtCamera:
.float 0,0,0,0  ; mtx
.float 0,0,0,0
.float 0,0,0,0
.int 0x1027B294 ; vtable
.float 10,2,-20 ; pos
.float 10,4,-19 ; target
.float 0,1,0    ; up

testForOverflow:
.int 0x60000000

agl__lyr__Layer__getRenderCamera:
lhz r0, 0x52(r3)
clrlwi. r12, r0, 31
bne testCameraFlags
extrwi. r11, r0, 1,26
beq returnCameraFrom1stParam

testCameraFlags:
extrwi. r11, r0, 1,30
beq returnCameraFrom1stParam
lwz r12, 0x19C(r3)
cmpwi r12, 0
bne returnDefaultCamera

returnCameraFrom1stParam:
lwz r0, 0x48(r3)
lis r3, sDefaultCameraMatrix@ha
addi r3, r3, sDefaultCameraMatrix@l
cmpwi r0, 0
beqlr ; if (layer->renderCamera == nullptr) return sDefaultCameraMatrix;
mr r3, r0 ; r3 = layer->renderCamera

; camera.pos.x.getLE() != 0.0f
lis r12, const_0@ha
lfs f12, const_0@l(r12)
lfs f13, 0x34(r3)
fcmpu cr0, f12, f13
beqlr ; return layer->renderCamera

; std::fabs(camera.at.z.getLE()) < std::numeric_limits<float>::epsilon()
fmr f13, f1
lfs f1, 0x48(r3)
.int 0xFC200A10 ; fabs f1, f1
fmr f12, f1
fmr f1, f13
lis r12, const_epsilon@ha
lfs f13, const_epsilon@l(r12)
fcmpu cr0, f12, f13
bltlr ; return layer->renderCamera

lis r12, useStubHooks@ha
lwz r12, useStubHooks@l(r12)
cmpwi r12, 1
lis r12, stub_hook_GetRenderCamera@ha
addi r12, r12, stub_hook_GetRenderCamera@l
mtctr r12
beq getRenderCamera
lis r12, import.coreinit.hook_GetRenderCamera@ha
addi r12, r12, import.coreinit.hook_GetRenderCamera@l
mtctr r12
getRenderCamera:

lis r11, currentEyeSide@ha
lwz r11, currentEyeSide@l(r11)
lis r12, modifiedCopy_seadLookAtCamera@ha
addi r12, r12, modifiedCopy_seadLookAtCamera@l
bctr ; jump to hook_GetRenderCamera
blr

returnDefaultCamera:
addi r3, r12, 4
blr

0x03AE4AA0 = ba agl__lyr__Layer__getRenderCamera


modifiedCopy_seadPerspectiveProjection:
.byte 0,0,0,0 ; dirty, deviceDirty, pad, pad
.float 0,0,0,0 ; mtx
.float 0,0,0,0
.float 0,0,0,0
.float 0,0,0,0
.float 0,0,0,0 ; deviceMtx
.float 0,0,0,0
.float 0,0,0,0
.float 0,0,0,0
.int 0 ; devicePosture
.float 0 ; deviceZScale
.float 0 ; deviceZOffset
.int 0x1027B54C ; vtable
.float 0.1 ; near
.float 25000 ; far
.float 0.8726647 ; angle
.float 0.4226183 ; fovySin
.float 0.9063078 ; fovyCos
.float 0.4663077 ; fovyTan
.float 1.7777778 ; aspect
.float 0 ; offsetX
.float 0 ; offsetY


0x1027B54C = seadPerspectiveProjection_vtbl:
0x10597928 = sDefaultSeadProjection:

agl__lyr__Layer__getRenderProjection:
lhz r0, 0x54(r3)
extrwi. r0, r0, 1,30
beq returnProjectionFrom1stParam
lwz r12, 0x19C(r3)
cmpwi r12, 0
bne useSpecialProjection

returnProjectionFrom1stParam:
lwz r12, 0x4C(r3)
lis r3, sDefaultSeadProjection@ha
addi r3, r3, sDefaultSeadProjection@l
cmpwi r12, 0
beqlr
mr r3, r12

; prevent modifying anything but sead::PerspectiveProjection
lis r11, seadPerspectiveProjection_vtbl@ha
addi r11, r11, seadPerspectiveProjection_vtbl@l
lwz r12, 0x90(r3)
cmpw r12, r11
bnelr

lis r12, useStubHooks@ha
lwz r12, useStubHooks@l(r12)
cmpwi r12, 1
lis r12, stub_hook_GetRenderProjection@ha
addi r12, r12, stub_hook_GetRenderProjection@l
mtctr r12
beq getRenderProjection
lis r12, import.coreinit.hook_GetRenderProjection@ha
addi r12, r12, import.coreinit.hook_GetRenderProjection@l
mtctr r12
getRenderProjection:

lis r12, currentEyeSide@ha
lwz r0, currentEyeSide@l(r12)
lis r12, modifiedCopy_seadPerspectiveProjection@ha
addi r12, r12, modifiedCopy_seadPerspectiveProjection@l
bctr ; jump to hook_GetRenderProjection
blr

useSpecialProjection:
lwz r3, 0xEC(r12)
blr

0x03AE4AEC = ba agl__lyr__Layer__getRenderProjection

; =================================================================================
; most rendering functions use getRenderProjection to get the projection matrix
; however, the light pre-pass ALSO uses sead::Projection::getProjectionMatrix
; the patch below fixes the lights not lighting up objects/walls correctly in VR
; =================================================================================

0x030C0F4C = sead_projection_updateMatrixImpl:
0x033DBBB8 = returnAddress_lightPrePassProjectionMatrix:

custom_sead__Projection__getProjectionMatrix:
mflr r0
stwu r1, -0x20(r1)
stw r31, 0x0C(r1)
stw r0, 0x24(r1)
stw r11, 0x18(r1)
stw r12, 0x14(r1)

; random crash fix where the projection is null
cmpwi r3, 0
beq returnDefaultProjectionMatrix

; prevent modifying anything but sead::PerspectiveProjection
lwz r12, 0x90(r3)
lis r11, seadPerspectiveProjection_vtbl@ha
addi r11, r11, seadPerspectiveProjection_vtbl@l
cmpw r12, r11
bne continue_sead__Projection__getProjectionMatrix

; only hook the light pre-pass projection matrix change
lis r11, returnAddress_lightPrePassProjectionMatrix@ha
addi r11, r11, returnAddress_lightPrePassProjectionMatrix@l
cmpw r0, r11
bne continue_sead__Projection__getProjectionMatrix

; use custom modified projection matrix for light pre-pass while stubbed hooks are enabled, instead of running C++ hooks
lis r12, useStubHooks@ha
lwz r12, useStubHooks@l(r12)
cmpwi r12, 1
beq returnDefaultProjectionMatrix

; call C++ code to modify the projection matrix to use the VR projection matrices for each eye
lis r11, currentEyeSide@ha
lwz r11, currentEyeSide@l(r11)
bla import.coreinit.hook_ModifyLightPrePassProjectionMatrix

continue_sead__Projection__getProjectionMatrix:
lis r31, sead_projection_updateMatrixImpl@ha
addi r31, r31, sead_projection_updateMatrixImpl@l
mtctr r31
mr r31, r3
bctrl ; bl sead::Projection::updateMatrix
b exit_custom_sead__Projection__getProjectionMatrix

returnDefaultProjectionMatrix:
lis r31, example_PerspectiveProjectionMatrix@ha
addi r31, r31, example_PerspectiveProjectionMatrix@l

exit_custom_sead__Projection__getProjectionMatrix:
addi r3, r31, 4
lwz r12, 0x14(r1)
lwz r11, 0x18(r1)
lwz r0, 0x24(r1)
lwz r31, 0x0C(r1)
mtlr r0
addi r1, r1, 0x20
blr

0x030C1008 = ba custom_sead__Projection__getProjectionMatrix

; =================================================================================

0x030C1958 = ba import.coreinit.hook_OverwriteSeadPerspectiveProjectionSet

0x03191BA0 = Camera__getPerspectiveProjection:
0x030C18F0 = sead__PerspectiveProjection__set:

custom_ModifyProjectionUsingCamera:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x1C(r1)
stw r4, 0x18(r1)
stw r5, 0x14(r1)
stw r6, 0x08(r1)
stw r7, 0x04(r1)
stw r30, 0x10(r1)
stw r31, 0x0C(r1)

mr. r31, r4
mr r30, r3
beq exit_custom_ModifyProjectionUsingCamera

lis r3, Camera__getPerspectiveProjection@ha
addi r3, r3, Camera__getPerspectiveProjection@l
mtctr r3
lis r3, Camera__sInstance@ha
lwz r3, Camera__sInstance@l(r3)
bctrl ; bl Camera__getPerspectiveProjection

lwz r12, 0x90(r3)
lwz r0, 0x3C(r12)
mtctr r0
bctrl ; sead__PerspectiveProjection__getAspect

lfs f3, 0x68(r30)
fmr f4, f1
lfs f2, 0x64(r30)
lfs f1, 0x60(r30)
lis r3, sead__PerspectiveProjection__set@ha
addi r3, r3, sead__PerspectiveProjection__set@l
mtctr r3
mr r3, r31
bctrl ; bl sead__PerspectiveProjection__set
lfs f0, 0x6C(r30)
li r11, 1
stfs f0, 0xB0(r31)
lfs f0, 0x70(r30)
stb r11, 0(r31)
stfs f0, 0xB4(r31)

;lwz r3, 0x1C(r1)
;lwz r3, 0x30(r3) ; load vtable address
;lwz r3, 0x24(r3) ; load updateMatrix address
;mtctr r3
;lwz r3, 0x1C(r1)
;mr r4, r3
;bctrl ; bl sead::Projection::updateMatrix

lwz r3, 0x1C(r1)
lwz r4, 0x18(r1)
lis r5, currentEyeSide@ha
lwz r5, currentEyeSide@l(r5)
lwz r6, 0x24(r1)

lwz r7, 0x10(r1)
lwz r7, 0x128(r7)

bla import.coreinit.hook_ModifyProjectionUsingCamera

exit_custom_ModifyProjectionUsingCamera:
lwz r3, 0x1C(r1)
lwz r4, 0x18(r1)
lwz r5, 0x14(r1)
lwz r6, 0x08(r1)
lwz r7, 0x04(r1)
lwz r30, 0x10(r1)
lwz r31, 0x0C(r1)
lwz r0, 0x24(r1)
addi r1, r1, 0x20
mtlr r0
blr

0x02C43450 = bla custom_ModifyProjectionUsingCamera

;0x0318FE7C = ba custom_ModifyProjectionUsingCamera
;0x0318FEFC = ba import.coreinit.hook_ModifyProjectionUsingCamera