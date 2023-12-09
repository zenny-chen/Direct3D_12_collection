set cmd="C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\dxc.exe"
%cmd%  -T vs_6_5  -E VSMain  -Fo basic.vert.cso  -Fc basic.vert.asm  basic.vert.hlsl
%cmd%  -T vs_6_5  -E VSMain  -Fo basic_texture.vert.cso  basic_texture.vert.hlsl
%cmd%  -T vs_6_5  -E VSMain  -Fo tfb_basic.vert.cso  -Fc tfb_basic.vert.asm  tfb_basic.vert.hlsl
%cmd%  -T vs_6_5  -E VSMain  -Fo proj_test.vs.cso  proj_test.vs.hlsl
%cmd%  -T vs_6_5  -E VSMain  -Fo vrs.vert.cso  vrs.vert.hlsl
%cmd%  -T vs_6_5  -E VSMain  -Fo cr.vert.cso  cr.vert.hlsl
%cmd%  -T vs_6_5  -E VSMain  -Fo cr_present.vert.cso  cr_present.vert.hlsl
%cmd%  -T ps_6_5  -E PSMain  -Fo basic.frag.cso  -Fc basic.frag.asm  basic.frag.hlsl
%cmd%  -T ps_6_5  -E PSMain  -Fo basic_texture.frag.cso  basic_texture.frag.hlsl
%cmd%  -T ps_6_5  -E PSMain  -Fo proj_test.ps.cso  proj_test.ps.hlsl
%cmd%  -T ps_6_5  -E PSMain  -Fo vrs.frag.cso  vrs.frag.hlsl
%cmd%  -T ps_6_5  -E PSMain  -Fo cr.frag.cso  cr.frag.hlsl
%cmd%  -T ps_6_5  -E PSMain  -Fo cr_present.frag.cso  cr_present.frag.hlsl
%cmd%  -T cs_6_5  -E CSMain  -Fo cr.comp.cso  cr.comp.hlsl
%cmd%  -T as_6_5  -E AmplificationMain  -Fo ms.amplification.cso  -Fc ms.amplification.asm  ms.amplification.hlsl
%cmd%  -T ms_6_5  -E MeshMain  -Fo ms.mesh.cso  -Fc ms.mesh.asm  ms.mesh.hlsl
%cmd%  -T ms_6_5  -E MeshMain  -Fo msonly.mesh.cso  msonly.mesh.hlsl
%cmd%  -T ms_6_5  -E MeshMain  -Fo simple_ms.mesh.cso  simple_ms.mesh.hlsl

