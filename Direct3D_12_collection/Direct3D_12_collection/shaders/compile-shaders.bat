set cmd="C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\dxc.exe"
%cmd%  -T vs_6_5  -E VSMain  -Fo basic.vert.cso  -Fc basic.vert.asm  basic.vert.hlsl
%cmd%  -T ps_6_5  -E PSMain  -Fo basic.frag.cso  -Fc basic.frag.asm  basic.frag.hlsl
%cmd%  -T as_6_5  -E AmplificationMain  -Fo ms.amplification.cso  -Fc ms.amplification.asm  ms.amplification.hlsl
%cmd%  -T ms_6_5  -E MeshMain  -Fo ms.mesh.cso  -Fc ms.mesh.asm  ms.mesh.hlsl

