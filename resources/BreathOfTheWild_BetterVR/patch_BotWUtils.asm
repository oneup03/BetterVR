[BetterVR_Utils_V208]
moduleMatches = 0x6267BFD0

.origin = codecave

; String Comparison Function (r3 = string1, r4 = string2, sets comparison register so use beq for true, bne for false)
_compareString:
stwu r1, -0x20(r1)
mflr r0
stw r0, 0x04(r1)
stw r3, 0x0C(r1)
stw r4, 0x10(r1)
stw r5, 0x14(r1)
stw r6, 0x18(r1)


startLoop:
lbz r5, 0(r3)
lbz r6, 0(r4)

cmpwi r5, 0
bne checkForMatch
cmpwi r6, 0
bne checkForMatch
b foundMatch

checkForMatch:
cmpw r5, r6
bne noMatch
addi r3, r3, 1
addi r4, r4, 1
b startLoop

noMatch:
; this sets the comparison register to 0 aka false
li r5, 0
cmpwi r5, 1337
b end

foundMatch:
li r5, 1337
cmpwi r5, 1337
b end

end:
lwz r3, 0x0C(r1)
lwz r4, 0x10(r1)
lwz r5, 0x14(r1)
lwz r6, 0x18(r1)
lwz r0, 0x04(r1)
mtlr r0
addi r1, r1, 0x20
blr


; Log to OSReport using format string
loadLineCharacter:
.int 10
.align 4
; r0 should be modifiable
; r3 = format string
; r4 = int arg1
; r5 = int arg2
; f1 = float arg1
; f2 = float arg2
printToLog:
mflr r0
stwu r1, -0x40(r1)
stw r0, 0x14(r1)
stw r5, 0x8(r1)
stw r6, 0xC(r1)

lis r6, loadLineCharacter@ha
lwz r6, loadLineCharacter@l(r6)
crxor 4*cr1+eq, 4*cr1+eq, 4*cr1+eq
bl import.coreinit.OSReport

lwz r6, 0xC(r1)
lwz r5, 0x8(r1)
lwz r0, 0x14(r1)
mtlr r0
addi r1, r1, 0x40 ; this was set to 0x10 before, but that makes no sense?
blr

; =======================================================================================================================


formatLayer3DDrawingStepsStr:
.string "gsys::Layer3D::draw( this = {:08X}, step = {} )"

logLayer3DDrawSteps:
mflr r11
stwu r1, -0x20(r1)
stw r11, 0x24(r1)
stw r3, 0x1C(r1)
stw r4, 0x18(r1)
stw r5, 0x14(r1)
stw r6, 0x10(r1)
stw r7, 0x0C(r1)
stw r8, 0x08(r1)

lwz r11, 0x0(r30) ; original instruction

lis r3, formatLayer3DDrawingStepsStr@ha
addi r3, r3, formatLayer3DDrawingStepsStr@l
lwz r4, 0x1C(r1)
mr r5, r11
bla import.coreinit.hook_OSReportToConsole4

lwz r8, 0x08(r1)
lwz r7, 0x0C(r1)
lwz r6, 0x10(r1)
lwz r5, 0x14(r1)
lwz r4, 0x18(r1)
lwz r3, 0x1C(r1)
lwz r11, 0x24(r1)
addi r1, r1, 0x20
mtlr r11

lwz r11, 0x0(r30) ; original instruction
blr

0x0397A9A0 = bla logLayer3DDrawSteps

; =======================================================================================================================

format_layerJobStr:
.string "agl::lyr::LayerJob::pushBackTo( this = {:08X}, layer = {:08X}, layerName = {}, renderDisplay = {:08X} )"

unknownLayerNameStr:
.string "Unknown Layer Name"

hook_lyr_LayerJob_pushbackJob:
stw r4, 0x14(r3)

mflr r4
stwu r1, -0x20(r1)
stw r4, 0x24(r1)
lwz r4, 0x1C(r3)
stw r3, 0x1C(r1)
stw r4, 0x18(r1)
stw r5, 0x14(r1)
stw r6, 0x10(r1)
stw r7, 0x0C(r1)
stw r8, 0x08(r1)

lwz r4, 0x1C(r1)
lwz r5, 0x10(r4)
cmpwi r5, 0
lis r6, unknownLayerNameStr@ha
addi r6, r6, unknownLayerNameStr@l
beq dontReadLayerName
addi r6, r5, 0x9C ; layer name

dontReadLayerName:
lis r3, format_layerJobStr@ha
addi r3, r3, format_layerJobStr@l
lwz r4, 0x1C(r1)
lwz r7, 0x14(r4)
; r3 = format_layerJobStr
; r4 = LayerJob* this
; r5 = LayerJob->layer*
; r6 = LayerJob->layer->name*
; r7 = LayerJob->renderDisplay*
bla import.coreinit.hook_OSReportToConsole3

lwz r3, 0x1C(r1)
lwz r4, 0x18(r1)
lwz r5, 0x14(r1)
lwz r6, 0x10(r1)
lwz r7, 0x0C(r1)
lwz r8, 0x08(r1)
lwz r4, 0x24(r1)
addi r1, r1, 0x20
mtlr r4

cmpwi r6, 0
stw r5, 0x18(r3)
beq loc_3B4B4E0
lwz r12, 0(r6)
lwz r0, 4(r6)
cmpw r12, r0
bgelr
lwz r0, 8(r6)
slwi r10, r12, 2
stwx r3, r10, r0
lwz r12, 0(r6)
addi r12, r12, 1
stw r12, 0(r6)
blr
loc_3B4B4E0:
lwz r12, 8(r3)
lwz r10, 0x14(r12)
mtctr r10
bctr # agl__lyr__LayerJob__invoke
blr

0x03B4B4A4 = ba hook_lyr_LayerJob_pushbackJob