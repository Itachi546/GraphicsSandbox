#version 460

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

#extension GL_GOOGLE_include_directive : require

#include "meshdata.glsl"

layout(push_constant) uniform Frustum {
   vec4 planes[6];
   uint nMesh;
};

layout(binding = 0) readonly buffer TransformBuffer {
   mat4 transforms[];
};

layout(binding = 1) readonly buffer MeshDrawBuffer {
  MeshDrawData meshData[];
};

layout(binding = 2) writeonly buffer DrawCommandBuffer {
  MeshDrawCommand drawCommands[];
};

layout(binding = 3) writeonly buffer DrawCommandCount {
  uint drawCommandCount;
};

layout(binding = 4) writeonly buffer TotalVisibleCount {
  uint visibleMeshCount;
};

void main() {
   //uint ti = gl_LocalInvocationID.x;
   //uint gi = gl_WorkGroupID.x;
   //uint di = gi * 32 + ti;
   uint di = gl_GlobalInvocationID.x;
   if(di < nMesh) {
      MeshDrawData mesh = meshData[di];
	  mat4 transform = transforms[di];

	  vec3 center = vec3(transform * vec4(mesh.boundingSphere.xyz, 1.0f));
	  float radius = mesh.boundingSphere.w;
	  bool visible = true;
	  for(int i = 0; i < 6; ++i) {
	     vec4 plane = planes[i];
		 if((dot(plane.xyz, center) + plane.w) < -radius)
		 {
		   visible = false;
		   break;
		 }
	  }

	  if(visible) {
        atomicAdd(visibleMeshCount, 1);
        uint dci = atomicAdd(drawCommandCount, 1);
        drawCommands[dci].drawId = di;
		drawCommands[dci].indexCount = mesh.indexCount;
		drawCommands[dci].instanceCount	=	1;
		drawCommands[dci].firstIndex = mesh.indexOffset;
		drawCommands[dci].vertexOffset = mesh.vertexOffset;
		drawCommands[dci].firstInstance	=	0;
		drawCommands[dci].taskCount	=	(mesh.meshletCount + 31) / 32;
		drawCommands[dci].firstTask	=	0;
	  }
   }
}