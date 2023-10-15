;
; Input signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; POSITION                 0   xyzw        0     NONE   float   xyzw
; COLOR                    0   xyzw        1     NONE   float   xyzw
;
;
; Output signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; SV_Position              0   xyzw        0      POS   float   xyzw
; COLOR                    0   xyzw        1     NONE   float   xyzw
;
; shader hash: d24e9153d770c1fc6ae5c351385c7533
;
; Pipeline Runtime Information: 
;
; Vertex Shader
; OutputPositionPresent=1
;
;
; Input signature:
;
; Name                 Index             InterpMode DynIdx
; -------------------- ----- ---------------------- ------
; POSITION                 0                              
; COLOR                    0                              
;
; Output signature:
;
; Name                 Index             InterpMode DynIdx
; -------------------- ----- ---------------------- ------
; SV_Position              0          noperspective       
; COLOR                    0                 linear       
;
; Buffer Definitions:
;
; cbuffer cbRotationAngle
; {
;
;   struct cbRotationAngle
;   {
;
;       struct struct.CBRotationAngle
;       {
;
;           float rotAngle;                           ; Offset:    0
;           int paddings[252];                        ; Offset:   16
;       
;       } cbRotationAngle;                            ; Offset:    0
;
;   
;   } cbRotationAngle;                                ; Offset:    0 Size:  4036
;
; }
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; cbRotationAngle                   cbuffer      NA          NA     CB0            cb0     1
;
;
; ViewId state:
;
; Number of inputs: 8, outputs: 8
; Outputs dependent on ViewId: {  }
; Inputs contributing to computation of Outputs:
;   output 0 depends on inputs: { 0, 1 }
;   output 1 depends on inputs: { 0, 1 }
;   output 2 depends on inputs: { 2, 3 }
;   output 3 depends on inputs: { 3 }
;   output 4 depends on inputs: { 4 }
;   output 5 depends on inputs: { 5 }
;   output 6 depends on inputs: { 6 }
;   output 7 depends on inputs: { 7 }
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.CBufRet.f32 = type { float, float, float, float }
%cbRotationAngle = type { %struct.CBRotationAngle }
%struct.CBRotationAngle = type { float, [252 x i32] }

define void @VSMain() {
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %3 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %4 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %5 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %6 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %7 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %8 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %9 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %10 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %11 = extractvalue %dx.types.CBufRet.f32 %10, 0
  %12 = fmul fast float %11, 0x3F91DF46A0000000
  %13 = call float @dx.op.unary.f32(i32 12, float %12)  ; Cos(value)
  %14 = call float @dx.op.unary.f32(i32 13, float %12)  ; Sin(value)
  %15 = fsub fast float -0.000000e+00, %14
  %16 = fmul fast float %13, %6
  %17 = call float @dx.op.tertiary.f32(i32 46, float %7, float %15, float %16)  ; FMad(a,b,c)
  %18 = fmul fast float %14, %6
  %19 = call float @dx.op.tertiary.f32(i32 46, float %7, float %13, float %18)  ; FMad(a,b,c)
  %20 = call float @dx.op.tertiary.f32(i32 46, float %8, float -1.000000e+00, float 0.000000e+00)  ; FMad(a,b,c)
  %21 = call float @dx.op.tertiary.f32(i32 46, float %9, float 0x3FD3333300000000, float %20)  ; FMad(a,b,c)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %17)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %19)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %21)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %9)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 0, float %2)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 1, float %3)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 2, float %4)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 3, float %5)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  ret void
}

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #0

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #1

; Function Attrs: nounwind readnone
declare float @dx.op.unary.f32(i32, float) #0

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32, %dx.types.Handle, i32) #2

; Function Attrs: nounwind readnone
declare float @dx.op.tertiary.f32(i32, float, float, float) #0

; Function Attrs: nounwind readonly
declare %dx.types.Handle @dx.op.createHandle(i32, i8, i32, i32, i1) #2

attributes #0 = { nounwind readnone }
attributes #1 = { nounwind }
attributes #2 = { nounwind readonly }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!2}
!dx.shaderModel = !{!3}
!dx.resources = !{!4}
!dx.viewIdState = !{!7}
!dx.entryPoints = !{!8}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 1, i32 5}
!2 = !{i32 1, i32 6}
!3 = !{!"vs", i32 6, i32 5}
!4 = !{null, null, !5, null}
!5 = !{!6}
!6 = !{i32 0, %cbRotationAngle* undef, !"", i32 0, i32 0, i32 1, i32 4036, null}
!7 = !{[10 x i32] [i32 8, i32 8, i32 3, i32 3, i32 4, i32 12, i32 16, i32 32, i32 64, i32 128]}
!8 = !{void ()* @VSMain, !"VSMain", !9, !4, null}
!9 = !{!10, !15, null}
!10 = !{!11, !14}
!11 = !{i32 0, !"POSITION", i8 9, i8 0, !12, i8 0, i32 1, i8 4, i32 0, i8 0, !13}
!12 = !{i32 0}
!13 = !{i32 3, i32 15}
!14 = !{i32 1, !"COLOR", i8 9, i8 0, !12, i8 0, i32 1, i8 4, i32 1, i8 0, !13}
!15 = !{!16, !17}
!16 = !{i32 0, !"SV_Position", i8 9, i8 3, !12, i8 4, i32 1, i8 4, i32 0, i8 0, !13}
!17 = !{i32 1, !"COLOR", i8 9, i8 0, !12, i8 2, i32 1, i8 4, i32 1, i8 0, !13}
