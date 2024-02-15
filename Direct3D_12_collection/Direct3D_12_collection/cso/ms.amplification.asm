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
; no parameters
; shader hash: 78bc2df118103e8f99e2213d07aebaa2
;
; Pipeline Runtime Information: 
;
;
;
; Buffer Definitions:
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%struct.MyPayloadType = type { [1024 x i32], i32, i32 }

define void @AmplificationMain() {
  %1 = call i32 @dx.op.groupId.i32(i32 94, i32 0)  ; GroupId(component)
  %2 = call i32 @dx.op.groupId.i32(i32 94, i32 1)  ; GroupId(component)
  %3 = call i32 @dx.op.groupId.i32(i32 94, i32 2)  ; GroupId(component)
  %4 = alloca %struct.MyPayloadType, align 4
  %5 = mul i32 %2, %1
  %6 = mul i32 %5, %3
  br label %7

; <label>:7                                       ; preds = %7, %0
  %8 = phi i32 [ 0, %0 ], [ %11, %7 ]
  %9 = add nsw i32 %8, %6
  %10 = getelementptr inbounds %struct.MyPayloadType, %struct.MyPayloadType* %4, i32 0, i32 0, i32 %8
  store i32 %9, i32* %10, align 4, !tbaa !8
  %11 = add nuw nsw i32 %8, 1
  %12 = icmp eq i32 %11, 1024
  br i1 %12, label %13, label %7

; <label>:13                                      ; preds = %7
  %14 = getelementptr inbounds %struct.MyPayloadType, %struct.MyPayloadType* %4, i32 0, i32 1
  store i32 %1, i32* %14, align 4, !tbaa !8
  %15 = getelementptr inbounds %struct.MyPayloadType, %struct.MyPayloadType* %4, i32 0, i32 2
  store i32 4, i32* %15, align 4, !tbaa !8
  call void @dx.op.dispatchMesh.struct.MyPayloadType(i32 173, i32 4, i32 1, i32 1, %struct.MyPayloadType* nonnull %4)  ; DispatchMesh(threadGroupCountX,threadGroupCountY,threadGroupCountZ,payload)
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @dx.op.groupId.i32(i32, i32) #0

; Function Attrs: nounwind
declare void @dx.op.dispatchMesh.struct.MyPayloadType(i32, i32, i32, i32, %struct.MyPayloadType*) #1

attributes #0 = { nounwind readnone }
attributes #1 = { nounwind }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!2}
!dx.shaderModel = !{!3}
!dx.entryPoints = !{!4}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 1, i32 5}
!2 = !{i32 1, i32 6}
!3 = !{!"as", i32 6, i32 5}
!4 = !{void ()* @AmplificationMain, !"AmplificationMain", null, null, !5}
!5 = !{i32 10, !6}
!6 = !{!7, i32 4104}
!7 = !{i32 128, i32 1, i32 1}
!8 = !{!9, !9, i64 0}
!9 = !{!"int", !10, i64 0}
!10 = !{!"omnipotent char", !11, i64 0}
!11 = !{!"Simple C/C++ TBAA"}
