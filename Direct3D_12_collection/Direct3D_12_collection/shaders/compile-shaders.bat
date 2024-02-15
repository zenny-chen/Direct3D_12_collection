set cmd="C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\dxc.exe"
%cmd%  -T vs_6_5  -E VSMain  -Fo ../cso/basic.vert.cso  -Fc ../cso/basic.vert.asm  basic.vert.hlsl
%cmd%  -T ps_6_5  -E PSMain  -Fo ../cso/basic.frag.cso  -Fc ../cso/basic.frag.asm  basic.frag.hlsl
%cmd%  -T as_6_5  -E AmplificationMain  -Fo ../cso/ms.amplification.cso  -Fc ../cso/ms.amplification.asm  ms.amplification.hlsl
%cmd%  -T ms_6_5  -E MeshMain  -Fo ../cso/ms.mesh.cso  -Fc ../cso/ms.mesh.asm  ms.mesh.hlsl
%cmd%  -T ms_6_5  -E MeshMain  -Fo ../cso/msonly.mesh.cso  ../cso/msonly.mesh.hlsl
%cmd%  -T ms_6_5  -E MeshMain  -Fo ../cso/simple_ms.mesh.cso  ../cso/simple_ms.mesh.hlsl

