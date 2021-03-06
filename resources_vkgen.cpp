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
/* Contact ckubisch@nvidia.com (Christoph Kubisch) for feedback */

#include "resources_vkgen.hpp"
#include <algorithm>

extern bool vulkanIsExtensionSupported(const char* name);


namespace csfthreaded {

  void ResourcesVKGen::init()
  {
    ResourcesVK::init();

    m_generatedCommandsSupport = load_VK_NVX_device_generated_commands(m_instance, vkGetInstanceProcAddr) ? true : false;
    assert(m_generatedCommandsSupport);
  }

  bool ResourcesVKGen::isAvailable()
  {
    return ResourcesVK::isAvailable() && vulkanIsExtensionSupported(VK_NVX_DEVICE_GENERATED_COMMANDS_EXTENSION_NAME);
  }

  bool ResourcesVKGen::initScene( const CadScene&scene )
  {
    bool okay = ResourcesVK::initScene(scene);

    if (okay){
      initObjectTable();
      return true;
    }
    return false;
  }

  void ResourcesVKGen::deinitScene()
  {
    synchronize();
    deinitObjectTable();

    ResourcesVK::deinitScene();
  }

  void ResourcesVKGen::updatedPrograms( nv_helpers_gl::ProgramManager &mgr )
  {
    ResourcesVK::updatedPrograms(mgr);
    updateObjectTablePipelines();
  }

  void ResourcesVKGen::initObjectTable()
  {
    VkObjectEntryTypeNVX restypes[] = {
      VK_OBJECT_ENTRY_PIPELINE_NVX,
      VK_OBJECT_ENTRY_INDEX_BUFFER_NVX,
      VK_OBJECT_ENTRY_VERTEX_BUFFER_NVX,
#if UNIFORMS_TECHNIQUE == UNIFORMS_PUSHCONSTANTS_INDEX
      VK_OBJECT_ENTRY_PUSH_CONSTANT_NVX
#elif UNIFORMS_TECHNIQUE == UNIFORMS_MULTISETSDYNAMIC || UNIFORMS_TECHNIQUE == UNIFORMS_MULTISETSSTATIC
      VK_OBJECT_ENTRY_DESCRIPTOR_SET_NVX
#else
  #error not implemented
#endif
    };
    VkObjectEntryUsageFlagsNVX resflags[] = {
      VK_OBJECT_ENTRY_USAGE_GRAPHICS_BIT_NVX,
      VK_OBJECT_ENTRY_USAGE_GRAPHICS_BIT_NVX,
      VK_OBJECT_ENTRY_USAGE_GRAPHICS_BIT_NVX,
      VK_OBJECT_ENTRY_USAGE_GRAPHICS_BIT_NVX,
    };

    uint32_t rescounts[] = {
      NUM_TABLE_PIPES,
#if USE_SINGLE_GEOMETRY_BUFFERS
      1,
      1,
#else
      m_geometry.size(),
      m_geometry.size(),
#endif
#if UNIFORMS_TECHNIQUE == UNIFORMS_MULTISETSDYNAMIC
      2
#elif UNIFORMS_TECHNIQUE == UNIFORMS_MULTISETSSTATIC
      m_descriptorSetsMaterials.size() + m_descriptorSetsMatrices.size()
#elif UNIFORMS_TECHNIQUE == UNIFORMS_PUSHCONSTANTS_INDEX
      2
#endif
    };

    assert(NV_ARRAYSIZE(restypes) == NV_ARRAYSIZE(rescounts));
    assert(NV_ARRAYSIZE(restypes) == NV_ARRAYSIZE(resflags));

    VkObjectTableCreateInfoNVX createInfo = { VkStructureType(VK_STRUCTURE_TYPE_OBJECT_TABLE_CREATE_INFO_NVX) };
    createInfo.objectCount = NV_ARRAYSIZE(restypes);
    createInfo.pObjectEntryCounts = rescounts;
    createInfo.pObjectEntryTypes = restypes;
    createInfo.pObjectEntryUsageFlags = resflags;

    createInfo.maxUniformBuffersPerDescriptor = 1;
    createInfo.maxStorageBuffersPerDescriptor = 0;
    createInfo.maxStorageImagesPerDescriptor  = 0;
    createInfo.maxSampledImagesPerDescriptor  = 0;
    createInfo.maxPipelineLayouts             = 1;

    VkResult result;
    result = vkCreateObjectTableNVX(m_device, &createInfo, NULL, &m_table.objectTable);
    assert(result == VK_SUCCESS);

    updateObjectTablePipelines();

    uint32_t resIndex;
    VkObjectTableEntryNVX* resEntry;

    {
      VkObjectTableIndexBufferEntryNVX  iboentry = { VK_OBJECT_ENTRY_INDEX_BUFFER_NVX,  VK_OBJECT_ENTRY_USAGE_GRAPHICS_BIT_NVX };
      VkObjectTableVertexBufferEntryNVX vboentry = { VK_OBJECT_ENTRY_VERTEX_BUFFER_NVX, VK_OBJECT_ENTRY_USAGE_GRAPHICS_BIT_NVX };

#if USE_SINGLE_GEOMETRY_BUFFERS
      {
        size_t i = 0;
        iboentry.buffer = buffers.ibo;
        iboentry.indexType = VK_INDEX_TYPE_UINT32;
        vboentry.buffer = buffers.vbo;
#else
      for (size_t i = 0 ; i < m_geometry.size(); i++){
        Geometry& geom = m_geometry[i];

        iboentry.buffer = geom.ibo;
        iboentry.indexType = VK_INDEX_TYPE_UINT32;
        vboentry.buffer = geom.vbo;
#endif

        resIndex = (uint32_t)i;

        resEntry = (VkObjectTableEntryNVX*)&iboentry;
        result = vkRegisterObjectsNVX(m_device, m_table.objectTable, 1, &resEntry, &resIndex);
        assert(result == VK_SUCCESS);
        m_table.indexBuffers.push_back(resIndex);

        resEntry = (VkObjectTableEntryNVX*)&vboentry;
        result = vkRegisterObjectsNVX(m_device, m_table.objectTable, 1, &resEntry, &resIndex);
        assert(result == VK_SUCCESS);
        m_table.vertexBuffers.push_back(resIndex);
      }
    }

#if UNIFORMS_TECHNIQUE == UNIFORMS_MULTISETSDYNAMIC
    {
      VkObjectTableDescriptorSetEntryNVX descrentry = { VK_OBJECT_ENTRY_DESCRIPTOR_SET_NVX, VK_OBJECT_ENTRY_USAGE_GRAPHICS_BIT_NVX };
      descrentry.pipelineLayout = m_pipelineLayout;
      resEntry = (VkObjectTableEntryNVX*)&descrentry;

      resIndex = 0;
      descrentry.descriptorSet = m_descriptorSet[UBO_MATRIX];
      result = vkRegisterObjectsNVX(m_device, m_table.objectTable, 1, &resEntry, &resIndex);
      assert(result == VK_SUCCESS);
      m_table.matrixDescriptorSets.push_back(resIndex);

      resIndex = 1;
      descrentry.descriptorSet = m_descriptorSet[UBO_MATERIAL];
      result = vkRegisterObjectsNVX(m_device, m_table.objectTable, 1, &resEntry, &resIndex);
      assert(result == VK_SUCCESS);
      m_table.materialDescriptorSets.push_back(resIndex);
    }
#elif UNIFORMS_TECHNIQUE == UNIFORMS_MULTISETSSTATIC
    {
      VkObjectTableDescriptorSetEntryNVX descrentry = { VK_OBJECT_ENTRY_DESCRIPTOR_SET_NVX, VK_OBJECT_ENTRY_USAGE_GRAPHICS_BIT_NVX };
      descrentry.pipelineLayout = m_pipelineLayout;
      resEntry = (VkObjectTableEntryNVX*)&descrentry;

      for (size_t i = 0; i < m_descriptorSetsMatrices.size(); i++){
        descrentry.descriptorSet = m_descriptorSetsMatrices[i];

        resIndex = i;
        result = vkRegisterObjectsNVX(m_device, m_table.objectTable, 1, &resEntry, &resIndex);
        assert(result == VK_SUCCESS);
        m_table.matrixDescriptorSets.push_back(resIndex);
      }

      for (size_t i = 0; i < m_descriptorSetsMaterials.size(); i++){
        descrentry.descriptorSet = m_descriptorSetsMaterials[i];

        resIndex = i + m_descriptorSetsMatrices.size();
        result = vkRegisterObjectsNVX(m_device, m_table.objectTable, 1, &resEntry, &resIndex);
        assert(result == VK_SUCCESS);
        m_table.materialDescriptorSets.push_back(resIndex);
      }
    }
#elif UNIFORMS_TECHNIQUE == UNIFORMS_PUSHCONSTANTS_INDEX
    {
      VkObjectTablePushConstantEntryNVX pushentry = { VK_OBJECT_ENTRY_PUSH_CONSTANT_NVX, VK_OBJECT_ENTRY_USAGE_GRAPHICS_BIT_NVX };
      pushentry.pipelineLayout = m_pipelineLayout;
      resEntry = (VkObjectTableEntryNVX*)&pushentry;

      uint32_t resIndex;
      resIndex = 0;
      pushentry.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
      result = vkRegisterObjectsNVX(m_device, m_table.objectTable, 1, &resEntry, &resIndex);
      assert(result == VK_SUCCESS);
      m_table.pushConstants.push_back(resIndex);

      resIndex = 1;
      pushentry.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
      result = vkRegisterObjectsNVX(m_device, m_table.objectTable, 1, &resEntry, &resIndex);
      assert(result == VK_SUCCESS);
      m_table.pushConstants.push_back(resIndex);
    }
#endif
  }


  void ResourcesVKGen::deinitObjectTable()
  {
    vkDestroyObjectTableNVX(m_device, m_table.objectTable, NULL);
    m_table = TableContent(); // reset
  }

  void ResourcesVKGen::updateObjectTablePipelines()
  {
    VkResult result;
    if (!m_table.objectTable) return;

    uint32_t resIndex;
    VkObjectEntryTypeNVX resType;

    if (!m_table.pipelines.empty()){
      resType = VK_OBJECT_ENTRY_PIPELINE_NVX;
      for (size_t i = 0; i < m_table.pipelines.size(); i++){
        resIndex = m_table.pipelines[i];
        vkUnregisterObjectsNVX(m_device, m_table.objectTable, 1, &resType, &resIndex);
      }
      m_table.pipelines.clear();
    }

    {
      VkObjectTablePipelineEntryNVX entry = { (VK_OBJECT_ENTRY_PIPELINE_NVX) };
      VkObjectTableEntryNVX* resEntry = (VkObjectTableEntryNVX*)&entry;
      resIndex = TABLE_PIPE_LINES;
      entry.pipeline = pipes.line;
      result = vkRegisterObjectsNVX(m_device, m_table.objectTable, 1, &resEntry, &resIndex);
      assert(result == VK_SUCCESS);
      m_table.pipelines.push_back(resIndex);

      resIndex = TABLE_PIPE_LINES_TRIANGLES;
      entry.pipeline = pipes.line_tris;
      result = vkRegisterObjectsNVX(m_device, m_table.objectTable, 1, &resEntry, &resIndex);
      assert(result == VK_SUCCESS);
      m_table.pipelines.push_back(resIndex);

      resIndex = TABLE_PIPE_TRIANGLES;
      entry.pipeline = pipes.tris;
      result = vkRegisterObjectsNVX(m_device, m_table.objectTable, 1, &resEntry, &resIndex);
      assert(result == VK_SUCCESS);
      m_table.pipelines.push_back(resIndex);
    }

    m_table.resourceIncarnation = m_pipeIncarnation;
  }

}

