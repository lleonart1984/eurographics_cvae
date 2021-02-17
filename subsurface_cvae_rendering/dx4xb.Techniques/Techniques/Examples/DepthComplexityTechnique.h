#pragma once

#include "dx4xb_scene.h"
#include "gui_traits.h"

class DepthComplexityTechnique : public Technique, public IManageScene {
public:

	~DepthComplexityTechnique() {}

	gObj<Buffer> VertexBuffer;
	gObj<Buffer> IndexBuffer;
	gObj<Buffer> Camera;
	gObj<Buffer> GeometryTransforms;
	gObj<Buffer> InstanceTransforms;
	gObj<Texture2D> Complexity;

	struct CameraCB {
		float4x4 Projection;
		float4x4 View;
	};

	struct Pipeline : public GraphicsPipeline {

		gObj<Texture2D> CountingBuffer;
		gObj<Buffer> InstanceTransforms;
		gObj<Buffer> GeometryTransforms;
		gObj<Buffer> Camera;

		struct ObjectInfoCB {
			int InstanceIndex;
			int TransformIndex;
			int MaterialIndex;
		} ObjectInfo;

		// Inherited via GraphicsPipelineBindings
		void Setup() override {
			set->VertexShader(ShaderLoader::FromFile("./Techniques/Examples/Basic_VS.cso"));
			set->PixelShader(ShaderLoader::FromFile("./Techniques/Examples/Counting_PS.cso"));
			set->InputLayout(SceneVertex::Layout());
		}

		void Bindings(gObj<GraphicsBinder> binder) {
			binder->OnSet(); {
				binder->PixelShader();
				binder->UAV(0, CountingBuffer);

				binder->VertexShader();
				binder->SRV(0, InstanceTransforms);
				binder->SRV(1, GeometryTransforms);
				binder->CBV(0, Camera);
			}

			binder->OnDispatch(); {
				binder->VertexShader();
				binder->CBV(1, ObjectInfo);
			}
		}
	};
	gObj<Pipeline> pipeline;
	gObj<ShowComplexityPipeline> showComplexity;
	gObj<Buffer> screenVertices;

	// Inherited via Technique
	virtual void OnLoad() override {

		auto desc = scene->getScene();

		// Allocate Memory for scene elements
		VertexBuffer = CreateBufferVB<SceneVertex>(desc->Vertices().Count);
		IndexBuffer = CreateBufferIB<int>(desc->Indices().Count);
		Camera = CreateBufferCB<CameraCB>();
		GeometryTransforms = CreateBufferSRV<float4x3>(desc->getTransformsBuffer().Count);
		InstanceTransforms = CreateBufferSRV<float4x4>(desc->Instances().Count);
		Complexity = CreateTexture2DUAV<uint>(CurrentRenderTarget()->Width(), CurrentRenderTarget()->Height());

		Load(showComplexity);
		Load(pipeline);
		
		pipeline->Camera = Camera;
		pipeline->GeometryTransforms = GeometryTransforms;
		pipeline->InstanceTransforms = InstanceTransforms;
		pipeline->CountingBuffer = CreateTexture2DDSV(CurrentRenderTarget()->Width(), CurrentRenderTarget()->Height());

		screenVertices = CreateBufferVB<float2>(6);
		screenVertices->Write({
			float2(-1, -1), float2(1, -1), float2(1, 1),
			float2(-1, -1), float2(1, 1), float2(-1, 1)
			});

		Execute_OnGPU(UpdateDirtyElements);
		Execute_OnGPU(LoadScreenVertices);
	}

	void LoadScreenVertices(gObj<GraphicsManager> manager) {
		manager->ToGPU(screenVertices);
	}

	void UpdateDirtyElements(gObj<GraphicsManager> manager) {

		auto elements = scene->Updated(sceneVersion);
		auto desc = scene->getScene();

		if (+(elements & SceneElement::Vertices))
		{
			VertexBuffer->Write(desc->Vertices().Data);
			manager->ToGPU(VertexBuffer);
		}

		if (+(elements & SceneElement::Indices))
		{
			IndexBuffer->Write(desc->Indices().Data);
			manager->ToGPU(IndexBuffer);
		}

		if (+(elements & SceneElement::Camera))
		{
			float4x4 proj, view;
			scene->getCamera().GetMatrices(CurrentRenderTarget()->Width(), CurrentRenderTarget()->Height(), view, proj);
			Camera->Write(CameraCB{
					proj,
					view
				});
			manager->ToGPU(Camera);
		}

		if (+(elements & SceneElement::GeometryTransforms))
		{
			GeometryTransforms->Write(desc->getTransformsBuffer().Data);
			manager->ToGPU(GeometryTransforms);
		}

		if (+(elements & SceneElement::InstanceTransforms))
		{
			float4x4* transforms = new float4x4[desc->Instances().Count];
			for (int i = 0; i < desc->Instances().Count; i++)
				transforms[i] = desc->Instances().Data[i].Transform;

			InstanceTransforms->Write(transforms);
			manager->ToGPU(InstanceTransforms);
			delete[] transforms;
		}
	}

	virtual void OnDispatch() override {
		// Update dirty elements
		Execute_OnGPU(UpdateDirtyElements);

		// Draw current Frame
		Execute_OnGPU(DrawScene);
		Execute_OnGPU(DrawComplexity);
	}

	void DrawScene(gObj<GraphicsManager> manager) {
		pipeline->CountingBuffer = Complexity;

		manager->SetPipeline(pipeline);
		manager->IndexBuffer(IndexBuffer);
		manager->Viewport(CurrentRenderTarget()->Width(), CurrentRenderTarget()->Height());

		manager->ClearRenderTarget(CurrentRenderTarget(), float3(0.2f, 0.2f, 0.5f));
		manager->ClearUAV(Complexity, uint4(0));

		auto desc = scene->getScene();

		for (int i = 0; i < desc->Instances().Count; i++) {
			pipeline->ObjectInfo.InstanceIndex = i;
			InstanceDescription instance = desc->Instances().Data[i];
			for (int j = 0; j < instance.Count; j++) {
				GeometryDescription geometry = desc->Geometries().Data[instance.GeometryIndices[j]];

				manager->VertexBuffer(VertexBuffer->Slice(geometry.StartVertex, geometry.VertexCount));

				pipeline->ObjectInfo.TransformIndex = geometry.TransformIndex;
				pipeline->ObjectInfo.MaterialIndex = geometry.MaterialIndex;
				manager->DrawIndexedTriangles(geometry.IndexCount, geometry.StartIndex);
			}
		}
	}

	void DrawComplexity(gObj<GraphicsManager> manager) {
		showComplexity->RenderTarget = CurrentRenderTarget();
		showComplexity->Complexity = Complexity;

		manager->SetPipeline(showComplexity);
		manager->VertexBuffer(screenVertices);
		manager->Viewport(CurrentRenderTarget()->Width(), CurrentRenderTarget()->Height());
		manager->DrawTriangles(6);
	}
};