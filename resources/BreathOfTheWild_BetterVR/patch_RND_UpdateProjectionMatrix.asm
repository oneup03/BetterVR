[BetterVR_UpdateProjectionMatrix_V208]
moduleMatches = 0x6267BFD0

.origin = codecave

; todo: prevent camera from moving 

updateCameraPositionAndTarget:
; repeat instructions from either branches
lfs f0, 0xEC0(r31)
stfs f0, 0x5CC(r31)
lfs f13, 0xEB8(r31)
stfs f13, 0x5C4(r31)

; function prologue
mflr r0
stwu r1, -0x58(r1)
stw r0, 0x5C(r1)
stw r3, 0x54(r1)

lis r3, currentEyeSide@ha
lwz r3, currentEyeSide@l(r3)
bl import.coreinit.hook_updateCameraOLD

exit_updateCameraPositionAndTarget:
; function epilogue
lwz r3, 0x54(r1)
lwz r0, 0x5C(r1)
mtlr r0
addi r1, r1, 0x58

addi r3, r31, 0xE78
blr


0x02C054FC = bla updateCameraPositionAndTarget
0x02C05590 = bla updateCameraPositionAndTarget


0x02C0378C = act_GetCamera:

; disable CameraChase()
; todo: implement an alternative camera rotation solution
custom_CameraChase_Update:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x1C(r1)
stw r4, 0x18(r1)
stw r5, 0x14(r1)

; call original act::GetCamera() function in case hook_CameraRotationControl isn't hooked
lis r3, act_GetCamera@ha
addi r3, r3, act_GetCamera@l
mtctr r3
lwz r3, 0x1C(r1)
bctrl

bl import.coreinit.hook_CameraRotationControl

lwz r5, 0x14(r1)
lwz r4, 0x18(r1)
;lwz r3, 0x1C(r1) ; r3 is the return value of CameraChase_Update
lwz r0, 0x24(r1)
addi r1, r1, 0x20
mtlr r0
blr

; disable CameraChase
0x02B9B930 = bla custom_CameraChase_Update
; disable CameraClimbObj
0x02B9EECC = bla custom_CameraChase_Update
