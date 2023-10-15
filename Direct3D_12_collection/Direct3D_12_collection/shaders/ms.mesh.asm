;
; Input signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; no parameters
;
; Output signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; SV_Position              0   xyzw        0      POS   float   xyzw
; COLOR                    0   xyzw        1     NONE   float   xyzw
;
; shader hash: afa7f7cd07846ccc60efab24eec00f88
;
; Pipeline Runtime Information: 
;
;
;
; Vertex Output signature:
;
; Name                 Index             InterpMode DynIdx
; -------------------- ----- ---------------------- ------
; SV_Position              0          noperspective       
; COLOR                    0                 linear       
;
; Buffer Definitions:
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
;
;
; ViewId state:
;
; Number of inputs: 0, outputs: 8, primitive outputs: 0
; Outputs dependent on ViewId: {  }
; Primitive Outputs dependent on ViewId: {  }
; Inputs contributing to computation of Outputs:
; Inputs contributing to computation of Primitive Outputs:
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%struct.MyPayloadType = type { [1024 x i32], i32, i32 }

@"\01?xOffsetList@?1??MeshMain@@YAXIV?$vector@I$02@@0UMyPayloadType@@Y0BAA@$$CAUMyOutputVertex@@Y0PO@$$CAV2@@Z@3QBMB" = internal unnamed_addr constant [4 x float] [float -5.000000e-01, float 5.000000e-01, float -5.000000e-01, float 5.000000e-01], align 4
@"\01?yOffsetList@?1??MeshMain@@YAXIV?$vector@I$02@@0UMyPayloadType@@Y0BAA@$$CAUMyOutputVertex@@Y0PO@$$CAV2@@Z@3QBMB" = internal unnamed_addr constant [4 x float] [float 5.000000e-01, float 5.000000e-01, float -5.000000e-01, float -5.000000e-01], align 4

define void @MeshMain() {
  %1 = call i32 @dx.op.threadIdInGroup.i32(i32 95, i32 0)  ; ThreadIdInGroup(component)
  %2 = call i32 @dx.op.groupId.i32(i32 94, i32 0)  ; GroupId(component)
  %3 = call %struct.MyPayloadType* @dx.op.getMeshPayload.struct.MyPayloadType(i32 170)  ; GetMeshPayload()
  call void @dx.op.setMeshOutputCounts(i32 168, i32 256, i32 254)  ; SetMeshOutputCounts(numVertices,numPrimitives)
  %4 = uitofp i32 %1 to float
  %5 = fmul fast float %4, 0x3F70204080000000
  %6 = fadd fast float %5, -2.500000e-01
  %7 = getelementptr inbounds [4 x float], [4 x float]* @"\01?xOffsetList@?1??MeshMain@@YAXIV?$vector@I$02@@0UMyPayloadType@@Y0BAA@$$CAUMyOutputVertex@@Y0PO@$$CAV2@@Z@3QBMB", i32 0, i32 %2
  %8 = load float, float* %7, align 4, !tbaa !15
  %9 = getelementptr inbounds [4 x float], [4 x float]* @"\01?yOffsetList@?1??MeshMain@@YAXIV?$vector@I$02@@0UMyPayloadType@@Y0BAA@$$CAUMyOutputVertex@@Y0PO@$$CAV2@@Z@3QBMB", i32 0, i32 %2
  %10 = load float, float* %9, align 4, !tbaa !15
  %11 = fadd fast float %8, %6
  %12 = fadd fast float %10, -2.500000e-01
  %13 = shl i32 %1, 1
  call void @dx.op.storeVertexOutput.f32(i32 171, i32 0, i32 0, i8 0, float %11, i32 %13)  ; StoreVertexOutput(outputSigId,rowIndex,colIndex,value,vertexIndex)
  call void @dx.op.storeVertexOutput.f32(i32 171, i32 0, i32 0, i8 1, float %12, i32 %13)  ; StoreVertexOutput(outputSigId,rowIndex,colIndex,value,vertexIndex)
  call void @dx.op.storeVertexOutput.f32(i32 171, i32 0, i32 0, i8 2, float 0x3FD3333300000000, i32 %13)  ; StoreVertexOutput(outputSigId,rowIndex,colIndex,value,vertexIndex)
  call void @dx.op.storeVertexOutput.f32(i32 171, i32 0, i32 0, i8 3, float 1.000000e+00, i32 %13)  ; StoreVertexOutput(outputSigId,rowIndex,colIndex,value,vertexIndex)
  %14 = fadd fast float %10, 2.500000e-01
  %15 = or i32 %13, 1
  call void @dx.op.storeVertexOutput.f32(i32 171, i32 0, i32 0, i8 0, float %11, i32 %15)  ; StoreVertexOutput(outputSigId,rowIndex,colIndex,value,vertexIndex)
  call void @dx.op.storeVertexOutput.f32(i32 171, i32 0, i32 0, i8 1, float %14, i32 %15)  ; StoreVertexOutput(outputSigId,rowIndex,colIndex,value,vertexIndex)
  call void @dx.op.storeVertexOutput.f32(i32 171, i32 0, i32 0, i8 2, float 0x3FD3333300000000, i32 %15)  ; StoreVertexOutput(outputSigId,rowIndex,colIndex,value,vertexIndex)
  call void @dx.op.storeVertexOutput.f32(i32 171, i32 0, i32 0, i8 3, float 1.000000e+00, i32 %15)  ; StoreVertexOutput(outputSigId,rowIndex,colIndex,value,vertexIndex)
  %16 = shl i32 %2, 8
  %17 = add i32 %16, %1
  %18 = getelementptr inbounds %struct.MyPayloadType, %struct.MyPayloadType* %3, i32 0, i32 0, i32 %17
  %19 = load i32, i32* %18, align 4, !tbaa !19
  %20 = sitofp i32 %19 to float
  %21 = fmul fast float %20, 3.906250e-03
  %22 = or i32 %16, 128
  %23 = add i32 %22, %1
  %24 = getelementptr inbounds %struct.MyPayloadType, %struct.MyPayloadType* %3, i32 0, i32 0, i32 %23
  %25 = load i32, i32* %24, align 4, !tbaa !19
  %26 = sitofp i32 %25 to float
  %27 = fmul fast float %26, 3.906250e-03
  %28 = call float @dx.op.unary.f32(i32 22, float %21)  ; Frc(value)
  %29 = fsub fast float 1.000000e+00, %28
  %30 = call float @dx.op.unary.f32(i32 22, float %27)  ; Frc(value)
  %31 = fsub fast float 1.000000e+00, %30
  %32 = call float @dx.op.binary.f32(i32 35, float %28, float 0x3FB99999A0000000)  ; FMax(a,b)
  %33 = call float @dx.op.binary.f32(i32 36, float %32, float 0x3FECCCCCC0000000)  ; FMin(a,b)
  %34 = call float @dx.op.binary.f32(i32 35, float %29, float 0x3FB99999A0000000)  ; FMax(a,b)
  %35 = call float @dx.op.binary.f32(i32 36, float %34, float 0x3FECCCCCC0000000)  ; FMin(a,b)
  %36 = call float @dx.op.binary.f32(i32 35, float %30, float 0x3FB99999A0000000)  ; FMax(a,b)
  %37 = call float @dx.op.binary.f32(i32 36, float %36, float 0x3FECCCCCC0000000)  ; FMin(a,b)
  %38 = call float @dx.op.binary.f32(i32 35, float %31, float 0x3FB99999A0000000)  ; FMax(a,b)
  %39 = call float @dx.op.binary.f32(i32 36, float %38, float 0x3FECCCCCC0000000)  ; FMin(a,b)
  call void @dx.op.storeVertexOutput.f32(i32 171, i32 1, i32 0, i8 0, float %33, i32 %13)  ; StoreVertexOutput(outputSigId,rowIndex,colIndex,value,vertexIndex)
  call void @dx.op.storeVertexOutput.f32(i32 171, i32 1, i32 0, i8 1, float %33, i32 %13)  ; StoreVertexOutput(outputSigId,rowIndex,colIndex,value,vertexIndex)
  call void @dx.op.storeVertexOutput.f32(i32 171, i32 1, i32 0, i8 2, float %35, i32 %13)  ; StoreVertexOutput(outputSigId,rowIndex,colIndex,value,vertexIndex)
  call void @dx.op.storeVertexOutput.f32(i32 171, i32 1, i32 0, i8 3, float 1.000000e+00, i32 %13)  ; StoreVertexOutput(outputSigId,rowIndex,colIndex,value,vertexIndex)
  call void @dx.op.storeVertexOutput.f32(i32 171, i32 1, i32 0, i8 0, float %37, i32 %15)  ; StoreVertexOutput(outputSigId,rowIndex,colIndex,value,vertexIndex)
  call void @dx.op.storeVertexOutput.f32(i32 171, i32 1, i32 0, i8 1, float %37, i32 %15)  ; StoreVertexOutput(outputSigId,rowIndex,colIndex,value,vertexIndex)
  call void @dx.op.storeVertexOutput.f32(i32 171, i32 1, i32 0, i8 2, float %39, i32 %15)  ; StoreVertexOutput(outputSigId,rowIndex,colIndex,value,vertexIndex)
  call void @dx.op.storeVertexOutput.f32(i32 171, i32 1, i32 0, i8 3, float 1.000000e+00, i32 %15)  ; StoreVertexOutput(outputSigId,rowIndex,colIndex,value,vertexIndex)
  %40 = icmp ugt i32 %1, 126
  br i1 %40, label %44, label %41

; <label>:41                                      ; preds = %0
  %42 = add i32 %13, 2
  %43 = add i32 %13, 3
  call void @dx.op.emitIndices(i32 169, i32 %13, i32 %13, i32 %15, i32 %42)  ; EmitIndices(PrimitiveIndex,VertexIndex0,VertexIndex1,VertexIndex2)
  call void @dx.op.emitIndices(i32 169, i32 %15, i32 %42, i32 %15, i32 %43)  ; EmitIndices(PrimitiveIndex,VertexIndex0,VertexIndex1,VertexIndex2)
  br label %44

; <label>:44                                      ; preds = %41, %0
  ret void
}

; Function Attrs: nounwind
declare void @dx.op.emitIndices(i32, i32, i32, i32, i32) #0

; Function Attrs: nounwind readonly
declare %struct.MyPayloadType* @dx.op.getMeshPayload.struct.MyPayloadType(i32) #1

; Function Attrs: nounwind readnone
declare i32 @dx.op.groupId.i32(i32, i32) #2

; Function Attrs: nounwind readnone
declare i32 @dx.op.threadIdInGroup.i32(i32, i32) #2

; Function Attrs: nounwind
declare void @dx.op.storeVertexOutput.f32(i32, i32, i32, i8, float, i32) #0

; Function Attrs: nounwind
declare void @dx.op.setMeshOutputCounts(i32, i32, i32) #0

; Function Attrs: nounwind readnone
declare float @dx.op.unary.f32(i32, float) #2

; Function Attrs: nounwind readnone
declare float @dx.op.binary.f32(i32, float, float) #2

attributes #0 = { nounwind }
attributes #1 = { nounwind readonly }
attributes #2 = { nounwind readnone }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!2}
!dx.shaderModel = !{!3}
!dx.viewIdState = !{!4}
!dx.entryPoints = !{!5}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 1, i32 5}
!2 = !{i32 1, i32 6}
!3 = !{!"ms", i32 6, i32 5}
!4 = !{[3 x i32] [i32 0, i32 8, i32 0]}
!5 = !{void ()* @MeshMain, !"MeshMain", !6, null, !12}
!6 = !{null, !7, null}
!7 = !{!8, !11}
!8 = !{i32 0, !"SV_Position", i8 9, i8 3, !9, i8 4, i32 1, i8 4, i32 0, i8 0, !10}
!9 = !{i32 0}
!10 = !{i32 3, i32 15}
!11 = !{i32 1, !"COLOR", i8 9, i8 0, !9, i8 2, i32 1, i8 4, i32 1, i8 0, !10}
!12 = !{i32 9, !13}
!13 = !{!14, i32 256, i32 254, i32 2, i32 4104}
!14 = !{i32 128, i32 1, i32 1}
!15 = !{!16, !16, i64 0}
!16 = !{!"float", !17, i64 0}
!17 = !{!"omnipotent char", !18, i64 0}
!18 = !{!"Simple C/C++ TBAA"}
!19 = !{!20, !20, i64 0}
!20 = !{!"int", !17, i64 0}
