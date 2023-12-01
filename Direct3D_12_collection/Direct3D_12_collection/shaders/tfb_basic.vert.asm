;
; Note: shader requires additional functionality:
;       UAVs at every shader stage
;
;
; Input signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; POSITION                 0   xyzw        0     NONE   float   xyzw
; COLOR                    0   xyzw        1     NONE   float   xyzw
; SV_VertexID              0   x           2   VERTID    uint   x   
;
;
; Output signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; SV_Position              0   xyzw        0      POS   float   xyzw
; COLOR                    0   xyzw        1     NONE   float   xyzw
;
; shader hash: f46d535ddaef66892c0724803736f6ef
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
; SV_VertexID              0                              
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
;           int paddings[63];                         ; Offset:   16
;       
;       } cbRotationAngle;                            ; Offset:    0
;
;   
;   } cbRotationAngle;                                ; Offset:    0 Size:  1012
;
; }
;
; Resource bind info for uavOutput
; {
;
;   struct struct.PSInput
;   {
;
;       float4 position;                              ; Offset:    0
;       float4 color;                                 ; Offset:   16
;   
;   } $Element;                                       ; Offset:    0 Size:    32
;
; }
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; cbRotationAngle                   cbuffer      NA          NA     CB0            cb0     1
; uavOutput                             UAV  struct         r/w      U0             u0     1
;
;
; ViewId state:
;
; Number of inputs: 9, outputs: 8
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
%"class.RWStructuredBuffer<PSInput>" = type { %struct.PSInput }
%struct.PSInput = type { <4 x float>, <4 x float> }
%cbRotationAngle = type { %struct.CBRotationAngle }
%struct.CBRotationAngle = type { float, [63 x i32] }

define void @VSMain() {
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %3 = call i32 @dx.op.loadInput.i32(i32 4, i32 2, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %4 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %5 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %6 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %7 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %8 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %9 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %10 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %11 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %12 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %2, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %13 = extractvalue %dx.types.CBufRet.f32 %12, 0
  %14 = fmul fast float %13, 0x3F91DF46A0000000
  %15 = call float @dx.op.unary.f32(i32 12, float %14)  ; Cos(value)
  %16 = call float @dx.op.unary.f32(i32 13, float %14)  ; Sin(value)
  %17 = fsub fast float -0.000000e+00, %16
  %18 = fmul fast float %15, %8
  %19 = call float @dx.op.tertiary.f32(i32 46, float %9, float %17, float %18)  ; FMad(a,b,c)
  %20 = fmul fast float %16, %8
  %21 = call float @dx.op.tertiary.f32(i32 46, float %9, float %15, float %20)  ; FMad(a,b,c)
  %22 = call float @dx.op.tertiary.f32(i32 46, float %10, float -1.000000e+00, float 0.000000e+00)  ; FMad(a,b,c)
  %23 = call float @dx.op.tertiary.f32(i32 46, float %11, float 0x3FD3333300000000, float %22)  ; FMad(a,b,c)
  call void @dx.op.rawBufferStore.f32(i32 140, %dx.types.Handle %1, i32 %3, i32 0, float %19, float %21, float %23, float %11, i8 15, i32 4)  ; RawBufferStore(uav,index,elementOffset,value0,value1,value2,value3,mask,alignment)
  call void @dx.op.rawBufferStore.f32(i32 140, %dx.types.Handle %1, i32 %3, i32 16, float %4, float %5, float %6, float %7, i8 15, i32 4)  ; RawBufferStore(uav,index,elementOffset,value0,value1,value2,value3,mask,alignment)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %19)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %21)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %23)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %11)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 0, float %4)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 1, float %5)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 2, float %6)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 3, float %7)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  ret void
}

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #0

; Function Attrs: nounwind readnone
declare i32 @dx.op.loadInput.i32(i32, i32, i32, i8, i32) #0

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #1

; Function Attrs: nounwind readnone
declare float @dx.op.unary.f32(i32, float) #0

; Function Attrs: nounwind
declare void @dx.op.rawBufferStore.f32(i32, %dx.types.Handle, i32, i32, float, float, float, float, i8, i32) #1

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
!dx.viewIdState = !{!10}
!dx.entryPoints = !{!11}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 1, i32 5}
!2 = !{i32 1, i32 6}
!3 = !{!"vs", i32 6, i32 5}
!4 = !{null, !5, !8, null}
!5 = !{!6}
!6 = !{i32 0, %"class.RWStructuredBuffer<PSInput>"* undef, !"", i32 0, i32 0, i32 1, i32 12, i1 false, i1 false, i1 false, !7}
!7 = !{i32 1, i32 32}
!8 = !{!9}
!9 = !{i32 0, %cbRotationAngle* undef, !"", i32 0, i32 0, i32 1, i32 1012, null}
!10 = !{[11 x i32] [i32 9, i32 8, i32 3, i32 3, i32 4, i32 12, i32 16, i32 32, i32 64, i32 128, i32 0]}
!11 = !{void ()* @VSMain, !"VSMain", !12, !4, !23}
!12 = !{!13, !20, null}
!13 = !{!14, !17, !18}
!14 = !{i32 0, !"POSITION", i8 9, i8 0, !15, i8 0, i32 1, i8 4, i32 0, i8 0, !16}
!15 = !{i32 0}
!16 = !{i32 3, i32 15}
!17 = !{i32 1, !"COLOR", i8 9, i8 0, !15, i8 0, i32 1, i8 4, i32 1, i8 0, !16}
!18 = !{i32 2, !"SV_VertexID", i8 5, i8 1, !15, i8 0, i32 1, i8 1, i32 2, i8 0, !19}
!19 = !{i32 3, i32 1}
!20 = !{!21, !22}
!21 = !{i32 0, !"SV_Position", i8 9, i8 3, !15, i8 4, i32 1, i8 4, i32 0, i8 0, !16}
!22 = !{i32 1, !"COLOR", i8 9, i8 0, !15, i8 2, i32 1, i8 4, i32 1, i8 0, !16}
!23 = !{i32 0, i64 65552}
