#version 440 core
/**/

//#extension GL_ARB_shading_language_include : enable
#include "common.h"

UBOBINDING(UBO_SCENE) uniform sceneBuffer {
  SceneData   scene;
};

#if USE_INDEXING
  SSBOBINDING(UBO_MATRIX) buffer matrixBuffer {
    MatrixData    matrices[];
  };
#else
  MATRIX_BINDING uniform matrixBuffer {
    MATRIX_LAYOUT
    ObjectData    object;
  };
#endif


in layout(location=VERTEX_POS)      vec3 pos;
in layout(location=VERTEX_NORMAL)   vec3 normal;

out Interpolants {
  vec3 wPos;
  vec3 wNormal;
} OUT;

void main()
{
#if USE_INDEXING
  vec3 wPos     = (matrices[matrixIndex].worldMatrix   * vec4(pos,1)).xyz;
  vec3 wNormal  = mat3(matrices[matrixIndex].worldMatrixIT) * normal;
#else
  vec3 wPos     = (object.worldMatrix   * vec4(pos,1)).xyz;
  vec3 wNormal  = mat3(object.worldMatrixIT) * normal;
#endif

  gl_Position   = scene.viewProjMatrix * vec4(wPos,1);
  OUT.wPos = wPos;
  OUT.wNormal = wNormal;
}

/*-----------------------------------------------------------------------
  Copyright (c) 2014-2016, NVIDIA. All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
   * Neither the name of its contributors may be used to endorse 
     or promote products derived from this software without specific
     prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
-----------------------------------------------------------------------*/
